/**
 * lcd_ui.c — Hiển thị status trên LCD 16x2 I2C (PCF8574 backpack 0x27).
 *
 * Layout (16 cột × 2 dòng):
 *   line0: "STATUS:OK   BAT:4.1V"
 *   line1: "PH:OK GAS:OK  12:30"
 *
 * Refresh 2Hz. Subscribe event bus, đọc state global.
 *
 * Driver PCF8574 đơn giản:
 *   - 8-bit I2C output expander
 *   - 4-bit LCD mode (D4-D7 = bit 4-7, EN = bit 2, RW = bit 1, RS = bit 0, BL = bit 3)
 */
#include "lcd_ui.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "lcd_ui";

// PCF8574 → LCD pin mapping (chuẩn của module Trung Quốc thông dụng)
#define LCD_RS  0x01
#define LCD_RW  0x02
#define LCD_EN  0x04
#define LCD_BL  0x08   // Backlight ON

static struct {
    bool phone_ok;
    bool gas_ok;
    float bat_v;
    bool usb_lost;
    bool wifi_ok;
} s_state = { 0 };

// ============== I2C low-level ==============

static esp_err_t i2c_write_byte(uint8_t data) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (I2C_ADDR_LCD << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_NUM, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static void lcd_pulse_en(uint8_t data) {
    i2c_write_byte(data | LCD_EN | LCD_BL);
    vTaskDelay(pdMS_TO_TICKS(1));
    i2c_write_byte((data & ~LCD_EN) | LCD_BL);
    vTaskDelay(pdMS_TO_TICKS(1));
}

static void lcd_write_4bits(uint8_t data) {
    lcd_pulse_en(data & 0xF0);
}

static void lcd_send(uint8_t value, uint8_t mode) {
    // 4-bit mode: gửi nibble cao trước rồi thấp
    uint8_t high = value & 0xF0;
    uint8_t low = (value << 4) & 0xF0;
    lcd_pulse_en(high | mode);
    lcd_pulse_en(low | mode);
}

static void lcd_cmd(uint8_t cmd) { lcd_send(cmd, 0); }
static void lcd_data(uint8_t d)  { lcd_send(d, LCD_RS); }

static void lcd_init(void) {
    vTaskDelay(pdMS_TO_TICKS(50));
    // HD44780 init sequence
    lcd_write_4bits(0x30); vTaskDelay(pdMS_TO_TICKS(5));
    lcd_write_4bits(0x30); vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_4bits(0x30); vTaskDelay(pdMS_TO_TICKS(1));
    lcd_write_4bits(0x20);  // set 4-bit mode

    lcd_cmd(0x28);  // 4-bit, 2 lines, 5x8 font
    lcd_cmd(0x08);  // display off
    lcd_cmd(0x01);  // clear
    vTaskDelay(pdMS_TO_TICKS(2));
    lcd_cmd(0x06);  // entry mode: increment, no shift
    lcd_cmd(0x0C);  // display on, no cursor
}

static void lcd_set_cursor(uint8_t col, uint8_t row) {
    uint8_t addr = (row == 0) ? 0x80 + col : 0xC0 + col;
    lcd_cmd(addr);
}

static void lcd_print(const char *s) {
    while (*s) lcd_data((uint8_t)*s++);
}

// ============== UI ==============

static void render(void) {
    char line0[17], line1[17];

    const char *st = s_state.phone_ok && s_state.gas_ok ? "OK     " :
                     !s_state.phone_ok ? "PHN-OFF" :
                     !s_state.gas_ok   ? "GAS-OFF" : "WARN   ";

    snprintf(line0, sizeof(line0), "ST:%-7s %.2fV", st, s_state.bat_v);
    snprintf(line1, sizeof(line1), "PH:%s GAS:%s %s",
             s_state.phone_ok ? "OK" : "--",
             s_state.gas_ok   ? "OK" : "--",
             s_state.usb_lost ? "BAT" : "USB");

    lcd_set_cursor(0, 0);
    lcd_print(line0);
    lcd_set_cursor(0, 1);
    lcd_print(line1);
}

static void handle_event(pbox_event_msg_t *msg) {
    switch (msg->type) {
        case EV_PHONE_HEALTHY:
        case EV_PHONE_RECOVERED:    s_state.phone_ok = true; break;
        case EV_PHONE_DEAD:         s_state.phone_ok = false; break;
        case EV_GAS_HEARTBEAT_OK:   s_state.gas_ok = true; break;
        case EV_GAS_STALE:
        case EV_GAS_UNREACHABLE:    s_state.gas_ok = false; break;
        case EV_BAT_LOW:
        case EV_BAT_CRITICAL:
        case EV_BAT_RECOVERED:      s_state.bat_v = msg->data.fval; break;
        case EV_POWER_USB_LOST:     s_state.usb_lost = true; break;
        case EV_POWER_USB_RESTORED: s_state.usb_lost = false; break;
        case EV_WIFI_CONNECTED:     s_state.wifi_ok = true; break;
        case EV_WIFI_DISCONNECTED:  s_state.wifi_ok = false; break;
        default: break;
    }
}

static void lcd_task(void *arg) {
    QueueHandle_t q = event_bus_subscribe("lcd_ui");
    lcd_init();

    lcd_set_cursor(0, 0);
    lcd_print("PaymentBox V8.2");
    lcd_set_cursor(0, 1);
    lcd_print("Booting...");
    vTaskDelay(pdMS_TO_TICKS(1500));
    lcd_cmd(0x01);  // clear

    while (1) {
        pbox_event_msg_t msg;
        while (xQueueReceive(q, &msg, 0) == pdTRUE) {
            handle_event(&msg);
        }
        render();
        vTaskDelay(pdMS_TO_TICKS(500));  // 2Hz refresh
    }
}

static void i2c_master_init(void) {
    i2c_config_t cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,    // có pull-up ngoài 4.7k
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_FREQ_HZ,
    };
    i2c_param_config(I2C_PORT_NUM, &cfg);
    i2c_driver_install(I2C_PORT_NUM, cfg.mode, 0, 0, 0);
}

void lcd_ui_start(void) {
    i2c_master_init();
    xTaskCreate(lcd_task, "lcd_ui", TASK_STACK_MEDIUM, NULL, PRIO_LOW, NULL);
}
