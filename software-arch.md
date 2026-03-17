# Remote Mower — Software Architecture
**Rev 1.0 · 2026-03-17**
Brain: Arduino UNO Q (Qualcomm QRB2210 Linux + STM32U585 MCU)

---

## Overview: Phased Approach

```
Phase 1  RC Receiver (iBUS) → STM32 → Motors     (hardware bringup, bench test)
Phase 2  Linux serial → STM32 → Motors            (WiFi/WebSocket plumbing)
Phase 3  Web UI → Linux WebSocket → STM32 → Motors (full remote control)
```

Each phase adds one layer. The STM32 sketch stays largely the same — only the
command *source* changes.

---

## Phase 1 — RC Receiver Control (STM32 Only)

### Goal
Motors spin on the bench driven by Flysky FS-i6X transmitter via FS-iA6B receiver. Validate:
- iBUS parsing and channel reading
- Motor driver wiring
- Differential drive math
- E-stop hardware circuit
- Failsafe (no signal → stop motors)

### Hardware
- **Transmitter:** Flysky FS-i6X (6-channel, 2.4GHz AFHDS 2A)
- **Receiver:** Flysky FS-iA6B (6-channel, iBUS + PWM output, 3.3V signal)
- **Protocol:** iBUS (preferred over raw PWM — single wire carries all 6 channels)

### iBUS Protocol
```
- Baud rate: 115200
- Packet length: 32 bytes
- Format: 0x20, 0x40, [CH1_L, CH1_H, CH2_L, CH2_H, ... CH14_L, CH14_H], checksum
- Channel values: 1000–2000 (µs equivalent), center = 1500
- Update rate: ~50Hz
```

### STM32 Sketch — `rc_drive/rc_drive.ino`

#### Pin Map
```
D0 (Serial1 RX) ← iBUS signal from FS-iA6B iBUS port
D2              → Cytron MDD10A DIR1  (left motor direction)
D3              → Cytron MDD10A PWM1  (left motor speed)   [PWM-capable]
D4              → Cytron MDD10A DIR2  (right motor direction)
D5              → Cytron MDD10A PWM2  (right motor speed)  [PWM-capable]
```

#### Channel Mapping
```
CH3 (left stick vertical)   → throttle: 1000=full reverse, 1500=stop, 2000=full forward
CH4 (left stick horizontal) → rudder:   1000=full left, 1500=straight, 2000=full right
```

#### Differential Drive Math
```cpp
// Map RC channel (1000–2000) to speed (-255 to 255)
int speed = map(ch3, 1000, 2000, -255, 255);  // throttle
int turn  = map(ch4, 1000, 2000, -255, 255);  // rudder

// Apply deadband (±30 around center)
speed = applyDeadband(speed, 30);
turn  = applyDeadband(turn, 30);

// Differential mixing
int left  = constrain(speed + turn, -255, 255);
int right = constrain(speed - turn, -255, 255);
```

#### Safety Watchdog
If no valid iBUS packet received for 500ms → set all motors to 0.
This handles: transmitter off, receiver out of range, signal loss.

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

- [ ] **M1** — FS-iA6B receiver bound to FS-i6X transmitter, iBUS signal confirmed
- [ ] **M2** — STM32 sketch compiles, RC stick moves motors on bench (Phase 1)
- [ ] **M3** — E-stop tested: physical cut stops motors immediately
- [ ] **M4** — Failsafe tested: transmitter off → motors stop within 500ms
- [ ] **M5** — Full chassis assembled, drives on floor
- [ ] **M6** — Linux serial control working (Phase 2)
- [ ] **M7** — Web UI driving the vehicle (Phase 3)
