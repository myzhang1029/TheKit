add_library(pico_thekit_util)

target_include_directories(pico_thekit_util PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
)

target_compile_definitions(pico_thekit_util PRIVATE RPI_PICO=1)

target_sources(pico_thekit_util PRIVATE
    base64.c
    gps_util.c
    pcm.c
)

target_link_libraries(pico_thekit_util
    hardware_pwm
    hardware_divider
    pico_stdlib
)
