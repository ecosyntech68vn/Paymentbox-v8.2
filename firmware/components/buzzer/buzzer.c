/**
 * buzzer.c — Active buzzer 5V drive qua Q2 NPN, PWM tone qua LEDC.
 *
 * Patterns:
 *   BEEP_TX_NEW      = 1 beep ngắn 80ms (giao dịch mới)
 *   BEEP_PHONE_DEAD  = 3 beep nhanh (phone offline)
 *   BEEP_CRITICAL    = continuous 1Hz cho đến khi clear
 *
 * Active buzzer chỉ cần ON/OFF — không cần PWM tone (buzzer tự rung
 * ở freq nội bộ). Tuy nhiên dùng LEDC để dễ generate pattern.
 */
#include "buzzer.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "buzzer";

typedef enum {
    BZ_OFF = 0,
    BZ_TX_NEW,
    BZ_PHONE_DEAD,
    BZ_CRITICAL,
} buzzer_pattern_t;

static buzzer_pattern_t s_pattern = BZ_OFF;
static QueueHandle_t s_pattern_q = NULL;
static int s_critical_cycles = 0;

static void buzzer_gpio_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << PIN_BUZZER,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(PIN_BUZZER, 0);
}

static void buzz_on(int ms) {
    gpio_set_level(PIN_BUZZER, 1);
    vTaskDelay(pdMS_TO_TICKS(ms));
    gpio_set_level(PIN_BUZZER, 0);
}

static void play_pattern(buzzer_pattern_t p) {
    switch (p) {
        case BZ_TX_NEW:
            buzz_on(80);
            break;
        case BZ_PHONE_DEAD:
            for (int i = 0; i < 3; i++) {
                buzz_on(100);
                vTaskDelay(pdMS_TO_TICKS(80));
            }
            break;
        case BZ_CRITICAL:
            buzz_on(500);
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        default: break;
    }
}

static void buzzer_task(void *arg) {
    QueueHandle_t q = event_bus_subscribe("buzzer");
    s_pattern_q = xQueueCreate(4, sizeof(buzzer_pattern_t));
    buzzer_gpio_init();

    while (1) {
        pbox_event_msg_t msg;
        // Drain events
        while (xQueueReceive(q, &msg, 0) == pdTRUE) {
            buzzer_pattern_t newp = BZ_OFF;
            switch (msg.type) {
                case EV_TX_NEW:           newp = BZ_TX_NEW; break;
                case EV_PHONE_DEAD:       newp = BZ_PHONE_DEAD; break;
                case EV_SYSTEM_CRITICAL:  newp = BZ_CRITICAL; break;
                case EV_BAT_RECOVERED:
                case EV_PHONE_RECOVERED:  newp = BZ_OFF; break;
                default: break;
            }
            if (newp != BZ_OFF) {
                xQueueSend(s_pattern_q, &newp, 0);
            } else if (msg.type == EV_BAT_RECOVERED || msg.type == EV_PHONE_RECOVERED) {
                s_pattern = BZ_OFF;
            }
        }

        // Run pattern khi có
        buzzer_pattern_t to_play;
        if (xQueueReceive(s_pattern_q, &to_play, 0) == pdTRUE) {
            s_pattern = to_play;
            ESP_LOGI(TAG, "Pattern %d", to_play);
            play_pattern(to_play);
            if (to_play != BZ_CRITICAL) s_pattern = BZ_OFF;
        }

        // Critical pattern lặp
        if (s_pattern == BZ_CRITICAL) {
            s_critical_cycles++;
            if (s_critical_cycles >= 1800) {  // ~30 phút (1800 × 1s)
                ESP_LOGW(TAG, "Critical buzzer timeout after 30min, auto-silence");
                s_pattern = BZ_OFF;
                s_critical_cycles = 0;
            } else {
                play_pattern(BZ_CRITICAL);
            }
        } else {
            s_critical_cycles = 0;
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }
}

void buzzer_start(void) {
    xTaskCreate(buzzer_task, "buzzer", TASK_STACK_SMALL, NULL, PRIO_LOW, NULL);
}
