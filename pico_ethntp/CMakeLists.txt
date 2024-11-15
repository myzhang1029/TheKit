add_subdirectory(pico_eth)

add_executable(pico_ethntp pico_ethntp.c ntp_client.c ntp_server.c ntp_common.c gps_util.c gps.c)

target_compile_definitions(pico_ethntp PRIVATE RPI_PICO=1)

target_link_libraries(pico_ethntp
    pico_eth
    pico_stdlib
    hardware_rtc
    hardware_uart
    pico_lwip
)


target_include_directories(pico_ethntp PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_ethntp)
pico_enable_stdio_usb(pico_ethntp 1)
pico_enable_stdio_uart(pico_ethntp 0)
