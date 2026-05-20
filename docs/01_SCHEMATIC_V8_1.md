# SCHEMATIC — PaymentBox V8.1-rev1.0

**PCB:** ESG-PB-V8.1
**Stack:** 2-layer FR-4 1.6mm, ENIG, 1oz copper
**Kích thước:** 60mm × 90mm
**Mục đích:** Watchdog (E) + PaymentBox (F) trên cùng 1 design, populate khác nhau

---

## 1. Block diagram

```
                              ┌─────────────────────┐
                              │  USB-C 5V Input     │
                              │  (J1, 16-pin SMD)   │
                              └──────────┬──────────┘
                                         │ 5V
                          ┌──────────────┼──────────────┐
                          │              │              │
                    ┌─────▼─────┐   ┌────▼────┐   ┌────▼────┐
                    │ AMS1117   │   │ LCD 16x2│   │ Buzzer  │
                    │ -3.3V LDO │   │ (5V)    │   │ (5V)    │
                    └─────┬─────┘   └────┬────┘   └────▲────┘
                          │ 3.3V         │             │
        ┌─────────────────┼──────────────┼─────────────┤
        │                 │              │             │
   ┌────▼────┐      ┌─────▼──────┐       │       ┌─────┴────┐
   │ ATECC   │◄─I2C─┤            ├──I2C──┘       │ GPIO buzz│
   │ 608B    │      │  ESP32-    │               │   PWM    │
   │ (F only)│      │  WROOM-32  ├──GPIO──┬──────┤          │
   └─────────┘      │            │        │      │          │
                    │  (U1)      │        │      │          │
   ┌─────────┐      │            ├──SPI───┤      │          │
   │ LED RGB │◄─PWM─┤            │        │      │          │
   │ (3 LEDs)│      │            │        │      │          │
   └─────────┘      │            │        │      │          │
                    │            │        ▼      │          │
                    │            │   ┌─────────┐ │          │
   ┌─────────┐      │            │   │ SD card │ │          │
   │ Reset   │──────┤ EN         │   │ (F only)│ │          │
   │ Boot    │──────┤ IO0        │   └─────────┘ │          │
   │ buttons │      │            │               │          │
   └─────────┘      │            ├──GPIO─────────┤          │
                    │            │               │          │
                    │            │               │   ┌──────▼─────┐
                    │            │               │   │ Relay 5V   │
                    │            │               │   │ (F only,   │
                    │            │               │   │ reset USB  │
                    │            │               │   │ of phone)  │
                    │            │               │   └────────────┘
                    │            │
                    │            ├──UART──── Pin header J2 (debug)
                    │            ├──I2C+GPIO──── Pin header J3 (expansion)
                    │            │
                    │  WiFi/BLE  │
                    └──────╫─────┘
                           ║ Onboard PCB antenna
                           ║ (ESP32-WROOM-32 module has antenna)
```

---

## 2. Power Tree

```
USB-C 5V (J1, 5V@2A)
       │
       ├──> 5V_SYS rail
       │     ├──> LCD 16x2 module (VCC pin)
       │     ├──> Buzzer (active 5V)
       │     ├──> Relay coil (Phương án F)
       │     └──> Pin header J3 (5V out for external)
       │
       └──> AMS1117-3.3V (U2)
              │
              └──> 3V3 rail
                    ├──> ESP32-WROOM-32 (VDD, ~500mA peak when WiFi TX)
                    ├──> ATECC608B (VCC, ~5mA)
                    ├──> SD card socket (VCC, ~100mA write)
                    ├──> LED RGB common anode (R-G-B legs through 220Ω)
                    └──> Decoupling caps

Estimated max current: 5V@1.2A peak, 3.3V@800mA peak
USB-C 5V/2A nguồn đủ rộng rãi
```

---

## 3. Pin mapping ESP32-WROOM-32

