# Remote Mower — Test Vehicle

A 3-wheel differential drive test vehicle, built as a proof-of-concept before attaching to an actual lawn mower.

## Project Status

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Modulino Joystick → Arduino → MDD10A → Motors (wired) | ✅ Complete |
| Phase 2 | Xbox controller (BLE) → UNO Q Linux → UART → STM32 | 🔜 Planned |
| Phase 3 | Chassis assembly, battery gauge, boundary sensing | 🔜 Future |

## Hardware (BOM Rev 0.5)

| Component | Part | Notes |
|-----------|------|-------|
| Brain (MCU) | Arduino UNO Q 4GB (ABX00173) | STM32U585 + Qualcomm QRB2210 Linux |
| Controller (Phase 1) | Arduino UNO R4 WiFi | Used for Phase 1 testing (Serial Monitor works) |
| Joystick | Arduino Modulino Joystick | QWIIC/I2C |
| Motor Driver | Cytron MDD10A | Dual 10A, Sign-Magnitude PWM |
| Motors | JGA37-520 12V 150RPM ×2 | Differential drive |
| Wheels | Pololu 90mm ×2 + ball caster | — |
| Power | 12V 5Ah Li-ion + buck 12→5V | — |
| Safety | E-stop NC in series on 12V line | STM32 watchdog |

See [BOM.md](BOM.md) for full bill of materials with pricing.

## Wiring

See [wiring-diagram.html](wiring-diagram.html) for the full interactive wiring diagram.

Key connections (Phase 1):
```
MDD10A  →  Arduino Pin
DIR1    →  4    (Left motor direction)
PWM1    →  3    (Left motor speed)
DIR2    →  7    (Right motor direction)
PWM2    →  6    (Right motor speed)
GND     →  GND

Modulino Joystick → QWIIC port
```

## Phase 1 Sketch

Located in [`phase1-sketch/phase1_motor_control_v2/`](phase1-sketch/phase1_motor_control_v2/)

**Controls:**
- Push forward → both motors drive forward
- Push backward → both motors reverse
- Push left → pivot left (left motor stops, right drives)
- Push right → pivot right (right motor stops, left drives)
- Center → both motors stop
- Press joystick button → emergency stop

**Key config:**
```cpp
const int JOY_MAX   = 98;   // Observed physical max from Modulino Joystick
const int MAX_SPEED = 200;  // PWM ceiling (0–255), keep at 200 for testing
const int DEADZONE  = 10;   // Extra deadzone on top of library's built-in ±26
```

**Library required:** Arduino_Modulino (install via Arduino Library Manager)

## Software Architecture

See [software-arch.md](software-arch.md) for the full software architecture document.

## Reference

- [TI Robotic Lawn Mower Reference Design](https://www.ti.com/solution/robotic-lawn-mower?variantid=34873&subsystemid=13901#block-diagram)

## Phase 2 Plan

Xbox controller (BLE, BT5.1) → UNO Q Linux (Python + evdev) → UART → STM32 → MDD10A

The Xbox controller pairs natively with Linux via `bluetoothctl`. Left analog stick controls differential drive speed and direction.
