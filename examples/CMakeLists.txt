cmake_minimum_required(VERSION 3.20)

project(mem_profile_examples)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(mem_profile REQUIRED)
message("Found mem_profile @ ${mem_profile_CONFIG}")

file(GLOB examples src/*.cpp)

foreach(file IN LISTS examples)
    cmake_path(GET file STEM target_name)
    add_executable(
        ${target_name}
        ${file}
    )

    target_link_libraries(
        ${target_name}
        PRIVATE
        mp::mp_unwind_shared
        mp::mp_build_with_plugin
    )
    target_compile_options(
        ${target_name} PRIVATE -Og -g
    )
endforeach()
