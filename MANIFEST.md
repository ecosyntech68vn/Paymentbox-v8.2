# MANIFEST — EcoSynTech PaymentBox V8.2 Complete Package

Danh sách đầy đủ files và mô tả từng file. Đảm bảo không thiếu gì khi nhận package.

---

## 📄 Documentation (`docs/`)

| File | Mô tả | Mức ưu tiên đọc |
|---|---|---|
| `FEASIBILITY.md` | Đánh giá khả thi phương án E (Watchdog) và F (PaymentBox) qua SWOT, 5W1H | ⭐⭐⭐ Đọc đầu tiên |
| `AUDIT_V8_1.md` | 11 lỗi phát hiện trong V8.1, mức độ và cách fix | ⭐⭐⭐ |
| `SCHEMATIC_V8_2.md` | **Schematic V8.2 production**: block diagram, pin map, BOM mới, safety checklist 10 bước | ⭐⭐⭐ |
| `GAS_ENDPOINTS_V8_2.md` | Spec endpoint GAS V9.5 cần implement để hỗ trợ firmware | ⭐⭐⭐ Đọc trước khi flash |
| `01_SCHEMATIC_V8_1.md` | Schematic V8.1 (deprecated, lưu lại để tra cứu) | ⭐ |
| `01_LAYOUT_V8_1.md` | Layout V8.1 (deprecated) | ⭐ |

---

## 🔧 Hardware (`hardware/`)

### ⭐ Files quan trọng cho fabrication

| File | Mô tả | Cách dùng |
|---|---|---|
| `paymentbox_v8_2_jlcpcb.zip` | **Submission ZIP cho JLCPCB**, chứa Gerbers + drill + BOM + CPL | Upload thẳng lên jlcpcb.com → đặt 5 board |
| `pcb_layout_v8_2.png` | Visual layout 6 zones, components màu | Mở xem để hiểu vị trí component |
| `BOM_E_watchdog.csv` | BOM phương án E (không có F-only DNP) | Đặt linh kiện LCSC/Shopee |
| `BOM_F_full.csv` | BOM phương án F (đầy đủ) | Khi nâng cấp lên PaymentBox |
| `board.step` | 3D model PCB | Import Fusion360/FreeCAD design case |
| `board_top.pdf` | PDF mặt trên (F.Cu + F.Silkscreen) | In ra để kiểm tra lắp ráp |
| `board_bottom.pdf` | PDF mặt dưới (B.Cu + B.Silkscreen) | In ra để kiểm tra lắp ráp |

### Files chi tiết

| File | Mô tả |
|---|---|
| `gerber/*.gbr` | 9 files Gerber riêng từng layer (F_Cu, B_Cu, F_Mask, B_Mask, F_Silkscreen, B_Silkscreen, F_Paste, B_Paste, Edge_Cuts) |
| `gerber/*.gbrjob` | Gerber job metadata |
| `drill/*.drl` | 2 file drill (PTH + NPTH) |
| `Pick_Place_F.csv` | CPL pick-and-place cho SMT assembly |
| `paymentbox_v8_2.kicad_pcb` | **File gốc KiCad** — mở bằng KiCad 7+ để edit/inspect |
| `dfm_report.json` | Báo cáo DFM check (0 blocker, 11 major acceptable) |
| `components_v8_2.py` | Source Python định nghĩa 74 components + vị trí |
| `nets_v8_2.py` | Source Python định nghĩa 45 nets + 201 pad connections |

---

## 💻 Firmware (`firmware/`)

### Top-level

| File | Mô tả |
|---|---|
| `README.md` | Hướng dẫn build & flash & config |
| `TEST_REPORT.md` | **Kết quả test**: 68/68 unit tests pass, 0 cppcheck warnings |
| `CMakeLists.txt` | Top-level CMake cho `idf.py build` |
| `sdkconfig.defaults` | ESP-IDF config (4MB flash, anti-rollback, mbedTLS HW accel) |
| `partitions.csv` | Partition table: factory + 2× OTA + spiffs |

### Main app (`firmware/main/`)

