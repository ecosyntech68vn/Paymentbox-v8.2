#pragma once
#include <stdbool.h>

void battery_monitor_start(void);

/**
 * Lấy điện áp pin trung bình gần nhất (Volts).
 * Trả về 0.0 nếu chưa có mẫu nào.
 */
float battery_read_voltage(void);

/**
 * Kiểm tra USB-C đang cấp nguồn hay không.
 */
bool battery_usb_present(void);
