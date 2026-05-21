idf_component_register(
    SRCS "telegram_alert.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos json_utils event_bus main esp_http_client
)
