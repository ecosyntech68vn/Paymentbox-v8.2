idf_component_register(
    SRCS "led_status.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos driver event_bus main
)
