/**
 * battery_monitor.c — Đọc ADC GPIO36 đo V_BAT, phát hiện USB lost & bat low.
 *
 * Mạch: BAT+ ── 100k ── ADC_BAT ── 47k ── GND
 * → V_ADC = V_BAT * 47/(100+47) = V_BAT * 0.32
 * → V_BAT = V_ADC * 3.13
 *
 * ESP32 ADC1 với attenuation 11dB → max ~3.3V → đủ dải đo V_BAT 0-4.2V (1.34V ADC max).
 *
 * Logic:
 * - Đọc mỗi 5s, smooth bằng moving average 5 samples
 * - V_BAT > 4.0V + stable = đang sạc → EV_POWER_USB_RESTORED
 * - V_BAT giảm theo thời gian (dV/dt < -0.001 V/s sustained) → EV_POWER_USB_LOST
 * - V_BAT < BAT_LOW_V (3.4V) → EV_BAT_LOW
 * - V_BAT < BAT_CRITICAL_V (3.0V) → EV_BAT_CRITICAL (chuẩn bị shutdown)
 * - V_BAT recover > 4.0V → EV_BAT_RECOVERED
 */
#include "battery_monitor.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "battery";

#define SAMPLE_INTERVAL_MS  5000
#define HISTORY_LEN         5

static adc_oneshot_unit_handle_t s_adc = NULL;
static adc_cali_handle_t s_cali = NULL;
static float s_history[HISTORY_LEN] = { 0 };
static int s_idx = 0;
static bool s_low_sent = false;
static bool s_critical_sent = false;
static bool s_usb_lost_sent = false;
static float s_last_voltage = 0;

static void adc_init(void) {
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    adc_oneshot_new_unit(&unit_cfg, &s_adc);

    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_11,
    };
    adc_oneshot_config_channel(s_adc, PIN_ADC_BAT, &ch_cfg);

    // Calibration (line fitting nếu efuse có)
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_cali_create_scheme_line_fitting(&cali_cfg, &s_cali);
}

static float read_voltage(void) {
    int raw = 0;
    adc_oneshot_read(s_adc, PIN_ADC_BAT, &raw);

    int mv = 0;
    adc_cali_raw_to_voltage(s_cali, raw, &mv);
    float v_adc = mv / 1000.0f;
    return v_adc * BAT_DIVIDER_RATIO;
}

static float avg_history(void) {
    float sum = 0;
    int n = 0;
    for (int i = 0; i < HISTORY_LEN; i++) {
        if (s_history[i] > 0) { sum += s_history[i]; n++; }
    }
    return n > 0 ? sum / n : 0;
}

static void check_thresholds(float v) {
    // BAT_CRITICAL
    if (v < BAT_CRITICAL_V && !s_critical_sent) {
        ESP_LOGE(TAG, "Battery CRITICAL: %.2fV", v);
        pbox_event_msg_t m = { .data.fval = v };
        event_bus_publish(EV_BAT_CRITICAL, &m);
        s_critical_sent = true;
    }
    // BAT_LOW
    if (v < BAT_LOW_V && v >= BAT_CRITICAL_V && !s_low_sent) {
        ESP_LOGW(TAG, "Battery LOW: %.2fV", v);
        pbox_event_msg_t m = { .data.fval = v };
        event_bus_publish(EV_BAT_LOW, &m);
        s_low_sent = true;
    }
    // RECOVERED (khi sạc lên)
    if (v >= BAT_FULL_V && (s_low_sent || s_critical_sent)) {
        ESP_LOGI(TAG, "Battery RECOVERED: %.2fV", v);
        pbox_event_msg_t m = { .data.fval = v };
        event_bus_publish(EV_BAT_RECOVERED, &m);
        s_low_sent = false;
        s_critical_sent = false;
    }

    // USB lost detection: V_BAT giảm liên tục trong N samples
    if (s_last_voltage > 0 && v > 0) {
        float dv = v - s_last_voltage;
        // Nếu giảm > 10mV mỗi 5s và đã có 3+ samples
        if (dv < -0.01f && !s_usb_lost_sent && v < 4.0f) {
            ESP_LOGW(TAG, "USB lost detected, dV=%.3f", dv);
            event_bus_publish(EV_POWER_USB_LOST, NULL);
            s_usb_lost_sent = true;
        } else if (dv > 0.05f && s_usb_lost_sent) {
            ESP_LOGI(TAG, "USB restored");
            event_bus_publish(EV_POWER_USB_RESTORED, NULL);
            s_usb_lost_sent = false;
        }
    }
    s_last_voltage = v;
}

static void battery_task(void *arg) {
    adc_init();
    ESP_LOGI(TAG, "Started, sampling every %dms", SAMPLE_INTERVAL_MS);

    // Warm up
    for (int i = 0; i < HISTORY_LEN; i++) {
        s_history[i] = read_voltage();
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    while (1) {
        float v = read_voltage();
        s_history[s_idx] = v;
        s_idx = (s_idx + 1) % HISTORY_LEN;

        float avg = avg_history();
        ESP_LOGI(TAG, "V_BAT=%.2fV (raw=%.2f avg=%.2f)", avg, v, avg);

        check_thresholds(avg);

        vTaskDelay(pdMS_TO_TICKS(SAMPLE_INTERVAL_MS));
    }
}

void battery_monitor_start(void) {
    xTaskCreate(battery_task, "battery", TASK_STACK_MEDIUM,
                NULL, PRIO_LOW, NULL);
}
