# FEASIBILITY ANALYSIS — EcoSynTech PaymentBox

**Mã sản phẩm:** ESG-PB-V8.1
**Ngày phân tích:** 20/05/2026
**Tác giả phân tích:** Claude (cho TA QUANG THUAN, EcoSynTech Global)
**Phạm vi:** Đánh giá kỹ thuật phương án E (Watchdog) và F (PaymentBox thương mại)

---

## 1. Tóm tắt điều hành

| Tiêu chí | Phương án E (Watchdog) | Phương án F (PaymentBox) |
|---|---|---|
| **Tính khả thi kỹ thuật** | ✅ Cao (90%) | ✅ Cao (80%) |
| **Thời gian build** | 2.5-3 tuần | 4-6 tuần |
| **Rủi ro lớn nhất** | Phone không reset được khi treo | OTA firmware update phức tạp + tem CE/MIC |
| **ROI** | Tăng uptime từ 90% → 99% | 30-50tr/tháng nếu bán 20 unit |
| **Phù hợp expertise** | ✅ Tận dụng 100% V8 PCB hiện có | ✅ Là full-stack EcoSynTech |
| **Khuyến nghị** | Build trước (MVP) | Build sau khi E vận hành 2-3 tháng |

**Quyết định kỹ thuật**: Build **1 PCB chung V8.1-PaymentBox** populate khác nhau (DNP — Do Not Populate) cho từng phiên bản E và F. Tiết kiệm chi phí tooling, đỡ phải làm 2 design.

---

## 2. Phương án E — Android Watchdog (chi tiết kỹ thuật)

### 2.1 Kiến trúc luồng dữ liệu

```
┌─────────────────────────────────────────────────────────────┐
│  PHONE Android (đã có code BankNotify-App)                  │
│  - NotificationListener đọc VCB/MB notification             │
│  - HTTP API local cổng 8765 (NanoHTTPD)                     │
│  - WebhookManager push trực tiếp tới GAS (giữ nguyên)       │
└─────────────────┬──────────────────────────────┬────────────┘
                  │ HTTP poll                     │ HTTPS POST
                  │ /api/v1/health  /transactions/recent
                  │                               │
                  ▼                               ▼
┌──────────────────────────┐         ┌────────────────────────┐
│  ESP32 V8.1 (mới)        │         │  GAS V9.5 endpoint     │
│  - LAN WiFi client       │ ◄───────│  - Webhook ingest      │
│  - Poll phone 30s        │  HTTPS  │  - Heartbeat watchdog  │
│  - LCD 16x2 trạng thái   │  GET    │  - Telegram alert      │
│  - LED status RGB         │  /heartbeat/last
│  - Buzzer khi alert      │                                  │
│  - SD card log local     │         │  Phone đã push trực    │
│  - Optional: relay reset │         │  tiếp từ trước         │
│    nguồn USB phone       │         └────────────────────────┘
└──────────────────────────┘
```

ESP32 **KHÔNG nằm trên data path chính** — phone vẫn push GAS trực tiếp. ESP32 chỉ:
1. **Poll phone health** mỗi 30s qua HTTP GET `http://<phone_ip>:8765/api/v1/health`
2. **Poll GAS heartbeat status** (sheet `BankHeartbeat` row latest) → biết GAS có nhận heartbeat của phone không
3. **Display + alert** dựa trên 2 nguồn data trên
4. **Local SD log**: lưu mọi event để forensic nếu cloud sập

→ **Phone code KHÔNG cần sửa.** Đây là điểm mạnh lớn nhất của phương án E.

### 2.2 Bài toán "reset phone khi treo" — Có 4 cách

