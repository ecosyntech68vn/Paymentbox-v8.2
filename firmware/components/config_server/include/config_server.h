#pragma once
#include <stdbool.h>

/**
 * Khởi động HTTP config server trên 192.168.4.1:80 (AP mode).
 *
 * Endpoints:
 *   GET  /              → HTML form cấu hình
 *   POST /save          → Save NVS, schedule restart sau 2s
 *   GET  /status        → JSON trạng thái hiện tại
 *   GET  /scan          → Scan WiFi networks (trả JSON list)
 */
bool config_server_start(void);

/**
 * Dừng server (gọi khi WiFi chuyển sang STA mode).
 */
void config_server_stop(void);
