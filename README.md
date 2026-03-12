# Remote Mower — Test Vehicle

A 3-wheel differential drive test vehicle, built as a stepping stone toward a fully autonomous remote-controlled lawn mower.

The project is intentionally incremental: each phase validates a key subsystem before the next is added. All hardware is off-the-shelf; all software is open source.

---

## Project Status

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | Modulino Joystick → Arduino UNO R4 WiFi → MDD10A → Motors | ✅ Complete |
| Phase 2 | Modulino Movement (IMU tilt) → Arduino UNO R4 WiFi → MDD10A → Motors | 🔄 In Progress |
| Phase 3 | Xbox controller (BLE) → UNO Q Linux → UART → STM32 → Motors | 🔜 Planned |
| Phase 4 | Chassis assembly, battery gauge, boundary sensing, navigation | 🔜 Future |

---

## Hardware

See [BOM.md](BOM.md) for the full bill of materials with pricing and sourcing notes.

| Component | Part | Notes |
|-----------|------|-------|
| Dev MCU | Arduino UNO R4 WiFi | Used for Phase 1 & 2 (USB CDC Serial works natively) |
| Target MCU | Arduino UNO Q 4GB (ABX00173) | STM32U585 + Qualcomm QRB2210 Linux — for Phase 3+ |
| Joystick (Phase 1) | Arduino Modulino Joystick | QWIIC / I2C |
| IMU (Phase 2) | Arduino Modulino Movement | LSM6DSOX, QWIIC / I2C |
| Motor Driver | Cytron MDD10A | Dual 10A, Sign-Magnitude PWM |
| Motors | JGA37-520 12V 150RPM ×2 | Differential drive |
| Wheels | Pololu 90mm ×2 + ball caster | — |
| Power | 12V 5Ah Li-ion + buck 12→5V | — |
| Safety | E-stop (NC) in series on 12V line | Hardware kill switch |

---

## Wiring

See [wiring-diagram.html](wiring-diagram.html) for the full interactive wiring diagram.

Motor driver pin assignments (Phases 1 & 2):

```
MDD10A Pin  →  Arduino Pin
DIR1        →  4    (Left motor direction)
PWM1        →  3    (Left motor speed)
DIR2        →  7    (Right motor direction)
PWM2        →  6    (Right motor speed)
GND         →  GND
```

---

## Phase 1 — Joystick Control

**Sketch:** [`phase1-sketch/phase1_motor_control_v2/`](phase1-sketch/phase1_motor_control_v2/)

Wired joystick connected via QWIIC to the Arduino. Differential drive with pivot turns.

**Controls:**
- Push forward/back → both motors forward/reverse
- Push left/right → pivot turn (inner motor stops)
- Center → stop
- Press joystick button → emergency stop

**Key config:**
```cpp
const int JOY_MAX   = 98;   // Physical max from Modulino Joystick (not 127)
const int MAX_SPEED = 200;  // PWM ceiling (0–255)
const int DEADZONE  = 10;   // On top of library's built-in ±26 deadzone
```

**Libraries:** `Arduino_Modulino` (Library Manager)

---

## Phase 2 — Tilt Control (IMU)

**Sketch:** [`phase2-sketch/phase2_tilt_control_v1/`](phase2-sketch/phase2_tilt_control_v1/)

The Modulino Movement (LSM6DSOX IMU) replaces the joystick. Tilt the sensor to drive — no cables, no fixed orientation required.

### Startup Calibration

On every power-on, a guided calibration sequence runs to detect how you're holding the sensor:

1. Hold still → baseline captured
2. LED matrix blinks **↑** → tilt forward
3. LED matrix blinks **↓** → tilt backward
4. LED matrix blinks **←** → tilt left
5. LED matrix blinks **→** → tilt right
6. Smiley face → ready to drive

The calibration maps your physical gestures to the correct axes regardless of sensor orientation. You can hold it like a steering wheel, a TV remote, or at any angle — it adapts.

### Runtime

The 12×8 LED matrix shows a live tilt indicator dot. Center 2×2 block = stopped. Flat position = natural dead-man stop.

**Key config:**
```cpp
const float DEADZONE      = 0.10f;  // g — below this = stopped
const float MAX_TILT      = 0.70f;  // g — full speed (~45° tilt)
const float CAL_THRESHOLD = 0.30f;  // g — gesture detection threshold
const int   MAX_SPEED     = 200;    // PWM ceiling (0–255)
```

**Speed curve:** Squared (tilt²) — fine control at small angles, full power at max lean.

**Libraries:** `Arduino_Modulino`, `Arduino_LED_Matrix` (bundled with R4 WiFi board package)

---

## Phase 3 — Wireless Xbox Controller (Planned)

Xbox controller (BLE / BT5.1) pairs natively with Linux via `bluetoothctl`. Plan:

```
Xbox Controller (BLE) → UNO Q Linux (Python + evdev) → UART → STM32 → MDD10A
```

Left analog stick → differential drive. No proprietary drivers needed.

---

## Software Architecture

See [software-arch.md](software-arch.md) for the full design document covering control flow, safety, and the phase roadmap.

---

## Reference

- [TI Robotic Lawn Mower Reference Design](https://www.ti.com/solution/robotic-lawn-mower?variantid=34873&subsystemid=13901#block-diagram)
- [Cytron MDD10A Datasheet](https://docs.cytron.io/mdd10a)
- [Arduino UNO Q Docs](https://docs.arduino.cc/hardware/uno-r4-wifi/)

---

## License

MIT — see [LICENSE](LICENSE).
