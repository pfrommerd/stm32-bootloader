cmake_minimum_required(VERSION 3.9)

if (WIN32)
    find_program(JLINK_EXE NAMES "JLinkGDBServerCL" "JLinkGDBServerCLExe"
                           PATHS "C:\\Program Files (x86)\\SEGGER\\JLink_V640")
else()
    find_program(JLINK_EXE NAMES "JLinkGDBServerCL" "JLinkGDBServerCLExe")
endif()

if ("${JLINK_EXE}" STREQUAL "JLINK_EXE-NOTFOUND")
    message("WARNING: Unable to find jlink upload command, jlink upload is disabled")
    message("To enable, please set the CMAKE_PROGRAM_PATH environment variable")
endif()

function(add_jlink_upload TARGET_NAME DEVICE)
    add_custom_target(${TARGET_NAME}-server
	    python3 ${CMAKE_SOURCE_DIR}/tools/gdbserver.py -e ${JLINK_EXE}  -d ${DEVICE}
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/gdbinit.py $<TARGET_FILE:${TARGET_NAME}>
	    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

    add_custom_target(${TARGET_NAME}-debug
	    COMMAND arm-none-eabi-gdb
	    -ex "target remote tcp:localhost:2331"
	    DEPENDS ${TARGET_NAME} ${TARGET_NAME}-server)

    add_custom_target(${TARGET_NAME}-upload
        COMMAND arm-none-eabi-gdb
            -ex "set confirm off"
	    -ex "target remote tcp:localhost:2331"
	    -ex "monitor reset"
	    -ex "monitor device ${DEVICE}"
	    -ex "load \"$<TARGET_FILE:${TARGET_NAME}>\""
	    -ex "q"
        DEPENDS ${TARGET_NAME} ${TARGET_NAME}-server)
endfunction(add_jlink_upload)
