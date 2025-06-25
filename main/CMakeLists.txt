idf_component_register(
    SRCS 
        "main.c"
        "ble.c"
        "temp_hum.c"
        "log_ble.c"
        "sensor.pb.c"
        "serial.c"
    INCLUDE_DIRS "."
    REQUIRES 
        bt 
        freertos 
        nvs_flash 
        nanopb
)
