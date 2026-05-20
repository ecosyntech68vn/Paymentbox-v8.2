/**
 * event_bus.h — FreeRTOS queue-based inter-task event broadcast
 *
 * Mỗi task subscribe vào event bus và nhận event. Không direct call giữa các task.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    EV_NONE = 0,

    // Phone status events
    EV_PHONE_HEALTHY,           // Phone responding bình thường
    EV_PHONE_TIMEOUT,           // Phone không respond 1 lần
    EV_PHONE_DEAD,              // Phone không respond >3 lần (90s)
    EV_PHONE_RECOVERED,         // Phone respond trở lại sau khi dead

    // GAS backend events
    EV_GAS_HEARTBEAT_OK,        // GAS sheet thấy heartbeat gần đây
    EV_GAS_STALE,               // GAS heartbeat > 5 phút
    EV_GAS_UNREACHABLE,         // Không gọi được GAS API

    // Transaction events
    EV_TX_NEW,                  // Có giao dịch mới (từ phone)
    EV_TX_PUSHED,               // Đã push GAS thành công

    // Power events
    EV_POWER_USB_LOST,          // Mất USB-C input
    EV_POWER_USB_RESTORED,
    EV_BAT_LOW,                 // Pin xuống dưới ngưỡng
    EV_BAT_CRITICAL,            // Pin sắp hết
    EV_BAT_RECOVERED,           // Pin sạc đầy

    // WiFi events
    EV_WIFI_CONNECTED,
    EV_WIFI_DISCONNECTED,
    EV_WIFI_AP_STARTED,         // Mode AP cho config

    // System events
    EV_SYSTEM_BOOT,
    EV_SYSTEM_ALERT,            // Alert cao cấp (bao trùm các sub-event)
    EV_SYSTEM_CRITICAL,
} pbox_event_t;

typedef struct {
    pbox_event_t type;
    uint32_t timestamp_ms;
    union {
        float    fval;
        int32_t  ival;
        char     str[32];
    } data;
} pbox_event_msg_t;

#define EVENT_BUS_QUEUE_LEN     16

/**
 * Init event bus. Gọi 1 lần ở main.
 */
void event_bus_init(void);

/**
 * Publish event tới tất cả subscriber.
 * Non-blocking — nếu queue đầy, drop event và log warn.
 */
bool event_bus_publish(pbox_event_t type, const pbox_event_msg_t *data);

/**
 * Subscribe — trả về queue handle để task của bạn xQueueReceive từ đó.
 * Mỗi subscriber có queue riêng.
 */
QueueHandle_t event_bus_subscribe(const char *subscriber_name);

/**
 * Tên event (debug).
 */
const char* event_name(pbox_event_t type);
