#pragma once

#include <cstddef>
#include <cstdint>
// Needed for std::hash
#include <utility>
// Needed for CHAR_BIT
#include <climits>
// Needed for GlobalContext
#include <mutex>
#include <unordered_map>
#include <vector>



namespace mp {
    /// Represents a function address, eg, one obtained from a backtrace
    using Addr = uintptr_t;

    /// Increments a counter on construction, decrements it on destruction
    /// Used to locally disable allocation recording (we don't want to record an
    /// allocation inside an allocation, as that can result in an infinite loop)
    struct [[nodiscard("Returns a scope guard")]] CounterGuard {
        size_t& counter;
        CounterGuard(size_t& counter) noexcept
          : counter(counter) {
            counter += 1;
        }
        CounterGuard(CounterGuard const&) = delete;

        ~CounterGuard() noexcept { counter -= 1; }
    };

    struct AllocCount {
        uint64_t num_bytes {};
        uint64_t num_allocs {};

        constexpr void reset() noexcept {
            num_bytes = 0;
            num_allocs = 0;
        }

        constexpr void record_alloc(size_t bytes) noexcept {
            this->num_allocs += 1;
            this->num_bytes += bytes;
        }

        void add(AllocCount const& other) noexcept {
            num_bytes += other.num_bytes;
            num_allocs += other.num_allocs;
        }

        void drain(AllocCount& other) noexcept {
            num_bytes += other.num_bytes;
            num_allocs += other.num_allocs;
            other.reset();
        }

        AllocCount& operator+=(AllocCount const& other) noexcept {
            num_bytes += other.num_bytes;
            num_allocs += other.num_allocs;
            return *this;
        }
    };


    struct FlatCounts {
        std::unordered_map<Addr, AllocCount> counts;

        std::vector<Addr> get_addrs() const {
            std::vector<Addr> vec(counts.size());

            size_t i = 0;
            for (auto& [key, _] : counts) {
                vec[i++] = key;
            }

            return vec;
        }
    };

    /// Represents a view on a backtrace
    /// If you have a() -> b() -> c() (a calls b calls c),
    /// pop() will return &a, then &b, then &c, then nullptr
    struct TraceView {
        Addr const* data_;
        size_t count;

        /// Removes the function call at the base of the trace
        /// Eg, if the trace has three functions `[a(), b(), c()]`, where
        /// `a()` called `b()` which called `c()`, pop() will remove `a()`
        /// If the trace is empty returns nullptr
        Addr pop() noexcept {
            if (count == 0) {
                return {};
            }

            count--;
            Addr result = data_[count];
            return result;
        }

        Addr const* data() const noexcept { return data_; }
        size_t size() const noexcept { return count; }

        std::vector<Addr> vec() const {
            return std::vector<Addr>(data_, data_ + count);
        }
    };

    /// Represents a call graph annotated with allocation counts
    struct CallGraph : private AllocCount {
       public:
        CallGraph() = default;
        CallGraph(CallGraph const&) = delete;
        CallGraph(CallGraph&&) = default;

        using AllocCount::num_allocs;
        using AllocCount::num_bytes;
        using AllocCount::record_alloc;

        std::unordered_map<Addr, CallGraph> child_counts;

        AllocCount count() const noexcept { return *this; }

        void add(CallGraph const& other) {
            AllocCount::add(other);

            for (auto& [addr, graph] : other.child_counts) {
                child_counts[addr].add(graph);
            }
        }
        void drain(CallGraph& other) {
            AllocCount::drain(other);

            for (auto& [addr, graph] : other.child_counts) {
                child_counts[addr].drain(graph);
            }
        }

        /// Gets the callgraph corresponding to the given address. Creates a new
        /// empty callgraph if none is found.
        CallGraph* get(Addr key) { return &child_counts[key]; }

        size_t num_nodes() const {
            size_t n = child_counts.size();
            for (auto& [addr, graph] : child_counts) {
                n += graph.num_nodes();
            }
            return n;
        }
    };

