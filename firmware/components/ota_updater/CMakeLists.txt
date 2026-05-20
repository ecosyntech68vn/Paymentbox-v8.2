idf_component_register(
    SRCS "ota_updater.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_https_ota nvs_flash event_bus battery_monitor main json_utils
)