| GPIO | Tên | Chức năng | Strapping |
|---|---|---|---|
| GPIO0 | BOOT | Pull-up + button | Strap (must HIGH at boot for normal) |
| GPIO1 | TX | UART debug | Header J2 pin 2 |
| GPIO2 | LED_B | Blue LED PWM | Strap (must LOW at boot for download) |
| GPIO3 | RX | UART debug | Header J2 pin 3 |
| GPIO4 | LED_G | Green LED PWM | - |
| GPIO5 | SPI_CS_SD | SD card CS | Strap (must HIGH at boot for SD) |
| GPIO12 | LED_R | Red LED PWM | Strap (must LOW at boot — flash voltage 3.3V) |
| GPIO13 | BUZZER | Buzzer PWM | - |
| GPIO14 | SPI_SCK | SD card clock | - |
| GPIO15 | RELAY_CTL | Relay control (F only) | Strap (no debug at boot) |
| GPIO16 | (reserved) | - | - |
| GPIO17 | (reserved) | - | - |
| GPIO18 | SPI_SCK | (alternative) | - |
| GPIO19 | SPI_MISO | SD card MISO | - |
| GPIO21 | I2C_SDA | I2C bus (LCD + ATECC) | - |
| GPIO22 | I2C_SCL | I2C bus | - |
| GPIO23 | SPI_MOSI | SD card MOSI | - |
| GPIO25 | (reserved) | DAC capable | - |
| GPIO26 | (reserved) | DAC capable | - |
| GPIO27 | EXP_GPIO1 | Header J3 expansion | - |
| GPIO32 | EXP_GPIO2 | Header J3 expansion | - |
| GPIO33 | (reserved) | - | - |
| GPIO34 | (input only) | (reserved) | - |
| GPIO35 | (input only) | (reserved) | - |
| GPIO36 | VP | Analog input (battery monitor optional) | - |
| GPIO39 | VN | (reserved) | - |
| EN | RESET | Reset button + pull-up | - |

**Strapping pins respected**: GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 đều được handle đúng (pull-up/down theo Espressif spec).

---

## 4. I2C bus (3V3, 100kHz default)

| Address | Device | Ghi chú |
|---|---|---|
| 0x27 hoặc 0x3F | PCF8574 (LCD backpack) | Tùy module mua sẵn |
| 0x60 | ATECC608B | (Phương án F only) |
| 0x68 | (Reserved cho RTC nếu cần sau) | DS3231 footprint nếu cần |

Pull-up 4.7kΩ trên SDA/SCL (R8, R9).

---

## 5. SPI bus (3V3, 1MHz cho SD)

| Pin | ESP32 | SD Card |
|---|---|---|
| SCK | GPIO14 | CLK |
| MOSI | GPIO23 | CMD |
| MISO | GPIO19 | DAT0 |
| CS | GPIO5 | DAT3 |

SD card socket: microSD slot SMT (Molex 47309-2691 hoặc tương đương).

---

## 6. LED RGB scheme

3 LEDs riêng (R/G/B) thay vì WS2812 RGB strip — đơn giản, không cần driver.

| LED | GPIO | Resistor | Trạng thái |
|---|---|---|---|
| LED_R (D1) | GPIO12 | 220Ω | Sự cố (đỏ pulse) |
| LED_G (D2) | GPIO4 | 220Ω | OK (xanh steady) |
| LED_B (D3) | GPIO2 | 220Ω | Hoạt động (xanh blink khi có giao dịch mới) |

Cathode chung GND. Anode qua resistor lên GPIO.

---

## 7. Buzzer (active 5V)

- Driver: GPIO13 → NPN transistor (Q2 SOT-23) → buzzer (+) → 5V
- Có thể PWM tone qua LEDC peripheral của ESP32
- Patterns: 
  - 1 beep ngắn = giao dịch mới
  - 3 beep liên tiếp = phone offline
  - Continuous = sự cố nghiêm trọng

---

## 8. Relay (Phương án F only — DNP cho E)

- K1: 5V SPDT relay (SRD-05VDC-SL-C tương đương)
- Drive: GPIO15 → R7 (1kΩ) → opto LED (PC817) → opto collector → Q1 NPN → relay coil low-side
- Flyback diode D4 (1N4148) ngược song song với relay coil
- Relay COM + NO/NC ra terminal block 3-pin để nối với USB power phone

---

## 9. Connector pinout

### J1 — USB-C 16-pin SMD (power only)
- VBUS (A4, A9, B4, B9): tất cả nối 5V_SYS
- GND (A1, A12, B1, B12): tất cả nối GND
- CC1, CC2 (A5, B5): qua 5.1kΩ xuống GND (báo device là source 5V/3A)
- Các pin khác NC

### J2 — UART debug header (1x4 2.54mm)
| Pin | Tín hiệu |
|---|---|
| 1 | GND |
| 2 | TX (GPIO1) |
| 3 | RX (GPIO3) |
| 4 | 3V3 (cấp nguồn USB-UART converter nếu cần) |

### J3 — Expansion header (1x6 2.54mm)
| Pin | Tín hiệu |
|---|---|
| 1 | 5V |
| 2 | 3V3 |
| 3 | GND |
| 4 | I2C_SDA |
| 5 | I2C_SCL |
| 6 | GPIO27 (free I/O) |

### J4 — Relay terminal block 3-pin (F only, DNP)
| Pin | Tín hiệu |
|---|---|
| 1 | COM (common) |
| 2 | NO (normally open) |
| 3 | NC (normally closed) |

---

## 10. Netlist (đầy đủ)

### 10.1 Power nets

