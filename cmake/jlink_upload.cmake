cmake_minimum_required(VERSION 3.9)

macro(subdirlist result curdir)
	FILE(GLOB children RELATIVE "${curdir}" "${curdir}/*")
	SET(dirlist "")
	foreach(child ${children})
		if(IS_DIRECTORY "${curdir}/${child}")
			LIST(APPEND dirlist "${curdir}/${child}")
		endif()
	endforeach()
	SET(${result} ${dirlist})
endmacro()

if (WIN32)
	subdirlist(JLINK_DIRS "C:/Program Files (x86)/SEGGER")
    find_program(JLINK_EXE NAMES "JLinkGDBServerCL" "JLinkGDBServerCLExe"
                           PATHS ${JLINK_DIRS})
else()
    find_program(JLINK_EXE NAMES "JLinkGDBServerCL" "JLinkGDBServerCLExe")
endif()

if ("${JLINK_EXE}" STREQUAL "JLINK_EXE-NOTFOUND")
    message("WARNING: Unable to find jlink upload command, jlink upload is disabled")
    message("To enable, please set the CMAKE_PROGRAM_PATH environment variable")
endif()

set(TOOLS_DIR "${CMAKE_CURRENT_LIST_DIR}/tools")
function(add_jlink_upload TARGET_NAME DEVICE)
    add_custom_target(${TARGET_NAME}-server
        python3 ${TOOLS_DIR}/gdbserver.py -e ${JLINK_EXE}  -d ${DEVICE}
        COMMAND python3 ${TOOLS_DIR}/gdbinit.py $<TARGET_FILE:${TARGET_NAME}>
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
