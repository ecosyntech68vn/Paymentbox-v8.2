idf_component_register(
    SRCS "lcd_ui.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos driver event_bus main
)
