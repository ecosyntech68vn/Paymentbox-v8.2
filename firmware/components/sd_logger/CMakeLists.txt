idf_component_register(
    SRCS "sd_logger.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos esp_driver_sdspi fatfs vfs event_bus main
)
