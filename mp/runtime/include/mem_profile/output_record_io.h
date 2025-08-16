#pragma once

#include <glaze/glaze.hpp>
#include <mem_profile/output_record.h>

#define MP_GLZ_ENTRY(type, name) #name, &type::name

template <> struct glz::meta<mp::event_type> {
    using enum mp::event_type;
    static constexpr auto value = enumerate(FREE, ALLOC, REALLOC);
};

template <> struct glz::meta<mp::output_object_info> {
    using T                     = mp::output_object_info;
    constexpr static auto value = object(
        //
        MP_GLZ_ENTRY(mp::output_object_info, trace_index),
        MP_GLZ_ENTRY(mp::output_object_info, object_id),
        MP_GLZ_ENTRY(mp::output_object_info, addr),
        MP_GLZ_ENTRY(mp::output_object_info, size),
        MP_GLZ_ENTRY(mp::output_object_info, type)
        //
    );
};

template <> struct glz::meta<mp::output_event> {
    using T                     = mp::output_event;
    constexpr static auto value = object(
        //
        MP_GLZ_ENTRY(mp::output_event, id),
        MP_GLZ_ENTRY(mp::output_event, type),
        MP_GLZ_ENTRY(mp::output_event, alloc_size),
        MP_GLZ_ENTRY(mp::output_event, alloc_addr),
        MP_GLZ_ENTRY(mp::output_event, alloc_hint),
        MP_GLZ_ENTRY(mp::output_event, pc_id),
        MP_GLZ_ENTRY(mp::output_event, object_info)
        //
    );
};


template <> struct glz::meta<mp::output_frame_table> {
    using T                     = mp::output_frame_table;
    constexpr static auto value = object(
        //
        MP_GLZ_ENTRY(mp::output_frame_table, pc),
        MP_GLZ_ENTRY(mp::output_frame_table, offsets),
        MP_GLZ_ENTRY(mp::output_frame_table, file),
        MP_GLZ_ENTRY(mp::output_frame_table, func),
        MP_GLZ_ENTRY(mp::output_frame_table, line),
        MP_GLZ_ENTRY(mp::output_frame_table, column),
        MP_GLZ_ENTRY(mp::output_frame_table, is_inline)
        //
    );
};


template <> struct glz::meta<mp::output_record> {
    using T                     = mp::output_record;
    static constexpr auto value = glz::object(
        //
        MP_GLZ_ENTRY(mp::output_record, frame_table),
        MP_GLZ_ENTRY(mp::output_record, events),
        MP_GLZ_ENTRY(mp::output_record, strtab)
        //
    );
};
