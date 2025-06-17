config:
    cmake -B build -G Ninja

build: config
    cmake --build build

install: build
    cmake --install build --prefix install

run exe *args: config
    cmake --build build --target {{exe}}
    build/{{exe}} {{args}}

test_plugin: build
    rm -rf ./a.out
    dsymutil build/libmp_plugin.dylib
    /opt/homebrew/opt/llvm/bin/clang++ \
        test_files/test.cpp -O3 \
        -L build -rpath build -l mp_unwind_shared \
        -Imp_types/include \
        -Imp_unwind/include \
        -fplugin=build/libmp_plugin.dylib \
        -Xclang=-add-plugin \
        --include=mp_hook_prelude/include/mp_hook_prelude.h \
        -Xclang=mp_instrument_dtors -g -o build/file_with_plugin
    build/file_with_plugin

ast_dump *args:
    /opt/homebrew/opt/llvm/bin/clang++ -Xclang=-ast-dump {{args}}

clang-tidy *args:
    /opt/homebrew/opt/llvm/bin/clang-tidy {{args}}

build_example: install
    cmake -S examples \
        -B examples/build \
        -DCMAKE_PREFIX_PATH={{justfile_directory()}}/install \
        -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
        -DCMAKE_C_COMPILER=/opt/homebrew/opt/llvm/bin/clang
    cmake --build examples/build

run_example example: build_example
    examples/build/{{example}}

clean:
    rm -rf build install examples/build
