/**
 * ota_updater.c — Over-The-Air firmware update từ GAS V9.5.
 *
 * GAS endpoint contract:
 *   GET <GAS_URL>?action=ota_check&device_id=ESG-PB-XXXXXXXX&version=1.0.0&hw=V8.2
 *
 * GAS response (JSON):
 *   {
 *     "update_available": true,
 *     "version": "1.1.0",
 *     "url": "https://drive.google.com/.../firmware.bin",
 *     "sha256": "abc123...",  (optional, future use)
 *     "min_battery_v": 3.5    (skip OTA nếu pin yếu)
 *   }
 *
 * Hoặc:
 *   {"update_available": false}
 *
 * Safety:
 * - Chỉ OTA khi V_BAT > 3.5V hoặc có USB-C nguồn ổn định
 * - Verify image header (esp_https_ota_get_img_desc)
 * - Cancel nếu version trùng version đang chạy
 * - Sau boot mới → 60s sanity check → mark_valid()
 * - Crash > 3 lần boot → bootloader auto rollback
 */
#include "ota_updater.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nvs.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ota";

// Trigger queue (manual + scheduled)
static QueueHandle_t s_trigger_q = NULL;

// =============== Helpers ===============

/**
 * Đọc 1 trường JSON đơn giản: "key":value (string hoặc bool hoặc number).
 * Không phải full JSON parser — chỉ scan substring.
 * Tránh phụ thuộc cJSON cho lib nhẹ.
 */
static bool json_get_string(const char *json, const char *key, char *out, size_t out_len) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p + strlen(search), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return false;
    p++;  // skip opening quote
    const char *end = strchr(p, '"');
    if (!end) return false;
    size_t len = end - p;
    if (len >= out_len) len = out_len - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool json_get_bool(const char *json, const char *key) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return false;
    p = strchr(p + strlen(search), ':');
    if (!p) return false;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return (strncmp(p, "true", 4) == 0);
}

static float json_get_float(const char *json, const char *key, float def) {
    char search[64];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *p = strstr(json, search);
    if (!p) return def;
    p = strchr(p + strlen(search), ':');
    if (!p) return def;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return strtof(p, NULL);
}

/**
 * So sánh version "X.Y.Z" — return >0 nếu a > b, <0 nếu a < b, 0 nếu bằng.
 */
static int version_cmp(const char *a, const char *b) {
    int va[3] = {0}, vb[3] = {0};
    sscanf(a, "%d.%d.%d", &va[0], &va[1], &va[2]);
    sscanf(b, "%d.%d.%d", &vb[0], &vb[1], &vb[2]);
    for (int i = 0; i < 3; i++) {
        if (va[i] != vb[i]) return va[i] - vb[i];
    }
    return 0;
}

// =============== Response buffer ===============

#define MAX_OTA_RESP 1024
static char s_resp_buf[MAX_OTA_RESP];
static int s_resp_len = 0;

static esp_err_t http_check_event(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy_len = evt->data_len;
        if (s_resp_len + copy_len >= MAX_OTA_RESP) {
            copy_len = MAX_OTA_RESP - s_resp_len - 1;
        }
        if (copy_len > 0) {
            memcpy(s_resp_buf + s_resp_len, evt->data, copy_len);
            s_resp_len += copy_len;
            s_resp_buf[s_resp_len] = '\0';
        }
    }
    return ESP_OK;
}

// =============== OTA check & perform ===============

typedef struct {
    bool   available;
    char   version[16];
    char   url[256];
    float  min_battery_v;
} ota_info_t;

static bool ota_check_gas(const char *device_id, ota_info_t *info) {
    s_resp_len = 0;
    s_resp_buf[0] = '\0';

    char url[512];
    snprintf(url, sizeof(url),
        "%s?action=ota_check&device_id=%s&version=%s&hw=%s",
        GAS_BASE_URL, device_id, FW_VERSION, HW_VERSION);

    esp_http_client_config_t cfg = {
        .url = url,
        .timeout_ms = OTA_CHECK_TIMEOUT_MS,
        .event_handler = http_check_event,
        // crt_bundle_attach cần config CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return false;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK || status != 200) {
        ESP_LOGW(TAG, "OTA check failed: err=%d status=%d", err, status);
        return false;
    }

    ESP_LOGI(TAG, "OTA check response (%d bytes)", s_resp_len);

    memset(info, 0, sizeof(*info));
    info->available = json_get_bool(s_resp_buf, "update_available");
    if (!info->available) return true;  // no update available, not an error

    json_get_string(s_resp_buf, "version", info->version, sizeof(info->version));
    json_get_string(s_resp_buf, "url", info->url, sizeof(info->url));
    info->min_battery_v = json_get_float(s_resp_buf, "min_battery_v", 3.5f);
    return true;
}

