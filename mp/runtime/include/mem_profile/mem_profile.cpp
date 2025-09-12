////////////////////////////////////////
////  Counters and global contexts  ////
////////////////////////////////////////
#include <mem_profile/env.h>
#include <mem_profile/prelude.h>
#include <mp_types/export.h>


namespace mp {
alloc_hook_table ALLOC_HOOK_TABLE{};
}

#include <atomic>
#include <mem_profile/counters.h>

/// Rules for contsruction and destruction:
/// 1. All thread_local objects are destroyed prior to any static objects
/// 2. If A was created before B, then B will be destroyed before A
///    A() -> B() -> ~B() -> ~A()
/// 3. (1) and (2) imply that thread_local objects are created *after* any
///    static objects
/// References:
/// - https://en.cppreference.com/w/cpp/language/siof
/// - https://en.cppreference.com/w/cpp/utility/program/exit

namespace mp {
/// Counts the number of living local contexts
std::atomic_int            LOCAL_CONTEXT_COUNT = 0;
/// true if tracing is enabled. See tracing_enabled
std::atomic_bool           TRACING_ENABLED     = true;
/// Keeps track of global allocation counts. Local Contexts synchronize with
/// the global context on their destruction
global_context             GLOBAL_CONTEXT{};
/// Keeps track of allocation counts on the current thread.
thread_local local_context LOCAL_CONTEXT{};

/// Tracing is enabled provided that there are living local contexts.
/// At the end of the program, all the local contexts will be destroyed
/// but this will occur BEFORE the main thread exits. It will also occur
/// BEFORE the global context is destroyed.
/// When the last local context is destroyed, it disables tracing and
/// synchronizes with the global context. Then when the global context is
/// destroyed, the global context generates a report
inline bool tracing_enabled() noexcept { return TRACING_ENABLED.load(std::memory_order_relaxed); }
} // namespace mp



///////////////////////////////////////
////  *alloc and free definitions  ////
///////////////////////////////////////

/// Used to get __GLIBC__
#include <cstdlib>
/// Used for backtrace()
#include <mem_profile/trace.h>
// Used to get underlying malloc implementation
#include <mem_profile/alloc.h>

#include <mp_unwind/mp_unwind.h>

namespace {
std::atomic_uint64_t EVENT_COUNTER = 0;
}

/// Records an allocation if tracing is enabled in the current context.
///
/// This is implemented as a macro in order to avoid adding another function to
/// the backtrace, so the top of the backtrace should always say "malloc" or
/// another allocation function
///
/// Procedure:
///
/// Checks if tracing is enabled globally. If there are no more living local
/// contexts, eg because the program is in the process of exiting, tracing will
/// be disabled globally.
///
/// If tracing is enabled globally, it's safe to obtain the current local
/// context. The local context is obtained, and then it checks if tracing is
/// enabled locally. If tracing is enabled locally, then:
/// - disable tracing temporarily (prevents infinite loops due to allocations
///   while mallocs are being traced)
/// - obtains a backtrace
/// - records the current allocation and it's backtrace
/// - re-enables tracing (the guard re-enables it upon destruction)
#define RECORD_ALLOC(_type, _alloc_size, _alloc_ptr, _alloc_hint)                                  \
    if (mp::tracing_enabled()) {                                                                   \
        auto& context = mp::LOCAL_CONTEXT;                                                         \
        if (context.nest_level == 0) {                                                             \
            auto guard = context.inc_nested();                                                     \
                                                                                                   \
            mp::addr_t trace_buff[BACKTRACE_BUFFER_SIZE];                                          \
            size_t     trace_size = mp::mp_unwind(BACKTRACE_BUFFER_SIZE, trace_buff);              \
            context.counter.record_alloc(EVENT_COUNTER++,                                          \
                                         _type,                                                    \
                                         _alloc_size,                                              \
                                         _alloc_ptr,                                               \
                                         _alloc_hint,                                              \
                                         trace_view{trace_buff, trace_size});                      \
        }                                                                                          \
    }

