/*
 * Test Vehicle — Phase 1 Sketch  v2
 * Joystick → Arduino → Cytron MDD10A → Motors
 *
 * Hardware:
 *   - Arduino UNO R4 WiFi (tested) or UNO Q 4GB
 *   - Modulino Joystick (I2C via QWIIC connector)
 *   - Cytron MDD10A dual motor driver (Sign-Magnitude PWM mode)
 *   - JGA37-520 12V 150RPM motors ×2
 *
 * Wiring — Motor Driver (Cytron MDD10A):
 *   MDD10A  →  Arduino Pin
 *   DIR1    →  4    (Left motor direction)
 *   PWM1    →  3    (Left motor speed — PWM capable)
 *   DIR2    →  7    (Right motor direction)
 *   PWM2    →  6    (Right motor speed — PWM capable)
 *   GND     →  Arduino GND (via MDD10A control header GND pin)
 *
 * Wiring — Modulino Joystick:
 *   QWIIC connector → Arduino QWIIC port
 *
 * Modulino Joystick API (Arduino_Modulino library):
 *   joystick.update()      — refresh values; returns true only if changed
 *   joystick.getX()        — int8_t, ~-98 (left) to ~+98 (right), 0 = center
 *   joystick.getY()        — int8_t, ~-98 (back) to ~+98 (forward), 0 = center
 *   joystick.isPressed()   — HIGH if button pressed, LOW if not
 *   Note: physical range observed ±98 (not full ±127); JOY_MAX set accordingly
 *
 * Changes from v1:
 *   - JOY_MAX = 98 (observed physical max) — full PWM range at full deflection
 *   - Explicit stopMotors() when joystick is centered — clean hard stop
 *   - Right motor direction inverted — motors are mounted mirror-image on chassis
 *   - Pivot turn mixing — inner wheel stops instead of reversing on pure turns:
 *       Forward:    Motor1 CW,   Motor2 CCW  (both drive)
 *       Backward:   Motor1 CCW,  Motor2 CW   (both drive)
 *       Left turn:  Motor1 stop, Motor2 CCW  (pivot on left wheel)
 *       Right turn: Motor1 CW,   Motor2 stop (pivot on right wheel)
 *       Center:     both stop
 *
 * Library required:
 *   Arduino_Modulino (install via Arduino Library Manager)
 *   Search: "Modulino" by Arduino
 */

#include <Modulino.h>

// ── Pin assignments ────────────────────────────────────────────────────────
const int DIR_L = 4;    // Left motor direction
const int PWM_L = 3;    // Left motor speed (PWM)
const int DIR_R = 7;    // Right motor direction
const int PWM_R = 6;    // Right motor speed (PWM)

// ── Config ─────────────────────────────────────────────────────────────────
const int DEADZONE  = 10;    // Extra deadzone on top of library's built-in ±26
const int JOY_MAX   = 98;    // Observed physical max from Modulino Joystick
const int MAX_SPEED = 200;   // PWM ceiling (0–255). Keep at 200 for bench testing.
const int LOOP_MS   = 50;    // Control loop interval (~20 Hz)

// ── Globals ────────────────────────────────────────────────────────────────
ModulinoJoystick joystick;

// ── Setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);
  Serial.println(F("=== Test Vehicle Phase 1 v2 ==="));

  // Motor driver output pins
  pinMode(DIR_L, OUTPUT);
  pinMode(PWM_L, OUTPUT);
  pinMode(DIR_R, OUTPUT);
  pinMode(PWM_R, OUTPUT);
  stopMotors();

  // Init Modulino bus + joystick
  Modulino.begin();
  bool joystickOK = joystick.begin();

  if (!joystickOK) {
    Serial.println(F("WARNING: Joystick not detected on I2C. Check QWIIC cable."));
  } else {
    Serial.println(F("Joystick OK."));
  }

  Serial.println(F("Ready. Move joystick to drive."));
  Serial.println(F("Press joystick button for emergency stop."));
}

// ── Main loop ──────────────────────────────────────────────────────────────
void loop() {
  joystick.update();

  int  joyX    = joystick.getX();
  int  joyY    = joystick.getY();
  bool pressed = joystick.isPressed() == HIGH;

  // ── Emergency stop via joystick button ───────────────────────────────────
  if (pressed) {
    stopMotors();
    Serial.println(F("BUTTON STOP — release button to resume."));
    while (joystick.isPressed() == HIGH) {
      joystick.update();
      delay(50);
    }
    return;
  }

  // ── Deadzone ──────────────────────────────────────────────────────────────
  if (abs(joyX) < DEADZONE) joyX = 0;
  if (abs(joyY) < DEADZONE) joyY = 0;

  // ── Explicit stop when centered ───────────────────────────────────────────
  if (joyX == 0 && joyY == 0) {
    stopMotors();
    delay(LOOP_MS);
    return;
  }

  // ── Pivot differential drive mix ─────────────────────────────────────────
  int leftSpeed  = joyY + joyX;
  int rightSpeed = joyY - joyX;

  // Pivot clamp: inner wheel stops instead of reversing during pure turns
  if (leftSpeed  < 0 && rightSpeed > 0) leftSpeed  = 0;
  if (rightSpeed < 0 && leftSpeed  > 0) rightSpeed = 0;

  leftSpeed  = constrain(leftSpeed,  -JOY_MAX, JOY_MAX);
  rightSpeed = constrain(rightSpeed, -JOY_MAX, JOY_MAX);

  // ── Drive ─────────────────────────────────────────────────────────────────
  // Right motor negated: motors are mounted mirror-image on chassis.
  driveMotor(DIR_L, PWM_L, leftSpeed);
  driveMotor(DIR_R, PWM_R, -rightSpeed);

  delay(LOOP_MS);
}

// ── Helper: drive one motor channel ───────────────────────────────────────
//   speed: -JOY_MAX (full reverse) … 0 (stop) … +JOY_MAX (full forward)
void driveMotor(int dirPin, int pwmPin, int speed) {
  bool forward = (speed >= 0);
  int  pwm     = map(abs(speed), 0, JOY_MAX, 0, MAX_SPEED);
  digitalWrite(dirPin, forward ? HIGH : LOW);
  analogWrite(pwmPin, pwm);
}

// ── Helper: stop all motors immediately ───────────────────────────────────
void stopMotors() {
  analogWrite(PWM_L, 0);
  analogWrite(PWM_R, 0);
  digitalWrite(DIR_L, LOW);
  digitalWrite(DIR_R, LOW);
}
