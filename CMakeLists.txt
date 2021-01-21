cmake_minimum_required (VERSION 3.18)
project (neo)

set (NEO_VERSION_MAJOR 0)
set (NEO_VERSION_MINOR 2)

set(NEO_CFG_FILE "etc/neo.json" CACHE FILEPATH "Config file path of Neo")

set(NEO_DEBUG OFF CACHE BOOL "Enable debug messages on neo")
if(CMAKE_BUILD_TYPE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
	set(NEO_DEBUG ON)
endif()
if(NEO_DEBUG)
	set(NEO_DEBUG 1)
else()
	set(NEO_DEBUG 0)
endif()

configure_file (
	"${PROJECT_BINARY_DIR}/include/config.h.in"
	"${PROJECT_BINARY_DIR}/include/config.h"
)

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")

add_executable(${PROJECT_NAME} neo.c utils/linux.c)
target_link_libraries(${PROJECT_NAME} dl)
target_link_libraries(${PROJECT_NAME} json-c)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-rdynamic")

add_subdirectory(neurons neurons)
