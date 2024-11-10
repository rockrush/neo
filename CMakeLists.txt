cmake_minimum_required (VERSION 3.18)
project (neo)

include(CheckIncludeFiles)
include(CheckLibraryExists)

set (NEO_VERSION_MAJOR 0)
set (NEO_VERSION_MINOR 3)

set(NEO_CFG_FILE "etc/neo.cfg" CACHE FILEPATH "Config file path of Neo")
set(NEO_NEURON_DIR "/usr/libexec/neo" CACHE PATH "Path of Neuron plugins")

set(CMAKE_C_FLAGS "-D_GNU_SOURCE")
set(NEO_DEBUG OFF CACHE BOOL "Enable debug messages on neo")
if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NEO_DEBUG 1)
else()
	set(NEO_DEBUG 0)
endif()

check_include_files("libconfig.h" HAVE_LIBCONFIG_H)
#check_library_exists("config" config_read_file "" HAVE_LIBCONFIG_SO)
find_library(CONFIG_LIBRARY config "/usr")

configure_file (
	"${PROJECT_SOURCE_DIR}/include/config.h.in"
	"${PROJECT_BINARY_DIR}/include/config.h"
)

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")

add_executable(${PROJECT_NAME} main.c utils/linux.c utils/thread.c)
target_link_libraries(${PROJECT_NAME} dl)
target_link_libraries(${PROJECT_NAME} config)
file(GLOB sources src/*.c)
target_sources(${PROJECT_NAME} PRIVATE ${sources})
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-rdynamic")

add_subdirectory(neurons neurons)

install(TARGETS neo)
install(FILES res/samples.tar.xz DESTINATION share/neo/neurons)
