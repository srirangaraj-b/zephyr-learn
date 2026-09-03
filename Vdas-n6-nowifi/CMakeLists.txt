cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(scpi_daq_stm32n657)

target_sources(app PRIVATE
    src/main.c
    src/zephyr_uart.c
    src/zephyr_adc.c
    src/zephyr_dac.c
    src/scpi_parser.c
    src/scpi_commands.c
    src/scpi_error.c
    src/control_pid.c
    src/application_daq.c
)

target_include_directories(app PRIVATE
    src
    include
)

target_link_libraries(app PRIVATE m)