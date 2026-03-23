// Remote Mower — App Lab Sketch
// Rev 1.0 · 2026-03-23
//
// Dual control: Flysky FS-iA6B RC receiver (iBUS) + Linux WebSocket (via Bridge RPC)
// RC is always active as fallback. Web commands take priority for 500ms after receipt.
//
// Wiring:
//   FS-iA6B iBUS port  →  Serial1 RX (D0)
//   FS-iA6B VCC        →  5V
//   FS-iA6B GND        →  GND
//   Cytron MDD10A DIR1 →  D2  (left motor direction)
//   Cytron MDD10A PWM1 →  D3  (left motor speed)
//   Cytron MDD10A DIR2 →  D4  (right motor direction)
//   Cytron MDD10A PWM2 →  D5  (right motor speed)
//
// RC Channel mapping (FS-i6X Mode 2):
//   CH1 = right stick horizontal → rudder  [spring-centered]
//   CH2 = right stick vertical   → throttle [spring-centered]

#include "Arduino_RouterBridge.h"

// ── Pin definitions ─────────────────────────────────────────────────────────
#define DIR1  2
#define PWM1  3
#define DIR2  4
#define PWM2  5

// ── iBUS config ─────────────────────────────────────────────────────────────
#define IBUS_BAUD        115200
#define IBUS_PACKET_LEN  32
#define IBUS_HEADER1     0x20
#define IBUS_HEADER2     0x40
#define NUM_CHANNELS     14

// ── Control config ──────────────────────────────────────────────────────────
#define CH_THROTTLE      1     // CH2 (0-indexed) — right stick vertical
#define CH_RUDDER        0     // CH1 (0-indexed) — right stick horizontal
#define RC_MIN           1000
#define RC_MAX           2000
#define DEADBAND         30
#define MAX_SPEED        255
#define RAMP_STEP        20
#define RC_WATCHDOG_MS   500   // Stop motors if no iBUS packet
#define WEB_PRIORITY_MS  500   // Web command takes priority for this long

// ── State ───────────────────────────────────────────────────────────────────
uint16_t channels[NUM_CHANNELS];
uint8_t  ibusBuffer[IBUS_PACKET_LEN];
uint8_t  bufIdx = 0;
unsigned long lastIbusPacketMs = 0;

int currentLeft  = 0;
int currentRight = 0;

// Web control state
int webLeft  = 0;
int webRight = 0;
unsigned long lastWebCmdMs = 0;

// ── RPC functions (called from Python via Bridge) ───────────────────────────

// drive(left, right): set motor targets from web (-255 to 255)
void drive(int left, int right) {
  webLeft  = constrain(left,  -MAX_SPEED, MAX_SPEED);
  webRight = constrain(right, -MAX_SPEED, MAX_SPEED);
  lastWebCmdMs = millis();
}

// stop(): emergency stop from web
void stop_motors() {
  webLeft  = 0;
  webRight = 0;
  lastWebCmdMs = millis();
}

// ── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  pinMode(DIR1, OUTPUT);
  pinMode(PWM1, OUTPUT);
  pinMode(DIR2, OUTPUT);
  pinMode(PWM2, OUTPUT);

  setMotor(DIR1, PWM1, 0);
  setMotor(DIR2, PWM2, 0);

  // iBUS from RC receiver
  Serial1.begin(IBUS_BAUD);

  // Linux Bridge RPC
  Bridge.begin();
  Bridge.provide("drive", drive);
  Bridge.provide("stop", stop_motors);
}

// ── Main loop ───────────────────────────────────────────────────────────────
void loop() {
  // Read iBUS packets
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    ibusBuffer[bufIdx++] = b;
    if (bufIdx == 1 && b != IBUS_HEADER1) { bufIdx = 0; continue; }
    if (bufIdx == 2 && b != IBUS_HEADER2) { bufIdx = 0; continue; }
    if (bufIdx == IBUS_PACKET_LEN) {
      if (verifyChecksum()) {
        parseChannels();
        lastIbusPacketMs = millis();
      }
      bufIdx = 0;
    }
  }

  int targetLeft, targetRight;

  // Web command takes priority for WEB_PRIORITY_MS after last web command
  if (millis() - lastWebCmdMs < WEB_PRIORITY_MS) {
    targetLeft  = webLeft;
    targetRight = webRight;
  }
  // RC control (with watchdog)
  else if (millis() - lastIbusPacketMs < RC_WATCHDOG_MS) {
    int throttle = applyDeadband(rcToSpeed(channels[CH_THROTTLE]));
    int rudder   = applyDeadband(rcToSpeed(channels[CH_RUDDER]));
    targetLeft   = constrain(throttle + rudder, -MAX_SPEED, MAX_SPEED);
    targetRight  = constrain(throttle - rudder, -MAX_SPEED, MAX_SPEED);
  }
  // No signal from either source — stop
  else {
    targetLeft  = 0;
    targetRight = 0;
  }

  currentLeft  = rampToward(currentLeft,  targetLeft);
  currentRight = rampToward(currentRight, targetRight);

  setMotor(DIR1, PWM1, currentLeft);
  setMotor(DIR2, PWM2, currentRight);

  delay(20);
}

// ── iBUS helpers ────────────────────────────────────────────────────────────
bool verifyChecksum() {
  uint16_t sum = 0;
  for (int i = 0; i < IBUS_PACKET_LEN - 2; i++) sum += ibusBuffer[i];
  uint16_t checksum = 0xFFFF - sum;
  uint16_t received = ibusBuffer[IBUS_PACKET_LEN - 2] | (ibusBuffer[IBUS_PACKET_LEN - 1] << 8);
  return checksum == received;
}

void parseChannels() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    channels[i] = ibusBuffer[2 + i * 2] | (ibusBuffer[3 + i * 2] << 8);
  }
}

// ── Control math ────────────────────────────────────────────────────────────
int rcToSpeed(uint16_t rcVal) {
  rcVal = constrain(rcVal, RC_MIN, RC_MAX);
  return map(rcVal, RC_MIN, RC_MAX, -MAX_SPEED, MAX_SPEED);
}

int applyDeadband(int val) {
  if (abs(val) < DEADBAND) return 0;
  return val;
}

int rampToward(int current, int target) {
  int diff = target - current;
  if (abs(diff) <= RAMP_STEP) return target;
  return current + (diff > 0 ? RAMP_STEP : -RAMP_STEP);
}

// ── Motor output ────────────────────────────────────────────────────────────
void setMotor(int dirPin, int pwmPin, int speed) {
  if (speed >= 0) {
    digitalWrite(dirPin, HIGH);
    analogWrite(pwmPin, speed);
  } else {
    digitalWrite(dirPin, LOW);
    analogWrite(pwmPin, -speed);
  }
}
