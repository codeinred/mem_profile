#pragma once

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <cpptrace/cpptrace.hpp>
#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <mem_profile/counters.h>
#include <mp_error/error.h>
#include <mp_types/types.h>


namespace mp {
using ankerl::unordered_dense::map;

struct string_table {
    std::vector<std::string>           strtab;
    map<std::string_view, str_index_t> lookup;
    // Some strings come in as c strings with static lifetime.
    // This provides a cache to look up those entries quickly.
    map<addr_t, str_index_t>           cstr_lookup;


    size_t size() const noexcept { return strtab.size(); }

    /// Inserts the given entry into the string table. Returns the address
    /// at which it was inserted.
    str_index_t insert(std::string_view key) {
        str_index_t index_if_new = size();

        auto [it, is_new] = lookup.try_emplace(key, index_if_new);

        // If it's not a new entry, return the index where it was found.
        if (!is_new) return it->second;

        strtab.emplace_back(key);
        return index_if_new;
    }

    /// Quickly look up and insert a c string. This is useful for, eg, object typenames
    /// collected while scanning the stack.
    str_index_t insert_cstr(char const* key) {
        constexpr size_t npos = ~size_t();

        addr_t cstr_addr = addr_t(key);

        auto [it, is_new] = cstr_lookup.try_emplace(cstr_addr, npos);

        // fast path! If it's not a new entry, return the index where it was found.
        if (!is_new) return it->second;

        // Insert as regular string_view
        auto result = insert(key);

        // Ensure that our cstr_lookup knows about the correct index
        it->second = result;

        return result;
    }
};


struct output_object_info {
    // Index into event stacktrace
    std::vector<size_t>      trace_index;
    // id of object being destroyed (unique over lifetime of program)
    std::vector<u64>         object_id;
    // address of the object at time of destruction(`this` pointer)
    std::vector<addr_t>      addr;
    // size of object being destroyed
    std::vector<size_t>      size;
    /// Index into string table - name of the object's type
    std::vector<str_index_t> type;

    output_object_info(string_table& strtab, std::vector<event_info> const& object_trace);
};


struct output_event {
    /// A unique 64-bit stamp that can be used to order events
    /// chronologically. Also uniquely identifies an event.
    /// The first event should have an id of 0
    u64 id;

    /// Event type
    event_type type;

    /// Size of allocation (if applicable, 0 for free)
    size_t alloc_size;

    /// Allocated pointer (or pointer passed  to free)
    u64 alloc_addr;

    /// Pointer passed as input (eg to realloc())
    u64 alloc_hint;

    // Call stack, expressed as a vector of program counter ids
    std::vector<size_t> pc_id;

    std::optional<output_object_info> object_info;
};


struct output_frame_table {
    /// Table of program counters
    std::vector<addr_t> pc;

    /// Each program counter corresponds to 1 or more frames.
    /// (If a function is inlined, a program counter will correspond to more than
    /// one frame)
    ///
    /// The frames for pc[i] range from offsets[i] to offsets[i + 1]
    std::vector<size_t> offsets;


    /// Source file corresponding to program counter (index into strtab)
    std::vector<str_index_t> file;
    /// Function name corresponding to program counter (index into strtab)
    std::vector<str_index_t> func;
    /// Symbol name corresponding to program counter (index into strtab)
    //std::vector<size_t> pc_sym_name;
    /// Source File Line Number corresponding to program counter. 0 if missing
    std::vector<u32>         line;
    /// Soruce File Columnn Number corresponding to program counter. 0 if missing
    std::vector<u32>         column;

    /// true if the given frame is inline. 0 if false, 1 if true
    std::vector<u8> is_inline;

    output_frame_table() = default;
    output_frame_table(string_table&                                  strtab,
                       std::vector<addr_t>                            pcs,
                       std::vector<cpptrace::stacktrace_frame> const& frames);
};



struct output_record {
    output_frame_table frame_table;

    /// Vector of events
    std::vector<output_event> events;

    /// String table
    std::vector<std::string> strtab;
};

/// Sanity check: we expect that the number of non-inline frames should match
/// the number of program counters
auto run_sanity_check_on_frames(size_t                                         pc_count,
                                std::vector<cpptrace::stacktrace_frame> const& frames) -> void;

/// Given an event_record, compute an ordering that sequences the event record,
/// such that events are ordered by their id.
auto compute_event_ordering(std::vector<event_record> const& events) -> std::vector<size_t>;

// Check if the vector of output events is sorted
auto is_events_sorted(std::vector<output_event> const& events) -> bool;

/// Fill in the size of 'FREE' events, based on the size of the corresponding allocation
auto compute_free_sizes(std::vector<output_event>& output_events) -> void;

auto compute_output_events(string_table&                    strtab,
                           std::vector<event_record> const& events,
                           map<addr_t, size_t> const& pc_ids_lookup) -> std::vector<output_event>;

/// Computes a sorted list of all program counters that appear in the event records
auto collect_pcs(std::vector<event_record> const& events) -> std::vector<addr_t>;

/// Given a table of program counters, computes a mapping of each program counter
/// to it's index in the table.
auto compute_pc_lookup(std::vector<addr_t> const& pcs) -> map<addr_t, size_t>;

/// Given the allocations that have occurred over the lifetime of the program,
/// produce an `output_record` - a compact serializable representation of that data
auto make_output_record(alloc_counter const& source) -> output_record;
} // namespace mp
