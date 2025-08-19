#include <mem_profile/output_record.h>


namespace mp {
output_record make_output_record(alloc_counter const& source) {
    auto const& events = source.events();

    auto raw_trace = cpptrace::raw_trace{collect_pcs(events)};

    auto object_trace  = raw_trace.resolve_object_trace();
    auto stack_trace   = raw_trace.resolve();
    auto pc_ids_lookup = compute_pc_lookup(raw_trace.frames);

    stack_trace.print();

    string_table strtab;

    return output_record{
        output_frame_table(strtab,
                           std::move(raw_trace.frames),
                           object_trace.frames,
                           stack_trace.frames),
        compute_output_events(strtab, events, pc_ids_lookup),
        std::move(strtab.strtab),
    };
}


output_frame_table::output_frame_table(string_table&                                  strtab,
                                       std::vector<addr_t>                            pcs,
                                       std::vector<cpptrace::object_frame> const&     object_frames,
                                       std::vector<cpptrace::stacktrace_frame> const& stack_frames)
  : pc(std::move(pcs))
  , object_path(pc.size())
  , object_address(pc.size())
  , offsets(pc.size() + 1)
  , file(stack_frames.size())
  , func(stack_frames.size())
  , line(stack_frames.size())
  , column(stack_frames.size())
  , is_inline(stack_frames.size()) {
    run_sanity_check_on_frames(pc.size(), stack_frames);

    MP_ASSERT_EQ(pc.size(),
                 object_frames.size(),
                 "Expected 1-to-1 relation betwween object frames and program counters");

    for (size_t i = 0; i < pc.size(); i++) {
        object_path[i]    = strtab.insert(object_frames[i].object_path);
        object_address[i] = object_frames[i].object_address;
    }

    size_t trace_size = stack_frames.size();
    size_t pc_i       = 0;
    for (size_t i = 0; i < trace_size; i++) {
        auto const& frame = stack_frames[i];
        if (!frame.is_inline) {
            offsets[++pc_i] = i + 1;
        }
        column[i]    = frame.column.value_or(0);
        line[i]      = frame.line.value_or(0);
        file[i]      = strtab.insert(frame.filename);
        func[i]      = strtab.insert(frame.symbol);
        is_inline[i] = frame.is_inline;
    }
}


auto compute_output_events(string_table&                    strtab,
                           std::vector<event_record> const& events,
                           map<addr_t, size_t> const& pc_ids_lookup) -> std::vector<output_event> {
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
            output_events[i].object_info = output_object_info(strtab, e.object_trace);
        }
    }

    compute_free_sizes(output_events);

    return output_events;
}



auto compute_event_ordering(std::vector<event_record> const& events) -> std::vector<size_t> {
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

void run_sanity_check_on_frames(size_t                                         pc_count,
                                std::vector<cpptrace::stacktrace_frame> const& frames) {
    size_t non_inline_frame_count = 0;
    for (auto const& frame : frames) {
        non_inline_frame_count += !frame.is_inline;
    }

    MP_ASSERT_EQ(non_inline_frame_count,
                 pc_count,
                 "The number of non_inline frames must match the number of program counters");
}
bool is_events_sorted(std::vector<output_event> const& events) {
    return std::is_sorted(
        events.begin(),
        events.end(),
        [](output_event const& rhs, output_event const& lhs) { return rhs.id < lhs.id; });
}
map<addr_t, size_t> compute_pc_lookup(std::vector<addr_t> const& pcs) {
    map<addr_t, size_t> pc_ids_lookup(pcs.size() * 2);

    for (size_t i = 0; i < pcs.size(); i++) {
        pc_ids_lookup[pcs[i]] = i;
    }

    return pc_ids_lookup;
}
std::vector<addr_t> collect_pcs(std::vector<event_record> const& events) {
    ankerl::unordered_dense::set<addr_t> pc_set;
    for (auto const& event : events) {
        pc_set.insert(event.trace.begin(), event.trace.end());
    }
    std::vector<addr_t> pcs(pc_set.begin(), pc_set.end());
    std::sort(pcs.data(), pcs.data() + pcs.size());
    return pcs;
}
output_object_info::output_object_info(string_table&                  strtab,
                                       std::vector<event_info> const& object_trace)
  : trace_index(object_trace.size())
  , object_id(object_trace.size())
  , addr(object_trace.size())
  , size(object_trace.size())
  , type(object_trace.size()) {
    size_t i = 0;
    for (auto const& event : object_trace) {
        trace_index[i] = event.trace_index;
        object_id[i]   = event.event_id;
        addr[i]        = event.object_ptr;
        size[i]        = event.object_size;
        type[i]        = strtab.insert_cstr(event.object_type_name);
        ++i;
    }
}

} // namespace mp
