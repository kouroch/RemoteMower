# Remote Mower — Software Architecture
**Rev 0.1 · 2026-02-24**
Brain: Arduino UNO Q (Qualcomm QRB2210 Linux + STM32U585 MCU)

---

## Overview: Phased Approach

```
Phase 1  Joystick → STM32 ADC → Motors           (hardware bringup, bench test)
Phase 2  Linux serial → STM32 → Motors            (WiFi/WebSocket plumbing)
Phase 3  Web UI → Linux WebSocket → STM32 → Motors (full remote control)
```

Each phase adds one layer. The STM32 sketch stays largely the same — only the
command *source* changes.

---

## Phase 1 — Joystick Control (STM32 Only)

### Goal
Motors spin on the bench before any WiFi/Linux work. Validate:
- Motor driver wiring
- Differential drive math
- E-stop hardware circuit
- PWM tuning

### STM32 Sketch — `joystick_drive.ino`

#### Pin Map
```
A0  ← Joystick VRx  (X axis → turn left/right)
A1  ← Joystick VRy  (Y axis → forward/back speed)
D2  → Cytron MDD10A DIR1  (left motor direction)
D3  → Cytron MDD10A PWM1  (left motor speed)   [PWM-capable pin]
D4  → Cytron MDD10A DIR2  (right motor direction)
D5  → Cytron MDD10A PWM2  (right motor speed)  [PWM-capable pin]
```
> Note: Verify exact STM32U585 PWM-capable pins in Arduino UNO Q pinout docs.
> The UNO Q exposes standard Arduino pin headers — D3, D5, D6, D9, D10, D11
> are typically PWM on UNO-form-factor boards.

#### Differential Drive Math
```
raw_y = analogRead(A1)   // 0–1023, center ~512 = stopped
raw_x = analogRead(A0)   // 0–1023, center ~512 = straight

speed = map(raw_y, 0, 1023, -255, 255)   // negative = reverse
turn  = map(raw_x, 0, 1023, -255, 255)   // negative = left

left_speed  = constrain(speed + turn, -255, 255)
right_speed = constrain(speed - turn, -255, 255)
```

#### Motor Output (Cytron MDD10A)
Cytron MDD10A uses DIR + PWM per channel:
```
DIR = HIGH → forward, LOW → reverse
PWM = 0–255 duty cycle (analogWrite)

// Left motor
digitalWrite(DIR1, left_speed >= 0 ? HIGH : LOW);
analogWrite(PWM1, abs(left_speed));

// Right motor
digitalWrite(DIR2, right_speed >= 0 ? HIGH : LOW);
analogWrite(PWM2, abs(right_speed));
```

#### Deadband
Joystick centers are rarely exactly 512. Add a deadband:
```cpp
int applyDeadband(int val, int deadband = 20) {
  if (abs(val) < deadband) return 0;
  return val;
}
speed = applyDeadband(speed);
turn  = applyDeadband(turn);
```

#### Full Loop
```cpp
void loop() {
  int raw_y = analogRead(A1);
  int raw_x = analogRead(A0);

  int speed = applyDeadband(map(raw_y, 0, 1023, -255, 255));
  int turn  = applyDeadband(map(raw_x, 0, 1023, -255, 255));

  int left  = constrain(speed + turn, -255, 255);
  int right = constrain(speed - turn, -255, 255);

  setMotor(DIR1, PWM1, left);
  setMotor(DIR2, PWM2, right);

  delay(20);  // ~50Hz update rate
}
```

---

## E-Stop Design (All Phases)

The e-stop is **hardware-only** — no software dependency:
```
Battery 12V → [E-Stop NC button] → [Main rocker switch] → Cytron MDD10A VIN
```
Pressing e-stop physically cuts 12V to the motor driver. The STM32 keeps
running (powered separately via buck converter → UNO Q USB-C). This means:
- Motors stop instantly, no software needed
- STM32 can detect the condition if wired to sense motor power rail
- Safe to release e-stop and resume without reset

---

## Phase 2 — Linux Serial Control (Future)

### Goal
Linux side sends drive commands over UART to STM32. WiFi joystick or simple
terminal control. Validates the Linux↔STM32 serial bridge.

### Serial Protocol (simple text)
```
D <left_speed> <right_speed>\n
  e.g. "D 180 -180\n" = spin in place left
       "D 255 255\n"  = full speed forward
       "D 0 0\n"      = stop

H\n   = heartbeat (Linux → STM32, every 200ms)
```

### STM32 Changes for Phase 2
- Parse serial commands in addition to (or instead of) joystick ADC
- Add watchdog: if no `H` received for 600ms (3× interval) → set motors to 0
- Can keep joystick as manual override fallback

### Linux Side (Python sketch)
```python
# phase2_control.py
import serial, time

ser = serial.Serial('/dev/ttySTM32', 115200, timeout=0.1)

def drive(left, right):
    ser.write(f"D {left} {right}\n".encode())

def heartbeat():
    ser.write(b"H\n")

# Main loop: read keyboard/WebSocket, send commands + heartbeat
```

---

## Phase 3 — Web UI Control (Future)

### Goal
Browser joystick (or WASD keys) → WebSocket → Linux Python server → serial → STM32.
Hosted on XPS app server at `http://kourosh-xps-8930/mower/`.

### Architecture
```
Browser (Web UI)
    ↕ WebSocket ws://uno-q-ip:8765
Linux (Python WebSocket server)
    ↕ Serial /dev/ttySTM32
STM32 (motor driver)
    ↕ PWM
Cytron MDD10A → Motors
```

### Notes
- Web UI fits naturally into existing Caddy subpath setup on XPS
- UNO Q Linux IP discoverable via Tailscale or mDNS
- Same serial protocol from Phase 2

---

## Safety Layers (All Phases)

| Layer | Mechanism | Handles |
|-------|-----------|---------|
| Hardware | E-stop NC in 12V motor line | Immediate physical stop |
| MCU watchdog | Heartbeat timeout → PWM=0 | Comms loss (Phase 2+) |
| Deadband | Joystick noise rejection | Unwanted drift at rest |
| Speed ramp | Gradual accel/decel | Tip-over, wheel slip |

Speed ramping (nice to have in Phase 1, important later):
```cpp
// Ramp actual speed toward target at max step/loop
int rampedSpeed(int current, int target, int maxStep = 15) {
  if (abs(target - current) <= maxStep) return target;
  return current + (target > current ? maxStep : -maxStep);
}
```

---

## Milestones

- [ ] **M1** — STM32 sketch compiles, joystick moves motors on bench (Phase 1)
- [ ] **M2** — E-stop tested: physical cut stops motors immediately
- [ ] **M3** — Full chassis assembled, drives on floor
- [ ] **M4** — Linux serial control working (Phase 2)
- [ ] **M5** — Web UI driving the vehicle (Phase 3)
