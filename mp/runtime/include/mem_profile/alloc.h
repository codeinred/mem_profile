#pragma once

#include <mem_profile/prelude.h>

/// There are two ways we can call the underlying version of malloc
/// from our hook. The first way is to use the "true" name of the
/// function, aka __libc_malloc (for glibc). On some platforms,
/// such as Mac, this isn't possible.
///
/// In that case, we defer to using a hook table which uses
/// dlsym to load the function from the c library implementation
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
#else
#define mperf_malloc ::mem_profile::ALLOC_HOOK_TABLE.get_malloc()
#define mperf_realloc ::mem_profile::ALLOC_HOOK_TABLE.get_realloc()
#define mperf_memalign ::mem_profile::ALLOC_HOOK_TABLE.get_memalign()
#define mperf_free ::mem_profile::ALLOC_HOOK_TABLE.get_free()
#define mperf_calloc ::mem_profile::ALLOC_HOOK_TABLE.get_calloc()
#endif
