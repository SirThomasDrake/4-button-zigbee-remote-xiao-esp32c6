# Wiring — 4-button XIAO ESP32-C6 remote

## Parts

| Part | Role |
|------|------|
| Seeed XIAO ESP32-C6 | MCU + Zigbee |
| TP4056 (often labeled TB4056) | Charge the pouch; feed ~3.7 V to the C6 |
| 3.7 V LiPo pouch | Pack |
| ADXL345 | I2C accelerometer |
| 4 × momentary buttons | GPIO 0, 1, 2, 4 |
| 4 × 10 kΩ | External pull-ups so pins are not floating in sleep |
| Optional 4 × LED + 330 Ω | Press indicators |
| Optional MAX17048 | I2C fuel gauge (untested on this firmware) |

Do **not** feed USB 5 V into the BAT pads. On the XIAO C6 those pads are **battery input (~3.7 V)**. 5 V on that pad can kill the board.

---

## 1. Power — pouch + TP4056 + C6

USB 5V ──► TP4056 IN+ / IN−
Pouch + ──► TP4056 B+
Pouch − ──► TP4056 B−
TP4056 OUT+ ──► XIAO BAT+   (pad on the underside)
TP4056 OUT− ──► XIAO BAT−


- B+ / B− go **only** to the cell.
- OUT+ / OUT− go **only** to the XIAO BAT pads.
- Charge from the TP4056 USB port, not from the XIAO USB-C, if you want the pack to charge while the board is assembled.
- A protected pouch is preferred. If the pouch already has a PH2.0 plug, a female PH2.0 on B+ / B− is cleaner than cutting the lead.

LED on the TP4056: red = charging, blue or green = done

---

## 2. Buttons + 10 kΩ pull-ups

Firmware expects **press = LOW** (wake on `ESP_EXT1_WAKEUP_ANY_LOW`).

XIAO 3V3 ──┬── 10 kΩ ─── GPIO0  (Button 1)
│          │             
│          └── Button 1 ── GND
├── 10 kΩ ──┬── GPIO1  (Button 2)
│           └── Button 2 ── GND
├── 10 kΩ ──┬── GPIO2  (Button 3)
│           └── Button 3 ── GND
└── 10 kΩ ──┬── GPIO4  (Button 4)
└── Button 4 ── GND


| Button | XIAO label | GPIO | Zigbee endpoint |
|--------|------------|------|-----------------|
| 1 | D0 | 0 | 1 |
| 2 | D1 | 1 | 2 |
| 3 | D2 | 2 | 3 |
| 4 | ── | 4 | 4 |

GPIO4 is chip GPIO4 - on the underside of the board labeled MTMS, not the header pin labeled D4 (D4 is GPIO22). Check the Seeed pinout before soldering.

The internal pull-up is also enabled in software. The 10 kΩ keeps the pin defined if the MCU pull-up is off in deep sleep.

---

## 3. Optional button LEDs (active LOW)

XIAO 3V3 ── 330 Ω ── LED anode ── LED cathode ── same GPIO as that button


The LED lights when the pin is LOW (button pressed). Do not also drive that GPIO as an output HIGH while the button can short it to GND.

---

## 4. ADXL345 (I2C)

Use **3.3 V only**. VIN/VCC = 3V3, not 5 V.

ADXL345 VCC / VIN ── XIAO 3V3
ADXL345 GND         ── XIAO GND
ADXL345 SDA         ── XIAO D4  (GPIO22)
ADXL345 SCL         ── XIAO D5  (GPIO23)
ADXL345 CS          ── 3V3      (I2C)
ADXL345 SDO / ALT   ── GND      (addr 0x53)
ADXL345 INT1        ── GPIO5    (MTDI)


## 5. Optional MAX17048 voltage sensor (untested)

Same I2C bus as the ADXL345.

MAX17048 VCC ── 3V3
MAX17048 GND ── GND
MAX17048 SDA ── D4 (GPIO22)
MAX17048 SCL ── D5 (GPIO23)


Default address is usually `0x36`. Do **not** also build a resistor divider into a button GPIO. This board is the gauge; the current 4-button firmware does not read it yet.

---

## 6. Ground

One common GND:

- XIAO GND
- TP4056 OUT− / B−
- Every button
- ADXL345 GND
- MAX17048 GND

Star them at the TP4056 OUT− or the XIAO GND pad.

---

## 7. What not to do

- No 5 V on BAT+.
- Do not leave a button pin open with no pull-up — it will float, wake the chip, and look “stuck.”
- Charge the pouch through the TP4056. The XIAO USB-C is for programming.

---

## 8. Quick continuity check

1. Pack installed, no USB on the XIAO: BAT+ to BAT− should read ~3.5–4.2 V.
2. Each button GPIO to GND: open = ~3.3 V, pressed = 0 V.
3. I2C: SDA/SCL idle high to 3V3.
