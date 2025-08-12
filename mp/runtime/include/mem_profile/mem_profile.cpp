////////////////////////////////////////
////  Counters and global contexts  ////
////////////////////////////////////////
#include <mem_profile/prelude.h>

namespace mem_profile {
AllocHookTable ALLOC_HOOK_TABLE{};
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

namespace mem_profile {
/// Counts the number of living local contexts
std::atomic_int           LOCAL_CONTEXT_COUNT = 0;
/// true if tracing is enabled. See tracing_enabled
std::atomic_bool          TRACING_ENABLED     = false;
/// Keeps track of global allocation counts. Local Contexts synchronize with
/// the global context on their destruction
GlobalContext             GLOBAL_CONTEXT{};
/// Keeps track of allocation counts on the current thread.
thread_local LocalContext LOCAL_CONTEXT{};

/// Tracing is enabled provided that there are living local contexts.
/// At the end of the program, all the local contexts will be destroyed
/// but this will occur BEFORE the main thread exits. It will also occur
/// BEFORE the global context is destroyed.
/// When the last local context is destroyed, it disables tracing and
/// synchronizes with the global context. Then when the global context is
/// destroyed, the global context generates a report
inline bool tracing_enabled() noexcept { return TRACING_ENABLED.load(std::memory_order_relaxed); }
} // namespace mem_profile



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
constexpr size_t BACKTRACE_BUFFER_SIZE = 1024;
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
    if (mem_profile::tracing_enabled()) {                                                          \
        auto& context = mem_profile::LOCAL_CONTEXT;                                                \
        if (context.nest_level == 0) {                                                             \
            auto guard = context.inc_nested();                                                     \
                                                                                                   \
            mem_profile::Addr trace_buff[BACKTRACE_BUFFER_SIZE];                                   \
            size_t            trace_size = mp::mp_unwind(BACKTRACE_BUFFER_SIZE, trace_buff);       \
            TraceView         trace_view{trace_buff, trace_size};                                  \
            context.counter.record_alloc(EVENT_COUNTER++,                                          \
                                         _type,                                                    \
                                         _alloc_size,                                              \
                                         _alloc_ptr,                                               \
                                         _alloc_hint,                                              \
                                         trace_view);                                              \
        }                                                                                          \
    }


using namespace mem_profile;

extern "C" void* malloc(size_t size) {
    auto result = mperf_malloc(size);

    RECORD_ALLOC(EventType::ALLOC, size, result, nullptr);

    return result;
}

extern "C" void* calloc(size_t n, size_t size) {
    auto result = mperf_calloc(n, size);

    RECORD_ALLOC(EventType::ALLOC, n * size, result, nullptr);

    return result;
}

extern "C" void* realloc(void* hint, size_t n) {
    auto result = mperf_realloc(hint, n);

    RECORD_ALLOC(EventType::ALLOC, n, result, hint);

    return result;
}


extern "C" void* memalign(size_t alignment, size_t size) {
    auto result = mperf_memalign(alignment, size);

    RECORD_ALLOC(EventType::ALLOC, size, result, nullptr);

    return result;
}


extern "C" void free(void* ptr) {
    RECORD_ALLOC(EventType::FREE, 0, ptr, nullptr);
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
            RECORD_ALLOC(EventType::ALLOC, count, result, nullptr);                                \
            return result;                                                                         \
        }                                                                                          \
        std::new_handler new_handler = std::get_new_handler();                                     \
        if (new_handler == nullptr) {                                                              \
            throw std::bad_alloc();                                                                \
        } else {                                                                                   \
            new_handler();                                                                         \
        }                                                                                          \
    }


void* operator new(size_t count) { ALLOCATE_OR_THROW(count, mperf_malloc(count)); }

void* operator new[](size_t count) { ALLOCATE_OR_THROW(count, mperf_malloc(count)); }

void* operator new(size_t count, std::align_val_t al) {
    ALLOCATE_OR_THROW(count, mperf_memalign(count, (size_t)al));
}

void* operator new[](size_t count, std::align_val_t al) {
    ALLOCATE_OR_THROW(count, mperf_memalign(count, (size_t)al));
}


void* operator new(size_t count, const std::nothrow_t&) noexcept {
    auto result = mperf_malloc(count);
    RECORD_ALLOC(EventType::ALLOC, count, result, nullptr);
    return result;
}

void* operator new[](size_t count, const std::nothrow_t&) noexcept {
    auto result = mperf_malloc(count);
    RECORD_ALLOC(EventType::ALLOC, count, result, nullptr);
    return result;
}

void* operator new(size_t count, std::align_val_t al, const std::nothrow_t&) noexcept {
    auto result = mperf_memalign(count, (size_t)al);
    RECORD_ALLOC(EventType::ALLOC, count, result, nullptr);
    return result;
}

void* operator new[](size_t count, std::align_val_t al, const std::nothrow_t&) noexcept {

    auto result = mperf_memalign(count, (size_t)al);
    RECORD_ALLOC(EventType::ALLOC, count, result, nullptr);
    return result;
}

