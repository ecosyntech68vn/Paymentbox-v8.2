/**
 * paymentbox_config.h — EcoSynTech PaymentBox V8.2 firmware config
 *
 * Tập trung MỌI hằng số tại đây. Không hardcode trong source.
 */
#pragma once

#include "driver/gpio.h"
#include "hal/adc_types.h"

// ============================================================
// VERSION
// ============================================================
#define FW_VERSION             "1.0.0"
#define FW_BUILD_DATE          __DATE__ " " __TIME__
#define HW_VERSION             "V8.2"

// ============================================================
// GPIO MAPPING — KHÔNG TỰ ĐỔI (phải đồng bộ với SCHEMATIC_V8_2.md)
// ============================================================

// LED status
#define PIN_LED_R              GPIO_NUM_12  // Đỏ — Sự cố
#define PIN_LED_G              GPIO_NUM_4   // Xanh lá — OK
#define PIN_LED_B              GPIO_NUM_2   // Xanh dương — Hoạt động

// Buzzer
#define PIN_BUZZER             GPIO_NUM_13

// Relay (phương án F only)
#define PIN_RELAY              GPIO_NUM_15

// I2C bus (LCD + ATECC608B)
#define PIN_I2C_SDA            GPIO_NUM_21
#define PIN_I2C_SCL            GPIO_NUM_22
#define I2C_PORT_NUM           I2C_NUM_0
#define I2C_FREQ_HZ            100000   // 100kHz
#define I2C_ADDR_LCD           0x27     // PCF8574 LCD backpack
#define I2C_ADDR_ATECC         0x60     // ATECC608B (F only)

// SPI bus (microSD)
#define PIN_SPI_SCK            GPIO_NUM_14
#define PIN_SPI_MOSI           GPIO_NUM_23
#define PIN_SPI_MISO           GPIO_NUM_19
#define PIN_SPI_CS_SD          GPIO_NUM_5

// ADC battery sense
#define PIN_ADC_BAT            ADC_CHANNEL_0   // GPIO36 / SENSOR_VP
#define BAT_DIVIDER_RATIO      3.13f           // V_BAT = ADC * 3.13

// UART debug
#define UART_NUM               UART_NUM_0
#define UART_BAUDRATE          115200

// ============================================================
// NETWORK
// ============================================================

// WiFi (qua wifi_manager — đọc từ NVS khi flash, hoặc qua AP config)
#define WIFI_DEFAULT_SSID      "EcoSynTech-Setup"
#define WIFI_DEFAULT_PSK       ""              // empty = open AP
#define WIFI_AP_FALLBACK_NAME  "PaymentBox-Setup"
#define WIFI_AP_PASSWORD       "ecosyntech"    // WPA2 PSK cho AP mode; đọc từ NVS key "wifi.ap_psk" nếu có
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_RECONNECT_DELAY_MS 5000

// Phone (BankNotify-App on Android)
// Tự discover qua mDNS hoặc set qua AP config UI
#define PHONE_DEFAULT_IP       "192.168.1.50"  // override via NVS
#define PHONE_HEALTH_URL       "http://%s:8765/api/v1/health"
#define PHONE_RECENT_URL       "http://%s:8765/api/v1/transactions/recent"
#define PHONE_POLL_INTERVAL_MS 60000           // 60s (1 phút quét 1 lần)
#define PHONE_TIMEOUT_MS       5000

// GAS backend (V9.5)
// Cần config khi flash production
#define GAS_BASE_URL           "https://script.google.com/macros/s/SHEET_ID/exec"
#define GAS_HEARTBEAT_PATH     "?action=getHeartbeat&device_id=%s"
#define GAS_POLL_INTERVAL_MS   60000           // 60s
#define GAS_TIMEOUT_MS         10000

// OTA update
#define OTA_CHECK_INTERVAL_MS  (24 * 3600 * 1000)   // 24h
#define OTA_CHECK_TIMEOUT_MS   15000
#define OTA_DOWNLOAD_TIMEOUT_MS 120000              // 2 phút
#define OTA_BOOT_SANITY_MS     60000                // Đợi 60s sau boot → mark valid

// Device identification
#define DEVICE_ID_PREFIX       "ESG-PB-"
#define DEVICE_ID_LEN          16              // ESG-PB-XXXXXXXX (8 hex từ MAC)

// ============================================================
// STATE MACHINE THRESHOLDS
// ============================================================

// Tính từ lần phone respond cuối:
#define HEALTH_OK_WINDOW_MS    60000           // <60s từ lần ping cuối = OK
#define HEALTH_WARN_WINDOW_MS  180000          // 1-3 phút = WARN (vàng)
#define HEALTH_ALERT_WINDOW_MS 300000          // 3-5 phút = ALERT (đỏ)
// > 5 phút = CRITICAL

// Battery thresholds
#define BAT_FULL_V             4.10f
#define BAT_LOW_V              3.40f
#define BAT_CRITICAL_V         3.00f

// ============================================================
// FREERTOS TASK CONFIG
// ============================================================
#define TASK_STACK_SMALL       2048
#define TASK_STACK_MEDIUM      4096
#define TASK_STACK_LARGE       8192

#define PRIO_LOW               2
#define PRIO_MED               5
#define PRIO_HIGH              10
#define PRIO_CRITICAL          15

// ============================================================
// SD CARD LOGGING
// ============================================================
#define SD_LOG_PATH            "/sdcard/paymentbox.log"
#define SD_LOG_MAX_BYTES       1048576         // 1MB → rotate
#define SD_LOG_ROTATE_KEEP     5               // keep 5 old logs

// ============================================================
// PROTOTYPING TOGGLES
// ============================================================
#define ENABLE_PHONE_POLLER    1
#define ENABLE_GAS_POLLER      1
#define ENABLE_LCD_UI          1
#define ENABLE_SD_LOG          0   // disable nếu phương án E không lắp SD
#define ENABLE_RELAY_CTRL      0   // F only — luôn 0 cho phương án E
#define ENABLE_BATTERY_MONITOR 1
#define ENABLE_CONFIG_SERVER   1   // HTTP server cho AP mode setup
#define ENABLE_OTA             1   // OTA firmware update qua GAS