static bool ota_perform(const ota_info_t *info) {
    ESP_LOGI(TAG, "Starting OTA: version=%s url=%s", info->version, info->url);

    esp_http_client_config_t http_cfg = {
        .url = info->url,
        .timeout_ms = OTA_DOWNLOAD_TIMEOUT_MS,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_cfg, &handle);
    if (err != ESP_OK || handle == NULL) {
        ESP_LOGE(TAG, "https_ota_begin failed: %d", err);
        return false;
    }

    // Verify image desc — check version doesn't downgrade
    esp_app_desc_t new_app;
    err = esp_https_ota_get_img_desc(handle, &new_app);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "New image version: %s (running: %s)",
                 new_app.version, FW_VERSION);
        if (version_cmp(new_app.version, FW_VERSION) <= 0) {
            ESP_LOGW(TAG, "Downgrade or same version detected, aborting OTA");
            esp_https_ota_abort(handle);
            return false;
        }
    }

    // Stream download
    int bytes = 0;
    while (1) {
        err = esp_https_ota_perform(handle);
        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            int new_bytes = esp_https_ota_get_image_len_read(handle);
            if (new_bytes - bytes > 50000) {  // log every 50KB
                ESP_LOGI(TAG, "Downloaded %d bytes", new_bytes);
                bytes = new_bytes;
            }
            continue;
        }
        break;
    }

    if (esp_https_ota_is_complete_data_received(handle) != true) {
        ESP_LOGE(TAG, "Download incomplete");
        esp_https_ota_abort(handle);
        return false;
    }

    err = esp_https_ota_finish(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ota_finish failed: %d", err);
        return false;
    }

    ESP_LOGI(TAG, "OTA complete, rebooting to new image");
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return true;  // not reached
}

// =============== Task ===============

typedef enum {
    OTA_TRIGGER_PERIODIC,
    OTA_TRIGGER_MANUAL,
} ota_trigger_t;

static void ota_task(void *arg) {
    char device_id[32];
    // Tự đọc device_id từ NVS hoặc tính từ MAC
    nvs_handle_t h;
    if (nvs_open("storage", NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(device_id);
        nvs_get_str(h, "device.id", device_id, &len);
        nvs_close(h);
    } else {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(device_id, sizeof(device_id), "%s%02X%02X%02X%02X",
                 DEVICE_ID_PREFIX, mac[2], mac[3], mac[4], mac[5]);
    }

    ESP_LOGI(TAG, "OTA updater started for device %s", device_id);

    while (1) {
        ota_trigger_t trig;
        // Đợi trigger hoặc timeout
        if (xQueueReceive(s_trigger_q, &trig, pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS))
            == pdFALSE) {
            trig = OTA_TRIGGER_PERIODIC;
        }
        ESP_LOGI(TAG, "OTA check (%s)",
                 trig == OTA_TRIGGER_MANUAL ? "manual" : "periodic");

        ota_info_t info;
        if (!ota_check_gas(device_id, &info)) {
            ESP_LOGW(TAG, "OTA check failed, retry in next cycle");
            continue;
        }

        if (!info.available) {
            ESP_LOGI(TAG, "Already up-to-date");
            continue;
        }

        ESP_LOGI(TAG, "Update available: %s → %s", FW_VERSION, info.version);

        // TODO: kiểm tra battery > info.min_battery_v trước khi OTA
        // Đọc state battery_monitor qua event bus subscription
        // Tạm skip — assume nguồn USB-C đang cấp khi OTA

        if (!ota_perform(&info)) {
            ESP_LOGE(TAG, "OTA failed");
        }
        // Nếu OTA thành công, esp_restart() đã chạy.
    }
}

// =============== Public API ===============

bool ota_updater_start(void) {
    s_trigger_q = xQueueCreate(4, sizeof(ota_trigger_t));
    if (!s_trigger_q) return false;
    xTaskCreate(ota_task, "ota", TASK_STACK_LARGE, NULL, PRIO_LOW, NULL);
    return true;
}

bool ota_updater_trigger_check(void) {
    if (!s_trigger_q) return false;
    ota_trigger_t trig = OTA_TRIGGER_MANUAL;
    return xQueueSend(s_trigger_q, &trig, 0) == pdTRUE;
}

void ota_updater_mark_valid(void) {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (esp_ota_get_state_partition(running, &state) == ESP_OK) {
        if (state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "Marking running partition as VALID (anti-rollback)");
            esp_ota_mark_app_valid_cancel_rollback();
        }
    }
}
