#pragma once

#include <mem_profile/prelude.h>

/// There are three ways our hooks can reach the underlying allocator:
///
/// - glibc: call the "true" names of the functions (__libc_malloc etc.)
///   directly.
/// - macOS: interposition happens through dyld's __DATA,__interpose section
///   (see mem_profile.cpp), which only rewrites bindings in *other* images.
///   Calls made from this image therefore reach the real libSystem
///   implementations directly, and the plain names can be used. memalign
///   does not exist on macOS, so it is emulated with posix_memalign.
/// - otherwise: fall back to a hook table which loads the functions from the
///   C library with dlsym(RTLD_NEXT).
///
/// Calling, eg ALLOC_HOOK_TABLE.get_malloc() will:
/// - check if malloc has been loaded into the hook table,
/// - load malloc from the OS's C standard library if it hasn't been loaded,
/// - And then return a pointer to malloc

#if __GLIBC__ >= 2
extern "C" void* __libc_malloc(size_t);
extern "C" void* __libc_realloc(void*, size_t);
extern "C" void* __libc_memalign(size_t alignment, size_t size);
extern "C" void __libc_free(void* ptr);
extern "C" void* __libc_calloc(size_t n_members, size_t size);

#define mperf_malloc __libc_malloc
#define mperf_realloc __libc_realloc
#define mperf_memalign __libc_memalign
#define mperf_free __libc_free
#define mperf_calloc __libc_calloc
#elif defined(__APPLE__)
#include <cstdlib>

namespace mp {
/// memalign emulation: posix_memalign requires the alignment to be at least
/// sizeof(void*), while callers (eg aligned operator new) may pass smaller
/// extended alignments.
inline void* darwin_memalign(size_t alignment, size_t size) {
    if (alignment < sizeof(void*)) {
        alignment = sizeof(void*);
    }
    void* result = nullptr;
    if (::posix_memalign(&result, alignment, size) != 0) {
        return nullptr;
    }
    return result;
}
} // namespace mp

#define mperf_malloc ::malloc
#define mperf_realloc ::realloc
#define mperf_memalign ::mp::darwin_memalign
#define mperf_free ::free
#define mperf_calloc ::calloc
#else
#define mperf_malloc ::mp::ALLOC_HOOK_TABLE.get_malloc()
#define mperf_realloc ::mp::ALLOC_HOOK_TABLE.get_realloc()
#define mperf_memalign ::mp::ALLOC_HOOK_TABLE.get_memalign()
#define mperf_free ::mp::ALLOC_HOOK_TABLE.get_free()
#define mperf_calloc ::mp::ALLOC_HOOK_TABLE.get_calloc()
#endif
