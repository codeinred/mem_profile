#include <mem_profile/containers.h>
#include <mem_profile/output_record.h>
#include <mp_hook_prelude.h>

#include <dlfcn.h>
#include <span>

namespace mp {
template <class T>
auto compute_lookup(view<T> values) -> map<T, size_t> {
    auto lookup = map<T, size_t>(values.size() * 2);

    for (size_t i = 0; i < values.size(); i++) {
        lookup[values[i]] = i;
    }

    return lookup;
}

output_record make_output_record(alloc_counter const& source, sv_store& store) {
    auto const& events = source.events();

    auto type_data = collect_type_data(events);

    auto raw_trace = cpptrace::raw_trace{collect_pcs(events)};

    auto object_trace     = raw_trace.resolve_object_trace();
    auto stack_trace      = raw_trace.resolve();
    auto pc_ids_lookup    = compute_lookup(view(raw_trace.frames));
    auto type_data_lookup = compute_lookup(view(type_data));

    string_table strtab{store};

    return output_record{
        output_frame_table(strtab,
                           std::move(raw_trace.frames),
                           object_trace.frames,
                           stack_trace.frames),
        output_type_data(strtab, type_data),
        compute_output_events(strtab, events, pc_ids_lookup, type_data_lookup),
        std::move(strtab.strtab),
    };
}


output_frame_table::output_frame_table(string_table&                    strtab,
                                       std::vector<addr_t>              pcs,
                                       view<cpptrace::object_frame>     object_frames,
                                       view<cpptrace::stacktrace_frame> stack_frames)
  : pc(std::move(pcs))
  , object_path(pc.size())
  , object_address(pc.size())
  , object_symbol(pc.size())
  , offsets(pc.size() + 1)
  , file(stack_frames.size())
  , func(stack_frames.size())
  , line(stack_frames.size())
  , column(stack_frames.size())
  , is_inline(stack_frames.size()) {
    run_sanity_check_on_frames(pc.size(), stack_frames);

    MP_ASSERT_EQ(pc.size(),
                 object_frames.size(),
                 "Expected 1-to-1 relation between object frames and program counters");

    std::vector<Dl_info> dl_info(pc.size());

    for (size_t i = 0; i < pc.size(); i++) {
        auto& info = dl_info[i];
        if (dladdr((const void*)pc[i], &info)) {
            object_path[i]    = strtab.insert_cstr(info.dli_fname);
            object_address[i] = pc[i] - uintptr_t(info.dli_fbase);
            object_symbol[i]  = strtab.insert_cstr(info.dli_sname);
        } else {
            info = {nullptr, nullptr, nullptr, nullptr};

            object_path[i]    = strtab.insert(object_frames[i].object_path);
            object_address[i] = object_frames[i].object_address;
            object_symbol[i]  = strtab.insert_cstr("");
        }
    }

    size_t trace_size = stack_frames.size();
    size_t pc_i       = 0;
    for (size_t i = 0; i < trace_size; i++) {
        auto const& frame = stack_frames[i];
        column[i]         = frame.column.value_or(0);
        line[i]           = frame.line.value_or(0);
        file[i]           = strtab.insert(frame.filename);
        func[i]           = strtab.insert(frame.symbol);
        is_inline[i]      = frame.is_inline;

        // Occasionally, cpptrace gives us incorrect info (eg, empty/missing symbol, invalid object address)
        // We will use `dladdr` to attempt to repair this information as best we can.
        bool info_needs_repair = object_frames[pc_i].object_address == 0 || frame.symbol.empty();

        if (info_needs_repair) {
            Dl_info info = dl_info[pc_i];

            if (info.dli_fname) file[i] = strtab.insert_cstr(info.dli_fname);
            if (info.dli_sname) func[i] = strtab.insert(cpptrace::demangle(info.dli_sname));
        }

        // If we finally encountered our non-inline frame, update the offsets
        if (!frame.is_inline) {
            offsets[++pc_i] = i + 1;
        }
    }
}


auto compute_output_events(string_table&                            strtab,
                           view<event_record>                       events,
                           map<addr_t, size_t> const&               pc_ids_lookup,
                           map<_mp_type_data const*, size_t> const& type_data_lookup)
    -> std::vector<output_event> {
    size_t events_size    = events.size();
    auto   output_events  = std::vector<output_event>(events_size);
    auto   event_ordering = compute_event_ordering(events);

    for (size_t i = 0; i < events_size; i++) {
        // Ensure that events are sequenced
        auto const& e = events[event_ordering[i]];

        std::vector<size_t> pc_ids(e.trace.size());
        for (size_t i = 0; i < e.trace.size(); i++) {
            pc_ids[i] = pc_ids_lookup.at(e.trace[i]);
        }

        output_events[i] = output_event{
            e.id,
            e.type,
            e.alloc_size,
            uintptr_t(e.alloc_ptr),
            uintptr_t(e.alloc_hint),
            std::move(pc_ids),
        };
        if (!e.object_trace.empty()) {
            output_events[i].object_info
                = output_object_info(strtab, e.object_trace, type_data_lookup);
        }
    }

    compute_free_sizes(output_events);

    return output_events;
}



auto compute_event_ordering(view<event_record> events) -> std::vector<size_t> {
    std::vector<size_t> event_ordering(events.size());
    for (size_t i = 0; i < events.size(); i++) {
        event_ordering[i] = i;
    }
    std::sort(event_ordering.begin(), event_ordering.end(), [&](size_t i, size_t j) -> bool {
        return events[i].id < events[j].id;
    });
    return event_ordering;
}


void compute_free_sizes(std::vector<output_event>& output_events) {
    MP_ASSERT_EQ(is_events_sorted(output_events),
                 true,
                 "Expected events to be ordered by event id by this stage");

    // Because events are ordered by event id, that means that every
    // free is sequenced after it's corresponding allocation. So we can
    // record the sizes of all frees by running through all of the events

    auto alloc_sizes = map<addr_t, size_t>(output_events.size());

    for (auto& event : output_events) {
        auto& saved_size = alloc_sizes[event.alloc_addr];
        if (event.type != event_type::FREE) {
            saved_size = event.alloc_size; // Save the size if it's an allocation
        } else {
            event.alloc_size = saved_size; // Use the saved size if it's a free
        }
    }
}

void run_sanity_check_on_frames(size_t pc_count, view<cpptrace::stacktrace_frame> frames) {
    size_t non_inline_frame_count = 0;
    for (auto const& frame : frames) {
        non_inline_frame_count += !frame.is_inline;
    }

    MP_ASSERT_EQ(non_inline_frame_count,
                 pc_count,
                 "The number of non_inline frames must match the number of program counters");
}
bool is_events_sorted(view<output_event> events) {
    return std::is_sorted(
        events.begin(),
        events.end(),
        [](output_event const& rhs, output_event const& lhs) { return rhs.id < lhs.id; });
}



std::vector<addr_t> collect_pcs(view<event_record> events) {
    ankerl::unordered_dense::set<addr_t> pc_set;
    for (auto const& event : events) {
        pc_set.insert(event.trace.begin(), event.trace.end());
    }
    std::vector<addr_t> pcs(pc_set.begin(), pc_set.end());
    std::sort(pcs.data(), pcs.data() + pcs.size());
    return pcs;
}


auto collect_type_data(view<event_record> events) -> std::vector<_mp_type_data const*> {
    set<_mp_type_data const*> type_data;
    type_data.max_load_factor(0.5);

    for (auto const& e : events) {
        for (auto const& obj : e.object_trace) {
            type_data.insert(obj.type_data);
        }
    }

    std::vector<_mp_type_data const*> values(type_data.begin(), type_data.end());
    // Sorting it now will help enable better cache locality when we access it later
    std::sort(values.begin(), values.end());
    return values;
}


output_object_info::output_object_info(string_table&                            strtab,
                                       view<event_info>                         object_trace,
                                       map<_mp_type_data const*, size_t> const& type_data_lookup)
  : trace_index(object_trace.size())
  , object_id(object_trace.size())
  , addr(object_trace.size())
  , size(object_trace.size())
  , type(object_trace.size())
  , type_data(object_trace.size()) {
    size_t i = 0;
    for (auto const& event : object_trace) {
        trace_index[i] = event.trace_index;
        object_id[i]   = event.event_id;
        addr[i]        = event.object_ptr;
        size[i]        = event.type_data->size;
        type[i]        = strtab.insert_cstr(event.type_data->type);
        type_data[i]   = type_data_lookup.at(event.type_data);
        ++i;
    }
}


output_type_data::output_type_data(string_table& strtab, view<_mp_type_data const*> type_data)
  : size(type_data.size())
  , type(type_data.size())
  , field_off(type_data.size() + 1)
  , base_off(type_data.size() + 1) {

    size_t count        = type_data.size();
    size_t total_fields = 0;
    size_t total_bases  = 0;
    for (size_t i = 0; i < count; i++) {
        auto const& ent = *type_data[i];
        size[i]         = ent.size;
        type[i]         = strtab.insert_cstr(ent.type);

        total_fields += ent.field_count;
        total_bases += ent.base_count;
        field_off[i + 1] = total_fields;
        base_off[i + 1]  = total_bases;
    }


    // Fill in data related to the fields and bases of each type

    field_names.resize(total_fields);
    field_types.resize(total_fields);
    field_sizes.resize(total_fields);
    field_offsets.resize(total_fields);

    base_types.resize(total_bases);
    base_sizes.resize(total_bases);
    base_offsets.resize(total_bases);

    size_t field_i = 0;
    size_t base_i  = 0;
    for (size_t i = 0; i < count; i++) {
        auto const& ent         = *type_data[i];
        size_t      field_count = ent.field_count;
        size_t      base_count  = ent.base_count;

        std::copy_n(ent.field_sizes, field_count, field_sizes.data() + field_i);
        std::copy_n(ent.field_offsets, field_count, field_offsets.data() + field_i);

        for (size_t j = 0; j < field_count; j++) {
            field_names[field_i + j] = strtab.insert_cstr(ent.field_names[j]);
            field_types[field_i + j] = strtab.insert_cstr(ent.field_types[j]);
        }

        std::copy_n(ent.base_sizes, base_count, base_sizes.data() + base_i);
        std::copy_n(ent.base_offsets, base_count, base_offsets.data() + base_i);

        for (size_t j = 0; j < base_count; j++) {
            base_types[base_i + j] = strtab.insert_cstr(ent.base_types[j]);
        }
        field_i += field_count;
        base_i += base_count;
    }
}
} // namespace mp
