cmake_minimum_required(VERSION 3.5)
add_compile_options(-fdiagnostics-color=always)

#set(EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components)

set(COMPONENTS "main")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iperf_runner_example)