| Cách | Mô tả | Khả thi | Khuyến nghị |
|---|---|---|---|
| **A.** Smart plug cắt nguồn 220V | Relay 220V điều khiển ổ cắm phone | ❌ Phone có pin → không tắt được ngay | Không dùng |
| **B.** Cắt nguồn USB sạc + tháo pin | Phone tháo pin, chỉ sống bằng USB direct | ✅ Cắt USB = phone tắt ngay | Tốt nhưng phải hack phần cứng phone |
| **C.** ADB over WiFi gửi `adb reboot` | ESP32 telnet ADB tới phone | ⚠️ Phải root phone, ADB không stable | Phức tạp |
| **D.** Chỉ alert, không reset tự động | LED đỏ + buzzer + Telegram + email | ✅ Đơn giản, không over-engineer | **MVP nên dùng** |

**Kết luận**: MVP phương án E chỉ làm cách D. Khi nào real-world vận hành thấy phone treo >2 lần/tháng → mới nâng cấp lên cách B (chỉ cần thêm 1 relay 5V trên cùng PCB, DNP sẵn).

### 2.3 Bài toán "phone không cần internet trực tiếp"

Đây là tính năng **rất hay** nhưng chỉ available ở phương án F (Approach 1: ESP32 nằm trên data path).

Phương án E giữ phone push GAS trực tiếp → phone vẫn cần WiFi internet. Đổi lại: code Android không phải sửa.

→ **Trade-off chấp nhận được cho MVP.**

### 2.4 Đánh giá rủi ro phương án E

| Rủi ro | Xác suất | Tác động | Mitigation |
|---|---|---|---|
| ESP32 firmware crash | Thấp | Trung bình | Hardware WDT của ESP32 |
| ESP32 mất WiFi | Trung bình | Thấp | Reconnect logic + LED báo |
| Phone treo không reset tự động | Trung bình | Cao | Alert + manual intervention; tăng frequency check sheet GAS |
| LCD hỏng | Thấp | Thấp | LED RGB backup status |
| SD card đầy/lỗi | Thấp | Thấp | Log rotation, alert khi >80% |
| Nguồn USB-C mất | Thấp | Cao | Buzzer continuous + LED đỏ pulse |

### 2.5 BOM phương án E (chỉ populate những gì cần)

| Ref | Linh kiện | Số lượng | Giá VN | Ghi chú |
|---|---|---|---|---|
| U1 | ESP32-WROOM-32 module | 1 | 75k | Đã có trong V8 design |
| U2 | AMS1117-3.3V LDO | 1 | 4k | 3.3V cho ESP32 |
| U3 | LCD 16x2 I2C PCF8574 module | 1 | 55k | Mua nguyên module, không SMT |
| LED1-3 | LED 5mm RGB common-cathode | 3 | 6k | Status: GREEN/YELLOW/RED |
| BZ1 | Buzzer active 5V SMD | 1 | 8k | |
| J1 | USB-C female 16-pin | 1 | 12k | Power input only |
| J2 | Pin header 1x4 (UART debug) | 1 | 3k | |
| J3 | Pin header 1x6 (expansion) | 1 | 3k | I2C + GPIO out |
| SW1 | Tactile button (RESET) | 1 | 2k | |
| SW2 | Tactile button (BOOT) | 1 | 2k | |
| C1-C8 | Tụ MLCC 100nF 0805 | 8 | 4k | Decoupling |
| C9-C10 | Tụ tantalum 10uF 0805 | 2 | 6k | Bulk |
| R1-R10 | Điện trở 0805 (220Ω, 10k, 1k) | 10 | 5k | LED, pull-up |
| PCB | V8.1 size 60×80mm 2-layer | 1 | 50k | JLCPCB 5 boards/$2 |
| Case | ABS in 3D | 1 | 30k | |
| Antenna onboard | (đã có trong ESP32 module) | - | - | |
| **TỔNG/thiết bị** | | | **~265k** | |

Cộng phone Android cũ 400-500k → **tổng phương án E: 665-765k/thiết bị**.

### 2.6 Firmware phương án E (ESP-IDF)

