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

## Phase 2 — Web Control via Linux RPC Bridge (Complete)
**Rev 2.0 · 2026-03-23**

### Goal
Browser joystick (WASD or virtual touch joystick) → WebSocket → Python server on
UNO Q Linux → msgpack RPC over serial → STM32 → motors.
RC receiver remains active as automatic fallback.

### Architecture
```
Browser (index.html — virtual joystick + WASD)
    ↕ WebSocket ws://larine.local:8765
UNO Q Linux (server.py — asyncio + websockets)
    ↕ msgpack RPC notify over /dev/ttySTM0 @ 115200
STM32 (rc_drive_v2.ino — Arduino_RPClite server on Serial2)
    ↕ PWM
Cytron MDD10A → Motors
         ↑
    Serial1 ← iBUS ← FS-iA6B RC receiver (always active, fallback)
```

### Serial Ports
| Port | Role |
|------|------|
| `/dev/ttySTM0` | Linux → STM32 bridge (Python writes RPC here) |
| `Serial1` (D0) | STM32 iBUS input from RC receiver (unchanged) |
| `Serial2` | STM32 RPC server — **hardware verification needed**: confirm this is the Linux bridge UART on UNO Q STM32 |

### RPC Protocol (Arduino_RPClite)
Python sends msgpack **notify** frames (fire-and-forget, no response expected):
```python
# notify: [2, method_name, [args]]
msgpack.packb([2, "drive", [left, right]])  # -255..255
msgpack.packb([2, "stop",  []])
```

### STM32 Sketch — `rc_drive_v2/rc_drive_v2.ino`
- Includes `Arduino_RPClite.h`, RPC server on Serial2
- Exposes `drive(int left, int right)` and `stop()` RPC methods
- **Priority**: if web command received within last 500ms → use web targets; else fall back to iBUS RC
- All existing watchdog/ramp/deadband/setMotor logic preserved
- RC receiver on Serial1 (D0 RX) is unchanged — always active as fallback

### Python Server — `web_control/server.py`
- asyncio + websockets, listens on `0.0.0.0:8765`
- Opens `/dev/ttySTM0` at 115200; reconnects on serial error
- Accepts JSON from browser: `{"left": 200, "right": 200}` · `{"stop": true}` · `{"status": true}`
- Heartbeat task: sends `stop()` RPC if no browser command for 600ms
- Logs all activity to stdout; managed by systemd user service

### Web UI — `web_control/index.html`
- Single-file, no CDN dependencies
- Dark theme (#0d1117 / #58a6ff — matches bom.html)
- Virtual joystick (canvas): vertical = throttle, horizontal = turn → differential mixing
- WASD / arrow key support
- Big red STOP button
- Connection status indicator
- Sends commands at 50ms interval while joystick active
- Connects to `ws://${host}:8765`; host from `?host=` URL param or `window.location.hostname`

### Deployment
```bash
# From mower workspace root:
./web_control/deploy.sh
# Deploys to arduino@larine.local:/home/arduino/mower-web/
# Installs systemd user service: mower-web.service
```

Web UI also accessible from XPS at `/mower/` via existing Caddy subpath setup.

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
