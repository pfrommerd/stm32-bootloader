cmake_minimum_required(VERSION 3.9)

function(PREPEND var prefix)
   SET(listVar "")
   FOREACH(f ${ARGN})
      LIST(APPEND listVar "${prefix}/${f}")
   ENDFOREACH(f)
   SET(${var} ${listVar} PARENT_SCOPE)
endfunction(PREPEND)

# Build the generation tool
add_custom_target(cdp-console
    COMMAND dotnet build "${CMAKE_SOURCE_DIR}/desktop/csharp/CDP Console Core")

function(add_per_generation NAME)
    set(options)
    set(oneValueArgs CONFIG_FILE PERDOS_FILE CAN_FILE)
    set(multiValueArgs GENERATED_TARGETS
                       GENERATED_SOURCES
                       GENERATED_INCLUDES
                       GENERATED_INCLUDE_DIRS
                       TARGETS_INCLUDE_DIRS
                       ALL_TARGETS_LINK_LIBS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(GEN_DIR "${CMAKE_BINARY_DIR}/generated/${NAME}")
    file(MAKE_DIRECTORY ${GEN_DIR})

    # Append generated sources to each
    # of the byproducts and the include dirs
    PREPEND(GENERATED_OUTPUTS ${GEN_DIR} ${ARG_GENERATED_SOURCES})
    PREPEND(GENERATED_INCLUDES ${GEN_DIR} ${ARG_GENERATED_INCLUDES})
    PREPEND(GENERATED_INCLUDE_DIRS ${GEN_DIR} ${ARG_GENERATED_INCLUDE_DIRS})

    # Create the command
    set(COMMAND dotnet run -p "${CMAKE_SOURCE_DIR}/desktop/csharp/CDP Console Core")
    set(DEPENDENCIES )
    if (ARG_CONFIG_FILE)
        set(COMMAND ${COMMAND} -config "${ARG_CONFIG_FILE}")
        set(DEPENDENCIES ${DEPENDENCIES} ${ARG_CONFIG_FILE})
    endif()
    if (ARG_PERDOS_FILE)
        set(COMMAND ${COMMAND} -existing-perdos "${ARG_PERDOS_FILE}")
        set(DEPENDENCIES ${DEPENDENCIES} ${ARG_PERDOS_FILE})
    endif()
    if (ARG_CAN_FILE)
        set(COMMAND ${COMMAND} -existing-can "${ARG_CAN_FILE}")
        set(DEPENDENCIES ${DEPENDENCIES} ${ARG_CAN_FILE})
    endif()
    #message(COMMAND "${COMMAND}")

    # Configure command to generate the files
    # including dependency on the config files
    add_custom_command(
        OUTPUT ${GENERATED_OUTPUTS} ${GENERATED_INCLUDES}
        COMMAND ${COMMAND}
        WORKING_DIRECTORY ${GEN_DIR}
        DEPENDS ${ARG_CONFIG_FILE} ${ARG_PERDOS_FILE} ${ARG_CAN_FILE})

    # Configure a common target for the file generation
    add_custom_target("${NAME}" DEPENDS ${GENERATED_OUTPUTS})

    list(LENGTH ARG_GENERATED_TARGETS NUM_GENERATED)
    math(EXPR GENERATED_RANGE "${NUM_GENERATED} - 1")

    foreach(idx RANGE ${GENERATED_RANGE})
        list(GET ARG_GENERATED_TARGETS ${idx} TARGET)
        list(GET ARG_TARGETS_INCLUDE_DIRS ${idx} TARGET_INCLUDE_DIR)
        list(GET GENERATED_OUTPUTS ${idx} GEN_SOURCE)
        list(GET GENERATED_INCLUDE_DIRS ${idx} GEN_INCLUDE_DIR)

        # Add a static library compile target corresponding to the
        # generated code for each generation target, complete
        # with include directories
        add_library("${TARGET}" STATIC ${GEN_SOURCE})

        target_include_directories("${TARGET}"
            PUBLIC ${GEN_INCLUDE_DIR} ${TARGET_INCLUDE_DIR})

        target_link_libraries("${TARGET}" ${ARG_ALL_TARGETS_LINK_LIBS})

        add_dependencies("${TARGET}" "${NAME}")
    endforeach()
endfunction(add_per_generation)
