#include "telegram_alert.h"
#include "json_utils.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "telegram";

#define COOLDOWN_S  3600
#define ALERT_BUF   512

static const char *alert_text(pbox_event_t t) {
    switch (t) {
        case EV_GAS_UNREACHABLE:  return "GAS backend unreachable";
        case EV_BAT_CRITICAL:     return "Battery CRITICAL";
        case EV_PHONE_DEAD:       return "Phone DEAD (3 consecutive failures)";
        case EV_POWER_USB_LOST:   return "USB power lost";
        default:                  return "Unknown alert";
    }
}

static bool alert_event_filter(pbox_event_t t) {
    return t == EV_GAS_UNREACHABLE || t == EV_BAT_CRITICAL
        || t == EV_PHONE_DEAD || t == EV_POWER_USB_LOST;
}

static void send_alert(const char *msg) {
    char body[ALERT_BUF];
    snprintf(body, sizeof(body),
        "{\"chat_id\":\"%s\",\"text\":\"[PaymentBox %s] %s\"}",
        TELEGRAM_CHAT_ID, FW_VERSION, msg);

    char url[128];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot%s/sendMessage",
             TELEGRAM_BOT_TOKEN);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
        .keep_alive_enable = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);

    if (err == ESP_OK && status == 200) {
        ESP_LOGI(TAG, "Alert sent: %s", msg);
    } else {
        ESP_LOGW(TAG, "Alert failed: err=%d status=%d", err, status);
    }
    esp_http_client_cleanup(client);
}

static void alert_task(void *arg) {
    QueueHandle_t q = event_bus_subscribe_filtered("telegram", alert_event_filter);
    int64_t last_send = 0;

    while (1) {
        pbox_event_msg_t ev;
        while (xQueueReceive(q, &ev, portMAX_DELAY) == pdTRUE) {
            int64_t now = ev.timestamp_ms / 1000;
            if (now - last_send < COOLDOWN_S) {
                ESP_LOGD(TAG, "Cooldown active, skipping %s", event_name(ev.type));
                continue;
            }
            send_alert(alert_text(ev.type));
            last_send = now;
        }
    }
}

void telegram_alert_start(void) {
#if ENABLE_TELEGRAM_ALERT
    xTaskCreate(alert_task, "telegram", TASK_STACK_MEDIUM, NULL, PRIO_LOW, NULL);
    ESP_LOGI(TAG, "Telegram alert started (cooldown=%ds)", COOLDOWN_S);
#else
    ESP_LOGI(TAG, "Telegram alert disabled");
#endif
}
