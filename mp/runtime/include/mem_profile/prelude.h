#pragma once

///////////////////
////  Prelude  ////
///////////////////

using size_t     = decltype(sizeof(0));
using malloc_t   = void* (*)(size_t);
using realloc_t  = void* (*)(void*, size_t);
using memalign_t = void* (*)(size_t alignment, size_t size);
using free_t     = void (*)(void* ptr);
using calloc_t   = void* (*)(size_t n_members, size_t size);
using ptrdiff_t  = decltype((char*)(nullptr) - (char*)(nullptr));

namespace mp {
/// Stores function pointers to malloc, realloc, memalign, free, and calloc
/// The ALLOC_HOOK_TABLE is set to null initially, and then as functions are
/// requested (eg, as someone calls ALLOC_HOOK_TABLE.get_malloc()), the
/// corresponding function will be found via dlsym
///
/// This is needed because our implementation of malloc & co replaces the C
/// standard library implementation, BUT we still need to call the C
/// standard library implementation under the hood
///
/// If we happen to be using glibc, then we can use __stdc_malloc & co
/// instead of using the alloc_hook_table, but this is provided as a fallback
class alloc_hook_table {
    malloc_t   malloc_;
    realloc_t  realloc_;
    memalign_t memalign_;
    free_t     free_;
    calloc_t   calloc_;

  public:
    malloc_t   get_malloc() noexcept;
    realloc_t  get_realloc() noexcept;
    memalign_t get_memalign() noexcept;
    free_t     get_free() noexcept;
    calloc_t   get_calloc() noexcept;
};

/// This should be initialized first, before any of the includes
/// it should be the first global symbol that's initialized
/// These should be initialized to null initially. Once get_<func>() is
/// called, the corresponding symbol will be found with dlsym and cached
extern alloc_hook_table ALLOC_HOOK_TABLE;


constexpr size_t BACKTRACE_BUFFER_SIZE = 1024;


constexpr size_t OBJECT_BUFFER_SIZE = 1024;
} // namespace mp
