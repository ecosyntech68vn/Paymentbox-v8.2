# LAYOUT V8.1 — PaymentBox PCB Zone Map

**PCB size:** 70mm × 100mm
**Layers:** 2 (F.Cu top, B.Cu bottom)
**Origin:** Bottom-left = (0, 0). KiCad convention: +Y goes DOWN visually.

---

## Zone overview

```
y=0   ┌─────────────────────────────────────────────────────┐
      │ Z0: ANTENNA KEEPOUT (top 12mm, no copper)           │
y=12  ├─────────────────────────────────────────────────────┤
      │                                                     │
      │ Z1: ESP32 MODULE                                    │
      │    U1 ESP32-WROOM-32 (centered)                     │
      │    C3-C5 decoupling                                 │
      │    SW1 RESET, SW2 BOOT                              │
      │    R6, R10 pull-ups                                 │
      │                                                     │
y=42  ├─────────────────────────────────────────────────────┤
      │                                                     │
      │ Z2: POWER                                           │
      │    J1 USB-C (left)                                  │
      │    U2 AMS1117-3.3V LDO                              │
      │    C1 bulk 10µF, C2 100nF                           │
      │    FB1 ferrite bead, D5 TVS                         │
      │                                                     │
y=62  ├─────────────────────────────────────────────────────┤
      │                                                     │
      │ Z3: PERIPHERALS — Watchdog (E version)              │
      │    LED1, LED2, LED3 (RGB indicator)                 │
      │    BZ1 buzzer + Q2 transistor                       │
      │    J2 UART header, J3 expansion header              │
      │                                                     │
y=82  ├─────────────────────────────────────────────────────┤
      │                                                     │
      │ Z4: PERIPHERALS — PaymentBox (F version) DNP for E  │
      │    U4 ATECC608B (SOIC-8)                            │
      │    U5 microSD socket                                │
      │    K1 relay, OPT1 opto, Q1 NPN, D4 flyback          │
      │    J4 terminal block 3-pin                          │
      │                                                     │
y=100 └─────────────────────────────────────────────────────┘
       x=0                                              x=70
```

---

## Detailed coordinates (mm)

### Z0 — Antenna keepout
- y = 0 to 12: **No copper pour, no traces, no components.**
- ESP32 antenna will radiate freely.

### Z1 — ESP32 module zone (y = 12 to 42)

| Ref | Component | x | y | Rot | Notes |
|---|---|---|---|---|---|
| U1 | ESP32-WROOM-32 | 35 | 24 | 0 | Centered, antenna up |
| C3 | 100nF 0805 | 24 | 28 | 0 | Decoupling pad 2 (3V3) |
| C4 | 100nF 0805 | 46 | 28 | 0 | Decoupling pad 20 (VDD_SPI) |
| C5 | 10µF tantalum 0805 | 24 | 32 | 0 | Bulk 3V3 |
| R6 | 10kΩ 0805 | 22 | 36 | 90 | EN pull-up |
| R10 | 10kΩ 0805 | 48 | 36 | 90 | GPIO0 pull-up |
| SW1 | Push 6x6mm | 12 | 38 | 0 | RESET (EN to GND) |
| SW2 | Push 6x6mm | 58 | 38 | 0 | BOOT (GPIO0 to GND) |

### Z2 — Power zone (y = 42 to 62)

| Ref | Component | x | y | Rot | Notes |
|---|---|---|---|---|---|
| J1 | USB-C Amphenol 12401548E4-2A | 10 | 50 | 90 | USB-C facing left edge |
| FB1 | Ferrite 0805 | 22 | 48 | 0 | VBUS to 5V_SYS |
| D5 | TVS PESD5V0S1UB SOT-23-3 | 22 | 52 | 0 | ESD protection |
| C1 | 10µF tantalum 0805 | 30 | 48 | 0 | Bulk 5V |
| U2 | AMS1117-3.3 SOT-223-3 | 40 | 50 | 0 | LDO 3.3V |
| C2 | 100nF 0805 | 33 | 52 | 0 | LDO input decoupling |
| C6 | 22µF tantalum 0805 | 50 | 50 | 0 | LDO output bulk |
| C7 | 100nF 0805 | 50 | 54 | 0 | LDO output decoupling |
| R11 | 5.1kΩ 0805 | 5 | 55 | 90 | USB-C CC1 pull-down |
| R12 | 5.1kΩ 0805 | 5 | 60 | 90 | USB-C CC2 pull-down |

### Z3 — Watchdog peripherals (y = 62 to 82)

