cmake_minimum_required(VERSION 3.9)

set(EXTERN_DIR "${CMAKE_CURRENT_LIST_DIR}/extern")

set(CUBE_URL "https://www.st.com/content/ccc/resource/technical/software/firmware/18/40/6e/b0/44/f7/43/1d/stm32cubef7.zip/files/stm32cubef7.zip/jcr:content/translations/en.stm32cubef7.zip")
set(CUBE_CHECKSUM "283d7b857c7126f9b625fa6ceb7612dd15fdd43528dbe886bc607668e7486786")
set(CUBE_FILE "${EXTERN_DIR}/stm32cube.zip")
set(CUBE_DIR "${EXTERN_DIR}/STM32Cube_FW_F7_V1.14.0/")

# Download stm32 cube
file(MAKE_DIRECTORY "${EXTERN_DIR}")
download("STM32 Cube" ${CUBE_URL} ${CUBE_CHECKSUM} ${CUBE_FILE})
unzip("STM32 Cube" ${CUBE_FILE} ${EXTERN_DIR} ${CUBE_DIR})


# Setup CMSIS

# patch the SystemInit() and linker scripts
# for the bootloader and the app
set(CMSIS_DIR "${CUBE_DIR}/Drivers/CMSIS/Deivce/ST/STM32F7xx/Source/Templates")
set(CMSIS_SRC "${CMSIS_DIR}/system_stm32f7xx.c")

use_platform(stm32f777)
