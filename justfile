clang_bin := if os() == "macos" {
    if path_exists("/opt/homebrew/opt/llvm/bin") == "true" {
        "/opt/homebrew/opt/llvm/bin/"
    } else {
        error("Could not find path to LLVM Clang. Please specify when running `just`: eg, `just clang_bin=/opt/homebrew/opt/llvm/bin <target>`")
    }
} else {
    ""
}

ld_preload := if os() == "macos" {
    "DYLD_INSERT_LIBRARIES"
} else {
    "LD_PRELOAD"
}

dsymutil_tgt := if os() == "macos" {
    "_dsymutil"
} else {
    "_noop"
}

lib_ext := if os() == "macos" { "dylib" } else { "so" }

clang_exe := join(clang_bin, 'clang')
clang_tidy := join(clang_bin, 'clang-tidy')

clang_cc := clang_exe
clang_cxx := clang_exe + '++'

install_dir := absolute_path("./install")

cwd := justfile_directory()

_noop *args:



_dsymutil *args:
    dsymutil {{args}}


clang_path:
    @echo "clang_cc={{clang_cc}}"
    @echo "clang_cxx={{clang_cxx}}"

config:
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

build: config
    cmake --build build

install: build
    cmake --install build --prefix {{install_dir}}

run exe *args: config
    cmake --build build --target {{exe}}
    build/{{exe}} {{args}}

build_test_file test_file: build
    mkdir -p build/test_files
    {{clang_cxx}} \
        -L build -rpath build -l mp_unwind_shared \
        -Imp/types/include \
        -Imp/unwind/include \
        -Imp/core/include \
        -Imp/hook_prelude/include \
        --include={{cwd}}/mp/hook_prelude/include/mp_hook_prelude.h \
        -fplugin=build/libmp_plugin.{{lib_ext}} \
        -Xclang=-add-plugin \
        -Xclang=mp_instrument_dtors \
        -Og -g \
        -o build/test_files/{{trim_end_match(test_file, ".cpp")}} \
        test_files/{{test_file}}

ast_dump *args:
    {{clang_cxx}} -Xclang=-ast-dump -c {{args}}

clang-tidy *args:
    {{clang_tidy}} {{args}}

build_example: install
    cmake -S examples \
        -B examples/build \
        -DCMAKE_PREFIX_PATH={{install_dir}} \
        -DCMAKE_CXX_COMPILER={{clang_cxx}} \
        -DCMAKE_C_COMPILER={{clang_cc}} \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build examples/build
    @just {{dsymutil_tgt}} build/libmp_runtime.{{lib_ext}}

build_ex_target example_name: install
    cmake -S examples \
        -B examples/build \
        -DCMAKE_PREFIX_PATH={{install_dir}} \
        -DCMAKE_CXX_COMPILER={{clang_cxx}} \
        -DCMAKE_C_COMPILER={{clang_cc}} \
        -DCMAKE_BUILD_TYPE=Debug
    cmake --build examples/build --target=clean
    cmake --build examples/build --target={{example_name}}
    @just {{dsymutil_tgt}} build/libmp_runtime.{{lib_ext}}

mp_run *args:
    env {{ld_preload}}=build/libmp_runtime.{{lib_ext}} {{args}}

_gen_test_file prog:
    @just {{dsymutil_tgt}} examples/build/{{prog}}
    @just mp_run MEM_PROFILE_OUT=stats_reader/test_files/{{os()}}/{{prog}}.json examples/build/{{prog}}

gen_test_files: build_example
    mkdir -p stats_reader/test_files/{{os()}}
    @just {{dsymutil_tgt}} build/libmp_runtime.{{lib_ext}}
    @just _gen_test_file simple_alloc
    @just _gen_test_file simple_nested_objects
    @just _gen_test_file single_alloc
    @just _gen_test_file string

run_example example: build_example
    examples/build/{{example}}

analyze_stats input_file:
    uv run --project stats_reader print-objects {{input_file}}

clean:
    rm -rf build install examples/build
