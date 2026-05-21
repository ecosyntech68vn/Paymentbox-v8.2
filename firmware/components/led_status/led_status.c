/**
 * led_status.c — State machine điều khiển 3 LED RGB.
 *
 * Đây là core orchestrator của phương án E. Subscribe event bus,
 * tích hợp trạng thái phone + GAS + battery thành 1 trong các state:
 *
 *   IDLE     — Booting, chưa connect WiFi → BLUE blink
 *   OK       — Phone OK + GAS OK + bat OK → GREEN solid
 *   WARN     — Một sub-system vừa fail → YELLOW (R+G pulse)
 *   ALERT    — Phone dead hoặc GAS unreachable >5min → RED pulse
 *   CRITICAL — Mất USB + bat <3V hoặc 2 sub-system fail → RED solid + buzzer
 *
 * LED RGB drive qua LEDC (PWM 5kHz) để có thể fade/pulse.
 */
#include "led_status.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "led_status";

static bool led_event_filter(pbox_event_t t) {
    return t == EV_PHONE_HEALTHY || t == EV_PHONE_TIMEOUT || t == EV_PHONE_DEAD || t == EV_PHONE_RECOVERED
        || t == EV_GAS_HEARTBEAT_OK || t == EV_GAS_STALE || t == EV_GAS_UNREACHABLE
        || t == EV_BAT_CRITICAL || t == EV_BAT_RECOVERED
        || t == EV_POWER_USB_LOST || t == EV_POWER_USB_RESTORED
        || t == EV_WIFI_CONNECTED || t == EV_WIFI_DISCONNECTED;
}

typedef enum {
    ST_IDLE = 0,
    ST_OK,
    ST_WARN,
    ST_ALERT,
    ST_CRITICAL,
} pbox_state_t;

static const char *state_name(pbox_state_t s) {
    switch (s) {
        case ST_IDLE:     return "IDLE";
        case ST_OK:       return "OK";
        case ST_WARN:     return "WARN";
        case ST_ALERT:    return "ALERT";
        case ST_CRITICAL: return "CRITICAL";
    }
    return "?";
}

// LEDC config (3 channels, 1 timer)
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_FREQ_HZ       5000
#define LEDC_RES           LEDC_TIMER_8_BIT
#define LEDC_CH_R          LEDC_CHANNEL_0
#define LEDC_CH_G          LEDC_CHANNEL_1
#define LEDC_CH_B          LEDC_CHANNEL_2

static void led_setup(void) {
    ledc_timer_config_t tcfg = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RES,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&tcfg);

    struct { int ch; int gpio; } ch[3] = {
        {LEDC_CH_R, PIN_LED_R},
        {LEDC_CH_G, PIN_LED_G},
        {LEDC_CH_B, PIN_LED_B},
    };
    for (int i = 0; i < 3; i++) {
        ledc_channel_config_t ccfg = {
            .speed_mode = LEDC_MODE,
            .channel = ch[i].ch,
            .timer_sel = LEDC_TIMER,
            .gpio_num = ch[i].gpio,
            .duty = 0,
            .hpoint = 0,
        };
        ledc_channel_config(&ccfg);
    }
}

static void led_set(uint8_t r, uint8_t g, uint8_t b) {
    ledc_set_duty(LEDC_MODE, LEDC_CH_R, r);
    ledc_set_duty(LEDC_MODE, LEDC_CH_G, g);
    ledc_set_duty(LEDC_MODE, LEDC_CH_B, b);
    ledc_update_duty(LEDC_MODE, LEDC_CH_R);
    ledc_update_duty(LEDC_MODE, LEDC_CH_G);
    ledc_update_duty(LEDC_MODE, LEDC_CH_B);
}

// Sub-system states — mỗi subsystem 1 trạng thái riêng, tổ hợp thành system state
typedef struct {
    bool phone_ok;
    bool gas_ok;
    bool bat_critical;
    bool usb_lost;
    bool wifi_ok;
} subsys_t;

static subsys_t s_sys = { false, false, false, false, false };

static pbox_state_t compute_state(const subsys_t *sys) {
    if (!sys->wifi_ok) return ST_IDLE;
    if (sys->bat_critical || (sys->usb_lost && !sys->phone_ok)) return ST_CRITICAL;
    if (!sys->phone_ok || !sys->gas_ok) return ST_ALERT;
    if (sys->usb_lost) return ST_WARN;
    return ST_OK;
}

static void handle_event(pbox_event_t ev) {
    switch (ev) {
        case EV_PHONE_HEALTHY:
        case EV_PHONE_RECOVERED:      s_sys.phone_ok = true; break;
        case EV_PHONE_DEAD:           s_sys.phone_ok = false; break;
        case EV_GAS_HEARTBEAT_OK:     s_sys.gas_ok = true; break;
        case EV_GAS_STALE:
        case EV_GAS_UNREACHABLE:      s_sys.gas_ok = false; break;
        case EV_BAT_CRITICAL:         s_sys.bat_critical = true; break;
        case EV_BAT_RECOVERED:        s_sys.bat_critical = false; break;
        case EV_POWER_USB_LOST:       s_sys.usb_lost = true; break;
        case EV_POWER_USB_RESTORED:   s_sys.usb_lost = false; break;
        case EV_WIFI_CONNECTED:       s_sys.wifi_ok = true; break;
        case EV_WIFI_DISCONNECTED:    s_sys.wifi_ok = false; break;
        default: break;
    }
}

static void on_state_enter(pbox_state_t old_state, pbox_state_t new_state) {
    ESP_LOGI(TAG, "State: %s → %s", state_name(old_state), state_name(new_state));
    if (new_state == ST_ALERT)    event_bus_publish(EV_SYSTEM_ALERT, NULL);
    if (new_state == ST_CRITICAL) event_bus_publish(EV_SYSTEM_CRITICAL, NULL);
    if (new_state == ST_OK)       event_bus_publish(EV_SYSTEM_BOOT, NULL);  // re-notify boot as healthy
}

static void led_status_task(void *arg) {
    QueueHandle_t q = event_bus_subscribe_filtered("led_status", led_event_filter);
    led_setup();

    pbox_state_t current = ST_IDLE;
    int tick = 0;

    while (1) {
        // Process events (non-blocking)
        pbox_event_msg_t msg;
        while (xQueueReceive(q, &msg, 0) == pdTRUE) {
            handle_event(msg.type);
        }

        pbox_state_t next = compute_state(&s_sys);
        if (next != current) {
            on_state_enter(current, next);
            current = next;
        }

        // Render LED based on state (100ms tick)
        uint8_t pulse = (tick % 10 < 5) ? 255 : 0;          // 1Hz blink
        uint8_t breathe = (tick % 20 < 10) ? (tick%20)*25 : (10-(tick%20-10))*25;

        switch (current) {
            case ST_IDLE:     led_set(0, 0, pulse); break;             // BLUE blink
            case ST_OK:       led_set(0, 80, 0); break;                // GREEN soft
            case ST_WARN:     led_set(breathe/2, breathe/2, 0); break; // YELLOW breathe
            case ST_ALERT:    led_set(pulse, 0, 0); break;              // RED pulse
            case ST_CRITICAL: led_set(255, 0, 0); break;                // RED solid
        }

        tick++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void led_status_start(void) {
    xTaskCreate(led_status_task, "led_status", TASK_STACK_MEDIUM,
                NULL, PRIO_MED, NULL);
}