```
main/
├── main.c                  # Entry, init WiFi, tasks
├── phone_poller.c          # Task polling phone /health + /transactions
├── gas_poller.c            # Task polling GAS heartbeat sheet
├── lcd_ui.c                # I2C LCD update task
├── led_status.c            # LED RGB state machine
├── buzzer.c                # Alert beep patterns
├── sd_logger.c             # Append-only event log
├── wifi_manager.c          # WiFi connect + reconnect + AP mode setup
└── http_client.c           # HTTP/HTTPS wrapper (mbedTLS)
```

**Estimated firmware size**: ~120KB. ESP32 có 4MB flash → thừa rộng.
**Estimated RAM usage**: 80KB / 520KB available.
**Build time với ESP-IDF**: ~2 phút first build, ~10s incremental.

### 2.7 Tính khả thi phương án E

| Yếu tố | Đánh giá |
|---|---|
| Kỹ thuật | ✅ 90% — Tất cả components đã được verify hoạt động trên V8 |
| Thời gian | ✅ 2.5-3 tuần realistic (1 tuần PCB + 1 tuần firmware + 0.5 tuần test) |
| Chi phí | ✅ ~265k/PCB + 5 PCBs từ JLCPCB ~$15 (350k) → 415k tổng cho 5 prototype |
| Skill | ✅ Bạn đã có toàn bộ stack: KiCad, ESP-IDF, ATECC608B (dù phương án E chưa cần ATECC) |
| Mở rộng F | ✅ PCB design same, chỉ populate thêm components → reuse 100% |

**KẾT LUẬN E: KHẢ THI CAO, KHUYẾN NGHỊ BUILD NGAY.**

---

## 3. Phương án F — PaymentBox thương mại

### 3.1 Khác biệt so với E

| Tính năng | E (Watchdog) | F (PaymentBox) |
|---|---|---|
| ESP32 trên data path | Không (chỉ monitor) | **Có** (relay payment data) |
| Phone cần internet | Có | **Không** (chỉ cần LAN) |
| ATECC608B HMAC | Không | **Có** (hardware sign) |
| SD card log | Tuỳ chọn | **Bắt buộc** (audit trail) |
| Relay reset phone | Tuỳ chọn | **Có** + DNP option |
| OTA firmware update | Manual | **Auto từ GAS** |
| Mobile app config qua BLE | Không | **Có** |
| LCD UI | 16x2 basic | **2.4" TFT** màu (hiển thị QR đơn) |
| Đóng gói thương mại | Case basic | **Case CNC + branding** |
| Chứng nhận | Không | **CE/FCC/MIC nếu xuất khẩu** |

### 3.2 Kiến trúc luồng dữ liệu phương án F

```
┌───────────────────────────────────────────────┐
│  PHONE Android (KHÔNG cần internet)           │
│  - NotificationListener                       │
│  - Push trực tiếp tới ESP32 LAN local         │
│    qua HTTP POST /ingest                      │
│  - Phone có thể là bất kỳ giá rẻ nào (300k)   │
└────────────────────┬──────────────────────────┘
                     │ HTTP POST (LAN, no HTTPS)
                     │ Local IP 192.168.4.x:8000
                     ▼
┌────────────────────────────────────────────────┐
│  ESP32 V8.1 PaymentBox                         │
│  - WiFi AP mode (phone connect vào AP của nó)  │
│  - HTTP server /ingest nhận từ phone           │
│  - ATECC608B sign HMAC                         │
│  - HTTPS POST tới GAS với HMAC                 │
│  - SD log mọi event                            │
│  - TFT 2.4" hiển thị QR + status               │
│  - Buzzer khi giao dịch mới                    │
│  - Relay reset phone optional                  │
│  - BLE config từ mobile app                    │
│  - OTA update qua HTTPS từ GAS                 │
└────────────────────┬───────────────────────────┘
                     │ HTTPS POST (4G/WiFi)
                     ▼
              [GAS V9.5 endpoint]
```

ESP32 ở phương án F = **bộ não trung tâm**, phone chỉ là cảm biến.

### 3.3 Điểm khó kỹ thuật của F

