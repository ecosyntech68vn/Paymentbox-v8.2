# SCHEMATIC V8.2 — PaymentBox Production-Ready

**Major changes từ V8.1**:
- Pin 18650 + TP4056 charger + MT3608 boost
- Chống ngược cực (P-FET AO3401)
- Chống sét/surge nâng cao (SMBJ5.0CA TVS + PPTC fuse)
- Fix tất cả 11 lỗi audit
- PCB tăng lên **75 × 110mm** (đủ chỗ cho power management chain)

---

## 1. Block diagram V8.2

```
USB-C 5V
    │
    F1 PPTC 500mA (over-current)
    │
    D6 SMBJ5.0CA bidirectional TVS (surge)
    │
    Q3 AO3401 P-FET (reverse polarity)
    │
    ├─────────────────────────────────────┐
    │                                     │
    ▼                                     ▼
TP4056 charger ────► Battery 18650 ────► MT3608 boost ──► 5V_SYS
   │                 (protected, JST)      3.7V → 5V        │
   ├── LED4 CHRG     │                                      │
   └── LED5 STDBY    │ ADC sense via                        │
                     R19/R20 → GPIO36                       │
                                                            ▼
                                                     AMS1117-3.3 ──► 3V3
                                                                       │
                                                                       ▼
                                                             [ESP32 + peripherals]
```

**Logic vận hành**:
1. Có USB-C → TP4056 sạc 18650 (1A) + cấp 5V cho phụ tải qua boost
2. Mất USB-C → 18650 tiếp tục cấp qua MT3608 → hệ thống chạy bình thường
3. Battery low → ADC GPIO36 detect → ESP32 cảnh báo + tắt LCD backlight
4. Battery critical < 2.8V → DW01A trên cell tự cắt (protection nội cell)

---

## 2. Power chain mới (V8.2)

### 2.1 Đầu vào USB-C (protected)

```
USB-C VBUS (5V@2A)
    │
    F1 (PPTC 0.5A trip @1.5A, reset-able)
    │
    +5V_PROT_RAW
    │
    D6 (SMBJ5.0CA, 6.8V breakdown, 600W transient)─── GND
    │
    Q3 source (AO3401 P-FET)
       │
       Q3 gate → R23 (100kΩ pull-up to source) → +5V_PROT_RAW
       Q3 gate → R24 (10kΩ) → GND
       Q3 drain → +5V_SAFE
```

Vận hành:
- Cấp đúng cực: VBUS dương → source dương → V_GS = -V_SAFE/10 ≈ -5V → P-FET ON (R_DS_on ~50mΩ) → drain ra dương → load nhận 5V
- Cấp ngược cực (V_BAT âm tại VBUS): source âm → V_GS dương → P-FET OFF → load không nhận → AN TOÀN

### 2.2 Sạc Li-ion (TP4056 1A)

```
+5V_SAFE → U6 TP4056 SOP-8
    Pin 4 VCC  ← +5V_SAFE
    Pin 1 TEMP ← NC (skip thermistor for simplicity)
    Pin 2 PROG ← R13 (1.2kΩ) → GND  [I_charge = 1200mV/1.2k = 1A]
    Pin 3 GND  ← GND
    Pin 5 BAT  → BAT+ → J5.1 (JST PH 2-pin)
    Pin 6 ~STDBY ← LED5 (green) → R15 (1kΩ) → +5V_SAFE
    Pin 7 ~CHRG  ← LED4 (red)   → R16 (1kΩ) → +5V_SAFE
    Pin 8 CE   ← Open (always charge when VCC ≥ BAT+0.3V)

C9 (10µF) on +5V_SAFE near VCC
C10 (10µF) on BAT pin
```

### 2.3 Battery 18650 (external, qua JST)

```
J5 JST-PH 2-pin
    Pin 1 = BAT+ (đỏ)
    Pin 2 = BAT- (đen, GND)

Pin 18650 chọn loại có sẵn protection:
- Samsung INR18650-25R 2500mAh có protected version
- Panasonic NCR18650B 3400mAh có protected version
- Hoặc lắp pack 1S1P với BMS rời gắn ngoài board
```

### 2.4 Boost converter 3.7V → 5V (MT3608)

