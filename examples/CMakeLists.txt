cmake_minimum_required(VERSION 3.20)

project(mem_profile_examples)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

find_package(mem_profile REQUIRED)
link_libraries(mp::mp_build_with_plugin)

include(cmake/get_cpm.cmake)

CPMAddPackage("gh:codeinred/tuplet@2.1.1")

file(GLOB examples src/*.cpp)

add_compile_options($<$<CONFIG:Debug>:-Og> -g)

foreach(file IN LISTS examples)
    cmake_path(GET file STEM target_name)
    add_executable(${target_name} ${file})
    target_include_directories(${target_name} PRIVATE include/)
endforeach()

file(GLOB test_files test_files/*.cpp)
foreach(file IN LISTS test_files)
    cmake_path(GET file STEM target_name)
    set(target_name "test_${target_name}")
    add_executable(${target_name} ${file})
    target_include_directories(${target_name} PRIVATE include/)
endforeach()

target_link_libraries(tuplet_demo tuplet::tuplet)
