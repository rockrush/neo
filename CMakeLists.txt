cmake_minimum_required (VERSION 3.18)
project (neo)

set (NEO_VERSION_MAJOR 0)
set (NEO_VERSION_MINOR 2)

#option (NEURON_DIR "Demo" "neurons")
set(NEURON_DIR "neurons" CACHE FILEPATH "Demo")

configure_file (
	"${PROJECT_BINARY_DIR}/include/config.h.in"
	"${PROJECT_BINARY_DIR}/include/config.h"
)

include_directories("${PROJECT_SOURCE_DIR}/include" "${PROJECT_BINARY_DIR}/include")

add_executable(${PROJECT_NAME} neo.c)
target_link_libraries(${PROJECT_NAME} dl)
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-rdynamic")

add_subdirectory(neurons neurons)