```
BAT+ ──► U7 MT3608 SOT-23-6
    Pin 1 SW    → L1 (4.7µH) → BAT+
                  → D7 anode (SS14 Schottky)
                  → +5V_SYS
    Pin 2 GND   → GND
    Pin 3 FB    → R17 (22kΩ to +5V_SYS) / R18 (3kΩ to GND) [V_OUT = 0.6 * (1 + R17/R18) = 0.6 * (1 + 22/3) = 5.0V]

Công thức MT3608: V_OUT = V_FB * (1 + R17/R18) với V_FB = 0.6V
→ V_OUT = 0.6 * (1 + 22/3) = 0.6 * 8.33 = 5.0V ✓

    Pin 4 EN    → +5V_SAFE (always on khi có battery OR USB-C, but only enable boost when battery is sole source — actually keep always on, MT3608 internal load shed handles it)
    Pin 5 NC
    Pin 6 IN    → BAT+ (3.7V battery)

C11 (10µF) input cap on BAT+
C12 (22µF) output cap on +5V_SYS
C13 (100nF) compensation
```

### 2.5 Battery voltage sensing

```
BAT+ ── R19 (100kΩ) ── ADC_BAT ── R20 (47kΩ) ── GND

Voltage at ADC_BAT = V_BAT * 47/(100+47) = V_BAT * 0.32
For V_BAT = 4.2V: ADC = 1.34V (within ESP32 ADC range 0-3.3V)
For V_BAT = 3.0V: ADC = 0.96V
→ Linear monitoring, software multiplies by 3.13

ADC_BAT → ESP32 GPIO36 (SENSOR_VP, ADC1_CH0, input-only)
```

### 2.6 Strapping pin fixes

```
GPIO12 (ESP32 pad 14) ── R21 (10kΩ) ── GND      [force LOW at boot, flash 3.3V correct]
GPIO15 (ESP32 pad 24) ── R22 (10kΩ) ── GND      [force LOW at boot, RELAY OFF default]
```

LED1 driver chain (now correct polarity):
```
GPIO12 (pad 14) ── R1 (220Ω) ── LED1.2 (anode) ── LED1.1 (cathode) ── GND
                              R21 (10k) parallel
```

### 2.7 SD card power stability

```
+3V3 ── C13_SD (10µF tantalum) ── U5 pin 4 (VDD)
                                ── U5 pin 6 (VSS) → GND
```

---

## 3. Updated pin mapping ESP32

| GPIO | Pad | Function | Strapping | Notes |
|---|---|---|---|---|
| GPIO0  | 26 | BOOT button | HIGH at boot (R10 pull-up) | Pull-up qua R10 10k |
| GPIO1  | 36 | UART TX | - | J2 |
| GPIO2  | 25 | LED_B drive | LOW at boot OK | LED, weak signal at boot |
| GPIO3  | 35 | UART RX | - | J2 |
| GPIO4  | 27 | LED_G drive | - | - |
| GPIO5  | 30 | SPI CS SD | HIGH at boot | SD CS, default high |
| GPIO12 | 14 | LED_R drive | **LOW at boot** | **R21 pull-down 10k (FIX)** |
| GPIO13 | 16 | BUZZER drive | - | - |
| GPIO14 | 13 | SPI SCK | - | - |
| GPIO15 | 24 | RELAY drive (F) | **LOW at boot** | **R22 pull-down 10k (FIX)** |
| GPIO19 | 32 | SPI MISO | - | - |
| GPIO21 | 34 | I2C SDA | - | R8 pull-up 4.7k |
| GPIO22 | 37 | I2C SCL | - | R9 pull-up 4.7k |
| GPIO23 | 38 | SPI MOSI | - | - |
| GPIO27 | 12 | Expansion GPIO | - | J3 pin 6 |
| **GPIO36** | 4 | **ADC battery voltage** | Input only | **R19/R20 voltage divider (NEW)** |

---

## 4. New components V8.2

