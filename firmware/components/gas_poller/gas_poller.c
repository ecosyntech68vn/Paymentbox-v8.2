/**
 * gas_poller.c — Poll GAS V9.5 backend để xác nhận phone đang push được.
 *
 * Logic:
 * - Gọi GAS endpoint mỗi GAS_POLL_INTERVAL_MS (60s) với device_id của phone
 * - GAS trả về timestamp của heartbeat gần nhất
 * - Tính age = now - last_heartbeat
 * - age < 5min → EV_GAS_HEARTBEAT_OK
 * - age >= 5min → EV_GAS_STALE
 * - HTTP fail → EV_GAS_UNREACHABLE
 *
 * Quan trọng: GAS poll giúp catch trường hợp phone "đứng" nhưng vẫn respond
 * health check local (NotificationListener treo, push xuống GAS không tới).
 */
#include "gas_poller.h"
#include "json_utils.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "gas_poller";
static char s_device_id[DEVICE_ID_LEN + 1] = "";

// Buffer cho response GAS
#define MAX_RESP_LEN 512
static char s_resp_buf[MAX_RESP_LEN];
static int s_resp_len = 0;

void gas_poller_set_device_id(const char *id) {
    if (id) strncpy(s_device_id, id, sizeof(s_device_id) - 1);
}

static esp_err_t http_event(esp_http_client_event_t *evt) {
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        int copy_len = evt->data_len;
        if (s_resp_len + copy_len >= MAX_RESP_LEN) {
            copy_len = MAX_RESP_LEN - s_resp_len - 1;
        }
        if (copy_len > 0) {
            memcpy(s_resp_buf + s_resp_len, evt->data, copy_len);
            s_resp_len += copy_len;
            s_resp_buf[s_resp_len] = '\0';
        }
    }
    return ESP_OK;
}

/**
 * Parse age (giây) từ JSON response GAS. Format đề xuất:
 * {"ok":true,"last_heartbeat_ms":1234567890,"age_s":45}
 */
static int parse_age_seconds(const char *json) {
    return json_get_int(json, "age_s", -1);
}

static void gas_poller_task(void *arg) {
    ESP_LOGI(TAG, "Started, device_id=%s", s_device_id);

    while (1) {
        s_resp_len = 0;
        s_resp_buf[0] = '\0';

        char url[256];
        snprintf(url, sizeof(url), "%s" GAS_HEARTBEAT_PATH, GAS_BASE_URL, s_device_id);

        esp_http_client_config_t config = {
            .url = url,
            .timeout_ms = GAS_TIMEOUT_MS,
            .event_handler = http_event,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = true,
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        int status = esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);

        if (err != ESP_OK || status != 200) {
            ESP_LOGW(TAG, "GAS unreachable: err=%d status=%d", err, status);
            event_bus_publish(EV_GAS_UNREACHABLE, NULL);
        } else {
            int age_s = parse_age_seconds(s_resp_buf);
            ESP_LOGI(TAG, "GAS heartbeat age=%ds", age_s);

            if (age_s < 0) {
                ESP_LOGW(TAG, "Failed parse, response: %s", s_resp_buf);
                event_bus_publish(EV_GAS_UNREACHABLE, NULL);
            } else if (age_s < 300) {
                pbox_event_msg_t msg = { .data.ival = age_s };
                event_bus_publish(EV_GAS_HEARTBEAT_OK, &msg);
            } else {
                pbox_event_msg_t msg = { .data.ival = age_s };
                event_bus_publish(EV_GAS_STALE, &msg);
            }
        }

        // Jitter ±10s để tránh thundering herd
        int jitter = (int)(esp_random() % 20001) - 10000;
        int delay_ms = GAS_POLL_INTERVAL_MS + jitter;
        if (delay_ms < 10000) delay_ms = 10000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void gas_poller_start(void) {
    xTaskCreate(gas_poller_task, "gas_poller", TASK_STACK_LARGE,
                NULL, PRIO_MED, NULL);
}
