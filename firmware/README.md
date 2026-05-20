# PaymentBox V8.2 Firmware — ESP-IDF

## Cấu trúc

```
firmware/
├── CMakeLists.txt          # top-level
├── sdkconfig.defaults      # config defaults (4MB flash, WiFi, OTA, anti-rollback)
├── partitions.csv          # factory + 2× OTA + spiffs
├── TEST_REPORT.md          # Cppcheck + 68 unit tests pass
│
├── main/
│   ├── main.c              # entry + spawn task + OTA boot sanity (60s)
│   ├── include/paymentbox_config.h  # TẤT CẢ pin + ngưỡng + URL + OTA
│   └── CMakeLists.txt
│
├── tests/                  # Host-side unit tests (gcc, no ESP-IDF cần)
│   ├── test_config_server.c    # 33 tests
│   └── test_ota_updater.c      # 35 tests
│
└── components/
    ├── event_bus/          # FreeRTOS queue-based pub/sub
    ├── wifi_manager/       # STA + AP fallback → auto-start config_server
    ├── config_server/      # ★ HTTP form 192.168.4.1 captive portal
    ├── ota_updater/        # ★ esp_https_ota từ GAS, downgrade protect, rollback
    ├── phone_poller/       # HTTP GET phone:8765/health 30s
    ├── gas_poller/         # HTTPS GET GAS heartbeat 60s
    ├── led_status/         # 5-state machine 3-LED PWM
    ├── buzzer/             # Beep patterns
    ├── lcd_ui/             # I2C LCD 16x2 PCF8574 driver
    ├── battery_monitor/    # ADC GPIO36, USB lost detection
    └── sd_logger/          # NDJSON log + rotation (F only)
```

★ = mới thêm trong V8.2 milestone 2.

## Build & Flash

Yêu cầu: ESP-IDF v5.0+

```bash
# Setup ESP-IDF (chỉ lần đầu)
. $IDF_PATH/export.sh
idf.py set-target esp32

# Build
cd firmware
idf.py build

# Flash + monitor
idf.py -p /dev/ttyUSB0 flash monitor

# Erase NVS nếu cần reset config
idf.py erase-flash
```

## Cấu hình lần đầu

1. Flash firmware → boot lần đầu, **WiFi sẽ ở mode AP** "PaymentBox-Setup" (open).
2. Connect điện thoại/laptop vào AP đó → vào `http://192.168.4.1`
3. Nhập:
   - SSID + password WiFi nhà
   - IP của phone Android chạy BankNotify-App (xem trong app Settings)
   - Device ID (auto-generate từ MAC nếu bỏ trống)
4. Save → reboot → vào STA mode.

NVS keys lưu:
```
wifi.ssid       Tên WiFi nhà
wifi.psk        Mật khẩu WiFi
phone.ip        IP phone (e.g. 192.168.1.50)
device.id       Tự sinh từ MAC, override được
```

## State machine (LED + buzzer)

| State | LED | Buzzer | Điều kiện |
|---|---|---|---|
| **IDLE** | Xanh dương blink | im | Boot/chưa connect WiFi |
| **OK** | Xanh lá steady | im | Phone OK + GAS OK + có nguồn |
| **WARN** | Vàng (R+G) thở | im | Mất USB nhưng phone+GAS còn OK |
| **ALERT** | Đỏ pulse | 3 beep khi vào | Phone dead hoặc GAS unreachable |
| **CRITICAL** | Đỏ solid | continuous 1Hz | Pin <3V hoặc 2+ subsystem fail |

## OTA Update

Hai partition app (ota_0, ota_1). Update path:
1. ESP32 mỗi 24h gọi GAS `?action=ota_check&version=X.Y.Z`
2. Nếu có version mới → download `firmware.bin` lên partition rỗng
3. Verify SHA256 từ GAS metadata
4. `esp_ota_set_boot_partition()` → reboot
5. Bootloader app rollback: nếu firmware mới crash > 3 lần boot → tự revert partition cũ

**Chưa implement trong skeleton này**. Code mẫu: ESP-IDF `examples/system/ota/native_ota_example`.

## Memory budget estimate

| Component | Stack | Heap usage |
|---|---|---|
| main idle | 4KB | - |
| event_bus | - | 2KB (queues) |
| wifi_manager | 4KB | 50KB (WiFi stack) |
| phone_poller | 4KB | 4KB (HTTP client) |
| gas_poller | 8KB | 8KB (HTTPS + mbedtls) |
| led_status | 4KB | - |
| buzzer | 2KB | - |
| lcd_ui | 4KB | - |
| battery_monitor | 4KB | 1KB (ADC cal) |
| sd_logger | 8KB | 16KB (FATFS) |
| **TOTAL** | ~42KB | ~80KB |

ESP32-WROOM-32 có 520KB SRAM → còn dư 400KB cho buffer + WiFi runtime.

## Testing manual

```bash
# Mở serial monitor
idf.py monitor

# Test trong code: simulate event
# (thêm vào main.c sau khi WiFi connect, để debug)
event_bus_publish(EV_PHONE_DEAD, NULL);
event_bus_publish(EV_BAT_CRITICAL, NULL);
```

## Production checklist

- [ ] Set GAS_BASE_URL trong paymentbox_config.h trước khi build production
- [ ] Set DEVICE_ID_PREFIX nếu khác EcoSynTech
- [ ] Disable ENABLE_SD_LOG cho phương án E (không có SD socket)
- [ ] Set ENABLE_RELAY_CTRL=1 cho phương án F
- [ ] Verify LCD I2C address (0x27 vs 0x3F) cho lô module mua
- [ ] Test ATECC608B nếu phương án F (chưa có trong firmware skeleton này — TODO)
- [ ] Sign firmware binary với GAS public key (TODO)

## Files tạo từ skeleton

Total: **~1100 lines C**, build size ước tính **~250KB** binary cho mỗi OTA partition.
