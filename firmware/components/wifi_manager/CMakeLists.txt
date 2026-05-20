idf_component_register(
    SRCS "wifi_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_wifi esp_event esp_netif nvs_flash event_bus config_server main
)
