idf_component_register(
    SRCS 
        "main.c"
        "ble_gatt.c"
        "ble_live.c"
        "ble_log.c"
        "temp_hum.c"
        "sensor.pb.c"
        "serial.c"
        "nvs_controller.c"
    INCLUDE_DIRS "."
    REQUIRES 
        bt 
        freertos 
        nvs_flash 
        nanopb
)
