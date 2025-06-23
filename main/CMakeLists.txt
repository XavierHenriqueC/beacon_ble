idf_component_register(
    SRCS 
        "main.c"
        "ble.c"
        "temp_hum.c"
        "sensor.pb.c"
    INCLUDE_DIRS "."
    REQUIRES 
        bt 
        freertos 
        nvs_flash 
        nanopb
)
