# EcoSynTech PaymentBox V8.2 — Complete Package

**Production-ready hardware + firmware** cho hệ thống tự động hóa thu tiền VietQR
qua phone Android + ESP32 watchdog.

Tác giả: TA QUANG THUAN — CEO/CTO EcoSynTech Global
Ngày: 20/05/2026

---

## Cấu trúc package

```
EcoSynTech_PaymentBox_V8.2_complete/
├── README.md                          ← Bạn đang đọc
├── docs/                              ← Documentation
│   ├── FEASIBILITY.md                  Đánh giá khả thi E và F
│   ├── AUDIT_V8_1.md                   11 lỗi tìm thấy trong V8.1
│   ├── 01_SCHEMATIC_V8_1.md            Schematic V8.1 (deprecated)
│   ├── 01_LAYOUT_V8_1.md               Layout V8.1 (deprecated)
│   ├── SCHEMATIC_V8_2.md               ★ Schematic V8.2 production
│   └── GAS_ENDPOINTS_V8_2.md           ★ Spec backend GAS hỗ trợ firmware
│
├── hardware/                          ← PCB V8.2 fab-ready
│   ├── paymentbox_v8_2_jlcpcb.zip      ★ Upload thẳng JLCPCB
│   ├── pcb_layout_v8_2.png             Layout visual (6 zones)
│   ├── board.step                      3D model cho case design
│   ├── board_top.pdf                   Top view PDF
│   ├── BOM_E_watchdog.csv              BOM phương án E
│   ├── BOM_F_full.csv                  BOM phương án F
│   ├── Pick_Place_F.csv                CPL cho SMT
│   ├── gerber/                         Gerber files
│   ├── drill/                          Drill files
│   ├── components_v8_2.py              Component placement source
│   └── nets_v8_2.py                    Netlist source
│
└── firmware/                          ← ESP-IDF V5.x firmware
    ├── README.md                       Build instructions
    ├── TEST_REPORT.md                  ★ 68/68 tests pass, 0 cppcheck warnings
    ├── CMakeLists.txt
    ├── sdkconfig.defaults
    ├── partitions.csv
    ├── main/
    │   ├── main.c                      Entry point
    │   └── include/paymentbox_config.h  Centralized config
    ├── tests/                          ★ Host-side unit tests
    │   ├── test_config_server.c        33 tests pass
    │   └── test_ota_updater.c          35 tests pass
    └── components/
        ├── event_bus/                  Pub/sub FreeRTOS queue
        ├── wifi_manager/               STA + AP fallback
        ├── config_server/              ★ HTTP form 192.168.4.1
        ├── ota_updater/                ★ esp_https_ota từ GAS
        ├── phone_poller/               Poll phone /health 30s
        ├── gas_poller/                 Poll GAS heartbeat 60s
        ├── led_status/                 5-state machine 3-LED PWM
        ├── buzzer/                     Beep patterns
        ├── lcd_ui/                     I2C LCD 16x2
        ├── battery_monitor/            ADC + USB-lost detection
        └── sd_logger/                  NDJSON log + rotation
```

★ = milestone V8.2 production-ready

---

## Quick start — 4 bước

### 1. Order PCB (1 ngày)
Upload `hardware/paymentbox_v8_2_jlcpcb.zip` lên JLCPCB:
- 5 boards, 2-layer, ENIG, HASL, 1.6mm
- Cost: ~$8 + ship $10 = **~450k VND total**

### 2. Order components (1 ngày, song song)
Dùng `hardware/BOM_E_watchdog.csv` đặt LCSC hoặc Shopee VN:
- Tổng linh kiện E: **~193k VND** mỗi board
- Plus pin 18650 protected 2500mAh: +50k VND
- Plus phone Android cũ: +400k VND
→ **Tổng ~700k VND/unit**

### 3. Lắp ráp + test hardware (1 buổi)
Theo `docs/SCHEMATIC_V8_2.md` section 7 (Safety checklist 10 điểm).
- Cấp USB-C → đo +5V_SAFE = 4.8-4.9V
- Cắm pin đúng → BAT+ = 3-4.2V
- **Cắm pin ngược → +5V_SAFE = 0V** (verify chống ngược cực)

### 4. Build + flash firmware (30 phút)
```bash
cd firmware
. $IDF_PATH/export.sh
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Reboot lần đầu → AP "PaymentBox-Setup" → mở `http://192.168.4.1` → nhập WiFi + phone IP → reboot vào STA mode → vận hành.

---

## Tính năng đã có

✅ **Hardware**:
- ESP32-WROOM-32 + đầy đủ peripheral
- USB-C input + PPTC fuse + SMBJ5.0CA TVS (chống sét)
- AO3401 P-FET (chống ngược cực pin)
- TP4056 sạc Li-ion + MT3608 boost 3.7→5V
- Pin 18650 external qua JST connector
- ADC battery monitor + USB-lost detection
- 3 LED indicator + buzzer + LCD 16x2
- I2C bus mở rộng, SPI cho SD card
- (Optional F) ATECC608B HMAC + microSD + relay reset phone

✅ **Firmware** (2654 LOC C):
- Event bus FreeRTOS pub/sub 8 subscribers
- WiFi STA + AP fallback config
- **HTTP config server** captive portal (Vietnamese UTF-8 form)
- **OTA update từ GAS** với anti-downgrade + boot sanity 60s + rollback
- Phone health poller 30s
- GAS heartbeat poller 60s
- 5-state LED state machine (IDLE/OK/WARN/ALERT/CRITICAL)
- Buzzer alert patterns
- LCD UI 2Hz refresh
- Battery monitor ADC + USB-lost dV/dt detection
- SD logger NDJSON với rotation

✅ **Testing**:
- 68/68 host-side unit tests pass
- 0 cppcheck warnings (warning, style, performance, portability)
- Hardware test plan trong `firmware/TEST_REPORT.md`

---

## Chưa làm (TODO)

| Feature | Mức | Khi nào cần |
|---|---|---|
| ATECC608B driver firmware | High | Phương án F production |
| HMAC sign transaction trước khi push GAS | High | Phương án F |
| Battery check trước OTA (skip nếu < 3.5V) | Med | Sau khi field test 1 tháng |
| SHA256 verify firmware từ GAS | Med | Bảo mật cao hơn |
| Mobile app config qua BLE | Low | Phương án F UX nâng cao |
| 2.4" TFT thay LCD 16x2 | Low | Phương án F UX nâng cao |
| SOP conformal coating spray | High (production) | Trước khi giao khách F |
| Hộp ABS IP65 + EPDM gasket | High (production) | Trước khi giao khách F |

---

## Liên hệ

Mọi câu hỏi / báo lỗi → TA QUANG THUAN
- Telegram: @AIThucChien_bot (cho hỗ trợ)
- Email/SĐT: liên hệ EcoSynTech Global
