cmake_minimum_required(VERSION 3.31)

project(mem_profile_clang_plugin)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/llvm)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

if(UNIX)
    add_compile_options(-Wall -fcolor-diagnostics -g)
endif()

include(cmake/get_cpm.cmake)

CPMAddPackage("gh:fmtlib/fmt#11.2.0")

find_package(Clang REQUIRED)
message(STATUS "Using Clang @ ${Clang_DIR} include=${CLANG_INCLUDE_DIRS})")

include(cmake/Utils.cmake)

set(clang_libs
    clangAST
    clangBasic
    clangFrontend
    clangSerialization
    clangTooling
)
check_targets_exist(TARGETS ${clang_libs})

mp_add_library(
    clang_core
    INTERFACE
    INCLUDE_DIRS
        ${CLANG_INCLUDE_DIRS}
    DEPS
        ${clang_libs}
)

mp_add_library(
    mp_core
    STATIC
    ROOT mp_core
    DEPS fmt::fmt
)

mp_add_library(
    mp_error
    STATIC
    ROOT mp_error
    DEPS
        fmt::fmt
        mp::mp_core
)

mp_add_library(
    mp_fs
    STATIC
    ROOT mp_fs
    DEPS
        mp::mp_error
        fmt::fmt
)

mp_add_library(
    mp_ast
    STATIC
    ROOT mp_ast
    DEPS
        mp::mp_error
        fmt::fmt
        mp::clang_core
)

mp_add_library(
    mp_plugin
    SHARED
    SRC_FILES src/plugin/plugin.cpp
    PRIVATE_DEPS
        mp::mp_ast
)

mp_add_library(
    mp_hook_prelude
    INTERFACE
    COMPILE_OPTIONS --include=${CMAKE_CURRENT_SOURCE_DIR}/mp_hook_prelude/include/mp_hook_prelude.h
)


add_executable(ast_printer tools/ast_printer.cpp)
target_link_libraries(ast_printer mp_ast mp_fs fmt::fmt)
