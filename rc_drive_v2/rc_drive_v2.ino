/**
 * rc_drive_v2.ino — Remote Mower V2 with Web RPC Control
 * RC control via Flysky FS-iA6B receiver (iBUS protocol) + Linux RPC bridge
 * Target: Arduino UNO Q (STM32U585 MCU)
 * Rev 2.0 · 2026-03-23
 *
 * Wiring:
 *   FS-iA6B iBUS port  →  D0 (Serial1 RX)
 *   FS-iA6B VCC        →  5V (from buck converter)
 *   FS-iA6B GND        →  GND
 *   Cytron MDD10A DIR1 →  D2
 *   Cytron MDD10A PWM1 →  D3  [left motor]
 *   Cytron MDD10A DIR2 →  D4
 *   Cytron MDD10A PWM2 →  D5  [right motor]
 *   Serial2            →  Linux bridge (UNO Q Linux side)
 *
 * Channel mapping (FS-i6X):
 *   CH1 = right stick horizontal → rudder (turn left/right)  [spring-centered]
 *   CH2 = right stick vertical   → throttle (forward/back)   [spring-centered]
 *   CH5 = SWA switch             → kill switch (UP=kill, DOWN=armed) [optional]
 *
 * Right stick is spring-centered on both axes — releasing it snaps to 1500
 * (center = stop), acting as a natural emergency stop. No startup procedure needed.
 *
 * Command priority in loop():
 *   - If web command received within last 500ms → use webLeft/webRight targets
 *   - Otherwise → fall back to iBUS RC targets
 *   RC is always the fallback — dropping WiFi/WebSocket reverts to RC control.
 */

#include <Arduino_RPClite.h>

// ── Pin definitions ────────────────────────────────────────────────────────
#define DIR1  2   // Left motor direction
#define PWM1  3   // Left motor speed (PWM)
#define DIR2  4   // Right motor direction
#define PWM2  5   // Right motor speed (PWM)

// ── iBUS config ────────────────────────────────────────────────────────────
#define IBUS_BAUD       115200
#define IBUS_PACKET_LEN 32
#define IBUS_HEADER1    0x20
#define IBUS_HEADER2    0x40
#define NUM_CHANNELS    14    // FS-iA6B supports up to 14 channels via iBUS

// ── Control config ─────────────────────────────────────────────────────────
#define CH_THROTTLE     1     // CH2 (0-indexed: CH2 = index 1) — right stick vertical, spring-centered
#define CH_RUDDER       0     // CH1 (0-indexed: CH1 = index 0) — right stick horizontal, spring-centered

#define CH_KILL         -1    // Kill switch disabled — right stick self-centers to zero
#define KILL_THRESHOLD  1500  // (unused)
#define RC_MIN          1000
#define RC_MID          1500
#define RC_MAX          2000
#define DEADBAND        30    // ±30 units around center (out of ±255)
#define MAX_SPEED       255
#define RAMP_STEP       20    // Max speed change per loop iteration
#define WATCHDOG_MS     500   // Stop motors if no packet for 500ms
#define WEB_TIMEOUT_MS  500   // Fall back to RC if no web command for 500ms

// ── RPC transport (Serial2 = Linux bridge) ─────────────────────────────────
// NOTE: Verify Serial2 is the Linux bridge port on UNO Q STM32
SerialTransport transport(Serial2);
RPCServer server(transport);

// ── State ──────────────────────────────────────────────────────────────────
uint16_t channels[NUM_CHANNELS];
uint8_t  ibusBuffer[IBUS_PACKET_LEN];
uint8_t  bufIdx = 0;
unsigned long lastPacketMs = 0;

int currentLeft  = 0;
int currentRight = 0;

// Web command state
int webLeft  = 0;
int webRight = 0;
unsigned long lastWebCmdMs = 0;

// ── RPC handlers ───────────────────────────────────────────────────────────

/**
 * RPC: drive(left, right)
 * Sets web drive targets. Values clamped to -255..255.
 */
void rpc_drive(int left, int right) {
  webLeft  = constrain(left,  -MAX_SPEED, MAX_SPEED);
  webRight = constrain(right, -MAX_SPEED, MAX_SPEED);
  lastWebCmdMs = millis();
}

/**
 * RPC: stop()
 * Immediately zeros web drive targets.
 */
void rpc_stop() {
  webLeft  = 0;
  webRight = 0;
  lastWebCmdMs = millis();
}

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  pinMode(DIR1, OUTPUT);
  pinMode(PWM1, OUTPUT);
  pinMode(DIR2, OUTPUT);
  pinMode(PWM2, OUTPUT);

  // Start with motors stopped
  setMotor(DIR1, PWM1, 0);
  setMotor(DIR2, PWM2, 0);

  // iBUS on Serial1 (hardware UART, D0=RX)
  Serial1.begin(IBUS_BAUD);

  // RPC server on Serial2 (Linux bridge)
  // NOTE: Verify Serial2 is the Linux bridge port on UNO Q STM32
  Serial2.begin(115200);
  server.bind("drive", rpc_drive);
  server.bind("stop",  rpc_stop);

  // Debug output on USB Serial (optional — comment out to save flash)
  Serial.begin(115200);
  Serial.println("RC Drive V2 ready — iBUS + RPC web control");
}

