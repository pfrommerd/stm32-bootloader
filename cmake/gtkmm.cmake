# Will setup gtkmm for use with per
find_package(PkgConfig  REQUIRED)
pkg_check_modules(GTKMM_PKG gtkmm-3.0 REQUIRED)

add_library(gtkmm INTERFACE)


# Link directories for all targets!
# bad style, but no other options
link_directories(${GTKMM_PKG_LIBRARY_DIRS})

target_link_libraries(gtkmm INTERFACE ${GTKMM_PKG_LIBRARIES})
target_include_directories(gtkmm INTERFACE ${GTKMM_PKG_INCLUDE_DIRS})
