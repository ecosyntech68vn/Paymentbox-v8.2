idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "include"
    REQUIRES
        nvs_flash
        esp_wifi
        event_bus
        wifi_manager
        phone_poller
        gas_poller
        led_status
        buzzer
        lcd_ui
        battery_monitor
        sd_logger
        config_server
        ota_updater
)
