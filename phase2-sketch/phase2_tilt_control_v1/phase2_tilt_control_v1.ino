/*
 * phase2_tilt_control_v1.ino
 * Tilt-based differential drive using Modulino Movement (LSM6DSOX IMU)
 * Arduino UNO R4 WiFi + Cytron MDD10A dual motor driver
 *
 * Motor pins: DIR_L=4, PWM_L=3, DIR_R=7, PWM_R=6
 * I2C (QWIIC): Wire1 on R4 WiFi (auto-selected by Modulino.begin())
 * LED Matrix: 12×8 (Arduino_LED_Matrix — bundled with R4 WiFi board package)
 *
 * Libraries required:
 *   Arduino_Modulino  (Library Manager)
 *   Arduino_LED_Matrix (bundled with R4 WiFi board package — no extra install)
 *
 * NOTE: ArduinoGraphics / beginDraw are NOT used — loadFrame() only.
 *       All uint8_t[8][12] bitmaps are converted to uint32_t[3] via makeFrame().
 */

#include <Arduino_Modulino.h>
#include "Arduino_LED_Matrix.h"

// ── Motor pins ────────────────────────────────────────────────────────────────
const int DIR_L     = 4;
const int PWM_L     = 3;
const int DIR_R     = 7;
const int PWM_R     = 6;
const int MAX_SPEED = 200;

// ── Tilt tuning ───────────────────────────────────────────────────────────────
const float DEADZONE      = 0.10f;  // g — below this = stopped
const float MAX_TILT      = 0.70f;  // g — at/above this = full speed (~45°)
const float CAL_THRESHOLD = 0.30f;  // g — gesture must exceed this to register
const float FLAT_Z_MIN    = 0.85f;  // Z must be above this when flat
const float FLAT_XY_MAX   = 0.20f;  // |X| and |Y| must be below this when flat
const int   LOOP_MS       = 50;     // 20 Hz control loop

// ── Calibration result ────────────────────────────────────────────────────────
struct TiltMapping {
  int   fwdAxis;   // 0=X, 1=Y
  float fwdSign;   // +1 or -1  (positive reading → forward motion)
  int   turnAxis;  // 0=X, 1=Y
  float turnSign;  // +1 or -1  (positive reading → left turn)
  float baseX;     // resting accelerometer X
  float baseY;     // resting accelerometer Y
} mapping;

// ── Hardware objects ──────────────────────────────────────────────────────────
ModulinoMovement movement;
ArduinoLEDMatrix matrix;

// ── Frame packing ─────────────────────────────────────────────────────────────
// loadFrame() requires uint32_t[3]: 96 bits packed MSB-first (row0col0 = bit95).
// We keep bitmaps as readable uint8_t[8][12] and convert at runtime.

void makeFrame(uint8_t bmp[8][12], uint32_t out[3]) {
  out[0] = out[1] = out[2] = 0;
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 12; c++) {
      if (bmp[r][c]) {
        int idx = r * 12 + c;               // 0..95, MSB = first pixel
        out[idx / 32] |= (1UL << (31 - idx % 32));
      }
    }
  }
}

void showFrame(uint8_t bmp[8][12]) {
  uint32_t f[3];
  makeFrame(bmp, f);
  matrix.loadFrame(f);
}

void clearMatrix() {
  uint32_t blank[3] = {0, 0, 0};
  matrix.loadFrame(blank);
}

// ── LED Matrix bitmaps (8 rows × 12 cols) ────────────────────────────────────

uint8_t frameUp[8][12] = {
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,1,1,1,0,0,0,0,0},
  {0,0,0,1,1,1,1,1,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frameDown[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0},
  {0,0,0,1,1,1,1,1,0,0,0,0},
  {0,0,0,0,1,1,1,0,0,0,0,0},
  {0,0,0,0,0,1,0,0,0,0,0,0}
};

