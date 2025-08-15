#pragma once

#include <cstdlib>
#include <mem_profile/alloc.h>
#include <string>
/// Used for abi::__cxa_demangle, to demangle function names
#include <cxxabi.h>

namespace mp {
    class NameDemangler {
        constexpr static size_t InitialBuffSize = 16384;
        std::string symbol_buff;
        /// Raw pointer passed to abi::__cxa_demangle.
        /// Must be a row pointer b/c __cxa_demangle mayresize it using realloc
        char* name_buff;
        size_t buff_size;

       public:
        NameDemangler()
          : name_buff((char*)mperf_malloc(InitialBuffSize))
          , buff_size(InitialBuffSize) {}

        /// Returns a string view with the demangled name. The lifetime of the
        /// view is bounded by the next call to demangle() on this instance, or
        /// until NameDemangler is destroyed, whichever is sooner
        ///
        /// If the name could not be demangled, returns the name unchanged
        ///
        /// @param symbol - the mangled name of the symbol
        auto demangle(std::string_view symbol) -> std::string_view {
            if (symbol.data() == nullptr) {
                return {};
            }

            // Assign to the symbol_buff, so that we can pass it as a c_string
            symbol_buff.assign(symbol.data(), symbol.size());

            int status {};

            // This will resize the buff with realloc if necessary
            char* result = abi::__cxa_demangle(
                symbol_buff.c_str(),
                name_buff,
                &buff_size,
                &status);

            if (status != 0 || result == nullptr) {
                /// Name demangling failed, just return the name we were passed
                /// in, and don't update the name buff
                return symbol;
            }

            /// Ensure that name_buff is updated on success (the buffer may have
            /// been resized with realloc)
            name_buff = result;

            return result;
        };

        /// Invokes NameDemangler::demangle
        auto operator()(std::string_view symbol) -> std::string_view {
            return demangle(symbol);
        }

        ~NameDemangler() { mperf_free(name_buff); }
    };
} // namespace mp
