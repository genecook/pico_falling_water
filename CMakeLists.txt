cmake_minimum_required(VERSION 3.13)

set(CMAKE_MODULE_PATH $ENV{PICO_SDK_PATH} $ENV{PICO_EXAMPLES})

include(pico_sdk_import)

set(PICO_LCD_LIB $ENV{HOME}/Desktop/pico/Pico-ResTouch-LCD-X_X_Code/c)

set(PICO_GUI $ENV{HOME}/Desktop/pico_gui)

project(rndstream C CXX ASM)

set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(${PICO_LCD_LIB}/lib/config lib/config)
add_subdirectory(${PICO_LCD_LIB}/lib/lcd lib/lcd)
add_subdirectory(${PICO_LCD_LIB}/lib/font lib/font)
add_subdirectory(${PICO_LCD_LIB}/examples examples)
add_subdirectory(${PICO_LCD_LIB}/lib/fatfs lib/fatfs)
add_subdirectory(${PICO_LCD_LIB}/lib/sdcard lib/sdcard)

include_directories(${PICO_LCD_LIB}/examples)
include_directories(${PICO_LCD_LIB}/lib/config)
include_directories(${PICO_LCD_LIB}/lib/lcd)
include_directories(${PICO_LCD_LIB}/lib/font)
include_directories(${PICO_LCD_LIB}/lib/fatfs)
include_directories(${PICO_LCD_LIB}/lib/sdcard)

include_directories(${PICO_GUI})

add_executable(rndstream main.c ${PICO_GUI}/lcd_touch_wrapper.c 
	       ${PICO_GUI}/my_fatfs_storage.c ${PICO_GUI}/my_track_touch.c)

target_link_libraries(rndstream pico_stdlib pico_multicore lcd font config
                      pico_stdlib hardware_spi fatfs)

target_compile_options(rndstream PRIVATE "-DUSE_MULTICORE=1 -DUSE_LCD=1")

pico_enable_stdio_usb(rndstream 0)
pico_enable_stdio_uart(rndstream 1)

pico_add_extra_outputs(rndstream)
