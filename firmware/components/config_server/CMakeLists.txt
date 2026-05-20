idf_component_register(
    SRCS "config_server.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_http_server esp_wifi nvs_flash main
)
