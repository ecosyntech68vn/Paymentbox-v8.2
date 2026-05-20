/**
 * phone_poller.c — Định kỳ poll phone (BankNotify-App) qua HTTP.
 *
 * Logic:
 * - Mỗi PHONE_POLL_INTERVAL_MS (30s) gọi GET http://phone:8765/api/v1/health
 * - Nếu OK → publish EV_PHONE_HEALTHY
 * - Nếu timeout 1 lần → EV_PHONE_TIMEOUT
 * - Nếu timeout 3 lần liên tiếp → EV_PHONE_DEAD
 * - Khi phone respond lại sau dead → EV_PHONE_RECOVERED
 *
 * KHÔNG đứng trên data path — phone vẫn push GAS trực tiếp.
 * Phone poller chỉ là watchdog.
 */
#include "phone_poller.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "phone_poller";
static char s_phone_ip[16] = PHONE_DEFAULT_IP;
static int s_consecutive_failures = 0;
static bool s_was_dead = false;

void phone_poller_set_ip(const char *ip) {
    if (ip && strlen(ip) < sizeof(s_phone_ip)) {
        strncpy(s_phone_ip, ip, sizeof(s_phone_ip) - 1);
        ESP_LOGI(TAG, "Phone IP set: %s", s_phone_ip);
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    // Bỏ qua data, chỉ check status
    return ESP_OK;
}

static bool ping_phone(void) {
    char url[128];
    snprintf(url, sizeof(url), PHONE_HEALTH_URL, s_phone_ip);

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = PHONE_TIMEOUT_MS,
        .event_handler = http_event_handler,
        .max_http_response_header_size = 512,
        .keep_alive_enable = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return false;

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    bool ok = (err == ESP_OK) && (status == 200);
    if (!ok) {
        ESP_LOGW(TAG, "Ping failed: err=%d status=%d", err, status);
    }
    return ok;
}

static void phone_poller_task(void *arg) {
    ESP_LOGI(TAG, "Started, polling %s every %dms", s_phone_ip, PHONE_POLL_INTERVAL_MS);

    while (1) {
        bool ok = ping_phone();

        if (ok) {
            if (s_was_dead) {
                ESP_LOGI(TAG, "Phone RECOVERED");
                event_bus_publish(EV_PHONE_RECOVERED, NULL);
                s_was_dead = false;
            }
            s_consecutive_failures = 0;
            event_bus_publish(EV_PHONE_HEALTHY, NULL);
        } else {
            s_consecutive_failures++;
            ESP_LOGW(TAG, "Failure #%d", s_consecutive_failures);

            if (s_consecutive_failures == 1) {
                event_bus_publish(EV_PHONE_TIMEOUT, NULL);
            } else if (s_consecutive_failures >= 3 && !s_was_dead) {
                ESP_LOGE(TAG, "Phone DEAD (3 consecutive failures)");
                event_bus_publish(EV_PHONE_DEAD, NULL);
                s_was_dead = true;
            }
        }

        // Jitter ±5s để tránh thundering herd
        int jitter = (int)(esp_random() % 10001) - 5000;
        int delay_ms = PHONE_POLL_INTERVAL_MS + jitter;
        if (delay_ms < 5000) delay_ms = 5000;
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

void phone_poller_start(void) {
    xTaskCreate(phone_poller_task, "phone_poller", TASK_STACK_MEDIUM,
                NULL, PRIO_MED, NULL);
}