| File | Mô tả | LOC |
|---|---|---|
| `main.c` | Entry point, spawn task, OTA boot sanity 60s | 124 |
| `include/paymentbox_config.h` | **TẤT CẢ pin + ngưỡng + URL + OTA constants** | 145 |
| `CMakeLists.txt` | Component dependencies |

### Components (`firmware/components/`)

| Component | Mô tả | LOC chính |
|---|---|---|
| `event_bus/` | FreeRTOS queue pub/sub (8 subscribers max, 21 event types) | 78 |
| `wifi_manager/` | STA mode + AP fallback → auto-start config_server | 136 |
| `config_server/` | ⭐ **HTTP form 192.168.4.1** captive portal Tiếng Việt UTF-8 | 234 |
| `ota_updater/` | ⭐ **esp_https_ota từ GAS**, anti-downgrade, boot sanity rollback | 246 |
| `phone_poller/` | HTTP GET phone:8765/health 30s, 3-strike dead detection | 98 |
| `gas_poller/` | HTTPS GET GAS heartbeat 60s, parse age_s | 113 |
| `led_status/` | 5-state machine (IDLE/OK/WARN/ALERT/CRITICAL) drive 3 LED PWM | 168 |
| `buzzer/` | LEDC PWM beep patterns | 114 |
| `lcd_ui/` | I2C LCD 16x2 PCF8574 driver, 2Hz refresh | 177 |
| `battery_monitor/` | ADC1 CH0 sampling, dV/dt USB-lost detection | 151 |
| `sd_logger/` | NDJSON log + rotation (1MB → 5 backups) | 120 |

Mỗi component có cấu trúc:
```
<component_name>/
├── CMakeLists.txt
├── <component_name>.c
└── include/
    └── <component_name>.h
```

### Tests (`firmware/tests/`)

| File | Số tests | Pass |
|---|---|---|
| `test_config_server.c` | 33 (IPv4 valid/invalid, URL decode, form parser, UTF-8 Việt) | ✅ 33/33 |
| `test_ota_updater.c` | 35 (JSON parse, version cmp, downgrade reject) | ✅ 35/35 |

Cách chạy: `gcc -Wall test_X.c -o /tmp/t && /tmp/t` (KHÔNG cần ESP-IDF).

---

## 📌 Files KHÔNG có trong package (cố ý)

- ❌ `firmware/build/` — generate khi `idf.py build`
- ❌ `firmware/sdkconfig` — generate khi `idf.py reconfigure`
- ❌ V8.1 PCB outputs (deprecated, V8.2 thay thế hoàn toàn)
- ❌ Build scripts pipeline KiCad (skill nội bộ, không thuộc project)

---

## ✅ Verification checklist khi unzip

```bash
unzip EcoSynTech_PaymentBox_V8.2_complete.zip
cd EcoSynTech_PaymentBox_V8.2_complete

# Verify file count
find . -type f | wc -l   # Phải ra ~75 files

# Verify quan trọng nhất
test -f hardware/paymentbox_v8_2_jlcpcb.zip  && echo "✓ JLCPCB ZIP" || echo "✗ MISSING"
test -f hardware/BOM_E_watchdog.csv          && echo "✓ BOM E"      || echo "✗ MISSING"
test -f firmware/main/main.c                 && echo "✓ main.c"     || echo "✗ MISSING"
test -f firmware/components/ota_updater/ota_updater.c && echo "✓ OTA" || echo "✗ MISSING"
test -f firmware/components/config_server/config_server.c && echo "✓ Config" || echo "✗ MISSING"
test -f firmware/TEST_REPORT.md              && echo "✓ Test report" || echo "✗ MISSING"
test -f docs/SCHEMATIC_V8_2.md               && echo "✓ Schematic V8.2" || echo "✗ MISSING"
test -f docs/GAS_ENDPOINTS_V8_2.md           && echo "✓ GAS spec"   || echo "✗ MISSING"

# Run unit tests offline (verify code OK)
cd firmware/tests
gcc -Wall test_config_server.c -o /tmp/t1 && /tmp/t1 | tail -3
gcc -Wall test_ota_updater.c -o /tmp/t2 && /tmp/t2 | tail -3
# Phải thấy "33 pass / 0 fail" và "35 pass / 0 fail"
```

Nếu **tất cả 8 checks ✓**, package complete.
