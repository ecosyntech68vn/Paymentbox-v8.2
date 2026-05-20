idf_component_register(
    SRCS "gas_poller.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_http_client event_bus main json_utils
)
