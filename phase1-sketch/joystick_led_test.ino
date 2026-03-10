/*
 * Joystick LED Test
 * LED lights up when joystick is moved, off when centered.
 * Use this to confirm QWIIC/I2C and joystick read are working
 * before connecting motors.
 *
 * LEDG is active LOW on UNO Q: LOW = on, HIGH = off
 */

#include <Modulino.h>

const int DEADZONE = 15;  // ignore small drift near center

ModulinoJoystick joystick;

void setup() {
  pinMode(LEDG, OUTPUT);
  digitalWrite(LEDG, HIGH);  // start with LED off

  Modulino.begin();
  joystick.begin();
}

void loop() {
  joystick.update();

  int x = joystick.getX();
  int y = joystick.getY();

  bool moved = (abs(x) > DEADZONE || abs(y) > DEADZONE);

  // LED on when joystick moved, off when centered
  digitalWrite(LEDG, moved ? LOW : HIGH);

  delay(50);
}
