cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(rndstream C CXX ASM)

set(CMAKE_BUILD_TYPE Debug)

pico_sdk_init()

add_executable(rndstream main.c)

pico_enable_stdio_usb(rndstream 0)
pico_enable_stdio_uart(rndstream 1)

pico_add_extra_outputs(rndstream)

target_link_libraries(rndstream pico_stdlib)


