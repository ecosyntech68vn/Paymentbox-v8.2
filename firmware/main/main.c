/**
 * main.c — EcoSynTech PaymentBox V8.2 firmware entry point.
 *
 * Task spawn theo thứ tự: WiFi → event_bus subscribers → pollers.
 *
 * Cách hoạt động:
 * - Boot → init NVS, event_bus, WiFi
 * - Đợi WiFi connect (hoặc AP mode nếu chưa config)
 * - Spawn các task con dựa trên feature toggle (paymentbox_config.h)
 * - Main idle forever
 */
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "paymentbox_config.h"
#include "event_bus.h"
#include "wifi_manager.h"
#include "phone_poller.h"
#include "gas_poller.h"
#include "led_status.h"
#include "buzzer.h"
#include "lcd_ui.h"
#include "battery_monitor.h"
#if ENABLE_SD_LOG
#include "sd_logger.h"
#endif
#if ENABLE_OTA
#include "ota_updater.h"
#endif

static const char *TAG = "main";

static char s_device_id[DEVICE_ID_LEN + 1];

static void compute_device_id(void) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_device_id, sizeof(s_device_id), "%s%02X%02X%02X%02X",
             DEVICE_ID_PREFIX, mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device ID: %s", s_device_id);
}

#if ENABLE_OTA
/**
 * Boot sanity task: đợi OTA_BOOT_SANITY_MS (60s), nếu hệ thống chưa crash
 * → mark partition đang chạy là VALID (hủy auto-rollback bootloader).
 */
static void boot_sanity_task(void *arg) {
    vTaskDelay(pdMS_TO_TICKS(OTA_BOOT_SANITY_MS));
    ESP_LOGI(TAG, "Boot sanity OK after %dms — marking firmware valid",
             OTA_BOOT_SANITY_MS);
    ota_updater_mark_valid();
    vTaskDelete(NULL);
}
#endif

void app_main(void) {
    ESP_LOGI(TAG, "==========================================");
    ESP_LOGI(TAG, "EcoSynTech PaymentBox %s", HW_VERSION);
    ESP_LOGI(TAG, "Firmware %s (build %s)", FW_VERSION, FW_BUILD_DATE);
    ESP_LOGI(TAG, "==========================================");

    // ===== Core systems =====
    nvs_flash_init();
    event_bus_init();
    compute_device_id();

    event_bus_publish(EV_SYSTEM_BOOT, NULL);

    // ===== Spawn tasks =====
    // Order quan trọng: led_status + buzzer + lcd subscribe TRƯỚC khi
    // pollers publish events đầu tiên.

    led_status_start();
    buzzer_start();

#if ENABLE_LCD_UI
    lcd_ui_start();
#endif

#if ENABLE_BATTERY_MONITOR
    battery_monitor_start();
#endif

#if ENABLE_SD_LOG
    sd_logger_start();
#endif

    // WiFi sau cùng (sẽ publish CONNECTED khi sẵn sàng)
    wifi_manager_start();

    // Đợi WiFi
    int wait_ms = 0;
    while (!wifi_manager_is_connected() && wait_ms < WIFI_CONNECT_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(500));
        wait_ms += 500;
    }

    if (wifi_manager_is_connected()) {
        ESP_LOGI(TAG, "WiFi ready, spawning pollers");

        gas_poller_set_device_id(s_device_id);

#if ENABLE_PHONE_POLLER
        phone_poller_start();
#endif
#if ENABLE_GAS_POLLER
        gas_poller_start();
#endif
#if ENABLE_OTA
        ota_updater_start();
        xTaskCreate(boot_sanity_task, "boot_san", 2048, NULL, PRIO_LOW, NULL);
#endif
    } else {
        ESP_LOGW(TAG, "WiFi không sẵn sàng — running offline mode (LCD/LED/buzzer only)");
    }

    // Main task chỉ idle. Mọi việc trong các task con.
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        ESP_LOGI(TAG, "Heap free: %zu bytes", esp_get_free_heap_size());
    }
}
