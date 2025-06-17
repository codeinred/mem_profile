config:
    cmake -B build -G Ninja

build: config
    cmake --build build

run exe *args: config
    cmake --build build --target {{exe}}
    build/{{exe}} {{args}}

test_plugin: build
    rm -rf ./a.out
    dsymutil build/libmp_plugin.dylib
    /opt/homebrew/opt/llvm/bin/clang++ \
        test_files/test.cpp -O3 \
        -fplugin=build/libmp_plugin.dylib \
        -Xclang=-add-plugin \
        --include=mp_hook_prelude/include/mp_hook_prelude.h \
        -Xclang=print_fns -g -o build/file_with_plugin
    build/file_with_plugin

ast_dump *args:
    /opt/homebrew/opt/llvm/bin/clang++ -Xclang=-ast-dump {{args}}

clang-tidy *args:
    /opt/homebrew/opt/llvm/bin/clang-tidy {{args}}
