cmake_minimum_required(VERSION 3.9)

set(CMAKE_USER_MAKE_RULES_OVERRIDE "cmake/disable_extralibs.cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(stm32-bootloader)
include(platform)
include(jlink_upload)
include(pkgtools)

# Build the external code
add_subdirectory(extern)


# Build the bootloader application
set(BOOTLOADER_SOURCES 
    "src/Bootloader.cpp"
    "src/Uart.cpp"
    "src/Can.cpp"
    "src/System.cpp"
    "src/Flash.cpp")

set(BOOTLOADER_INCLUDES
    "include/Bootloader.hpp"
    "include/Uart.hpp"
    "include/Can.hpp"
    "include/System.hpp"
    "include/Buffer.hpp"
    "include/Flash.hpp"
    "include/Pin.hpp")

use_platform(stm32f777vi)

function(add_bootloader BOARD_NAME MAIN)
    add_executable("${BOARD_NAME}-bootloader" ${BOOTLOADER_SOURCES} ${BOOTLOADER_INCLUDES} ${MAIN})

    target_link_libraries("${BOARD_NAME}-bootloader"
                    startup_stm32f777vi
                    hal_stm32f777vi)
    target_include_directories("${BOARD_NAME}-bootloader"
                    PUBLIC "include")

    add_jlink_upload("${BOARD_NAME}-bootloader" stm32f777vi)

endfunction(add_bootloader)

# Add the default bootloader
add_bootloader(default "src/main.cpp")
add_bootloader(boarda "config/boarda.cpp")
add_bootloader(boardb "config/boardb.cpp")
