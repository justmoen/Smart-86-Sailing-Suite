# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5..3.27)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXTRA_COMPONENT_DIRS
    ./components/bsp_extra
    )

add_compile_options(-Wno-format)
project(sailing_suite VERSION 0.7.3)
add_compile_definitions(FIRMWARE_VERSION=\"${PROJECT_VERSION}\")
