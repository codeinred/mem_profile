cmake_minimum_required(VERSION 3.15)

project(plugin_example)

if(NOT DEFINED PLUGIN_PATH)
    message(FATAL_ERROR "PLUGIN_PATH must be defined")
endif()

if(NOT DEFINED PLUGIN_NAME)
    message(FATAL_ERROR "PLUGIN_NAME must be defined")
endif()

if(APPLE)
    set(CMAKE_C_COMPILER /opt/homebrew/opt/llvm/bin/clang)
    set(CMAKE_CXX_COMPILER /opt/homebrew/opt/llvm/bin/clang++)
else()
    set(CMAKE_C_COMPILER clang)
    set(CMAKE_CXX_COMPILER clang++)
endif()

set(plugin_args
    -Xclang=-load
    -Xclang=${PLUGIN_PATH}
    -Xclang=-add-plugin
    -Xclang=${PLUGIN_NAME})
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:${plugin_args}>"
)
set(CMAKE_CXX_STANDARD 17)

add_executable(main src/main.cpp)
