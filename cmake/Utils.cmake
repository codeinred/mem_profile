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
        "NO_GLOB_HEADERS"
        "ROOT;NAMESPACE"
        "SRC_DIRS;SRC_FILES;DEPS;PRIVATE_DEPS;INCLUDE_DIRS;PRIVATE_INCLUDE_DIRS;COMPILE_OPTIONS;COMPILE_FEATURES;PRIVATE_COMPILE_FEATURES;PRIVATE_COMPILE_OPTIONS;PRELUDE_HEADERS"
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

    set(api_include_dirs)

    if(DEFINED arg_ROOT)
        file(GLOB_RECURSE files ${arg_ROOT}/include/*.cpp)
        list(APPEND source_files ${files})
        list(APPEND api_include_dirs "${arg_ROOT}/include")
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
        list(APPEND api_include_dirs ${arg_INCLUDE_DIRS})
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
    if(DEFINED arg_COMPILE_FEATURES)
        target_compile_features(${name} ${public_kw} ${arg_COMPILE_FEATURES})
    endif()
    if(DEFINED arg_PRIVATE_COMPILE_FEATURES)
        target_compile_features(${name} PRIVATE ${arg_PRIVATE_COMPILE_FEATURES})
    endif()
    if(DEFINED arg_PRELUDE_HEADERS)
        foreach(path IN LISTS arg_PRELUDE_HEADERS)
            cmake_path(ABSOLUTE_PATH path NORMALIZE)
            target_compile_options(${name} ${public_kw} "--include=${path}")
        endforeach()
    endif()

    if(arg_NO_GLOB_HEADERS)
        target_include_directories(${name} ${public_kw} ${api_include_dirs})
    else()
        list(TRANSFORM api_include_dirs APPEND "/*.h" OUTPUT_VARIABLE header_glob)
        file(GLOB_RECURSE headers LIST_DIRECTORIES false ${header_glob})
        message("mp_add_library: lib=${name} header_glob=${header_glob} headers=${headers}")

        target_sources(
            ${name}
            ${public_kw}
            FILE_SET HEADERS
            BASE_DIRS
                ${api_include_dirs}
            FILES
                ${headers}
                ${arg_PRELUDE_HEADERS}
        )
    endif()
endfunction()

file(
    GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/mp_empty.cpp"
    CONTENT "// This file is used as a stub because `add_library` requires sources
"
)
# Build a shared library out of constituent static libraries
function(mp_make_shared name)
    cmake_parse_arguments(
        PARSE_ARGV
        1
        arg
        ""
        ""
        "API_MODULES"
    )

    add_library(${name} SHARED ${CMAKE_CURRENT_BINARY_DIR}/mp_empty.cpp)
    foreach(lib IN LISTS arg_API_MODULES)
        get_target_property(lib_type ${lib} TYPE)
        get_target_property(lib_include_dirs ${lib} INTERFACE_INCLUDE_DIRECTORIES)
        get_target_property(lib_headers ${lib} HEADER_SET)
        get_target_property(lib_dirs ${lib} HEADER_DIRS)

        message("Adding the following headers to ${name}: ${lib_headers}")

        target_sources(
            ${name}
            PUBLIC
            FILE_SET HEADERS
            BASE_DIRS ${lib_dirs}
            FILES ${lib_headers}
        )
        if(lib_type STREQUAL "STATIC_LIBRARY")
            target_link_libraries(${name} PRIVATE $<LINK_LIBRARY:WHOLE_ARCHIVE,${lib}>)
        elseif(NOT(lib_type STREQUAL "INTERFACE"))
            message("Cannot add library '${lib}' with type ${lib_type} as API module for ${name}")
        endif()
    endforeach()
endfunction()
