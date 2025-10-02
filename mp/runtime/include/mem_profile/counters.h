#pragma once

#include <climits> // Needed for CHAR_BIT
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>   // Needed for global_context
#include <unordered_map>
#include <utility> // Needed for std::hash
#include <vector>

#include <mem_profile/prelude.h>
#include <mem_profile/alloc.h>
#include <mem_profile/allocator.h>
#include <mp_types/types.h>
#include <mp_unwind/mp_unwind.h>


namespace mp {
/// Increments a counter on construction, decrements it on destruction
/// Used to locally disable allocation recording (we don't want to record an
/// allocation inside an allocation, as that can result in an infinite loop)
struct [[nodiscard("Returns a scope guard")]] counter_guard {
    size_t& counter;
    counter_guard(size_t& counter) noexcept : counter(counter) { counter += 1; }
    counter_guard(counter_guard const&) = delete;

    ~counter_guard() noexcept { counter -= 1; }
};

struct alloc_count {
    uint64_t num_bytes{};
    uint64_t num_allocs{};

    constexpr void reset() noexcept {
        num_bytes  = 0;
        num_allocs = 0;
    }

    constexpr void record_alloc(size_t bytes) noexcept {
        this->num_allocs += 1;
        this->num_bytes += bytes;
    }

    void add(alloc_count const& other) noexcept {
        num_bytes += other.num_bytes;
        num_allocs += other.num_allocs;
    }

    void drain(alloc_count& other) noexcept {
        num_bytes += other.num_bytes;
        num_allocs += other.num_allocs;
        other.reset();
    }

    alloc_count& operator+=(alloc_count const& other) noexcept {
        num_bytes += other.num_bytes;
        num_allocs += other.num_allocs;
        return *this;
    }
};



/// Represents a view on a backtrace
/// If you have a() -> b() -> c() (a calls b calls c),
/// pop() will return &a, then &b, then &c, then nullptr
struct trace_view {
    addr_t const* data_;
    size_t        count;

    /// Removes the function call at the base of the trace
    /// Eg, if the trace has three functions `[a(), b(), c()]`, where
    /// `a()` called `b()` which called `c()`, pop() will remove `a()`
    /// If the trace is empty returns nullptr
    addr_t pop() noexcept {
        if (count == 0) {
            return {};
        }

        count--;
        addr_t result = data_[count];
        return result;
    }

    addr_t const* data() const noexcept { return data_; }
    size_t        size() const noexcept { return count; }

    _vec<addr_t> vec() const { return _vec<addr_t>(data_, data_ + count); }
};

/// Represents a call graph annotated with allocation counts
struct call_graph : private alloc_count {
  public:
    call_graph()                  = default;
    call_graph(call_graph const&) = delete;
    call_graph(call_graph&&)      = default;

    using alloc_count::num_allocs;
    using alloc_count::num_bytes;
    using alloc_count::record_alloc;

    std::unordered_map<addr_t, call_graph> child_counts;

    alloc_count count() const noexcept { return *this; }

    void add(call_graph const& other) {
        alloc_count::add(other);

        for (auto& [addr, graph] : other.child_counts) {
            child_counts[addr].add(graph);
        }
    }
    void drain(call_graph& other) {
        alloc_count::drain(other);

        for (auto& [addr, graph] : other.child_counts) {
            child_counts[addr].drain(graph);
        }
    }

    /// Gets the callgraph corresponding to the given address. Creates a new
    /// empty callgraph if none is found.
    call_graph* get(addr_t key) { return &child_counts[key]; }

    size_t num_nodes() const {
        size_t n = child_counts.size();
        for (auto& [addr, graph] : child_counts) {
            n += graph.num_nodes();
        }
        return n;
    }
};

enum class event_type { FREE, ALLOC, REALLOC };
struct event_record {
    /// A unique 64-bit stamp that can be used to order events
    /// chronologically. Also uniquely identifies an event.
    /// The first event should have an id of 0
    uint64_t id;

    /// Type of the event
    event_type type;

