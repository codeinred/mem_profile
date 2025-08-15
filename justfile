clang_bin := if os() == "macos" {
    if path_exists("/opt/homebrew/opt/llvm/bin") == "true" {
        "/opt/homebrew/opt/llvm/bin/"
    } else {
        error("Could not find path to LLVM Clang. Please specify when running `just`: eg, `just clang_bin=/opt/homebrew/opt/llvm/bin <target>`")
    }
} else {
    ""
}

clang_exe := join(clang_bin, 'clang')
clang_tidy := join(clang_bin, 'clang-tidy')

clang_cc := clang_exe
clang_cxx := clang_exe + '++'

install_dir := absolute_path("./install")

cwd := justfile_directory()


clang_path:
    @echo "clang_cc={{clang_cc}}"
    @echo "clang_cxx={{clang_cxx}}"

config:
    cmake -B build -G Ninja

build: config
    cmake --build build

install: build
    cmake --install build --prefix {{install_dir}}

run exe *args: config
    cmake --build build --target {{exe}}
    build/{{exe}} {{args}}

test_plugin: build
    rm -rf ./a.out
    {{clang_cxx}} \
        test_files/test.cpp -O3 \
        -L build -rpath build -l mp_unwind_shared \
        -Imp_types/include \
        -Imp_unwind/include \
        --include={{cwd}}/mp_hook_prelude/include/mp_hook_prelude.h \
        -fplugin=build/libmp_plugin.dylib \
        -Xclang=-add-plugin \
        -Xclang=mp_instrument_dtors -g -o build/file_with_plugin
    build/file_with_plugin

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

run_example example: build_example
    examples/build/{{example}}

clean:
    rm -rf build install examples/build