// ── Main loop ──────────────────────────────────────────────────────────────
void loop() {
  // Process incoming RPC messages from Linux
  server.run();

  // Read incoming iBUS bytes
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    ibusBuffer[bufIdx++] = b;

    // Sync on header bytes
    if (bufIdx == 1 && b != IBUS_HEADER1) { bufIdx = 0; continue; }
    if (bufIdx == 2 && b != IBUS_HEADER2) { bufIdx = 0; continue; }

    // Full packet received
    if (bufIdx == IBUS_PACKET_LEN) {
      if (verifyChecksum()) {
        parseChannels();
        lastPacketMs = millis();
      }
      bufIdx = 0;
    }
  }

  // Watchdog: stop motors if RC signal lost AND no web commands active
  bool webActive = (millis() - lastWebCmdMs < WEB_TIMEOUT_MS);
  if (!webActive && millis() - lastPacketMs > WATCHDOG_MS) {
    currentLeft  = rampToward(currentLeft,  0);
    currentRight = rampToward(currentRight, 0);
    setMotor(DIR1, PWM1, currentLeft);
    setMotor(DIR2, PWM2, currentRight);
    return;
  }

  int targetLeft, targetRight;

  if (webActive) {
    // Web command takes priority
    targetLeft  = webLeft;
    targetRight = webRight;
  } else {
    // Fall back to iBUS RC control
    int throttle = rcToSpeed(channels[CH_THROTTLE]);
    int rudder   = rcToSpeed(channels[CH_RUDDER]);

    throttle = applyDeadband(throttle);
    rudder   = applyDeadband(rudder);

    targetLeft  = constrain(throttle + rudder, -MAX_SPEED, MAX_SPEED);
    targetRight = constrain(throttle - rudder, -MAX_SPEED, MAX_SPEED);
  }

  // Ramp toward target (smooth acceleration)
  currentLeft  = rampToward(currentLeft,  targetLeft);
  currentRight = rampToward(currentRight, targetRight);

  setMotor(DIR1, PWM1, currentLeft);
  setMotor(DIR2, PWM2, currentRight);

  delay(20);  // ~50Hz loop
}

// ── iBUS parsing ───────────────────────────────────────────────────────────

/**
 * Verify iBUS checksum.
 * Checksum = 0xFFFF - sum of first 30 bytes
 */
bool verifyChecksum() {
  uint16_t sum = 0;
  for (int i = 0; i < IBUS_PACKET_LEN - 2; i++) {
    sum += ibusBuffer[i];
  }
  uint16_t checksum = 0xFFFF - sum;
  uint16_t received = ibusBuffer[IBUS_PACKET_LEN - 2] |
                      (ibusBuffer[IBUS_PACKET_LEN - 1] << 8);
  return checksum == received;
}

/**
 * Parse 14 channel values from verified iBUS packet.
 * Each channel is 2 bytes, little-endian, starting at byte 2.
 */
void parseChannels() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i] = ibusBuffer[2 + i * 2] | (ibusBuffer[3 + i * 2] << 8);
  }
}

// ── Control math ───────────────────────────────────────────────────────────

/**
 * Map RC channel value (1000–2000) to motor speed (-255 to 255).
 */
int rcToSpeed(uint16_t rcVal) {
  rcVal = constrain(rcVal, RC_MIN, RC_MAX);
  return map(rcVal, RC_MIN, RC_MAX, -MAX_SPEED, MAX_SPEED);
}

/**
 * Apply deadband around zero. Values within ±deadband become 0.
 */
int applyDeadband(int val) {
  if (abs(val) < DEADBAND) return 0;
  return val;
}

/**
 * Ramp current value toward target by at most RAMP_STEP per call.
 */
int rampToward(int current, int target) {
  int diff = target - current;
  if (abs(diff) <= RAMP_STEP) return target;
  return current + (diff > 0 ? RAMP_STEP : -RAMP_STEP);
}

// ── Motor output ───────────────────────────────────────────────────────────

/**
 * Drive a Cytron MDD10A motor channel.
 * speed: -255 (full reverse) to 255 (full forward), 0 = stop
 */
void setMotor(int dirPin, int pwmPin, int speed) {
  if (speed >= 0) {
    digitalWrite(dirPin, HIGH);
    analogWrite(pwmPin, speed);
  } else {
    digitalWrite(dirPin, LOW);
    analogWrite(pwmPin, -speed);
  }
}
