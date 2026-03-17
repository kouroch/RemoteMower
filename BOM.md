# Remote Mower — Test Vehicle BOM
**Rev 1.0 · 2026-03-17**

## Reference Design
- **TI Robotic Lawn Mower**: https://www.ti.com/solution/robotic-lawn-mower?variantid=34873&subsystemid=13901#block-diagram
- Subsystems roadmap: motor drive · battery gauging/protection/charging · boundary sensing · collision/lift detection · navigation (GPS/IMU) · MCU+WiFi

Brain: Arduino UNO Q 4GB (dual-brain: Qualcomm Linux + STM32 MCU)

✅ = Received | ⏳ = Pending delivery | 🔲 = Not yet ordered

---

## Electronics

| # | Component | Spec | Actual/Est Price | Status |
|---|-----------|------|--------|--------|
| 1 | Arduino UNO Q 4GB | 4GB LPDDR4, 32GB eMMC, WiFi5/BT5.1, Qualcomm QRB2210 + STM32U585 | $59.00 (SKU ABX00173) | ✅ Received |
| 2 | Arduino Plug and Make Kit | Kit with UNO R4 WiFi + Modulino modules (bonus) | $87.40 (SKU AKX00069) | ✅ Received |
| 3 | Arduino Nano ESP32 with headers | ESP32-S3, WiFi/BT, 16MB flash | $19.30 (SKU ABX00083) | ✅ Received |
| 4 | Nano Connector Carrier | Carrier board for Nano | $11.80 (SKU ASX00061) | ✅ Received |
| 5 | Modulino Joystick | X/Y analog + button, Modulino form factor | $15.23 (SKU ABX00135) | ✅ Received (repurposed — not used in V3 control) |
| 5b | Flysky FS-i6X Transmitter | 6-ch 2.4GHz AFHDS 2A, LCD screen | ~$50 | ✅ Received |
| 5c | Flysky FS-iA6B Receiver | 6-ch, PWM + iBUS output, 3.3V signal | (included) | ✅ Received |
| 6 | Cytron MDD10A Motor Driver | Dual channel, 10A/ch, 6–30V, PWM | ~$30 | ✅ Received |
| 7 | 12V Li-ion Battery Pack | 5Ah, XT60 connector | ~$35 | ✅ Received |
| 8 | DC-DC Buck Converter | 12V → 5V 3A (powers UNO Q) | ~$8 | ✅ Received |
| 9 | Main rocker switch | 20A rated, panel mount | ~$5 | ✅ Received |
| 10 | E-stop button | Red mushroom, normally-closed (NC) | ~$8 | ✅ Received |
| 11 | XT60 connector pair | Battery disconnect | ~$5 | ✅ Received |
| 12 | Jumper wires + terminal blocks | Assorted | ~$10 | ✅ Received |

## Mechanical

| # | Component | Spec | ~Price | Status |
|---|-----------|------|--------|--------|
| 13 | 12V DC gear motors ×2 | JGA37-520, ~150 RPM, D-shaft | ~$30 | ✅ Received |
| 14 | Rubber wheels 90mm ×2 | Matched to motor D-shaft (pair) | ~$10 | ✅ Received |
| 15 | Front ball caster ×1 | ½″ metal ball, Pololu | ~$3 | ✅ Received |
| 16 | Aluminum chassis plate | 2mm, 300×220mm | ~$15 | 🔲 Not ordered |
| 17 | Motor mount clamps ×2 | 37mm bore L-bracket | ~$10 | 🔲 Not ordered |
| 18 | M3/M4 hardware kit | Screws, nuts, standoffs assortment | ~$10 | 🔲 Not ordered |

---

## Cost Summary

| Category | Amount |
|---|---|
| ✅ Received (Arduino order) | **$192.73** |
| ⏳ Pending electronics | ~$101 |
| 🔲 Not yet ordered (mechanical) | ~$35 |
| **Total estimate** | **~$328** |

*Note: Rev 0.4 reflects actual received items. Plug and Make Kit and Nano ESP32 are additions not in original BOM. Joystick is Modulino form factor vs original KY-023 estimate.*

---

## Architecture Notes

### Brain: Arduino UNO Q (dual-brain)
- **Linux side (Qualcomm QRB2210):** Runs Debian OS + Python WebSocket server + web UI. Handles WiFi comms, control logic, telemetry. *(Phase 2+)*
- **Real-time side (STM32U585):** Handles motor PWM via Cytron MDD10A, safety watchdog, e-stop logic. If comms heartbeat drops → MCU cuts motors immediately.

### Control Input — Phased Approach
- **Phase 1 (prototype):** Modulino Joystick wired to STM32 ADC pins. Pure hardware loop — no Linux, no WiFi, no latency. Goal: motors spinning on the bench.
- **Phase 2:** Linux WebSocket server replaces joystick as command source; same STM32 motor code, different input path.
- **Phase 3:** Web UI on XPS app server sends commands to Linux side over WiFi.

### Additional Hardware Notes
- **Arduino Plug and Make Kit + Nano ESP32 + Nano Connector Carrier:** Ordered with future projects in mind, not exclusively for the test vehicle. The Modulino ecosystem and Nano ESP32 are versatile enough to serve as building blocks across multiple builds. Will be allocated to specific projects as they emerge.
- **Learning goal:** This entire project is also a learning exercise — embedded systems, motor control, real-time MCUs, Linux on hardware. No deadline, no pressure. Explore and figure it out.

### Wiring Summary (V3 — RC Control)
```
FS-iA6B Receiver
  ├── VCC ←── Buck 5V rail
  ├── GND ←── Common GND
  └── iBUS ──→ STM32 Serial1 RX (D0)   [single wire, all 6 channels]

Battery (12V)
  ├── [E-Stop, NC] ──→ [Main Switch] ──→ Cytron MDD10A ──→ Motor L (M1)
  │                                                     └──→ Motor R (M2)
  └── [Buck 12→5V] ──→ UNO Q (5V USB-C)
                         └── STM32 Serial1 RX ←── FS-iA6B iBUS
                         └── STM32 GPIO PWM ──→ MDD10A DIR1/PWM1/DIR2/PWM2
                         └── Linux side ──→ (idle in Phase 1)

Channel mapping:
  CH3 (left stick vertical)   → throttle (forward/back)
  CH4 (left stick horizontal) → rudder (turn left/right)
```

### Safety Watchdog Design
- STM32 Arduino sketch expects heartbeat from Linux side every ~200ms
- If heartbeat missed × 3 → STM32 sets all motor PWM to 0 immediately
- E-stop button wired NC in series with 12V motor supply line (hardware cut, no software dependency)

---

## Files
- `BOM.md` — this file
- `chassis-drawing.html` — top-down + side view technical drawing
- `software-arch.md` — STM32 sketch architecture + phased software plan
