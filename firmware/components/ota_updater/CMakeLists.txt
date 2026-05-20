idf_component_register(
    SRCS "ota_updater.c"
    INCLUDE_DIRS "include"
    REQUIRES
        freertos
        app_update
        esp_https_ota
        esp_http_client
        esp-tls
        mbedtls
        nvs_flash
        event_bus
        main
)
