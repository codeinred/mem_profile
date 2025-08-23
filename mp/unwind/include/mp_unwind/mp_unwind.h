#pragma once

#include <mp_types/types.h>
#include <mp_types/export.h>
namespace mp {


struct event_info {
    /// Index into the trace where the event was discovered
    size_t trace_index;
    /// there is an atomic global event counter used to ensure
    /// that each event can be uniquely recorded and distinguished
    /// from other events. This is the id for that event
    ull_t  event_id;

    /// Size of object being acted on
    size_t object_size;

    /// This pointer of object being acted on
    uintptr_t object_ptr;

    /// Name of the object's type
    char const* object_type_name;
};

struct stack_counts {
    size_t frame_count;
    size_t event_count;
};

/// Unwind the stack. Only record instruction pointers, not stack pointers
size_t mp_unwind(size_t max_frames, uintptr_t* ipp);

/// Performs stack unwind. Unwinds up to max_frames. Returns the number of frames unwound.
size_t mp_unwind(size_t max_frames, uintptr_t* ipp, uintptr_t* spp);

/// Scan the stack for event info. `spp` is an array of stack locations
/// delimiting each stack framej
size_t mp_extract_events(size_t           max_events,
                         event_info*      event_buffer,
                         size_t           spp_count,
                         uintptr_t const* spp);


/// Print a trace to standard out
MP_EXPORT void mp_unwind_show_trace();
} // namespace mp
