cmake_minimum_required(VERSION 3.9)

set(EXTERN_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(EXTERN_BUILD_DIR "${CMAKE_CURRENT_LIST_DIR}/build")

# Download stm32 cube
set(CUBE_URL "https://www.st.com/content/ccc/resource/technical/software/firmware/18/40/6e/b0/44/f7/43/1d/stm32cubef7.zip/files/stm32cubef7.zip/jcr:content/translations/en.stm32cubef7.zip")
set(CUBE_CHECKSUM "283d7b857c7126f9b625fa6ceb7612dd15fdd43528dbe886bc607668e7486786")
set(CUBE_FILE "${EXTERN_BUILD_DIR}/stm32cube.zip")
set(CUBE_DIR "${EXTERN_BUILD_DIR}/STM32Cube_FW_F7_V1.14.0/")

download("STM32 Cube" ${CUBE_URL} ${CUBE_CHECKSUM} ${CUBE_FILE})
unzip("STM32 Cube" ${CUBE_FILE} ${EXTERN_BUILD_DIR} ${CUBE_DIR})

# Download pluscallback
set(PLUSCALLBACK_URL "https://github.com/codeplea/pluscallback/archive/master.zip")
set(PLUSCALLBACK_CHECKSUM "7c19076fa19764be911c19dfd2dde45fca99c53dbba4a3fe6dacf8fef5d4d395")
set(PLUSCALLBACK_FILE "${EXTERN_BUILD_DIR}/pluscallback.zip")
set(PLUSCALLBACK_DIR "${EXTERN_BUILD_DIR}/pluscallback-master")

download("Plus Callback" ${PLUSCALLBACK_URL} ${PLUSCALLBACK_CHECKSUM} ${PLUSCALLBACK_FILE})
unzip("Plus Callback" ${PLUSCALLBACK_FILE} ${EXTERN_BUILD_DIR} ${PLUSCALLBACK_DIR})

# Download catch
set(CATCH_URL "https://github.com/catchorg/Catch2/releases/download/v2.5.0/catch.hpp")
set(CATCH_CHECKSUM "7c19076fa19764be911c19dfd2dde45fca99c53dbba4a3fe6dacf8fef5d4d395")
set(CATCH_DIR "${EXTERN_BUILD_DIR}/catch")
set(CATCH_FILE "${CATCH_DIR}/catch.hpp")

file(MAKE_DIRECTORY "${CATCH_DIR}")

download("Catch" ${CATCH_URL} ${CATCH_CHECKSUM} ${CATCH_FILE})


# run all patches that we have
file(GLOB PATCHES "${EXTERN_DIR}/patches/*.patch")
foreach(PATCH ${PATCHES})
    patch("${PATCH}" "${EXTERN_BUILD_DIR}")
endforeach()

# ---------------------------------------------------------------
# ---------------------------------------------------------------

# Directories:
set(CMSIS_DIR "${CUBE_DIR}/Drivers/CMSIS")
set(HAL_DIR "${CUBE_DIR}/Drivers/STM32F7xx_HAL_Driver")
set(CONFIG_DIR "${CMAKE_CURRENT_LIST_DIR}/config")

#------------------ CMSIS -------------------

set(CMSIS_SOURCES "${CMSIS_DIR}/Device/ST/STM32F7xx/Source/Templates/system_stm32f7xx.c")

use_platform(stm32f777vi)
add_library(cmsis_stm32f777vi STATIC ${CMSIS_SOURCES})

target_include_directories(cmsis_stm32f777vi
    PUBLIC # These all need to be public since they
           # are all referenced in publicly-exposed headers
        ${HAL_DIR}/Inc
        "${CMAKE_CURRENT_LIST_DIR}/config"
        ${CMSIS_DIR}/Include
        ${CMSIS_DIR}/Device/ST/STM32F7xx/Include)

target_compile_definitions(cmsis_stm32f777vi
    PUBLIC
        USE_HAL_DRIVER
        STM32F777xx
        STM32F777VI
        ARM_MATH_CM7
        _USE_MATH_DEFINES)

#------------------ Startup -------------------

set(STARTUP_SOURCES "${CMSIS_DIR}/Device/ST/STM32F7xx/Source/Templates/gcc/startup_stm32f777xx.s")
use_platform(stm32f777vi)
add_library(startup_stm32f777vi STATIC ${STARTUP_SOURCES})

#------------------ HAL -------------------

# Get the HAL sources
file(GLOB HAL_SOURCES "${HAL_DIR}/Src/*.c")
file(GLOB HAL_HEADERS "${HAL_DIR}/Inc/*.h")

use_platform(stm32f777vi)

add_library(hal_stm32f777vi STATIC ${HAL_SOURCES} ${HAL_HEADERS})

target_include_directories(hal_stm32f777vi
    PUBLIC
        ${HAL_DIR}/Inc
        "${CMAKE_CURRENT_LIST_DIR}/config")

target_link_libraries(hal_stm32f777vi cmsis_stm32f777vi)

#------------------ FreeRTOS -------------------
set(FREE_RTOS_DIR "${CUBE_DIR}/Middlewares/Third_Party/FreeRTOS")
file(GLOB FREE_RTOS_CORE_SRCS "${FREE_RTOS_DIR}/Source/*.c")
#set(FREE_RTOS_CMSIS_SRCS "${FREE_RTOS_VER_DIR}/Source/CMSIS_RTOS/cmsis_os.c")
set(FREE_RTOS_ARM_SRCS "${FREE_RTOS_DIR}/Source/portable/GCC/ARM_CM7/r0p1/port.c")
set(FREE_RTOS_MEM_SRCS "${FREE_RTOS_DIR}/Source/portable/MemMang/heap_4.c")

file(GLOB FREE_RTOS_CORE_INCS "${FREE_RTOS_DIR}/Source/include/*.h")
file(GLOB FREE_RTOS_CMSIS_INCS "${FREE_RTOS_VER_DIR}/Source/CMSIS_RTOS/cmsis_os.h")

set(FREE_RTOS_SRCS ${FREE_RTOS_CORE_SRCS} ${FREE_RTOS_CMSIS_SRCS}
                   ${FREE_RTOS_ARM_SRCS} ${FREE_RTOS_MEM_SRCS})
set(FREE_RTOS_INCS ${FREE_RTOS_CORE_INCS} ${FREE_RTOS_CMSIS_INCS})

function(add_free_rtos TARGET_NAME PLATFORM)
    set(options APP)
    set(oneValueArgs TOTAL_HEAP_SIZE)
    set(multiValueArgs)
    cmake_parse_arguments(CONF "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    use_platform(${PLATFORM})
    add_library(${TARGET_NAME} STATIC ${FREE_RTOS_SRCS} ${FREE_RTOS_INCS})

    target_include_directories(${TARGET_NAME}
        PUBLIC
            "${FREE_RTOS_DIR}/Source/include"
            "${FREE_RTOS_DIR}/Source/CMSIS_RTOS"
            "${FREE_RTOS_DIR}/Source/portable/GCC/ARM_CM7/r0p1"
            "${CONFIG_DIR}")

    target_link_libraries(${TARGET_NAME} cmsis_stm32f777vi)

    if(CONF_TOTAL_HEAP_SIZE)
        target_compile_definitions(${TARGET_NAME}
            PUBLIC configTOTAL_HEAP_SIZE=${CONF_TOTAL_HEAP_SIZE})

    endif()
endfunction(add_free_rtos)

# Default free rtos
add_free_rtos(free_rtos_stm32f777vi stm32f777vi)

# ------------- FatFS ----------------
set(FATFS_DIR "${CUBE_DIR}/Middlewares/Third_Party/FatFs/src")

file(GLOB FATFS_C_SRCS "${FATFS_DIR}/*.c")
file(GLOB FATFS_INCS "${FATFS_DIR}/*.h")

use_platform(stm32f777vi)
add_library(fatfs_stm32f777vi STATIC ${FATFS_C_SRCS} ${FATFS_CPP_SRCS} ${FATFS_INCS})

target_include_directories(fatfs_stm32f777vi
        PUBLIC  "${FATFS_DIR}" "${CONFIG_DIR}")

target_link_libraries(fatfs_stm32f777vi free_rtos_stm32f777vi)

# --------------- Callback ---------------

add_library(callback INTERFACE)
target_include_directories(callback INTERFACE "${PLUSCALLBACK_DIR}")

# --------------- Catch ---------------

add_library(catch INTERFACE)
target_include_directories(catch INTERFACE "${CATCH_DIR}")
