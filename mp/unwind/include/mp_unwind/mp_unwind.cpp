#include <algorithm>
#include <mp_unwind/mp_unwind.h>

#include <mp_core/colors.h>
#include <mp_types/types.h>

#include <libunwind.h>

//#include <fmt/core.h>
#include <bit>
#include <cstdio>
#include <fmt/format.h>
#include <string>
#include <string_view>

namespace mp {

namespace {
std::string_view getErrorMessage(int errc) {
    switch (errc) {
    case UNW_ESUCCESS:     return "no error";
    case UNW_EUNSPEC:      return "unspecified (general) error";
    case UNW_ENOMEM:       return "out of memory";
    case UNW_EBADREG:      return "bad register number";
    case UNW_EREADONLYREG: return "attempt to write read-only register";
    case UNW_ESTOPUNWIND:  return "stop unwinding";
    case UNW_EINVALIDIP:   return "invalid IP";
    case UNW_EBADFRAME:    return "bad frame";
    case UNW_EINVAL:       return "unsupported operation or bad value";
    case UNW_EBADVERSION:  return "unwind info has unsupported version";
    case UNW_ENOINFO:      return "no unwind info found";
    }
    return "<unknown error code>";
}

struct CheckUnwindError {
    char const* context;

    int throw_if_bad(int errc) const {
        if (errc < 0) {
            throw std::runtime_error(
                fmt::format("{}. {} (code={})", context, getErrorMessage(errc), errc));
        }
        return errc;
    }
};

int operator|(int errc, CheckUnwindError e) { return e.throw_if_bad(errc); }

CheckUnwindError check(char const* msg) { return CheckUnwindError{msg}; }

void dec_ipp(uintptr_t* ipp, size_t count) {
    for (size_t i = 0; i < count; i++) {
        ipp[i] -= 1;
    }
}

} // namespace

size_t mp_unwind(size_t max_frames, uintptr_t* ipp, uintptr_t* spp) {
    unw_cursor_t  cursor;
    unw_context_t uc;

    unw_getcontext(&uc) | check("mp_unwind: Unable to get context");
    unw_init_local(&cursor, &uc) | check("mp_unwind: Unable to initialize cursor.");

    size_t i = 0;
    for (; i < max_frames; i++) {
        unw_get_reg(&cursor, UNW_REG_IP, ipp + i) | check("mp_unwind: Cannot read UNW_REG_IP");
        unw_get_reg(&cursor, UNW_REG_SP, spp + i) | check("mp_unwind: Cannot read UNW_REG_SP");
        int step_result = unw_step(&cursor) | check("mp_unwind: unable to step");
        // We reached the final frame
        if (step_result == 0) break;
    }

    dec_ipp(ipp, i);
    return i;
}

size_t mp_unwind(size_t max_frames, uintptr_t* ipp) {
    unw_cursor_t  cursor;
    unw_context_t uc;

    unw_getcontext(&uc) | check("mp_unwind: Unable to get context");
    unw_init_local(&cursor, &uc) | check("mp_unwind: Unable to initialize cursor.");

    size_t i = 0;
    for (; i < max_frames; i++) {
        unw_get_reg(&cursor, UNW_REG_IP, ipp + i) | check("mp_unwind: Cannot read UNW_REG_IP");
        int step_result = unw_step(&cursor) | check("mp_unwind: unable to step");
        // We reached the final frame
        if (step_result == 0) break;
    }

    dec_ipp(ipp, i);
    return i;
}



size_t mp_extract_events(size_t           max_events,
                         event_info*      event_buffer,
                         size_t           spp_count,
                         const uintptr_t* spp) {
    constexpr size_t _mp_frame_information_elem_count
        = sizeof(_mp_frame_information) / sizeof(ull_t);

    size_t event_i = 0;
    for (size_t spp_i = 0; spp_i < spp_count - 1; spp_i++) {
        unw_word_t frame_end   = spp[spp_i + 1];
        unw_word_t frame_start = spp[spp_i];

        ull_t const* frame_start_ptr = (ull_t const*)frame_start;
        ull_t const* frame_end_ptr   = (ull_t const*)frame_end;


        int64_t search_limit
            = (frame_end_ptr - frame_start_ptr) - _mp_frame_information_elem_count + 1;

        int search_size = std::min(search_limit, int64_t(16));

        for (int i = 0; i < search_size; i++) {
            if (frame_start_ptr[i] == _mp_frame_tag) {
                _mp_frame_information info;
                __builtin_memcpy(&info, frame_start_ptr + i, sizeof(info));

                bool check_good = mp::check_frame(frame_start_ptr + i);
                if (check_good) {
                    if (event_i == max_events) return event_i;

                    event_buffer[event_i++] = {
                        spp_i,
                        info.call_count,
                        (uintptr_t)info.this_ptr,
                        info.type_data,
                    };
                }
                break;
            }
        }
    }

    return event_i;
}


void dump(u64 const* block, size_t size) {
    puts("Block:");

    for (size_t i = 0; i < size; i++) {
        printf("  %p\n", (void const*)block[i]);
    }
}



void mp_unwind_show_trace() {
// These macros are colored tags used for printing
#define s_reg_ip MP_COLOR_BB "reg_ip:" MP_COLOR_Re
#define s_frame_start MP_COLOR_BB "frame_start:" MP_COLOR_Re
#define s_frame_end MP_COLOR_BB "frame_end:" MP_COLOR_Re
#define s_frame_size MP_COLOR_BB "frame_size:" MP_COLOR_Re
#define s_frame_info MP_COLOR_BB "frame_info:" MP_COLOR_Re
#define s_tag MP_COLOR_BB "tag:" MP_COLOR_Re
#define s_call_count MP_COLOR_BB "call_count:" MP_COLOR_Re
#define s_this_ptr MP_COLOR_BB "this_ptr:" MP_COLOR_Re
#define s_checksum MP_COLOR_BB "checksum:" MP_COLOR_Re
#define s_type_data MP_COLOR_BB "type_data:" MP_COLOR_Re
#define s_size MP_COLOR_BB "size:" MP_COLOR_Re
#define s_type MP_COLOR_BB "type:" MP_COLOR_Re
#define s_base_count MP_COLOR_BB "base_count:" MP_COLOR_Re
#define s_field_count MP_COLOR_BB "field_count:" MP_COLOR_Re
#define s_field MP_COLOR_BB "field:" MP_COLOR_Re
#define s_fields MP_COLOR_BB "fields:" MP_COLOR_Re
#define s_bases MP_COLOR_BB "bases:" MP_COLOR_Re

    char          function_name[8192];
    unw_cursor_t  cursor;
    unw_context_t uc;
    unw_word_t    ip, sp, offset;
    using namespace colors;

    constexpr size_t _mp_frame_information_elem_count
        = sizeof(_mp_frame_information) / sizeof(ull_t);

    unw_getcontext(&uc);
    unw_init_local(&cursor, &uc);

    puts("Begin backtrace");

    unw_word_t  ipp[1024];
    unw_word_t  spp[1024];
    std::string names[1024];
    size_t      count = 0;
    do {
        unw_get_reg(&cursor, UNW_REG_IP, &ip);
        unw_get_reg(&cursor, UNW_REG_SP, &sp);
        ipp[count] = ip;
        spp[count] = sp;
        bool has_name
            = unw_get_proc_name(&cursor, function_name, sizeof(function_name), &offset) == 0;
        char const* name = has_name ? function_name : "???";
        names[count]     = name;
        count++;
    } while (unw_step(&cursor) > 0);

    for (size_t i = 0; i < count - 1; i++) {
        unw_word_t  frame_end   = spp[i + 1];
        unw_word_t  frame_start = spp[i];
        unw_word_t  frame_size  = frame_end - frame_start;
        auto const& name        = names[i];

        printf("%s%s%s\n", BG, name.c_str(), Re);
        printf("├── " s_reg_ip "      %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re,
               ipp[i],
               ipp[i]);
        printf("├── " s_frame_start " %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re,
               frame_start,
               frame_start);
        printf("├── " s_frame_end "   %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re,
               frame_end,
               frame_end);
        printf("├── " s_frame_size "  %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re,
               frame_size,
               frame_size);

        ull_t const* frame_start_ptr = (ull_t const*)frame_start;
        ull_t const* frame_end_ptr   = (ull_t const*)frame_end;


        int64_t search_limit
            = (frame_end_ptr - frame_start_ptr) - _mp_frame_information_elem_count + 1;

        int search_size = std::min(search_limit, int64_t(16));

        bool has_frame_info = false;
        for (int i = 0; i < search_size; i++) {
            if (frame_start_ptr[i] == _mp_frame_tag) {
                _mp_frame_information info;
                __builtin_memcpy(&info, frame_start_ptr + i, sizeof(info));

                bool check_good = mp::check_frame(frame_start_ptr + i);
                auto type_data  = *info.type_data;
                if (check_good) {
                    printf("└── " s_frame_info MP_COLOR_GRAY
                           "  # (at stack[%d] in frame)\n" MP_COLOR_Re //
                           "    ├── " s_tag "        %llu\n"
                           "    ├── " s_call_count " %llu\n"
                           "    ├── " s_this_ptr "   %p\n"
                           "    ├── " s_type_data "\n"
                           "    │   ├── " s_type "         " MP_COLOR_M "%s" MP_COLOR_Re "\n"
                           "    │   ├── " s_size "         %zu\n"
                           "    │   ├── " s_base_count "   %zu\n"
                           "    │   ├── " s_bases "\n",
                           i,
                           info.tag,
                           info.call_count,
                           info.this_ptr,
                           type_data.type,
                           type_data.size,
                           type_data.base_count);

                    for (size_t i = 0; i < type_data.base_count; i++) {
                        char const* joiner = "├── ";
                        if (i == type_data.base_count - 1) {
                            joiner = "└── ";
                        }

                        size_t      off0 = type_data.base_offsets[i];
                        size_t      off1 = off0 + type_data.base_sizes[i];
                        char const* ty   = type_data.base_types[i];
                        printf("    │   │   %s       " MP_COLOR_BY "%4zu..%4zu" MP_COLOR_Re
                               ": " MP_COLOR_M "%s" MP_COLOR_Re "\n",
                               joiner,
                               off0,
                               off1,
                               ty);
                    }

                    printf("    │   ├── " s_field_count "  %zu\n"
                           "    │   └── " s_fields "\n",
                           type_data.field_count);

                    for (size_t i = 0; i < type_data.field_count; i++) {
                        char const* joiner = "├── ";
                        if (i == type_data.field_count - 1) {
                            joiner = "└── ";
                        }

                        size_t      off0 = type_data.field_offsets[i];
                        size_t      off1 = off0 + type_data.field_sizes[i];
                        char const* ty   = type_data.field_types[i];
                        char const* name = type_data.field_names[i];
                        printf("    │       %s       " MP_COLOR_BY "%4zu..%4zu" MP_COLOR_Re
                               ": " MP_COLOR_M "%s " MP_COLOR_BG "%s" MP_COLOR_Re " \n",
                               joiner,
                               off0,
                               off1,
                               ty,
                               name);
                    }

                    printf("    └── " s_checksum "   %llu  " MP_COLOR_GRAY
                           "# (checksum good)\n" MP_COLOR_Re,
                           info.checksum);
                } else {
                    printf("└── " s_frame_info "  <found frame with bad checksum>\n");
                }
                has_frame_info = true;
                break;
            }
        }
        if (!has_frame_info) {
            printf("└── " s_frame_info "  <none>\n");
        }
    }

    puts("End backtrace");
}
} // namespace mp