| Ref | Component | Footprint | Function | Cost VND |
|---|---|---|---|---|
| F1 | PPTC 500mA SMD | Fuse_1812 | Over-current USB-C | 3,000 |
| D6 | SMBJ5.0CA bidir TVS | D_SMB | Surge protection 600W | 5,000 |
| Q3 | AO3401 P-FET SOT-23 | SOT-23 | Reverse polarity | 3,000 |
| R23 | 100kΩ 0805 | R_0805 | Q3 gate pull-up | 500 |
| R24 | 10kΩ 0805 | R_0805 | Q3 gate pull-down | 500 |
| U6 | TP4056 SOP-8 | SOIC-8 | Li-ion charger 1A | 8,000 |
| R13 | 1.2kΩ 0805 | R_0805 | TP4056 PROG | 500 |
| R15 | 1kΩ 0805 | R_0805 | LED5 series | 500 |
| R16 | 1kΩ 0805 | R_0805 | LED4 series | 500 |
| LED4 | LED RED 0805 | LED_0805 | CHRG indicator | 1,500 |
| LED5 | LED GREEN 0805 | LED_0805 | DONE indicator | 1,500 |
| C9 | 10µF tantalum 0805 | C_0805 | TP4056 input | 3,000 |
| C10 | 10µF tantalum 0805 | C_0805 | TP4056 output | 3,000 |
| J5 | JST-PH 2-pin SMD | (use 1x2 2.54mm header) | Battery connector | 2,000 |
| U7 | MT3608 SOT-23-6 | SOT-23-6 | Boost 3.7→5V | 5,000 |
| L1 | Inductor 4.7µH 1A | L_0805 (Bourns SRR2818) | Boost inductor | 8,000 |
| D7 | SS14 Schottky SMA | D_SMA | Boost rectifier | 3,000 |
| R17 | 22kΩ 0805 | R_0805 | Boost FB high | 500 |
| R18 | 3kΩ 0805 | R_0805 | Boost FB low | 500 |
| C11 | 22µF tantalum 0805 | C_0805 | Boost input | 4,000 |
| C12 | 22µF tantalum 0805 | C_0805 | Boost output | 4,000 |
| C13 | 100nF 0805 | C_0805 | Boost compensation | 500 |
| R19 | 100kΩ 0805 | R_0805 | ADC divider high | 500 |
| R20 | 47kΩ 0805 | R_0805 | ADC divider low | 500 |
| R21 | 10kΩ 0805 | R_0805 | **GPIO12 strapping (FIX)** | 500 |
| R22 | 10kΩ 0805 | R_0805 | **GPIO15 strapping (FIX)** | 500 |
| C_SD | 10µF tantalum 0805 | C_0805 | **SD bulk (FIX)** | 3,000 |
| D4_new | SS14 Schottky SMA | D_SMA | **Relay flyback (FIX, replace 1N4148)** | 3,000 |
| **TOTAL ADD V8.2** | | | | **~67,000 VND** |

**Removed from V8.1**:
- D5 (PESD5V0) — replaced by D6 SMBJ5.0CA
- Old D4 (1N4148W) — replaced by SS14

**Total cost V8.2**:
- Phương án E (Watchdog only): V8.1 206k + 67k = **~273k VND/PCB**
- Phương án F (full PaymentBox): V8.1 313k + 67k = **~380k VND/PCB**
- Plus 18650 protected cell: +50k VND
- Plus Phone Android cũ: +400k VND

**Tổng giá thành thiết bị production**:
- Phương án E: **~730k VND** (vs V8.1 ~640k → tăng 90k cho production-grade)
- Phương án F: **~830k VND** (vs V8.1 ~740k)

---

## 5. New PCB layout (75 × 110mm)