**A. WiFi AP + STA đồng thời:**
- ESP32 có thể chạy AP+STA mode đồng thời (ESP-WIFI-MESH)
- AP mode: phone connect vào ESP32
- STA mode: ESP32 connect WiFi nhà/4G dongle để gửi GAS
- Phức tạp config nhưng có nhiều example sẵn

**B. OTA firmware update an toàn:**
- GAS host file `firmware.bin` + signature
- ESP32 polling mỗi 24h
- Download → verify signature bằng ATECC608B public key → flash partition B → reboot
- Đã có esp_ota library + esp_https_ota
- Phải có rollback nếu firmware mới crash > 3 lần boot

**C. BLE config từ mobile app:**
- ESP32 BLE peripheral, mobile app (Flutter/React Native) là central
- Khách hàng setup WiFi SSID/password qua BLE thay vì hardcode
- Có nhiều ESP-IDF example, không khó

**D. Bảo mật ATECC608B:**
- ATECC608B store HMAC secret trong slot 0
- ESP32 nói chuyện I2C → request `Sign(message)` → trả về 32 byte
- Slot config khoá 1 lần → không đọc ra được dù tháo chip
- Đã verify hoạt động trong V8

**E. TFT 2.4" hiển thị QR + status:**
- Module TFT ILI9341 SPI giá ~100k
- Library TFT_eSPI hoặc esp_lcd (ESP-IDF native)
- QR generate bằng library `qrcode` C
- Hiển thị: số dư realtime, đơn mới, status

### 3.4 BOM phương án F (full populate)

| Ref | Linh kiện | Số lượng | Giá VN | Ghi chú |
|---|---|---|---|---|
| Tất cả E | (như trên) | - | 265k | Base |
| U4 | ATECC608B SOT-23-8 | 1 | 35k | HMAC hardware |
| U5 | SD card socket SMT | 1 | 25k | Hoặc microSD slot |
| U6 | TFT 2.4" ILI9341 module | 1 | 100k | Thay LCD 16x2 |
| K1 | Relay 5V SPDT | 1 | 30k | Reset phone optional |
| Q1 | NPN transistor SOT-23 (relay drive) | 1 | 2k | |
| D1 | Diode flyback 1N4148 | 1 | 2k | |
| OPT1 | Opto-isolator PC817 | 1 | 5k | Relay isolation |
| C extra | Tụ thêm cho relay/RF | 5 | 4k | |
| **Bổ sung F** | | | **+203k** | |
| **TỔNG F** | | | **~468k** | |

Cộng phone cũ 400k + case CNC 150k + packaging brand 30k → **tổng F: ~1.05tr/thiết bị xuất xưởng**.

**Giá bán đề xuất**: 1.5-2tr/unit (margin 40-50%) hoặc bundle với AI Stack Việt.

### 3.5 Đánh giá rủi ro phương án F

| Rủi ro | Xác suất | Tác động | Mitigation |
|---|---|---|---|
| OTA update bricks device | Thấp | **Catastrophic** | Dual partition + rollback + factory reset button |
| ATECC608B sign latency >500ms | Thấp | Thấp | Async I2C |
| TFT module ESP32 memory shortage | Trung bình | Trung bình | Dùng PSRAM (ESP32 module có version PSRAM) |
| BLE security yếu | Trung bình | Cao | Pairing với passkey 6 số, không Just Works |
| Chứng nhận CE/FCC nếu xuất khẩu | - | Cao | Bán nội địa trước, không cần |
| Khách dùng sai → mất tiền | - | **Catastrophic** | EULA + warranty + Telegram support 24/7 |
| 1 unit sập = 1 khách giận | - | Cao | RMA process, replacement 24h |

### 3.6 Tính khả thi phương án F

