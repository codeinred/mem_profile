#include <cli/cli.h>
#include <filesystem>

using namespace cli;
namespace fs = std::filesystem;

struct Args {
    bool     profile_children = false;
    fs::path output_file      = "malloc_stats.json";
};

constexpr static auto parser = cli::arg_parser{
    prelude{
        "mem_profile",
        "",
        "An Ownership-Aware Memory Profiler",
        "",
        "USAGE",
        "",
        "  mem_profile [options] <program> <program_args>...",
        "  mem_profile [options] -- <program> <program_args>...",
        "",
    },
    flag_closure{
        flag{help_flag{}, "Print help info for mem_profile"},
        flag{
            bool_flag<&Args::profile_children>{"--profile-children"},
            "Profile child processes spawned by the parent",
        },
        flag{
            string_flag<&Args::output_file>("-o", "--output"),
            "Output file to place statistics (default: malloc_stats.json)",
        },
    },
};

int main(int argc, char const* argv[]) {

    Args dest;

    auto cursor = parser.parse(argv, dest);
    fmt::println("profile_children: {}", dest.profile_children);
    fmt::println("output filename:  {}", dest.output_file.string());
    puts("Remaining args:");
    while (auto arg = cursor.pop()) {
        puts(arg.c_str());
    }
}
