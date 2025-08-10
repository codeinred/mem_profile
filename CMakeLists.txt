cmake_minimum_required(VERSION 3.23)

project(mem_profile)

if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/llvm)
endif()
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MEM_PROFILE_BUILD_TOOLS "Build tools" ON)


if(UNIX)
    add_compile_options(-Wall -fcolor-diagnostics -g)
endif()

include(cmake/get_cpm.cmake)

CPMAddPackage("gh:fmtlib/fmt#11.2.0")

find_package(Clang REQUIRED)
message(STATUS "Using Clang @ ${Clang_DIR}\n\tinclude: ${CLANG_INCLUDE_DIRS}\n\tconfig: ${Clang_CONFIG}\n\tCLANG_LINK_CLANG_DYLIB: ${CLANG_LINK_CLANG_DYLIB}")
message(STATUS "Using LLVM @ ${LLVM_DIR}\n\tconfig: ${LLVM_CONFIG}")

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
    NO_GLOB_HEADERS
    INCLUDE_DIRS
        ${CLANG_INCLUDE_DIRS}
    DEPS
        ${clang_libs}
)

mp_add_library(
    mp_types
    INTERFACE
    ROOT mp_types
)

mp_add_library(
    mp_core
    STATIC
    ROOT mp_core
    DEPS
        mp_types
    PRIVATE_DEPS
        fmt::fmt
)

mp_add_library(
    mp_error
    STATIC
    ROOT mp_error
    DEPS
        mp::mp_core
    PRIVATE_DEPS
        fmt::fmt
)

mp_add_library(
    mp_fs
    STATIC
    ROOT mp_fs
    DEPS
        mp::mp_error
    PRIVATE_DEPS
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
    INCLUDE_DIRS mp_hook_prelude/include
    PRELUDE_HEADERS mp_hook_prelude/include/mp_hook_prelude.h
)

mp_add_library(
    mp_unwind
    STATIC
    ROOT mp_unwind
    DEPS
        mp::mp_error
        mp::mp_core
        fmt::fmt
        mp::mp_hook_prelude
)

add_library(
    mp_build_with_plugin
    INTERFACE
)

target_compile_options(
    mp_build_with_plugin
    INTERFACE
    -fplugin=$<TARGET_FILE:mp::mp_plugin>
    -Xclang=-add-plugin
    -Xclang=mp_instrument_dtors
    $<INSTALL_INTERFACE:--include=$<INSTALL_PREFIX>/include/mp_hook_prelude.h>
)

mp_make_shared(
    mp_unwind_shared
    API_MODULES
        mp::mp_unwind
)



# Set the directory for mem_profileConfig based on the Config mode search procedure
# https://cmake.org/cmake/help/latest/command/find_package.html#config-mode-search-procedure
if(WIN32)
    set(CONFIG_DIR ${PROJECT_NAME})
else()
    set(CONFIG_DIR share/cmake/${PROJECT_NAME})
endif()

include(CMakePackageConfigHelpers)
configure_package_config_file(
    cmake/${PROJECT_NAME}Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CONFIG_DIR}
)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake
    DESTINATION ${CONFIG_DIR}
)

install(
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION ${CONFIG_DIR}
    NAMESPACE mp::
)

install(
    TARGETS
        mp_types
        mp_hook_prelude
        mp_unwind_shared
        mp_plugin
        mp_build_with_plugin
    EXPORT  ${PROJECT_NAME}Targets
    FILE_SET HEADERS
)


if(MEM_PROFILE_BUILD_TOOLS)
    add_executable(ast_printer tools/ast_printer.cpp)
    target_link_libraries(ast_printer mp_ast mp_fs fmt::fmt mp::clang_tooling)
endif()