**+5V_SYS** (USB-C VBUS):
- J1 pins A4, A9, B4, B9 (USB-C VBUS — 4 pads ghép parallel)
- C1 (bulk 10µF tantalum)
- C2 (decoupling 100nF)
- U2 IN (AMS1117 input pin 3)
- LCD_VCC (qua J3 thực ra, header để cắm LCD module ngoài)
- BZ1 collector side
- K1 coil + (Phương án F)

**+3V3** (LDO output):
- U2 OUT (AMS1117 output pin 2)
- C3 (10µF tantalum)
- C4-C5 (100nF decoupling)
- U1 (ESP32) all 3V3 pins: pin 2 (3V3)
- U4 (ATECC608B) pin 8 VCC — F only
- SD_VCC (qua SD socket)
- LED anode commons (LED1, LED2, LED3 anodes thực ra mỗi LED có anode riêng)
- I2C pull-up R8, R9 high side

**GND**:
- All components GND pins (chi tiết trong nets.py)

### 10.2 Signal nets

**I2C_SDA** (GPIO21): U1 pin 33, U4 pin 5 (F), R8 low side, J3 pin 4
**I2C_SCL** (GPIO22): U1 pin 36, U4 pin 6 (F), R9 low side, J3 pin 5

**SPI_SCK** (GPIO14): U1 pin 13, SD_CLK
**SPI_MOSI** (GPIO23): U1 pin 37, SD_CMD
**SPI_MISO** (GPIO19): U1 pin 31, SD_DAT0
**SPI_CS_SD** (GPIO5): U1 pin 29, SD_DAT3

**UART_TX** (GPIO1): U1 pin 35, J2 pin 2
**UART_RX** (GPIO3): U1 pin 34, J2 pin 3

**LED_R_DRV** (GPIO12): U1 pin 14, R1 (220Ω) → LED1 anode
**LED_G_DRV** (GPIO4): U1 pin 26, R2 (220Ω) → LED2 anode
**LED_B_DRV** (GPIO2): U1 pin 24, R3 (220Ω) → LED3 anode

**BUZZ_DRV** (GPIO13): U1 pin 16, R4 (1kΩ) → Q2 base, Q2 emitter → GND, Q2 collector → BZ1 (-) → BZ1 (+) → +5V_SYS

**RELAY_DRV** (GPIO15, F only): U1 pin 23, R7 (1kΩ) → OPT1 LED anode, OPT1 LED cathode → GND, OPT1 collector → Q1 base via R5, Q1 emitter → GND, Q1 collector → K1 coil (-), K1 coil (+) → 5V_SYS, D4 anode → K1 coil (-), D4 cathode → 5V_SYS

**SW1_RESET**: EN pin pulled high via R6 (10kΩ) to 3V3, SW1 connects EN to GND
**SW2_BOOT**: GPIO0 pulled high via R10 (10kΩ) to 3V3, SW2 connects GPIO0 to GND

---

## 11. Bảo vệ và ESD

- TVS diode D5 (PESD5V0S1UB) trên USB-C VBUS chống ESD
- Tụ 100nF gần mỗi IC VCC pin (ESP32 × 2, ATECC × 1)
- Ferrite bead FB1 (giữa USB-C VBUS và 5V_SYS) chống nhiễu

---

## 12. Mounting

- 4 mounting holes M3 ở 4 góc, cách edge 5mm
- 3 fiducial markers cho pick-and-place

---

## 13. Manufacturing constraints

- **Min trace width**: 0.2mm (0.15mm cho signal mật độ cao)
- **Min via**: 0.4mm hole / 0.6mm pad
- **Min clearance**: 0.15mm
- **Solder mask**: green standard
- **Silkscreen**: white, top side only
- **Castellated edges**: Không
- **Edge plating**: Không
- **Surface finish**: ENIG (đẹp + chống oxy hóa)
- **Quantity batch**: 5 board prototype, sau đó 50 nếu vào production

JLCPCB cost estimate (5 boards 60×90mm ENIG 2-layer): ~$8 + ship ~$10 = **~450k VND** cho 5 prototype.

---

## 14. Mở rộng tương lai (designed-in nhưng DNP)

| Feature | Trạng thái V8.1 | V9.0+ |
|---|---|---|
| ATECC608B | DNP cho E, populate cho F | Populate luôn |
| SD card | DNP cho E, populate cho F | Populate luôn |
| Relay 5V | DNP cho E, populate cho F | Populate luôn |
| TFT 2.4" connector | KHÔNG có ở V8.1 (dùng LCD 16x2 đơn giản) | Thêm header SPI 8-pin |
| Battery LiPo | KHÔNG (dùng USB-C only) | Thêm TP4056 + 18650 |
| eMMC onboard | KHÔNG (dùng SD) | Thêm chip 8GB onboard |
| 4G LTE | KHÔNG (dùng WiFi only) | Thêm module qua UART |

---

**Tiếp: LAYOUT.md** sẽ định vị từng component trên PCB 60×90mm.
