cmake_minimum_required (VERSION 3.10)
project (neo)

set (NEO_VERSION_MAJOR 0)
set (NEO_VERSION_MINOR 1)

configure_file (
	"${PROJECT_SOURCE_DIR}/include/config.h.in"
	"${PROJECT_BINARY_DIR}/include/config.h"
)

include_directories("${PROJECT_BINARY_DIR}/include" "${PROJECT_SOURCE_DIR}/include")

add_executable(neo src/neo.c src/neuron.c src/pulse.c)
