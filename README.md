# mem_profile: an RAII-based memory profiler

Build with `cmake`:

```sh
cmake -B build -G Ninja
cmake --build build
cmake --install build --target ./install
```

You may need to provide a `CMAKE_PREFIX_PATH` to clang if cmake cannot find
clang when building.

To use the profiler, build your executable with the clang plugin (included as
part of the installation), and then load the profiler with `LD_PRELOAD` or
`DYLD_INSERT_LIBRARIES`:

Building with the plugin:

```cmake
find_package(mem_profile REQUIRED)
link_libraries(mp::mp_build_with_plugin)
```

Running with the profiler:

```sh
# linux
env LD_PRELOAD=/usr/local/lib/libmp_runtime.so build/shared

# macos
env DYLD_INSERT_LIBRARIES=/usr/local/lib/libmp_runtime.dylib build/shared
```

This will output `malloc_stats.json`, which you can analyze with `mp_reader`,
included in the `stats_reader/` directory. Use
`uv run python -m mp_reader <subcommand>` to use it.

If you're here from CppCon, check back later today or tomorrow for more
information!
