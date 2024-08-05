cmake_minimum_required(VERSION 3.12)

include(pico_sdk_import.cmake)
include(pico_extras_import_optional.cmake)

project(main C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(main
  src/main.c
)

target_link_libraries(main
  hardware_pwm
  hardware_spi
  pico_stdlib
)

pico_add_extra_outputs(main)