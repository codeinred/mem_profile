#include <algorithm>
#include <mp_unwind/mp_unwind.h>

#include <mp_core/colors.h>
#include <mp_core/types.h>

#include <libunwind.h>

//#include <fmt/core.h>
#include <cstdio>
#include <string>
namespace mp {

void dump(u64 const* block, size_t size) {
    puts("Block:");

    for (size_t i = 0; i < size; i++) {
        printf("  %p\n", (void const*)block[i]);
    }
}



void mp_unwind_show_trace() {
// These macros are colored tags used for printing
#define s_frame_start MP_COLOR_BB "frame_start:" MP_COLOR_Re
#define s_frame_end MP_COLOR_BB "frame_end:" MP_COLOR_Re
#define s_frame_size MP_COLOR_BB "frame_size:" MP_COLOR_Re
#define s_frame_info MP_COLOR_BB "frame_info:" MP_COLOR_Re
#define s_tag MP_COLOR_BB "tag:" MP_COLOR_Re
#define s_call_count MP_COLOR_BB "call_count:" MP_COLOR_Re
#define s_this_size MP_COLOR_BB "this_size:" MP_COLOR_Re
#define s_this_ptr MP_COLOR_BB "this_ptr:" MP_COLOR_Re
#define s_type_name MP_COLOR_BB "type_name:" MP_COLOR_Re
#define s_checksum MP_COLOR_BB "checksum:" MP_COLOR_Re


    char          function_name[8192];
    unw_cursor_t  cursor;
    unw_context_t uc;
    unw_word_t    ip, sp, offset;
    using namespace colors;

    constexpr size_t _mp_frame_information_elem_count =
        sizeof(_mp_frame_information) / sizeof(ull_t);

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
        bool has_name =
            unw_get_proc_name(&cursor, function_name, sizeof(function_name), &offset) == 0;
        char const* name = has_name ? function_name : "???";
        names[count]     = name;
        count++;
    } while (unw_step(&cursor) > 0);

    for (int i = 0; i < count - 1; i++) {
        unw_word_t  frame_end   = spp[i + 1];
        unw_word_t  frame_start = spp[i];
        unw_word_t  frame_size  = frame_end - frame_start;
        auto const& name        = names[i];

        printf("%s%s%s\n", BG, name.c_str(), Re);
        printf("├── " s_frame_start " %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re, frame_start, frame_start);
        printf("├── " s_frame_end "   %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re, frame_end, frame_end);
        printf("├── " s_frame_size "  %-16lu   " MP_COLOR_GRAY "# 0x%016lx\n" MP_COLOR_Re, frame_size, frame_size);

        ull_t const* frame_start_ptr = (ull_t const*)frame_start;
        ull_t const* frame_end_ptr   = (ull_t const*)frame_end;


        int64_t search_limit =
            (frame_end_ptr - frame_start_ptr) - _mp_frame_information_elem_count + 1;

        int search_size = std::min(search_limit, int64_t(16));

        bool has_frame_info = false;
        for (int i = 0; i < search_size; i++) {
            if (frame_start_ptr[i] == _mp_frame_tag) {
                _mp_frame_information info;
                __builtin_memcpy(&info, frame_start_ptr + i, sizeof(info));

                bool check_good = (info.tag ^ info.call_count) == info.checksum;
                if (check_good) {
                    printf("└── " s_frame_info MP_COLOR_GRAY "  # (at stack[%d] in frame)\n" MP_COLOR_Re
                           "    ├── " s_tag "        %llu\n"
                           "    ├── " s_call_count " %llu\n"
                           "    ├── " s_this_size "  %lu\n"
                           "    ├── " s_this_ptr "   %p\n"
                           "    ├── " s_type_name "  " MP_COLOR_BM "\"%s\"\n" MP_COLOR_Re
                           "    └── " s_checksum "   %llu  " MP_COLOR_GRAY "# (checksum good)\n" MP_COLOR_Re,
                           i,
                           info.tag,
                           info.call_count,
                           info.this_size,
                           info.this_ptr,
                           info.type_name,
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