/// Corresponding delete operators
/// See: https://en.cppreference.com/w/cpp/memory/new/operator_delete
void operator delete(void* ptr) noexcept {
    RECORD_ALLOC(EventType::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
void operator delete[](void* ptr) noexcept {
    RECORD_ALLOC(EventType::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
void operator delete(void* ptr, std::align_val_t al) noexcept {
    RECORD_ALLOC(EventType::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}
void operator delete[](void* ptr, std::align_val_t al) noexcept {
    RECORD_ALLOC(EventType::FREE, 0, ptr, nullptr);
    mperf_free(ptr);
}


////////////////////////////////////////////////////
////  Local and global context implementations  ////
////////////////////////////////////////////////////

namespace mem_profile {
LocalContext::LocalContext() noexcept {
    LOCAL_CONTEXT_COUNT.fetch_add(1, std::memory_order_relaxed);
}
LocalContext::~LocalContext() {
    GLOBAL_CONTEXT.drain(*this);
    int prev_value = LOCAL_CONTEXT_COUNT.fetch_sub(1, std::memory_order_relaxed);

    // if there was only 1 living counter, disable the has_living_counters_v
    // flag
    if (prev_value == 1) {
        TRACING_ENABLED.store(false, std::memory_order_relaxed);
    }
}

void GlobalContext::drain(LocalContext& context) {
    auto guard  = std::lock_guard(context_lock);
    auto guard2 = LOCAL_CONTEXT.inc_nested();
    // Drain the counts from the local_context
    counter.drain(context.counter);
}

void GlobalContext::generate_report() {
    auto guard = std::lock_guard(context_lock);
    counter.dump_json("malloc_stats.json");
}

GlobalContext::~GlobalContext() { generate_report(); }
} // namespace mem_profile



///////////////////////////////////
////  AllocCounter::dump_json  ////
///////////////////////////////////

/// Used to serialize output
#include <mem_profile/json.h>
/// Used to obtnain symbol information (mangled function name, file name,
/// offsets, etc) from an address
#include <mem_profile/sym_info.h>
/// Used to demangle symbol names
#include <mem_profile/abi.h>
/// Used to sort addresses
#include <algorithm>
#include <cstring>
#include <mem_profile/containers.h>
#include <memory>
#include <optional>
#include <queue>



/// Serializes a node in a call graph
namespace mem_profile {

inline void json_print(FILE* dest, EventType type) {
    switch (type) {
    case EventType::ALLOC:   json_print(dest, "ALLOC"); return;
    case EventType::REALLOC: json_print(dest, "REALLOC"); return;
    case EventType::FREE:    json_print(dest, "FREE"); return;
    default:                 json_print(dest, (int)type);
    }
}
struct CallgraphSerializer {
  private:
    std::unordered_map<Addr, size_t> const* addr_index;
    using iterator = typename std::unordered_map<Addr, CallGraph>::const_iterator;

  public:
    CallgraphSerializer(std::unordered_map<Addr, size_t> const& addr_index) noexcept
      : addr_index(&addr_index) {}

    auto operator()(std::pair<Addr const, CallGraph> const& pair) -> std::
        tuple<size_t, uint64_t, uint64_t, Source<MapIterSource<iterator, CallgraphSerializer>>> {
        Addr             key = pair.first;
        CallGraph const& n   = pair.second;
        size_t           idx = addr_index->at(key);

        return std::make_tuple(idx,
                               n.num_allocs,
                               n.num_bytes,
                               make_source(n.child_counts, CallgraphSerializer{*this}));
    }
};
} // namespace mem_profile

namespace {
/// Computes the sizes of all frees that took place, based on the size
/// of the corresponding malloc
///
/// Assumes events are sorted by ID
void match_allocs_and_frees(std::vector<EventRecord>& records) {
    using ptr_t = void const*;
    std::unordered_map<ptr_t, size_t> sizes;

    for (auto& record : records) {
        if (record.type == EventType::FREE) {
            record.alloc_size = sizes[record.alloc_ptr];
        } else {
            sizes[record.alloc_ptr] = record.alloc_size;
        }
    }
}
} // namespace

void mem_profile::AllocCounter::dump_json(char const* filename) {
    using namespace mem_profile;

    // Sort events by ID
    std::sort(events_.begin(), events_.end(), [](EventRecord const& a, EventRecord const& b) {
        return a.id < b.id;
    });

    match_allocs_and_frees(events_);

    auto              counts = get_counts();
    std::vector<Addr> addrs  = counts.get_addrs();
    std::sort(addrs.data(), addrs.data() + addrs.size(), std::less<Addr>{});

    using mem_profile::InfoStore;
    using mem_profile::PCInfo;

    size_t const                     count = addrs.size();
    std::vector<PCInfo>              info(count);
    std::unordered_map<Addr, size_t> addr_index;
    addr_index.reserve(count);

    InfoStore info_store;
    for (size_t i = 0; i < count; i++) {
        Addr addr        = addrs[i];
        addr_index[addr] = i;
        info[i]          = info_store.get_info(addr);
    }
    fprintf(stderr, "Count: %zu\n", count);

    std::vector<mem_profile::DebugInfo> debug_info_buff;

    auto get_debug_info = [&](Addr a) {
        info_store.full_debug_info(a, debug_info_buff);

        return make_source(debug_info_buff, [](DebugInfo const& dbg) {
            return std::array<size_t, 3>{dbg.src_file, dbg.func_name, dbg.lineno};
        });
    };

    auto get_alloc_count = [&](Addr a) { return counts.counts[a].num_allocs; };
    auto get_alloc_bytes = [&](Addr a) { return counts.counts[a].num_bytes; };

    auto file = fopen(filename, "wb");

    print_object(
        file,
        Field{"version", "0.0.0"},
        /// Stats
        Field{"total_allocs", total_allocs_.num_allocs},
        Field{"total_bytes", total_allocs_.num_bytes},
        Field{"pc.alloc_counts", make_source(addrs, get_alloc_count)},
        Field{"pc.alloc_bytes", make_source(addrs, get_alloc_bytes)},
        /// Sym info
        Field{"pc.object_file", make_source<&PCInfo::object_file>(info)},
        Field{"pc.addr", make_source<&PCInfo::addr>(info)},
        Field{"pc.sym_name", make_source<&PCInfo::sym_name>(info)},
        Field{"pc.sym_addr", make_source<&PCInfo::sym_addr>(info)},
        /// Debug info
        Field{"pc.source_file", make_source<&PCInfo::source_file>(info)},
        Field{"pc.func_name", make_source<&PCInfo::func_name>(info)},
        Field{"pc.func_lineno", make_source<&PCInfo::func_lineno>(info)},
        Field{"pc.debug_info", make_source(addrs, get_debug_info)},
        /// Names of functions, symbols, and files
        Field{"source_files", lazy_writer([&] { return make_source(info_store.source_files); })},
        Field{"object_files", lazy_writer([&] { return make_source(info_store.object_files); })},
        Field{"sym_names", lazy_writer([&] { return make_source(info_store.sym_names); })},
        Field{"func_names", lazy_writer([&] { return make_source(info_store.func_names); })},
        Field{"events", make_source(events_, [&](EventRecord const& event) {
                  return std::tuple{Field{"id", event.id},
                                    Field{"type", event.type},
                                    Field{"alloc_size", event.alloc_size},
                                    Field{"alloc_ptr", event.alloc_ptr},
                                    Field{"alloc_hint", event.alloc_hint},
                                    Field{"trace",
                                          make_source(event.trace,
                                                      [&](Addr addr) {
                                                          auto it = addr_index.find(addr);
                                                          if (it != addr_index.end()) {
                                                              return it->second;
                                                          } else {
                                                              fprintf(stderr,
                                                                      "Cannot find address %llx\n",
                                                                      (long long unsigned)addr);
                                                              return (unsigned long)(-1);
                                                          }
                                                      })},
                                    Field{"trace", make_source(event.trace, [&](Addr addr) {
                                              auto it = addr_index.find(addr);
                                              if (it != addr_index.end()) {
                                                  return it->second;
                                              } else {
                                                  fprintf(stderr,
                                                          "Cannot find address %llx\n",
                                                          (long long unsigned)addr);
                                                  return (unsigned long)(-1);
                                              }
                                          })}};
              })});


    fclose(file);
}



/////////////////////////////////////////////////
////  AllocHookTable::get_* implementatinos  ////
/////////////////////////////////////////////////

/// Used to generate alloc hook table
#include <mem_profile/dlsym.h>

namespace mem_profile {
malloc_t AllocHookTable::get_malloc() noexcept {
    if (malloc_ == nullptr) [[unlikely]] {
        malloc_ = dlsymLoadOrExitAs<malloc_t>(RTLD_NEXT, "malloc");
    }
    return malloc_;
}
realloc_t AllocHookTable::get_realloc() noexcept {
    if (realloc_ == nullptr) [[unlikely]] {
        realloc_ = dlsymLoadOrExitAs<realloc_t>(RTLD_NEXT, "realloc");
    }
    return realloc_;
}
memalign_t AllocHookTable::get_memalign() noexcept {
    if (memalign_ == nullptr) [[unlikely]] {
        memalign_ = dlsymLoadOrExitAs<memalign_t>(RTLD_NEXT, "memalign");
    }
    return memalign_;
}
free_t AllocHookTable::get_free() noexcept {
    if (free_ == nullptr) [[unlikely]] {
        free_ = dlsymLoadOrExitAs<free_t>(RTLD_NEXT, "free");
    }
    return free_;
}
calloc_t AllocHookTable::get_calloc() noexcept {
    if (calloc_ == nullptr) [[unlikely]] {
        calloc_ = dlsymLoadOrExitAs<calloc_t>(RTLD_NEXT, "calloc");
    }
    return calloc_;
}
} // namespace mem_profile
