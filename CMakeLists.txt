cmake_minimum_required(VERSION 3.9)

set(CMAKE_USER_MAKE_RULES_OVERRIDE "cmake/disable_extralibs.cmake")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake")

project(stm32-bootloader)
include(platform)
include(pkgtools)

# Build the external code
include(extern.cmake)