uint8_t frameLeft[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,1,0,0,0,0,0,0,0,0},
  {0,0,1,0,0,0,0,0,0,0,0,0},
  {0,1,1,1,1,1,1,1,1,0,0,0},
  {0,0,1,0,0,0,0,0,0,0,0,0},
  {0,0,0,1,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frameRight[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,0,0},
  {0,0,0,1,1,1,1,1,1,1,0,0},
  {0,0,0,0,0,0,0,0,0,1,0,0},
  {0,0,0,0,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frameCheck[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,1,0,0},
  {0,0,0,0,0,0,0,0,1,0,0,0},
  {0,0,0,0,0,0,0,1,0,0,0,0},
  {0,1,0,0,0,0,1,0,0,0,0,0},
  {0,0,1,0,0,1,0,0,0,0,0,0},
  {0,0,0,1,1,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frameFlat[8][12] = {
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,1,1,1,1,1,1,1,1,1,1,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0},
  {0,0,0,0,0,0,0,0,0,0,0,0}
};

uint8_t frameSmiley[8][12] = {
  {0,0,0,1,1,1,1,1,1,0,0,0},
  {0,0,1,0,0,0,0,0,0,1,0,0},
  {0,1,0,0,1,0,0,1,0,0,1,0},
  {0,1,0,0,0,0,0,0,0,0,1,0},
  {0,1,0,1,0,0,0,0,1,0,1,0},
  {0,1,0,0,1,1,1,1,0,0,1,0},
  {0,0,1,0,0,0,0,0,0,1,0,0},
  {0,0,0,1,1,1,1,1,1,0,0,0}
};

// ── Instruction display ───────────────────────────────────────────────────────
// 3 quick blinks of the frame ("pay attention"), then hold steady ("do this now")
void showInstruction(uint8_t bmp[8][12]) {
  for (int i = 0; i < 3; i++) {
    showFrame(bmp);
    delay(200);
    clearMatrix();
    delay(150);
  }
  showFrame(bmp);  // hold steady until gesture detected
}

void flashCheck() {
  for (int i = 0; i < 3; i++) {
    showFrame(frameCheck);
    delay(250);
    clearMatrix();
    delay(150);
  }
}

// ── Flat detection / wait-for-level ──────────────────────────────────────────
bool isFlat() {
  movement.update();
  return (movement.getZ()        >  FLAT_Z_MIN  &&
          fabsf(movement.getX()) < FLAT_XY_MAX  &&
          fabsf(movement.getY()) < FLAT_XY_MAX);
}

void waitForFlat() {
  showInstruction(frameFlat);  // 3 blinks of flat line then hold
  while (!isFlat()) delay(50);
  delay(400);  // brief settle
  clearMatrix();
  delay(200);
}

// ── Gesture capture ───────────────────────────────────────────────────────────
// Shows instruction blinks + arrow, waits for decisive tilt, returns axis+sign.
void captureGesture(uint8_t bmp[8][12], int &axis, float &sign) {
  showInstruction(bmp);  // blink arrow 3×, then hold arrow

  while (true) {
    movement.update();
    float dx = movement.getX() - mapping.baseX;
    float dy = movement.getY() - mapping.baseY;

    if (fabsf(dx) > CAL_THRESHOLD || fabsf(dy) > CAL_THRESHOLD) {
      if (fabsf(dx) >= fabsf(dy)) {
        axis = 0;
        sign = (dx > 0) ? 1.0f : -1.0f;
      } else {
        axis = 1;
        sign = (dy > 0) ? 1.0f : -1.0f;
      }
      break;
    }
    delay(30);
  }

  flashCheck();
}

// ── Calibration ───────────────────────────────────────────────────────────────
void calibrate() {
  // Step 0: baseline — hold still (flat line, no blinks)
  showFrame(frameFlat);
  float sumX = 0, sumY = 0;
  for (int i = 0; i < 50; i++) {
    movement.update();
    sumX += movement.getX();
    sumY += movement.getY();
    delay(20);
  }
  mapping.baseX = sumX / 50.0f;
  mapping.baseY = sumY / 50.0f;
  delay(200);

  // Step 1: FORWARD
  int fwdAxis; float fwdSign;
  captureGesture(frameUp, fwdAxis, fwdSign);
  mapping.fwdAxis = fwdAxis;
  mapping.fwdSign = fwdSign;
  waitForFlat();

  // Step 2: BACKWARD (verify axis/sign consistency)
  int bkAxis; float bkSign;
  captureGesture(frameDown, bkAxis, bkSign);
  if (bkAxis != mapping.fwdAxis || bkSign == mapping.fwdSign) {
    Serial.println("WARN: backward inconsistent with forward.");
  }
  waitForFlat();

  // Step 3: LEFT
  int turnAxis; float turnSign;
  captureGesture(frameLeft, turnAxis, turnSign);
  mapping.turnAxis = turnAxis;
  mapping.turnSign = turnSign;
  if (mapping.turnAxis == mapping.fwdAxis) {
    Serial.println("WARN: turn and forward on same axis.");
  }
  waitForFlat();

  // Step 4: RIGHT (verify)
  int rtAxis; float rtSign;
  captureGesture(frameRight, rtAxis, rtSign);
  if (rtAxis != mapping.turnAxis || rtSign == mapping.turnSign) {
    Serial.println("WARN: right inconsistent with left.");
  }
  waitForFlat();

  // Done
  Serial.println("=== CALIBRATION COMPLETE ===");
  Serial.print("  Baseline X="); Serial.print(mapping.baseX, 3);
  Serial.print("  Y=");          Serial.println(mapping.baseY, 3);
  Serial.print("  Forward:  axis="); Serial.print(mapping.fwdAxis  == 0 ? "X" : "Y");
  Serial.print("  sign=");           Serial.println(mapping.fwdSign);
  Serial.print("  Turn:     axis="); Serial.print(mapping.turnAxis == 0 ? "X" : "Y");
  Serial.print("  sign=");           Serial.println(mapping.turnSign);

  for (int i = 0; i < 2; i++) {
    showFrame(frameSmiley); delay(600);
    clearMatrix();          delay(300);
  }
  showFrame(frameSmiley);
  delay(1200);
  clearMatrix();
}

// ── Motor control ─────────────────────────────────────────────────────────────
void driveMotor(int dirPin, int pwmPin, int speed, bool invert) {
  if (invert) speed = -speed;
  if (speed > 0) {
    digitalWrite(dirPin, HIGH);
    analogWrite(pwmPin, speed);
  } else if (speed < 0) {
    digitalWrite(dirPin, LOW);
    analogWrite(pwmPin, -speed);
  } else {
    digitalWrite(dirPin, LOW);
    analogWrite(pwmPin, 0);
  }
}

void stopMotors() {
  analogWrite(PWM_L, 0);
  analogWrite(PWM_R, 0);
}

// Squared speed curve: precision at small tilts, full power at MAX_TILT
int tiltToSpeed(float tilt) {
  if (fabsf(tilt) < DEADZONE) return 0;
  float t = constrain((fabsf(tilt) - DEADZONE) / (MAX_TILT - DEADZONE), 0.0f, 1.0f);
  int speed = (int)(t * t * MAX_SPEED);
  return (tilt > 0) ? speed : -speed;
}

// ── Live tilt dot ─────────────────────────────────────────────────────────────
void showTiltDot(float tiltFwd, float tiltTurn) {
  uint8_t dotBmp[8][12];
  memset(dotBmp, 0, sizeof(dotBmp));

  if (fabsf(tiltFwd) < DEADZONE && fabsf(tiltTurn) < DEADZONE) {
    // 2×2 centre block = stopped
    dotBmp[3][5] = 1; dotBmp[3][6] = 1;
    dotBmp[4][5] = 1; dotBmp[4][6] = 1;
  } else {
    int row = constrain(4 - (int)roundf(tiltFwd  * 3.5f), 0, 7);
    int col = constrain(6 + (int)roundf(tiltTurn * 5.0f), 0, 11);
    dotBmp[row][col] = 1;
  }

  showFrame(dotBmp);
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(DIR_L, OUTPUT);
  pinMode(DIR_R, OUTPUT);
  stopMotors();

  matrix.begin();
  Modulino.begin();

  if (!movement.begin()) {
    // Blink all-on pattern to signal IMU error, then halt
    uint8_t errBmp[8][12];
    memset(errBmp, 1, sizeof(errBmp));
    while (true) {
      showFrame(errBmp); delay(300);
      clearMatrix();     delay(300);
    }
  }

  calibrate();
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  unsigned long t0 = millis();

  movement.update();

  float rawFwd  = (mapping.fwdAxis  == 0) ? movement.getX() : movement.getY();
  float baseFwd = (mapping.fwdAxis  == 0) ? mapping.baseX   : mapping.baseY;
  float tiltFwd = (rawFwd - baseFwd) * mapping.fwdSign;

  float rawTurn  = (mapping.turnAxis == 0) ? movement.getX() : movement.getY();
  float baseTurn = (mapping.turnAxis == 0) ? mapping.baseX   : mapping.baseY;
  float tiltTurn = (rawTurn - baseTurn) * mapping.turnSign;

  int speedFwd  = tiltToSpeed(tiltFwd);
  int speedTurn = tiltToSpeed(tiltTurn);

  if (speedFwd == 0 && speedTurn == 0) {
    stopMotors();
  } else {
    int leftSpeed  = constrain(speedFwd + speedTurn, -MAX_SPEED, MAX_SPEED);
    int rightSpeed = constrain(speedFwd - speedTurn, -MAX_SPEED, MAX_SPEED);
    driveMotor(DIR_L, PWM_L, leftSpeed,  false);
    driveMotor(DIR_R, PWM_R, rightSpeed, true);  // right motor physically inverted
  }

  showTiltDot(tiltFwd, tiltTurn);

  long elapsed = (long)(millis() - t0);
  if (elapsed < LOOP_MS) delay(LOOP_MS - elapsed);
}
