#pragma once

#include <algorithm>
#include <ankerl/unordered_dense.h>
#include <cpptrace/cpptrace.hpp>
#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <mem_profile/allocator.h>
#include <mem_profile/containers.h>
#include <mem_profile/counters.h>
#include <mp_error/error.h>
#include <mp_types/types.h>
#include <span>
#include <string_view>


namespace mp {
template <class T>
struct view : std::span<T const> {
    using std::span<T const>::span;
};

template <class Range>
view(Range const&)->view<typename Range::value_type>;


inline std::string_view _safe_sv(char const* cstr) {
    if (cstr == nullptr) {
        return std::string_view();
    }
    return std::string_view(cstr);
}
using ankerl::unordered_dense::map;
using ankerl::unordered_dense::set;

struct string_table {
    sv_store& store;

    std::vector<std::string_view>      strtab;
    map<std::string_view, str_index_t> lookup;
    // Some strings come in as c strings with static lifetime.
    // This provides a cache to look up those entries quickly.
    map<addr_t, str_index_t>           cstr_lookup;


    size_t size() const noexcept { return strtab.size(); }

    /// Inserts the given entry into the string table, adding it to the store
    /// if it's a new string. Returns the address at which it was inserted.
    str_index_t insert(std::string_view key) {
        auto it = lookup.find(key);
        if (it == lookup.end()) {
            return insert_static(store.add(key));
        }
        return it->second;
    }


    /// Insert a key that will outlive the string_table
    str_index_t insert_static(std::string_view key) {
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

        auto result = insert_static(_safe_sv(key));

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

    /// Index into type data table
    std::vector<size_t> type_data;

    output_object_info(string_table&                            strtab,
                       view<event_info>                         object_trace,
                       map<_mp_type_data const*, size_t> const& type_data_lookup);
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


struct output_type_data {
    /// size[i] is the size of the i-th type in the table
    std::vector<size_t>      size;
    /// type[i] is the typename of the i-th type in the table
    std::vector<str_index_t> type;

    /// Each time has 0 or more fields.
    ///
    /// For a given type i,
    /// let slice = field_off[i]..field_off[i + 1].
    ///
    ///
    /// Then
    /// - `field_names[slice]` is the name of each field for type i
    /// - `field_types[slice]` is the type of each field for type i
    /// - `field_sizes[slice]` is the size of each field for type i
    /// - `field_offsets[slice]` is the offset of each field for type i
    std::vector<size_t>      field_off;
    std::vector<str_index_t> field_names;
    std::vector<str_index_t> field_types;
    std::vector<size_t>      field_sizes;
    std::vector<size_t>      field_offsets;

    /// Each type has 0 or more bases.
    ///
    /// For a given type i,
    /// let slice = base_off[i]..base_off[i + 1].
    ///
    /// Then
    /// - `base_types[slice]` is the type of each base of type i
    /// - `base_sizes[slice]` is the size of each base of type i
    /// - `base_offsets[slice]` is the offset of each base of type i
    std::vector<size_t>      base_off;
    std::vector<str_index_t> base_types;
    std::vector<size_t>      base_sizes;
    std::vector<size_t>      base_offsets;

    output_type_data(string_table& strtab, view<_mp_type_data const*> type_data);
};


struct output_frame_table {
    /// Table of program counters
    std::vector<addr_t> pc;

    /// Object path
    std::vector<str_index_t> object_path;

    /// Address within the object
    std::vector<addr_t> object_address;

    /// Symbol name within the object (value returned by dladdr)
    std::vector<str_index_t> object_symbol;


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
    std::vector<u32> line;

    /// Soruce File Columnn Number corresponding to program counter. 0 if missing
    std::vector<u32> column;

    /// true if the given frame is inline. 0 if false, 1 if true
    std::vector<u8> is_inline;

    /// Get the program counter for the given entry
    addr_t get_pc(size_t i) { return pc[i]; }

    /// Get the number of frames for the i-th program counter
    /// If inlining occurs, a program counter may have more than one associated
    /// frame.
    size_t frame_count(size_t i) { return offsets[i + 1] - offsets[i]; }

    output_frame_table() = default;
    output_frame_table(string_table&                               strtab,
                       std::vector<addr_t>                         pcs,
                       view<cpptrace::object_frame>     object_frames,
                       view<cpptrace::stacktrace_frame> stack_frames);
};



struct output_record {
    /// Holds entries in the stacktrace.
    output_frame_table frame_table;

    /// Holds type information: type names, type sizes, fields, etc
    output_type_data type_data_table;

    /// Vector of events
    std::vector<output_event> event_table;

    /// String table
    std::vector<std::string_view> strtab;
};

/// Sanity check: we expect that the number of non-inline frames should match
/// the number of program counters
auto run_sanity_check_on_frames(size_t pc_count, view<cpptrace::stacktrace_frame> frames)
    -> void;

/// Given an event_record, compute an ordering that sequences the event record,
/// such that events are ordered by their id.
auto compute_event_ordering(view<event_record> events) -> std::vector<size_t>;

// Check if the vector of output events is sorted
auto is_events_sorted(view<output_event> events) -> bool;

/// Fill in the size of 'FREE' events, based on the size of the corresponding allocation
auto compute_free_sizes(std::vector<output_event>& output_events) -> void;

auto compute_output_events(string_table&                            strtab,
                           view<event_record>            events,
                           map<addr_t, size_t> const&               pc_ids_lookup,
                           map<_mp_type_data const*, size_t> const& type_data_lookup)
    -> std::vector<output_event>;

/// Computes a sorted list of all program counters that appear in the event records
auto collect_pcs(view<event_record> events) -> std::vector<addr_t>;

auto collect_type_data(view<event_record> events) -> std::vector<_mp_type_data const*>;

/// Given the allocations that have occurred over the lifetime of the program,
/// produce an `output_record` - a compact serializable representation of that data
auto make_output_record(alloc_counter const& source, sv_store& store) -> output_record;
} // namespace mp