| Yếu tố | Đánh giá |
|---|---|
| Kỹ thuật | ✅ 80% — Có rủi ro với OTA + BLE config nhưng đều có path |
| Thời gian | ⚠️ 4-6 tuần (PCB 1 tuần + Firmware 2-3 tuần + Mobile app 1 tuần + Test/SOP 1 tuần) |
| Chi phí build | ⚠️ ~5-10tr cho 5 prototype + certification optional |
| Skill | ✅ Bạn có. Mobile app cần học Flutter/React Native nếu chưa |
| ROI | ✅ Bán 20 unit × 1.5tr = 30tr/tháng nếu bán được |

**KẾT LUẬN F: KHẢ THI nhưng KHÔNG nên build ngay.** Build E trước, vận hành 2-3 tháng để:
1. Học từ thực tế what works / what doesn't
2. Validate có ai mua không (presale)
3. Tích lũy data thực tế (uptime, sự cố)
4. Refactor design F dựa trên feedback E

---

## 4. Đánh giá hai phương án qua khung 5W1H

| W/H | Phương án E | Phương án F |
|---|---|---|
| **Why** | Tăng uptime self-hosted, không phụ thuộc 3rd party | Sản phẩm thương mại có thể bán |
| **What** | Hardware watchdog cho Android farm | Thiết bị tự chủ payment hoàn chỉnh |
| **Who** | EcoSynTech tự dùng | Bán cho SME/freelancer khác |
| **When** | Tuần 2-4 sau khi go-live Android | Sau khi E vận hành 2-3 tháng |
| **Where** | Đặt chung với phone trong văn phòng | Bán qua website / Shopee / showcase |
| **How** | V8.1 PCB + firmware ESP-IDF | V8.1 PCB full populate + mobile app + cloud |

---

## 5. Phân tích SWOT

### Phương án E

| | Tích cực | Tiêu cực |
|---|---|---|
| **Nội tại** | **S**: Tận dụng V8 hoàn toàn, không phải học stack mới, code Android KHÔNG đổi | **W**: Vẫn phụ thuộc phone (chỉ giám sát, không thay thế) |
| **Ngoại tại** | **O**: Có thể nâng cấp lên F, thành sản phẩm bán | **T**: Phone OEM update có thể phá local API |

### Phương án F

| | Tích cực | Tiêu cực |
|---|---|---|
| **Nội tại** | **S**: Full-stack sản phẩm, brand EcoSynTech, ATECC608B differentiator | **W**: Tốn nhiều thời gian dev, cần support khách hàng |
| **Ngoại tại** | **O**: Market gap (SePay đắt, Casso đắt), 30-50tr/tháng tiềm năng | **T**: Bank đổi format → cập nhật firmware OTA cho mọi unit; rủi ro pháp lý nếu khách kiện do mất tiền |

---

## 6. Quyết định cuối cùng

### Build 1 PCB V8.1-PaymentBox chung, populate khác nhau

```
PCB V8.1-PaymentBox-rev1.0  (60mm × 90mm, 2-layer)

Đầy đủ footprint cho cả E và F. Khi build:
- Phiên bản E: bỏ ATECC608B, SD socket, TFT, relay (DNP)
- Phiên bản F: full populate

→ Tiết kiệm tooling, đơn order PCB JLCPCB 1 lần dùng cả 2 năm
```

### Lộ trình 6 tuần

| Tuần | Nội dung |
|---|---|
| 1 | **Deploy Android farm** (code đã sẵn). Bắt đầu kiếm tiền AI Thực Chiến. |
| 2 | **Build PCB V8.1** schematic + layout + Gerber. Order JLCPCB. |
| 3 | **Firmware ESP-IDF phương án E** (poller + LCD + LED + buzzer + SD log) |
| 4 | **Nhận PCB, lắp ráp, test integration** với phone |
| 5 | **Vận hành E + Android song song** 1 tuần. Đối soát. |
| 6 | **Quyết định**: nâng cấp lên F hay giữ E? Dựa trên data thực tế. |

---

**KẾT LUẬN: ĐI VỚI PHƯƠNG ÁN E. PCB design backwards-compatible với F.**

Tiếp theo trong document SCHEMATIC.md sẽ là spec kỹ thuật chi tiết cho PCB.
