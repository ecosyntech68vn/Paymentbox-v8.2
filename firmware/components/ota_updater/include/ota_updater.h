#pragma once
#include <stdbool.h>

/**
 * Khởi động OTA updater task.
 *
 * Task chạy mỗi OTA_CHECK_INTERVAL_MS (default 24h):
 * 1. HTTPS GET tới GAS endpoint với version hiện tại
 * 2. Nếu GAS trả về update_available=true và version mới → download
 * 3. esp_https_ota stream firmware → partition OTA rỗng
 * 4. Verify SHA256 (TODO: signature qua ATECC608B)
 * 5. esp_ota_set_boot_partition → restart
 *
 * Sau boot firmware mới:
 * 6. Wait 60s, nếu hệ thống ổn định → esp_ota_mark_app_valid_cancel_rollback
 * 7. Nếu crash > N lần → bootloader tự rollback về partition cũ
 */
bool ota_updater_start(void);

/**
 * Trigger OTA check ngay (gọi từ HTTP endpoint hoặc nút thủ công).
 * Non-blocking — đẩy lệnh vào queue của task.
 */
bool ota_updater_trigger_check(void);

/**
 * Mark current firmware as valid (gọi sau boot OK).
 * Nếu KHÔNG gọi → bootloader sẽ rollback ở lần boot kế tiếp.
 */
void ota_updater_mark_valid(void);
