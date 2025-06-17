# Debugging

This document contains Debugging information for common problems related to the
plugin

## Linker Errors

If you include large numbers of linker errors when building the plugin, the most
likely cause is ABI incompatibility between libclang and the plugin.

Make sure that you are using the _same compiler used to build libclang_ when
building the plugin.

For example, if using the homebrew llvm package on macos, the plugin should be
built with AppleClang, because homebrew llvm is built with AppleClang.

If you try to use Homebrew's llvm to build the package, you will get linker
errors!
