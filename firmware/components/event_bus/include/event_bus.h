/**
 * event_bus.h — FreeRTOS queue-based inter-task event broadcast
 *
 * Mỗi task subscribe vào event bus và nhận event. Không direct call giữa các task.
 * Hỗ trợ filter: subscriber chỉ nhận event type mình cần, giảm queue pressure.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    EV_NONE = 0,

    // Phone status events
    EV_PHONE_HEALTHY,
    EV_PHONE_TIMEOUT,
    EV_PHONE_DEAD,
    EV_PHONE_RECOVERED,

    // GAS backend events
    EV_GAS_HEARTBEAT_OK,
    EV_GAS_STALE,
    EV_GAS_UNREACHABLE,

    // Transaction events
    EV_TX_NEW,
    EV_TX_PUSHED,

    // Power events
    EV_POWER_USB_LOST,
    EV_POWER_USB_RESTORED,
    EV_BAT_LOW,
    EV_BAT_CRITICAL,
    EV_BAT_RECOVERED,

    // WiFi events
    EV_WIFI_CONNECTED,
    EV_WIFI_DISCONNECTED,
    EV_WIFI_AP_STARTED,

    // System events
    EV_SYSTEM_BOOT,
    EV_SYSTEM_ALERT,
    EV_SYSTEM_CRITICAL,
} pbox_event_t;

typedef struct {
    pbox_event_t type;
    int64_t timestamp_ms;
    union {
        float    fval;
        int32_t  ival;
        char     str[32];
    } data;
} pbox_event_msg_t;

#define EVENT_BUS_QUEUE_LEN     32

/** Filter callback: return true để nhận event, false để bỏ qua. */
typedef bool (*event_filter_fn)(pbox_event_t type);

/** Init event bus. Gọi 1 lần ở main. */
void event_bus_init(void);

/**
 * Publish event tới tất cả subscriber.
 * Non-blocking — nếu queue đầy, drop event và log warn.
 */
bool event_bus_publish(pbox_event_t type, const pbox_event_msg_t *data);

/**
 * Subscribe (nhận tất cả events). Backward-compatible.
 */
QueueHandle_t event_bus_subscribe(const char *subscriber_name);

/**
 * Subscribe với filter — chỉ nhận event mà filter() trả về true.
 * Giảm queue pressure, tránh drop event không cần thiết.
 */
QueueHandle_t event_bus_subscribe_filtered(const char *subscriber_name, event_filter_fn filter);

/** Tên event (debug). */
const char* event_name(pbox_event_t type);
