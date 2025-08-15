#pragma once

// #include <backtrace.h>
#include <optional>
#include <stdexcept>
#include <string_view>


#include <mem_profile/abi.h>
#include <mem_profile/containers.h>
#include <mem_profile/trace.h>

#include <dlfcn.h>


namespace mp {
    using Addr = uintptr_t;

    /// Attempts to construct a string_view from the given C string.
    /// If the given C string is null, returns the given fallback message
    inline std::string_view unwrap_cstr_or(
        char const* c_str,
        std::string_view alt) noexcept {
        return c_str ? std::string_view(c_str) : alt;
    }


    /// Holds information about a program counter
    struct PCInfo {
        static inline intptr_t get_offset(Addr base, Addr addr) noexcept {
            return (intptr_t)addr - (intptr_t)base;
        }


        /// @section Sym info

        /// ID for object file this symbol was loaded from
        size_t object_file;
        /// ID for mangled symbol name
        size_t sym_name;
        /// Address of symbol in the object file
        size_t sym_addr;
        /// Address of the program counter in the object file
        size_t addr;

        /// @section Debug info

        /// ID for demangled name of function
        size_t func_name;
        /// ID for source file containing function definition
        size_t source_file;
        /// Line number where function was defined
        size_t func_lineno;
    };

    struct DebugInfo {
        size_t src_file;
        size_t func_name;
        size_t lineno;
    };

    struct InfoStore {
        constexpr static std::string_view EMPTY_STRING {};
        /// List of object files found in call graph
        IDStore object_files;
        /// List of mangled function names found in call graph
        IDStore sym_names;
        /// List of demangled function names
        IDStore func_names;
        /// List of source files
        IDStore source_files;

        /// Name demangler
        NameDemangler demangler;


        /// Fill in dest.source_file, dest.func_name, and dest.func_lineno based
        /// on the given func_addr. This wil typically be the declaration info
        /// for the function, unless the function's address couldn't be
        /// determined, in which case the address used will be the program
        /// counter
        void fill_declaration_info(Addr func_addr, PCInfo& dest) {
            bool obtained_debug_info = false;
            auto callback = [&, this](
                                [[maybe_unused]] uint64_t pc,
                                const char* filename,
                                int lineno,
                                const char* function) {
                if (filename == nullptr && lineno == 0 && function == nullptr) {
                    return 0;
                }

                if (filename) {
                    dest.source_file = source_files.add(filename);
                } else {
                    dest.source_file = source_files.add(EMPTY_STRING);
                }

                if (function) {
                    dest.func_name = func_names.add(
                        demangler.demangle(function));
                } else {
                    dest.func_name = func_names.add(EMPTY_STRING);
                }

                dest.func_lineno = lineno;

                obtained_debug_info = true;
                return 0;
            };

            /// TODO: REPLACE BACKTRACE

            // backtrace_pcinfo(
            //     BACKTRACE_STATE,
            //     func_addr,
            //     get_callback(callback),
            //     noop_error_callback,
            //     &callback);


            if (!obtained_debug_info) {
                dest.source_file = source_files.add(EMPTY_STRING);
                dest.func_name = func_names.add(EMPTY_STRING);
                dest.func_lineno = 0;
            }
        }

        PCInfo get_info(Addr pc) {
            PCInfo result {};

            Dl_info info {};

            /// Get information about the addr from dladdr
            /// attempt to use the symaddr if it's been set,
            /// otherwise use the program counter as an address
            ///
            /// On success, dladdr_success is set to a nonzero value
            ///
            /// See: https://man7.org/linux/man-pages/man3/dladdr.3.html
            int dladdr_success = dladdr((void const*)pc, &info);

            uintptr_t func_addr = 0;

            if (dladdr_success && info.dli_fbase) {
                uintptr_t dl_base_addr = uintptr_t(info.dli_fbase);
                uintptr_t sym_addr = uintptr_t(info.dli_saddr);

                result.object_file = object_files.add(info.dli_fname);

                result.addr = size_t(pc - dl_base_addr);

                if (sym_addr) {
                    func_addr = sym_addr;
                    result.sym_addr = sym_addr - dl_base_addr;
                    result.sym_name = sym_names.add(info.dli_sname);
                } else {
                    auto callback =
                        [&]([[maybe_unused]] uintptr_t pc,
                            const char* symname,
                            uintptr_t symval,
                            [[maybe_unused]] uintptr_t symsize) -> void {
                        func_addr = symval;
                        result.sym_addr = symval ? symval - dl_base_addr : 0;
                        result.sym_name = sym_names.add(symname);
                    };

                    /// TODO: REPLACE BACKTRACE

                    // backtrace_syminfo(
                    //     BACKTRACE_STATE,
                    //     pc,
                    //     get_callback(callback),
                    //     noop_error_callback,
                    //     &callback);
                }
            } else {
                result.object_file = object_files.add(EMPTY_STRING);
                result.sym_name = sym_names.add(EMPTY_STRING);
                result.sym_addr = 0;
                result.addr = 0;
            }

            if (func_addr) {
                fill_declaration_info(func_addr, result);
            } else {
                fill_declaration_info(pc, result);
            }


            /// If the dbg_info contained an empty string for the func name,
            /// but we have a symname, we can obtain the func name by demangling
            /// the symname
            if (!sym_names.is_null(result.sym_name)
                && func_names.is_null(result.func_name)) {
                result.func_name = func_names.add(
                    demangler.demangle(sym_names.at(result.sym_name)));
            }

            return result;
        }




        /// Get the full set of debug info for a symbol. Will unroll inlined
        /// functions to identify the call stack
        void full_debug_info(Addr pc, std::vector<DebugInfo>& dest) {
            dest.clear();

            auto callback = [&, this](
                                [[maybe_unused]] uint64_t pc,
                                const char* filename,
                                int lineno,
                                const char* function) {
                /// If there's no useful info, just skip this.
                /// Avoids having useless debug info in the stats file
                if (filename == nullptr && lineno == 0 && function == nullptr) {
                    return 0;
                }
                auto file = unwrap_cstr_or(filename, {});
                auto func = function ? demangler.demangle(function)
                                     : std::string_view {};

                dest.push_back(DebugInfo {
                    source_files.add(file),
                    func_names.add(func),
                    size_t(lineno),
                });
                return 0;
            };

            /// TODO: replace backtrace
            // backtrace_pcinfo(
            //     BACKTRACE_STATE,
            //     pc,
            //     get_callback(callback),
            //     noop_error_callback,
            //     &callback);
        }
    };
} // namespace mp
