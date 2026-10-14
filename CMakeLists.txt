cmake_minimum_required (VERSION 3.20)
project (neo)

set (NEO_VERSION_MAJOR 0)
set (NEO_VERSION_MINOR 4)

set(NEO_CFG_FILE "/etc/${PROJECT_NAME}.cfg" CACHE FILEPATH "Config file path of ${PROJECT_NAME}")
set(NEO_NEURON_DIR "/usr/libexec/${PROJECT_NAME}" CACHE PATH "Path of Neuron plugins")

set(CMAKE_C_FLAGS "-D_GNU_SOURCE")
set(NEO_DEBUG OFF CACHE BOOL "Enable debug messages on ${PROJECT_NAME}")
if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NEO_DEBUG 1)
else()
	set(NEO_DEBUG 0)
endif()

include(GNUInstallDirs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(cyaml REQUIRED libfyaml)

configure_file (
	"${PROJECT_SOURCE_DIR}/include/${PROJECT_NAME}.h.in"
	"${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}.h"
)

include_directories(
	"${PROJECT_SOURCE_DIR}/include"
	"${PROJECT_BINARY_DIR}/include"
	"${PROJECT_SOURCE_DIR}/ext/clog"
)

file(GLOB sources main.c src/*.c platform/*.c)
add_executable(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE ${sources})
target_link_libraries(${PROJECT_NAME} dl fyaml)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-rdynamic")

add_subdirectory(neurons neurons)

install(TARGETS ${PROJECT_NAME})
install(FILES "${PROJECT_BINARY_DIR}/include/${PROJECT_NAME}.h" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES res/samples.tar.xz DESTINATION share/${PROJECT_NAME}/neurons)
