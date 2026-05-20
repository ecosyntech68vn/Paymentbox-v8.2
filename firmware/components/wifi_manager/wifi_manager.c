/**
 * wifi_manager.c — STA mode + AP fallback.
 *
 * Logic:
 * - Boot → đọc SSID/PSK từ NVS (key "wifi.ssid", "wifi.psk")
 * - Nếu KHÔNG có config → khởi động AP "PaymentBox-Setup" mở
 * - User connect vào AP qua http://192.168.4.1 → set WiFi + phone IP
 * - Khi có config: STA mode connect, retry mãi nếu fail
 * - Publish EV_WIFI_CONNECTED / DISCONNECTED
 *
 * NVS keys:
 *   wifi.ssid       — STA SSID
 *   wifi.psk        — STA password
 *   phone.ip        — phone IP (cho phone_poller)
 *   device.id       — device ID (cho gas_poller)
 */
#include "wifi_manager.h"
#include "config_server.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "wifi_mgr";

static bool s_sta_connected = false;
static int s_retry_count = 0;

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    if (base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                s_sta_connected = false;
                event_bus_publish(EV_WIFI_DISCONNECTED, NULL);
                s_retry_count++;
                ESP_LOGW(TAG, "STA disconnected, retry #%d", s_retry_count);
                vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_DELAY_MS));
                esp_wifi_connect();
                break;
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started: " WIFI_AP_FALLBACK_NAME);
                event_bus_publish(EV_WIFI_AP_STARTED, NULL);
                break;
        }
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        s_sta_connected = true;
        s_retry_count = 0;
        event_bus_publish(EV_WIFI_CONNECTED, NULL);
    }
}

static esp_err_t load_creds(char *ssid, size_t ssid_len,
                             char *psk, size_t psk_len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &h);
    if (err != ESP_OK) return err;

    size_t len_ssid = ssid_len;
    err = nvs_get_str(h, "wifi.ssid", ssid, &len_ssid);
    if (err == ESP_OK) {
        size_t len_psk = psk_len;
        nvs_get_str(h, "wifi.psk", psk, &len_psk);
    }
    nvs_close(h);
    return err;
}

static void start_sta(const char *ssid, const char *psk) {
    wifi_config_t cfg = { 0 };
    strncpy((char*)cfg.sta.ssid, ssid, sizeof(cfg.sta.ssid));
    strncpy((char*)cfg.sta.password, psk, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_start();
    ESP_LOGI(TAG, "STA mode → connect to %s", ssid);
}

static void start_ap_fallback(void) {
    wifi_config_t cfg = { 0 };
    strncpy((char*)cfg.ap.ssid, WIFI_AP_FALLBACK_NAME, sizeof(cfg.ap.ssid));
    cfg.ap.ssid_len = strlen(WIFI_AP_FALLBACK_NAME);
    cfg.ap.channel = 1;
    cfg.ap.authmode = WIFI_AUTH_OPEN;  // open AP — config UI will set password later
    cfg.ap.max_connection = 4;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &cfg);
    esp_wifi_start();
}

void wifi_manager_start(void) {
    // NVS init (gọi 1 lần ở main, an toàn để gọi lại)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    char ssid[33] = "", psk[65] = "";
    if (load_creds(ssid, sizeof(ssid), psk, sizeof(psk)) == ESP_OK && strlen(ssid) > 0) {
        start_sta(ssid, psk);
    } else {
        ESP_LOGW(TAG, "No WiFi creds — starting AP fallback for config");
        start_ap_fallback();
        // Auto-start HTTP config server cho captive portal
        config_server_start();
    }
}

bool wifi_manager_is_connected(void) {
    return s_sta_connected;
}