    /// Size of allocation (if applicable, 0 for free)
    size_t      alloc_size = 0;
    /// Allocated pointer (or pointer passed to free)
    void const* alloc_ptr  = nullptr;

    /// Pointer passed as input (eg to realloc())
    void const* alloc_hint = nullptr;

    /// Trace of the event
    _vec<addr_t> trace;

    /// Records information about objects taking part in the trace
    _vec<event_info> object_trace;
};

class alloc_counter {
    alloc_count        total_allocs_;
    _vec<event_record> events_;

  public:
    alloc_counter()                     = default;
    alloc_counter(alloc_counter const&) = delete;
    alloc_counter(alloc_counter&&)      = default;

    _vec<event_record> const& events() const noexcept { return events_; }


    void record_alloc(uint64_t    id,
                      event_type  type,
                      size_t      alloc_size,
                      void const* alloc_ptr,
                      void const* alloc_hint,
                      trace_view  trace) {
        if (type != event_type::FREE) {
            total_allocs_.record_alloc(alloc_size);
        }
        events_.push_back(event_record{id, type, alloc_size, alloc_ptr, alloc_hint, trace.vec()});
    }


    /// Records an allocation and extracts any events discovered on the stack
    void record_alloc_with_events(uint64_t    id,
                                  event_type  type,
                                  size_t      alloc_size,
                                  void const* alloc_ptr,
                                  void const* alloc_hint,
                                  trace_view  trace,
                                  trace_view  spp) {
        if (type != event_type::FREE) {
            total_allocs_.record_alloc(alloc_size);
        }
        event_info event_buffer[OBJECT_BUFFER_SIZE];
        size_t     event_count
            = mp_extract_events(OBJECT_BUFFER_SIZE, event_buffer, spp.size(), spp.data());

        events_.push_back(event_record{
            id,
            type,
            alloc_size,
            alloc_ptr,
            alloc_hint,
            trace.vec(),
            _vec<event_info>(event_buffer, event_buffer + event_count),
        });
    }


    void drain(alloc_counter& other) {
        total_allocs_.drain(other.total_allocs_);

        auto& oe = other.events_;
        events_.insert(events_.end(),
                       std::move_iterator(oe.data()),
                       std::move_iterator(oe.data() + oe.size()));
        oe.clear();
    }


    alloc_count total_allocs() const { return total_allocs_; }


    /// Dump an annotated json representation of the alloc_counter to a file
    ///
    /// (Annotated meaning function names are provided and demangled)
    void dump_json(char const* filename);
};


/// Keeps track of allocations on a particular thread
struct local_context {
    /// Don't record allocations etc if this flag is nonzero
    /// Used so that the allocation counter's own internal allocations
    /// aren't recorded It is incremented at the beginning of a scope that
    /// disables recording, and decremented at the end of that scope
    size_t        nest_level = 0;
    alloc_counter counter{};

    local_context() = default;

    /// We delete the copy constructor because we don't want to move a
    /// local_context. it records a pointer to itself in the global_context,
    /// and copying it or moving it would invalidate that pointer.
    local_context(local_context const&) = delete;

    /// This function returns a set_guard which will re-enable allocation
    /// recording at the end of the scope. It must therefore be assigned to
    /// a local variable, so it's not destructed as a temporary.
    counter_guard inc_nested() { return counter_guard(nest_level); }

    // These are provided so that allocating a new local_context on the heap
    // circumvents the mem_profile tracking machinery, so that we avoid allocating
    // while tracking other allocations
    void* operator new(size_t count) noexcept { return mperf_malloc(count); }
    void  operator delete(void* ptr) noexcept { return mperf_free(ptr); }
};


/// Stores record of reports from individual local_contexts for individual
/// threads, and generates a report for the entire program on destruction.
/// local_contexts synchronize with the global_context on their destruction
struct global_context {
    std::mutex context_lock;

    // the global context knows about all existant counters
    _vec<std::unique_ptr<local_context>> counters;

    local_context* new_local_context();

    void generate_report();

    /// Invokes generate_report()
    ~global_context();
};
} // namespace mp
