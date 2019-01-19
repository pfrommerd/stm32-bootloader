cmake_minimum_required(VERSION 3.9)

# The actual compilers/linkers to be used
set(TOOLCHAIN_C_COMPILER arm-none-eabi-gcc)
set(TOOLCHAIN_CXX_COMPILER arm-none-eabi-g++)
set(TOOLCHAIN_ASM_COMPILER arm-none-eabi-gcc)

set(TOOLCHAIN_C_LINK_CMD "<CMAKE_C_COMPILER> <FLAGS> <CMAKE_C_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(TOOLCHAIN_CXX_LINK_CMD "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")
set(TOOLCHAIN_ASM_LINK_CMD "<CMAKE_ASM_COMPILER> <FLAGS> <CMAKE_ASM_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

# For the static library generation
# to use the proper ranlib/ar versions
set(TOOLCHAIN_AR arm-none-eabi-ar)
set(TOOLCHAIN_RANLIB arm-none-eabi-ranlib)