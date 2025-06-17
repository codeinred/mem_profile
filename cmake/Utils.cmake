function(check_targets_exist)
    cmake_parse_arguments(
        PARSE_ARGV
        0
        arg
        ""
        ""
        "TARGETS"
    )

    foreach(target IN LISTS arg_TARGETS)
        if(NOT TARGET ${target})
            message(FATAL_ERROR "Cannot find target ${target}")
        endif()
    endforeach()
endfunction()


function(mp_add_library name kind)
    cmake_parse_arguments(
        PARSE_ARGV
        2
        arg
        ""
        "ROOT;NAMESPACE"
        "SRC_DIRS;SRC_FILES;DEPS;PRIVATE_DEPS;INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS;COMPILE_OPTIONS;PRIVATE_COMPILE_OPTIONS"
    )
    set(source_files)
    add_library(${name} ${kind})

    set(public_kw "PUBLIC")
    if(kind STREQUAL "INTERFACE")
        set(public_kw "INTERFACE")
    endif()

    if(NOT DEFINED arg_NAMESPACE)
        set(arg_NAMESPACE "mp")
    endif()

    add_library(${arg_NAMESPACE}::${name} ALIAS ${name})

    if(DEFINED arg_ROOT)
        file(GLOB_RECURSE files ${arg_ROOT}/include/*.cpp)
        list(APPEND source_files ${files})
        target_include_directories(${name} ${public_kw} ${arg_ROOT}/include)
    endif()

    if(DEFINED arg_SRC_DIRS)
        foreach(dir IN LISTS arg_SRC_DIRS)
            file(GLOB_RECURSE files ${dir}/*.cpp)
            list(APPEND source_files ${files})
        endforeach()
    endif()

    if(DEFINED arg_SRC_FILES)
        list(APPEND source_files ${arg_SRC_FILES})
    endif()

    if(source_files)
        if(kind STREQUAL "INTERFACE")
            message(FATAL_ERROR "Error: Library ${name} was registered as an INTERFACE, however it was given source files ${source_files}")
        endif()
        target_sources(${name} PRIVATE ${source_files})
    endif()

    if(DEFINED arg_DEPS)
        target_link_libraries(${name} ${public_kw} ${arg_DEPS})
    endif()
    if(DEFINED arg_PRIVATE_DEPS)
        target_link_libraries(${name} PRIVATE ${arg_PRIVATE_DEPS})
    endif()
    if(DEFINED arg_INCLUDE_DIRS)
        target_include_directories(${name} ${public_kw} ${arg_INCLUDE_DIRS})
    endif()
    if(DEFINED arg_PRIVATE_INCLUDE_DIRS)
        target_include_directories(${name} PRIVATE ${arg_PRIVATE_INCLUDE_DIRS})
    endif()
    if(DEFINED arg_COMPILE_OPTIONS)
        target_compile_options(${name} ${public_kw} ${arg_COMPILE_OPTIONS})
    endif()
    if(DEFINED arg_PRIVATE_COMPILE_OPTIONS)
        target_compile_options(${name} PRIVATE ${arg_PRIVATE_COMPILE_OPTIONS})
    endif()
endfunction()
