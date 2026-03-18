# Remote Mower — Lawn Mower Platform BOM
**Rev 1.1 · 2026-03-17**
**Target: 50 lb payload on grass, outdoor differential drive**

> This BOM covers the upgrade path from the test vehicle (bench prototype) to a
> real lawn mower platform. Items marked ♻️ carry over from the test vehicle BOM unchanged.

Brain: Arduino UNO Q 4GB (dual-brain: Qualcomm Linux + STM32 MCU)

✅ = In hand | 🔲 = To order | ♻️ = Reused from test vehicle

---

## Torque Justification

| Parameter | Value |
|-----------|-------|
| Total platform weight | ~50 lbs (23 kg) |
| Target speed | 3.5–4 km/h (typical robotic mower pace) |
| Grass rolling resistance coefficient | 0.25–0.3 |
| Required drive force (total) | 23 kg × 9.8 × 0.3 = **68 N** |
| Per motor (2 drive wheels) | **34 N** |
| Wheel radius (200mm) | 0.1 m |
| Required torque per motor | 34 N × 0.1 m = **~3.4 N·m** |
| Safety margin (2×) | **~5 N·m per motor** |
| Required RPM for 4 km/h | 4000m/h ÷ 60 ÷ (π × 0.2m) = **~106 RPM** |

**Speed check:** 100 RPM × π × 0.2m wheel = **3.8 km/h** ✅ suitable mowing speed.
Current JGA37-520 at 150RPM/12V produces ~0.7 N·m — both too fast and too weak for grass hauling.

---

## Electronics

| # | Component | Spec | Est Price | Status |
|---|-----------|------|-----------|--------|
| 1 | Arduino UNO Q 4GB | Qualcomm QRB2210 + STM32U585, WiFi5/BT5.1 | $59.00 | ♻️ Reused |
| 2 | Flysky FS-i6X + FS-iA6B | 6-ch RC transmitter + receiver, iBUS | ~$50 | ♻️ Reused |
| 3 | **Cytron MDD20A Motor Driver** | Dual channel, **20A/ch**, 6–30V, PWM — upgrade from MDD10A | ~$55 | 🔲 Order |
| 4 | **24V 10Ah LiFePO4 Battery** | XT60, deep cycle, safer chemistry for outdoor use | ~$80 | 🔲 Order |
| 5 | **DC-DC Buck 24V → 5V 5A** | Powers UNO Q + receiver from 24V rail | ~$12 | 🔲 Order |
| 6 | Main rocker switch | 30A rated, panel mount | ~$8 | 🔲 Order |
| 7 | E-stop button | Red mushroom, NC, IP65 rated (outdoor) | ~$12 | 🔲 Order |
| 8 | XT60 connector pair | Battery disconnect | ~$5 | ♻️ Reused |
| 9 | 14 AWG silicone wire (10ft) | High-current motor wiring | ~$10 | 🔲 Order |
| 10 | Waterproof terminal blocks | IP65, outdoor wiring | ~$12 | 🔲 Order |

**Electronics subtotal: ~$293**

---

## Motors & Drive

| # | Component | Spec | Est Price | Status |
|---|-----------|------|-----------|--------|
| 11 | **High-torque gear motors ×2** | **24V 100RPM JGA37-520 variant, ~4 N·m** — same form factor, 24V for torque+speed | ~$45 | 🔲 Order |
| 12 | **Pneumatic wheels 200mm ×2** | Inflatable rubber tire, 200mm, D-shaft or keyway hub — grass traction | ~$35 | 🔲 Order |
| 13 | **Heavy-duty front caster ×2** | Swivel, 3″–4″, rated for 30+ lbs each | ~$20 | 🔲 Order |

> **Alternative motors worth considering:**
> - **EMG30 (24V 170RPM, ~2 N·m)** — 5.3 km/h, outdoor robot standard, ~$45/each
> - **Greartisan 24V 100RPM high-torque** — Amazon available, ~$20/each, good value
> - **BaneBots RS-775 + 26:1 gearbox** — ~3 N·m at ~100RPM, FRC-grade, ~$40/each
>
> **Speed reference:** 100 RPM × π × 0.2m wheel = 3.8 km/h · 150 RPM = 5.7 km/h · 60 RPM = 2.3 km/h (too slow)

**Motors/drive subtotal: ~$100**

---

## Chassis & Mechanical

| # | Component | Spec | Est Price | Status |
|---|-----------|------|-----------|--------|
| 14 | **Steel chassis plate** | 3mm steel, 500×400mm — replaces Al plate, handles outdoor stress | ~$30 | 🔲 Order |
| 15 | Motor mount clamps ×2 | 37mm bore L-bracket (same as test vehicle) | ~$10 | 🔲 Order |
| 16 | M4/M5 stainless hardware kit | Outdoor-rated screws, nuts, standoffs | ~$15 | 🔲 Order |
| 17 | **Electronics enclosure (IP65)** | Weatherproof box for UNO Q + motor driver | ~$20 | 🔲 Order |
| 18 | **Mower deck mount hardware** | Custom brackets to attach actual mower deck (TBD design) | ~$25 | 🔲 Order |

**Chassis subtotal: ~$100**

---

## Cost Summary

| Category | Amount |
|----------|--------|
| ♻️ Reused from test vehicle | **~$110** |
| 🔲 Electronics (new) | **~$183** |
| 🔲 Motors & drive | **~$100** |
| 🔲 Chassis & mechanical | **~$100** |
| **Total new spend** | **~$383** |
| **Total platform cost** | **~$493** |

---

## Key Upgrades vs Test Vehicle

| Component | Test Vehicle | Lawn Mower Platform | Reason |
|-----------|-------------|---------------------|--------|
| Motors | JGA37-520 150RPM/12V, ~0.7 N·m | JGA37-520 100RPM/24V, ~4 N·m | Torque + correct speed (3.8 km/h) |
| Motor driver | MDD10A (10A/ch) | MDD20A (20A/ch) | Higher stall current on grass |
| Wheels | 90mm rubber | 200mm pneumatic | Traction + ground clearance |
| Battery | 12V 5Ah Li-ion | 24V 10Ah LiFePO4 | Runtime + outdoor safety |
| Chassis | 2mm Al, 300×220mm | 3mm steel, 500×400mm | Weight + durability |
| Wiring | Jumper wires | 14 AWG silicone | Current capacity |
| Enclosure | Open | IP65 weatherproof box | Rain/moisture |

---

## Architecture Notes

All electronics, software, and control architecture carry over from the test vehicle:
- Arduino UNO Q brain (Linux + STM32)
- Flysky RC control via iBUS (rc_drive.ino sketch)
- Cytron MDD20A replaces MDD10A — same DIR+PWM interface, no firmware changes needed
- E-stop NC wired in series on battery line

**Voltage note:** Moving to 24V doubles torque output from same motors (if motors are 24V rated).
Verify motor voltage rating before ordering — some JGA37-520 variants are 12V only.
EMG30 is available in 24V and is a safer choice for 24V systems.

---

## Next Steps
1. Finish test vehicle chassis assembly (current)
2. Validate RC control + differential drive on the bench/floor
3. Order lawn mower platform components once test vehicle is proven
4. Design mower deck attachment geometry