#define RECORD_ALLOC_WITH_OBJECT_INFO(_type, _alloc_size, _alloc_ptr, _alloc_hint)                      \
    if (mp::tracing_enabled()) {                                                                        \
        auto& context = mp::LOCAL_CONTEXT;                                                              \
        if (context.nest_level == 0) {                                                                  \
            auto guard = context.inc_nested();                                                          \
                                                                                                        \
            mp::addr_t trace_buff[BACKTRACE_BUFFER_SIZE];                                               \
            mp::addr_t spp_buff[BACKTRACE_BUFFER_SIZE];                                                 \
            size_t     trace_size = mp::mp_unwind(BACKTRACE_BUFFER_SIZE, trace_buff, spp_buff);         \
            context.counter.record_alloc_with_events(EVENT_COUNTER++,                                   \
                                                     _type,                                             \
                                                     _alloc_size,                                       \
                                                     _alloc_ptr,                                        \
                                                     _alloc_hint,                                       \
                                                     trace_view{trace_buff, trace_size},                \
                                                     trace_view{spp_buff, trace_size});                 \
        }                                                                                               \
    }


using namespace mp;

extern "C" MP_EXPORT void* malloc(size_t size) {
    auto result = mperf_malloc(size);

    RECORD_ALLOC(event_type::ALLOC, size, result, nullptr);

    return result;
}

extern "C" MP_EXPORT void* calloc(size_t n, size_t size) {
    auto result = mperf_calloc(n, size);

    RECORD_ALLOC(event_type::ALLOC, n * size, result, nullptr);

    return result;
}

extern "C" MP_EXPORT void* realloc(void* hint, size_t n) {
    auto result = mperf_realloc(hint, n);

    RECORD_ALLOC(event_type::ALLOC, n, result, hint);

    return result;
}


extern "C" MP_EXPORT void* memalign(size_t alignment, size_t size) {
    auto result = mperf_memalign(alignment, size);

    RECORD_ALLOC(event_type::ALLOC, size, result, nullptr);

    return result;
}