    enum class EventType { FREE, ALLOC, REALLOC };
    struct EventRecord {
        /// A unique 64-bit stamp that can be used to order events
        /// chronologically. Also uniquely identifies an event.
        /// The first event should have an id of 0
        uint64_t id;

        /// Type of the event
        EventType type;

        /// Size of allocation (if applicable, 0 for free)
        size_t alloc_size = 0;
        /// Allocated pointer (or pointer passed to free)
        void const* alloc_ptr = nullptr;

        /// Pointer passed as input (eg to realloc())
        void const* alloc_hint = nullptr;

        /// Trace of the event
        std::vector<Addr> trace;
    };

    class AllocCounter {
        AllocCount total_allocs_;
        std::vector<EventRecord> events_;

       public:
        AllocCounter() = default;
        AllocCounter(AllocCounter const&) = delete;
        AllocCounter(AllocCounter&&) = default;

        std::vector<EventRecord> const& events() const noexcept {
            return events_;
        }


        void record_alloc(
            uint64_t id,
            EventType type,
            size_t alloc_size,
            void const* alloc_ptr,
            void const* alloc_hint,
            TraceView trace) {
            if (type != EventType::FREE) {
                total_allocs_.record_alloc(alloc_size);
            }
            events_.push_back(EventRecord {
                id,
                type,
                alloc_size,
                alloc_ptr,
                alloc_hint,
                trace.vec()});
        }


        void drain(AllocCounter& other) {
            total_allocs_.drain(other.total_allocs_);

            auto& oe = other.events_;
            events_.insert(
                events_.end(),
                std::move_iterator(oe.data()),
                std::move_iterator(oe.data() + oe.size()));
            oe.clear();
        }


        FlatCounts get_counts() const {
            std::unordered_map<Addr, AllocCount> result;

            for (auto const& record : events_) {
                for (Addr addr : record.trace) {
                    auto& dest = result[addr];
                    if (record.type != EventType::FREE) {
                        dest.record_alloc(record.alloc_size);
                    }
                }
            }
            return {result};
        }


        AllocCount total_allocs() const { return total_allocs_; }


        /// Dump an annotated json representation of the AllocCounter to a file
        ///
        /// (Annotated meaning function names are provided and demangled)
        void dump_json(char const* filename);
    };


    /// Keeps track of allocations on a particular thread
    struct LocalContext {
        /// Don't record allocations etc if this flag is nonzero
        /// Used so that the allocation counter's own internal allocations
        /// aren't recorded It is incremented at the beginning of a scope that
        /// disables recording, and decremented at the end of that scope
        size_t nest_level = 0;
        AllocCounter counter {};

        /// Initializes a LocalContext and increments the LOCAL_CONTEXT_COUNT
        LocalContext() noexcept;

        /// We delete the copy constructor because we don't want to move a
        /// LocalContext. it records a pointer to itself in the GlobalContext,
        /// and copying it or moving it would invalidate that pointer.
        LocalContext(LocalContext const&) = delete;

        /// This function returns a SetGuard which will re-enable allocation
        /// recording at the end of the scope. It must therefore be assigned to
        /// a local variable, so it's not destructed as a temporary.
        CounterGuard inc_nested() { return CounterGuard(nest_level); }

        /// Destroys a LocalContext. Synchronizes counts with the global
        /// context, and decriments the LOCAL_CONTEXT_COUNT
        ~LocalContext();
    };


    /// Stores record of reports from individual LocalContexts for individual
    /// threads, and generates a report for the entire program on destruction.
    /// LocalContexts synchronize with the GlobalContext on their destruction
    struct GlobalContext {
        std::mutex context_lock;
        AllocCounter counter;

        void drain(LocalContext& context);

        void generate_report();

        /// Invokes generate_report()
        ~GlobalContext();
    };
} // namespace mp