```
y=0   ┌─────────────────────────────────────────────────────┐
      │ Z0: ANTENNA KEEPOUT (y=0 to 12)                     │
y=12  ├─────────────────────────────────────────────────────┤
      │ Z1: ESP32 MODULE (y=12 to 42)                       │
      │     U1 ESP32 at (35, 28) (dịch xuống 4mm vs V8.1)   │
      │     C3, C4, C5 decoupling, R6/R10 strapping pull-ups│
      │     R21, R22 (NEW) strapping pull-down GPIO12/15    │
      │     SW1 RESET, SW2 BOOT                             │
y=42  ├─────────────────────────────────────────────────────┤
      │ Z2: POWER INPUT + PROTECTION (y=42 to 60)           │
      │     J1 USB-C, F1 PPTC, D6 SMBJ5.0CA TVS             │
      │     Q3 P-FET, R23/R24 gate resistors                │
      │     Ferrite FB1, R11/R12 USB-C CC                   │
y=60  ├─────────────────────────────────────────────────────┤
      │ Z2A: BATTERY CHARGING + BOOST (y=60 to 80) — NEW    │
      │     U6 TP4056, R13 PROG, LED4 CHRG, LED5 DONE       │
      │     C9, C10 TP4056 caps                             │
      │     J5 JST connector for battery                    │
      │     U7 MT3608, L1 inductor, D7 SS14, R17/R18 FB     │
      │     C11/C12 boost caps                              │
      │     R19/R20 ADC battery divider                     │
      │     U2 AMS1117 LDO 3V3                              │
      │     C1, C2, C6, C7 LDO caps                         │
y=80  ├─────────────────────────────────────────────────────┤
      │ Z3: WATCHDOG PERIPHERALS (y=80 to 95)               │
      │     LED1/2/3 RGB indicators with corrected polarity │
      │     R1/R2/R3 LED resistors                          │
      │     BZ1 Buzzer + Q2 transistor                      │
      │     R8/R9 I2C pull-ups                              │
      │     J2 UART header, J3 expansion                    │
y=95  ├─────────────────────────────────────────────────────┤
      │ Z4: PAYMENTBOX PERIPHERALS (y=95 to 110) — DNP for E│
      │     U4 ATECC608B, U5 microSD + C_SD bulk (NEW)      │
      │     OPT1 PC817, Q1 NPN, D4 SS14 (NEW, was 1N4148)   │
      │     R5/R7 driver resistors                          │
      │     K1 relay, J4 terminal block                     │
y=110 └─────────────────────────────────────────────────────┘
       x=0                                              x=75
```

---

## 6. Battery runtime estimate

**Cell**: 18650 protected, 2500-3400mAh @ 3.7V nominal

| Cell capacity | Stored energy | Avg load | Runtime |
|---|---|---|---|
| 2500mAh | 9.25 Wh | 0.8W (idle + LCD off) | **11.5 h** |
| 2500mAh | 9.25 Wh | 1.2W (LCD on + WiFi active) | **7.7 h** |
| 3400mAh | 12.6 Wh | 0.8W | **15.7 h** |
| 3400mAh | 12.6 Wh | 1.2W | **10.5 h** |

→ **MVP: 8-12 giờ backup khi mất điện.** Đủ qua đêm cho hầu hết trường hợp.

Production-grade upgrade option: 2 cells parallel (1S2P) → 24h+ runtime, cost +50k.

---

## 7. Safety checklist trước khi power on lần đầu

1. ☐ Verify Q3 P-FET orientation đúng chiều (source ở phía USB-C, drain ở phía load)
2. ☐ Verify Q3 gate kết nối đúng (gate → R23/R24 divider)
3. ☐ Cắm USB-C → đo +5V_SAFE = ~4.8-4.9V (drop qua P-FET ~100mV)
4. ☐ Cắm pin 18650 chiều đúng (đỏ → BAT+, đen → BAT-) → đo BAT+ = 3.0-4.2V
5. ☐ Cắm pin ngược → đo +5V_SAFE = 0V (chứng tỏ chống ngược hoạt động)
6. ☐ Đo +5V_SYS = 5.0V ± 0.1V khi chỉ chạy pin
7. ☐ Đo +3V3 = 3.30V ± 0.05V
8. ☐ Đo dòng tổng @ 5V_SYS khi idle = 80-100mA
9. ☐ Bấm RESET → ESP32 boot OK qua UART
10. ☐ Verify LED test pattern qua test firmware: tất cả 3 LED bật khi GPIO drive HIGH

---

## 8. Field testing protocol

**Test sét/surge** (mô phỏng):
- Sử dụng máy phát surge IEC 61000-4-5 tại lab điện tử (Trung tâm Đo lường Hà Nội/HCM cho thuê)
- Apply 1kV pulse vào USB-C VBUS qua adapter → board phải còn hoạt động
- Nếu không có lab: test bằng cách bật/tắt máy hàn 220V cạnh thiết bị nhiều lần

**Test ngược cực**:
- Tháo pin, cắm ngược → đo board không bị nóng (P-FET cắt thành công)
- Cắm đúng cực lại → board boot OK

**Test mất điện**:
- Đang chạy USB-C → rút USB-C → board phải vẫn chạy (boost từ pin)
- LED status không tắt (nguồn liền mạch)
- Sau 8h chạy pin → battery low warning bật

---

**Tiếp theo: components_paymentbox_v8_2.py và nets_paymentbox_v8_2.py**