extern "C" MP_EXPORT void free(void* ptr) {
    RECORD_ALLOC_WITH_OBJECT_INFO(event_type::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}


//////////////////////////////////////////////////////////
////  Overloads for new and delete (needed on MacOS)  ////
//////////////////////////////////////////////////////////

#include <new>

/// Implements the body of a new operator by using the given expression to do
/// the actual allocation. See:
/// https://en.cppreference.com/w/cpp/memory/new/operator_new
#define ALLOCATE_OR_THROW(count, alloc_expr)                                                       \
    for (;;) {                                                                                     \
        void* result = alloc_expr;                                                                 \
        if (result) {                                                                              \
            RECORD_ALLOC(event_type::ALLOC, count, result, nullptr);                               \
            return result;                                                                         \
        }                                                                                          \
        std::new_handler new_handler = std::get_new_handler();                                     \
        if (new_handler == nullptr) {                                                              \
            throw std::bad_alloc();                                                                \
        } else {                                                                                   \
            new_handler();                                                                         \
        }                                                                                          \
    }


MP_EXPORT void* operator new(size_t count) { ALLOCATE_OR_THROW(count, mperf_malloc(count)); }

MP_EXPORT void* operator new[](size_t count) { ALLOCATE_OR_THROW(count, mperf_malloc(count)); }

MP_EXPORT void* operator new(size_t count, std::align_val_t al) {
    ALLOCATE_OR_THROW(count, mperf_memalign(count, (size_t)al));
}

MP_EXPORT void* operator new[](size_t count, std::align_val_t al) {
    ALLOCATE_OR_THROW(count, mperf_memalign(count, (size_t)al));
}


MP_EXPORT void* operator new(size_t count, const std::nothrow_t&) noexcept {
    auto result = mperf_malloc(count);
    RECORD_ALLOC(event_type::ALLOC, count, result, nullptr);
    return result;
}

MP_EXPORT void* operator new[](size_t count, const std::nothrow_t&) noexcept {
    auto result = mperf_malloc(count);
    RECORD_ALLOC(event_type::ALLOC, count, result, nullptr);
    return result;
}

MP_EXPORT void* operator new(size_t count, std::align_val_t al, const std::nothrow_t&) noexcept {
    auto result = mperf_memalign(count, (size_t)al);
    RECORD_ALLOC(event_type::ALLOC, count, result, nullptr);
    return result;
}

MP_EXPORT void* operator new[](size_t count, std::align_val_t al, const std::nothrow_t&) noexcept {

    auto result = mperf_memalign(count, (size_t)al);
    RECORD_ALLOC(event_type::ALLOC, count, result, nullptr);
    return result;
}

/// Corresponding delete operators
/// See: https://en.cppreference.com/w/cpp/memory/new/operator_delete
MP_EXPORT void operator delete(void* ptr) noexcept {
    RECORD_ALLOC_WITH_OBJECT_INFO(event_type::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
MP_EXPORT void operator delete[](void* ptr) noexcept {
    RECORD_ALLOC_WITH_OBJECT_INFO(event_type::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
MP_EXPORT void operator delete(void* ptr, std::align_val_t al) noexcept {
    RECORD_ALLOC_WITH_OBJECT_INFO(event_type::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
MP_EXPORT void operator delete[](void* ptr, std::align_val_t al) noexcept {
    RECORD_ALLOC_WITH_OBJECT_INFO(event_type::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}


////////////////////////////////////////////////////
////  Local and global context implementations  ////
////////////////////////////////////////////////////

namespace mp {
local_context::local_context() noexcept {
    LOCAL_CONTEXT_COUNT.fetch_add(1, std::memory_order_relaxed);
}
local_context::~local_context() {
    GLOBAL_CONTEXT.drain(*this);
    int prev_value = LOCAL_CONTEXT_COUNT.fetch_sub(1, std::memory_order_relaxed);

    // if there was only 1 living counter, disable the has_living_counters_v
    // flag
    if (prev_value == 1) {
        TRACING_ENABLED.store(false, std::memory_order_relaxed);
    }
}

void global_context::drain(local_context& context) {
    auto guard  = std::lock_guard(context_lock);
    auto guard2 = LOCAL_CONTEXT.inc_nested();
    // Drain the counts from the local_context
    counter.drain(context.counter);
}

void global_context::generate_report() {
    auto guard = std::lock_guard(context_lock);
    counter.dump_json(mp::mem_profile_out());
}

global_context::~global_context() { generate_report(); }
} // namespace mp



///////////////////////////////////
////  alloc_counter::dump_json  ////
///////////////////////////////////

/// Used to obtnain symbol information (mangled function name, file name,
/// offsets, etc) from an address
#include <cstring>
#include <mem_profile/abi.h>
#include <mem_profile/containers.h>

#include <mem_profile/output_record.h>
#include <mem_profile/output_record_io.h>

void mp::alloc_counter::dump_json(char const* filename) {
    using namespace mp;

    sv_store store;
    auto data = make_output_record(*this, store);

    constexpr glz::opts opts{.skip_null_members = false};

    auto errc = glz::write_file_json<opts>(data, filename, std::string{});
    if (errc) {
        std::string glz_error = glz::format_error(errc, std::string{});
        throw ERR("Error when dumping json - {}", glz_error);
    }
}



/////////////////////////////////////////////////
////  alloc_hook_table::get_* implementatinos  ////
/////////////////////////////////////////////////

/// Used to generate alloc hook table
#include <mem_profile/dlsym.h>

namespace mp {
malloc_t alloc_hook_table::get_malloc() noexcept {
    if (malloc_ == nullptr) [[unlikely]] {
        malloc_ = dlsym_load_or_exit_as<malloc_t>(RTLD_NEXT, "malloc");
    }
    return malloc_;
}
realloc_t alloc_hook_table::get_realloc() noexcept {
    if (realloc_ == nullptr) [[unlikely]] {
        realloc_ = dlsym_load_or_exit_as<realloc_t>(RTLD_NEXT, "realloc");
    }
    return realloc_;
}
memalign_t alloc_hook_table::get_memalign() noexcept {
    if (memalign_ == nullptr) [[unlikely]] {
        memalign_ = dlsym_load_or_exit_as<memalign_t>(RTLD_NEXT, "memalign");
    }
    return memalign_;
}
free_t alloc_hook_table::get_free() noexcept {
    if (free_ == nullptr) [[unlikely]] {
        free_ = dlsym_load_or_exit_as<free_t>(RTLD_NEXT, "free");
    }
    return free_;
}
calloc_t alloc_hook_table::get_calloc() noexcept {
    if (calloc_ == nullptr) [[unlikely]] {
        calloc_ = dlsym_load_or_exit_as<calloc_t>(RTLD_NEXT, "calloc");
    }
    return calloc_;
}
} // namespace mp
