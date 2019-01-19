cmake_minimum_required(VERSION 3.9)

set(PLATFORM_DEFAULT_TOOLCHAIN gcc-arm-none-eabi)

# Parse the options for the stm32 platform
# to see whether this should be built as an app
# or as a standalone
set(options APP)
set(oneValueArgs)
set(multiValueArgs)
cmake_parse_arguments(USE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

# Common options
set(CPU "-mcpu=cortex-m7 -march=armv7e-m")
set(FPU "-mfpu=fpv5-d16")
set(FLOAT_ABI "-mfloat-abi=hard")
set(MCU "${CPU} -mthumb -mthumb-interwork ${FPU} ${FLOAT_ABI}")

set(COMMON_FLAGS "${MCU} -O0 -Wall -fdata-sections -ffunction-sections")
set(COMMON_FLAGS "${COMMON_FLAGS}")

# Export the linking flags
# to disable the system link flags

if (USE_APP)
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/extern/config/stm32f777vi_app.lds")
else (USE_APP)
    set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/extern/config/stm32f777vi.lds")
endif (USE_APP)

set(LINK_LIB_FLAGS "-lc -lm -lnosys")
set(LINK_FLAGS "-specs=nosys.specs ${MCU} ${LINK_LIB_FLAGS} -T\"${LINKER_SCRIPT}\" -Wl,-Map=info.map,--cref -Wl,--gc-sections")

# Export what will actually be used
# for the flags

set(PLATFORM_C_FLAGS "${COMMON_FLAGS}")
set(PLATFORM_CXX_FLAGS "${COMMON_FLAGS}")
set(PLATFORM_ASM_FLAGS "${COMMON_FLAGS}")

set(PLATFORM_C_LINK_FLAGS "${LINK_FLAGS}")
set(PLATFORM_CXX_LINK_FLAGS "${LINK_FLAGS}")
set(PLATFORM_ASM_LINK_FLAGS "${LINK_FLAGS}")