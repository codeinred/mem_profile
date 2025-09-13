cmake_minimum_required(VERSION 3.20)

project(mem_profile_examples)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mem_profile REQUIRED)
link_libraries(mp::mp_build_with_plugin)

include(cmake/get_cpm.cmake)

CPMAddPackage("gh:codeinred/tuplet@2.1.1")

file(GLOB examples src/*.cpp)

add_compile_options(-Og -g)

foreach(file IN LISTS examples)
    cmake_path(GET file STEM target_name)
    add_executable(${target_name} ${file})
endforeach()

target_link_libraries(tuplet_demo tuplet::tuplet)
