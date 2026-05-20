/**
 * sd_logger.c — Append-only NDJSON log lên microSD (F version only).
 *
 * Format: 1 dòng JSON / event:
 *   {"ts":1234567890,"ev":"PHONE_DEAD","data":""}
 *
 * Rotation: khi file >= 1MB → rename .log → .log.1, shift .1→.2, ..., delete .5
 *
 * Mount SPI bus tới microSD. Sử dụng SDMMC SPI mode (đơn giản hơn 4-bit SDIO).
 */
#include "sd_logger.h"
#include "event_bus.h"
#include "paymentbox_config.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

static const char *TAG = "sd_log";
static bool s_mounted = false;
static FILE *s_log = NULL;

static void mount_sd(void) {
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_SPI_MOSI,
        .miso_io_num = PIN_SPI_MISO,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };
    if (spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA) != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed");
        return;
    }

    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.gpio_cs = PIN_SPI_CS_SD;
    dev_cfg.host_id = SPI2_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_cfg = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };
    sdmmc_card_t *card = NULL;
    esp_err_t err = esp_vfs_fat_sdspi_mount("/sdcard", &host, &dev_cfg,
                                              &mount_cfg, &card);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SD mount failed: %d", err);
        return;
    }
    s_mounted = true;
    ESP_LOGI(TAG, "SD mounted, size=%lluMB",
             ((uint64_t)card->csd.capacity) * card->csd.sector_size / (1024*1024));
}

static void rotate_if_needed(void) {
    struct stat st;
    if (stat(SD_LOG_PATH, &st) != 0) return;
    if (st.st_size < SD_LOG_MAX_BYTES) return;

    if (s_log) { fclose(s_log); s_log = NULL; }

    // Shift .4 → .5, .3 → .4, ..., .log → .log.1
    char src[64], dst[64];
    for (int i = SD_LOG_ROTATE_KEEP - 1; i >= 1; i--) {
        snprintf(src, sizeof(src), "%s.%d", SD_LOG_PATH, i);
        snprintf(dst, sizeof(dst), "%s.%d", SD_LOG_PATH, i+1);
        if (rename(src, dst) != 0 && errno != ENOENT) {
            ESP_LOGW(TAG, "Rotate rename %s → %s failed: errno=%d", src, dst, errno);
        }
    }
    snprintf(dst, sizeof(dst), "%s.1", SD_LOG_PATH);
    if (rename(SD_LOG_PATH, dst) != 0) {
        ESP_LOGE(TAG, "Log rotate failed: errno=%d", errno);
        return;
    }
    ESP_LOGI(TAG, "Log rotated");
}

static void log_event(pbox_event_msg_t *msg) {
    if (!s_mounted) return;

    rotate_if_needed();

    if (!s_log) {
        s_log = fopen(SD_LOG_PATH, "a");
        if (!s_log) {
            ESP_LOGE(TAG, "Open log failed");
            return;
        }
    }

    fprintf(s_log, "{\"ts\":%u,\"ev\":\"%s\",\"data\":%.2f}\n",
            (unsigned)msg->timestamp_ms,
            event_name(msg->type),
            msg->data.fval);
    fflush(s_log);
}

static void sd_logger_task(void *arg) {
    QueueHandle_t q = event_bus_subscribe("sd_logger");
    mount_sd();

    while (1) {
        pbox_event_msg_t msg;
        if (xQueueReceive(q, &msg, portMAX_DELAY) == pdTRUE) {
            log_event(&msg);
        }
    }
}

void sd_logger_start(void) {
    xTaskCreate(sd_logger_task, "sd_log", TASK_STACK_LARGE, NULL, PRIO_LOW, NULL);
}
