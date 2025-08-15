#pragma once

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <cpptrace/cpptrace.hpp>
#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <mem_profile/counters.h>
#include <mp_error/error.h>
#include <mp_types/types.h>

#define MP_GLZ_ENTRY2(a, b) a, b

#define MP_GLZ_ENTRY(type, name) #name, &type::name

namespace mp {

struct output_event {
    /// A unique 64-bit stamp that can be used to order events
    /// chronologically. Also uniquely identifies an event.
    /// The first event should have an id of 0
    u64 id;

    /// Event type
    EventType type;

    /// Size of allocation (if applicable, 0 for free)
    size_t alloc_size;

    /// Allocated pointer (or pointer passed  to free)
    u64 alloc_addr;

    /// Pointer passed as input (eg to realloc())
    u64 alloc_hint;

    // Call stack, expressed as a vector of program counter ids
    std::vector<size_t> pc_id;
};
} // namespace mp

template <> struct glz::meta<mp::output_event> {
    using T                     = mp::output_event;
    constexpr static auto value = object(
        //
        MP_GLZ_ENTRY(mp::output_event, id),
        MP_GLZ_ENTRY(mp::output_event, type),
        MP_GLZ_ENTRY(mp::output_event, alloc_size),
        MP_GLZ_ENTRY(mp::output_event, alloc_addr),
        MP_GLZ_ENTRY(mp::output_event, alloc_hint),
        MP_GLZ_ENTRY(mp::output_event, pc_id)
        //
    );
};

namespace mp {

struct file_loc {
    u32 lineno{};
    u32 colno{};
};

struct output_record {
    /// String table
    std::vector<std::string> strtab;

    /// Table of program counters
    std::vector<Addr>   pc;
    /// Source file corresponding to program counter (index into strtab)
    std::vector<size_t> pc_filename;
    /// Function name corresponding to program counter (index into strtab)
    std::vector<size_t> pc_func;
    /// Symbol name corresponding to program counter (index into strtab)
    //std::vector<size_t> pc_sym_name;
    /// Source File Line Number corresponding to program counter
    std::vector<u32>    pc_line;
    /// Soruce File Columnn Number corresponding to program counter
    std::vector<u32>    pc_column;

    /// Vector of events
    std::vector<output_event> events;
};
} // namespace mp


template <> struct glz::meta<mp::output_record> {
    using T                     = mp::output_record;
    static constexpr auto value = glz::object(
        //
        MP_GLZ_ENTRY(mp::output_record, strtab),
        MP_GLZ_ENTRY(mp::output_record, pc),
        MP_GLZ_ENTRY(mp::output_record, pc_filename),
        MP_GLZ_ENTRY(mp::output_record, pc_func),
        MP_GLZ_ENTRY(mp::output_record, pc_line),
        MP_GLZ_ENTRY(mp::output_record, pc_column),
        MP_GLZ_ENTRY(mp::output_record, events)
        //
    );
};


namespace mp {
struct string_table {
    std::vector<std::string>                               strtab;
    ankerl::unordered_dense::map<std::string_view, size_t> lookup;


    size_t size() const noexcept { return strtab.size(); }

    /// Inserts the given entry into the string table. Returns the address
    /// at which it was inserted.
    size_t insert(std::string_view key) {
        size_t index_if_new = size();

        auto [it, is_new] = lookup.try_emplace(key, index_if_new);

        // If it's not a new entry, return the index where it was found.
        if (!is_new) return it->second;

        strtab.emplace_back(key);
        return index_if_new;
    }
};


inline output_record make_output_record(AllocCounter const& source) {
    auto const& events      = source.events();
    size_t      events_size = events.size();

    ankerl::unordered_dense::set<Addr> pc_set;
    for (auto const& event : events) {
        pc_set.insert(event.trace.begin(), event.trace.end());
    }
    std::vector<Addr> pcs(pc_set.begin(), pc_set.end());
    std::sort(pcs.data(), pcs.data() + pcs.size());
    ankerl::unordered_dense::map<Addr, size_t> pc_ids_lookup(pcs.size() * 2);

    for (size_t i = 0; i < pcs.size(); i++) {
        pc_ids_lookup[pcs[i]] = i;
    }

    // Get the trace
    auto trace = [&]() -> cpptrace::stacktrace {
        cpptrace::raw_trace trace;
        trace.frames = pcs;
        return trace.resolve();
    }();
    trace.print();
    size_t trace_size = trace.frames.size();

    MP_ASSERT_EQ(trace.frames.size(), pcs.size(), "trace_size must match pcs.size()");


    output_record record;

    record.pc = pcs;
    record.pc_column.resize(trace_size);
    record.pc_line.resize(trace_size);
    record.pc_func.resize(trace_size);
    record.pc_filename.resize(trace_size);



    string_table strtab;

    for (size_t i = 0; i < trace_size; i++) {
        auto const& frame     = trace.frames[i];
        record.pc_column[i]   = frame.column.value_or(0);
        record.pc_line[i]     = frame.line.value_or(0);
        record.pc_filename[i] = strtab.insert(frame.filename);
        record.pc_func[i]     = strtab.insert(frame.symbol);
    }
    record.strtab = std::move(strtab.strtab);

    record.events.resize(events_size);
    for (size_t i = 0; i < events_size; i++) {
        auto const& e = events[i];

        std::vector<size_t> pc_ids(e.trace.size());
        for (size_t i = 0; i < e.trace.size(); i++) {
            pc_ids[i] = pc_ids_lookup[e.trace[i]];
        }

        record.events[i] = output_event{
            e.id,
            e.type,
            e.alloc_size,
            uintptr_t(e.alloc_ptr),
            uintptr_t(e.alloc_hint),
            std::move(pc_ids),
        };
    }

    return record;
}
} // namespace mp
