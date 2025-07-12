idf_component_register(
    SRCS 
        "main.c"
        "ble_gatt.c"
        "ble_live.c"
        "ble_log.c"
        "sth31d.c"
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
        driver
)

set(GENERATED_FILE "${CMAKE_CURRENT_SOURCE_DIR}/build_time.h")
add_custom_command(
    OUTPUT ${GENERATED_FILE}
    COMMAND ${CMAKE_COMMAND} -E echo "Gerando build_time.h com timestamp atual..."
    COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/gen_build_time.py ${GENERATED_FILE}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/gen_build_time.py
)

add_custom_target(generate_build_time_header DEPENDS ${GENERATED_FILE})
add_dependencies(${COMPONENT_LIB} generate_build_time_header)

# Inclui o header no build
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})