| Ref | Component | x | y | Rot | Notes |
|---|---|---|---|---|---|
| LED1 | LED 0805 RED | 10 | 66 | 0 | Sự cố |
| LED2 | LED 0805 GREEN | 10 | 70 | 0 | OK status |
| LED3 | LED 0805 BLUE | 10 | 74 | 0 | Hoạt động |
| R1 | 220Ω 0805 | 15 | 66 | 0 | LED1 series |
| R2 | 220Ω 0805 | 15 | 70 | 0 | LED2 series |
| R3 | 220Ω 0805 | 15 | 74 | 0 | LED3 series |
| BZ1 | Buzzer CUI CPT-9019S-SMT | 28 | 72 | 0 | Active buzzer 5V |
| Q2 | NPN SOT-23-3 (MMBT3904) | 25 | 78 | 0 | Buzzer drive |
| R4 | 1kΩ 0805 | 22 | 78 | 0 | Q2 base |
| R8 | 4.7kΩ 0805 | 38 | 66 | 0 | I2C SDA pull-up |
| R9 | 4.7kΩ 0805 | 38 | 70 | 0 | I2C SCL pull-up |
| J2 | PinHeader 1x4 2.54mm | 55 | 68 | 90 | UART debug |
| J3 | PinHeader 1x6 2.54mm | 62 | 72 | 90 | Expansion (LCD module) |

### Z4 — PaymentBox peripherals (y = 82 to 100, DNP for E)

| Ref | Component | x | y | Rot | Notes |
|---|---|---|---|---|---|
| U4 | ATECC608B SOIC-8 | 12 | 88 | 0 | HMAC hardware (F only) |
| C8 | 100nF 0805 | 18 | 92 | 0 | ATECC decoupling |
| U5 | microSD Hirose DM3AT | 32 | 92 | 0 | SD socket (F only) |
| OPT1 | PC817 DIP-4 | 50 | 88 | 0 | Relay opto-isolator (F only) |
| R7 | 1kΩ 0805 | 45 | 86 | 0 | Opto LED resistor |
| R5 | 1kΩ 0805 | 50 | 92 | 0 | Q1 base |
| Q1 | NPN SOT-23 | 55 | 88 | 0 | Relay drive (F only) |
| D4 | 1N4148 SOD-123 | 60 | 88 | 0 | Relay flyback (F only) |
| K1 | Relay SPDT THT (SRD-05VDC) | 60 | 94 | 0 | Reset relay (F only) |
| J4 | TerminalBlock 1x3 P5mm | 66 | 90 | 0 | Relay output (F only) |

### Mounting & fiducials

| Ref | Component | x | y | Notes |
|---|---|---|---|---|
| MH1 | M3 mounting | 4 | 4 | Top-left |
| MH2 | M3 mounting | 66 | 4 | Top-right |
| MH3 | M3 mounting | 4 | 96 | Bottom-left |
| MH4 | M3 mounting | 66 | 96 | Bottom-right |
| FID1 | Fiducial 1mm | 4 | 50 | Left edge |
| FID2 | Fiducial 1mm | 66 | 50 | Right edge |
| FID3 | Fiducial 1mm | 35 | 96 | Bottom center |

---

## Routing strategy

- **Power**: Star topology from USB-C → 5V_SYS rail. Wide traces 0.4mm.
- **GND**: Full pour both layers, stitching vias on perimeter every 5mm.
- **I2C**: Short traces, 0.2mm width, parallel run between U1, U4, J3.
- **SPI**: Bottom layer mostly, 0.2mm width, length-matched within ±2mm.
- **GPIO signals**: 0.2mm width, route freely.
- **Antenna keepout**: Z0 area NO traces, NO copper pour. Add KEEPOUT zone in pcbnew.

---

## DNP (Do Not Populate) for E version

Components that exist on PCB but NOT soldered for Phương án E:
- U4 (ATECC608B)
- U5 (microSD socket)
- C8 (ATECC decoupling)
- OPT1, Q1, D4, K1, J4, R5, R7 (relay subsystem)

→ Same PCB Gerber for both versions, **save tooling cost 100%**.

---

## Design rules summary

| Item | Value |
|---|---|
| Min trace width | 0.20 mm |
| Min clearance | 0.15 mm |
| Min via | 0.40 / 0.60 mm (hole / pad) |
| Via stitching pitch (GND) | 5 mm |
| Power trace width | 0.40 mm |
| HV-LV separation | N/A (no HV on this board) |
| Castellated edges | No |
| Edge plating | No |
| Surface finish | ENIG |
| Solder mask | Green |
| Silkscreen | White, top only |
| Copper weight | 1 oz |
| PCB thickness | 1.6 mm |

---

**Tiếp**: `components.py` và `nets.py` sẽ encode toàn bộ LAYOUT này thành dữ liệu cho pipeline KiCad.
