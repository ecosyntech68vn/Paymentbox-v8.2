#include "event_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "event_bus";

#define MAX_SUBSCRIBERS 16

static QueueHandle_t s_subs[MAX_SUBSCRIBERS] = { NULL };
static const char *s_sub_names[MAX_SUBSCRIBERS] = { NULL };
static int s_sub_count = 0;
static SemaphoreHandle_t s_lock = NULL;

void event_bus_init(void) {
    s_lock = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "Event bus initialized");
}

QueueHandle_t event_bus_subscribe(const char *name) {
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_sub_count >= MAX_SUBSCRIBERS) {
        ESP_LOGE(TAG, "Max subscribers exceeded");
        xSemaphoreGive(s_lock);
        return NULL;
    }
    QueueHandle_t q = xQueueCreate(EVENT_BUS_QUEUE_LEN, sizeof(pbox_event_msg_t));
    s_subs[s_sub_count] = q;
    s_sub_names[s_sub_count] = name;
    s_sub_count++;
    ESP_LOGI(TAG, "Subscriber '%s' added (total: %d)", name, s_sub_count);
    xSemaphoreGive(s_lock);
    return q;
}

bool event_bus_publish(pbox_event_t type, const pbox_event_msg_t *data) {
    pbox_event_msg_t msg = { 0 };
    if (data) msg = *data;
    msg.type = type;
    msg.timestamp_ms = esp_timer_get_time() / 1000;

    int dropped = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int i = 0; i < s_sub_count; i++) {
        if (xQueueSend(s_subs[i], &msg, 0) != pdTRUE) {
            ESP_LOGW(TAG, "Queue full for '%s', event %d dropped", s_sub_names[i], type);
            dropped++;
        }
    }
    xSemaphoreGive(s_lock);
    return dropped == 0;
}

const char* event_name(pbox_event_t type) {
    switch (type) {
        case EV_PHONE_HEALTHY:       return "PHONE_HEALTHY";
        case EV_PHONE_TIMEOUT:       return "PHONE_TIMEOUT";
        case EV_PHONE_DEAD:          return "PHONE_DEAD";
        case EV_PHONE_RECOVERED:     return "PHONE_RECOVERED";
        case EV_GAS_HEARTBEAT_OK:    return "GAS_HEARTBEAT_OK";
        case EV_GAS_STALE:           return "GAS_STALE";
        case EV_GAS_UNREACHABLE:     return "GAS_UNREACHABLE";
        case EV_TX_NEW:              return "TX_NEW";
        case EV_TX_PUSHED:           return "TX_PUSHED";
        case EV_POWER_USB_LOST:      return "POWER_USB_LOST";
        case EV_POWER_USB_RESTORED:  return "POWER_USB_RESTORED";
        case EV_BAT_LOW:             return "BAT_LOW";
        case EV_BAT_CRITICAL:        return "BAT_CRITICAL";
        case EV_BAT_RECOVERED:       return "BAT_RECOVERED";
        case EV_WIFI_CONNECTED:      return "WIFI_CONNECTED";
        case EV_WIFI_DISCONNECTED:   return "WIFI_DISCONNECTED";
        case EV_WIFI_AP_STARTED:     return "WIFI_AP_STARTED";
        case EV_SYSTEM_BOOT:         return "SYSTEM_BOOT";
        case EV_SYSTEM_ALERT:        return "SYSTEM_ALERT";
        case EV_SYSTEM_CRITICAL:     return "SYSTEM_CRITICAL";
        default:                     return "UNKNOWN";
    }
}
