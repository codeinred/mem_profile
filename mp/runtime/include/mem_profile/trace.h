#if 0
#pragma once



#include <exception>
#include <mem_profile/io.h>
#include <string>
#include <dlfcn.h>

namespace mem_profile {
    extern backtrace_state* const BACKTRACE_STATE;

    inline void backtrace_print_error(void*, char const* msg, int errnum) {
        std::setbuf(stderr, nullptr);
        fwrite_msg(
            stderr,
            "mem_profile::backtrace: Error obtaining backtrace. ");
        fwrite_msg(stderr, msg);
        fwrite_msg(stderr, "\n");
    }

    /// Provides callback and error handlers for libbacktrace's backtrace_simple
    class SimpleBacktraceHandler {
        uintptr_t* buff;
        size_t size_;
        size_t capacity_;
        bool has_error_ = false;

       public:
        enum Status {
            GOOD = 0,
            OUT_OF_SPACE = 1,
            ERROR = 2,
        };

        SimpleBacktraceHandler(uintptr_t* buff, size_t count) noexcept
          : buff(buff)
          , size_(0)
          , capacity_(count)
          , has_error_(false) {}

        /// Same a program counter. returns GOOD on success and OUT_OF_SPACE on
        /// failure
        int save_pc(uintptr_t pc) noexcept {
            if (size_ >= capacity_) {
                return OUT_OF_SPACE;
            }
            buff[size_++] = pc;
            return GOOD;
        }

        void print_error(char const* msg, int errnum) noexcept {
            has_error_ = true;
            /// Disable buffering for stderr (we don't want stderr to
            /// allocate if malloc breaks)
            std::setbuf(stderr, nullptr);
            fwrite_msg(
                stderr,
                "mem_profile::backtrace: Error obtaining backtrace. ");
            fwrite_msg(stderr, msg);
            fwrite_msg(stderr, "\n");
        }

        [[nodiscard]] bool good() const noexcept { return !has_error_; }

        constexpr size_t size() const noexcept { return size_; }

        /// Expects data to be a pointer to a SimpleBacktraceHandler. Invokes
        /// SimpleBacktraceHandler::save_pc(pc)
        static int callback(void* data, uintptr_t pc) noexcept {
            return reinterpret_cast<SimpleBacktraceHandler*>(data)->save_pc(pc);
        }

        /// Expects data to be a pointer to a SimpleBacktraceHandler. Invokes
        /// SimpleBacktraceHandler::print_error(msg, errnum)
        static void error_callback(
            void* data,
            char const* msg,
            int errnum) noexcept {
            return reinterpret_cast<SimpleBacktraceHandler*>(data)->print_error(
                msg,
                errnum);
        }
    };

    /// Represents an errror obtained when computing a backtrace
    struct BacktraceError : std::exception {
        std::string msg;
        int errnum;

        BacktraceError(std::string msg, int errnum) noexcept
          : msg(static_cast<std::string&&>(msg))
          , errnum(errnum) {}

        const char* what() const noexcept override { return msg.c_str(); }
    };

    /// Used when an error can be treated the same as nothing happening
    inline void noop_error_callback(void*, char const*, int) noexcept {}

    inline void fatal_error_callback(
        void*,
        char const* msg,
        int errnum) noexcept {
        std::setbuf(stderr, nullptr);
        fwrite_msg(stderr, "mem_profile: Encountered fatal error. What: ");
        fwrite_msg(stderr, msg);
        fflush(stderr);
        std::exit(1);
    }

    template <class F>
    auto get_callback(F&) {
        return [](void* data, auto... args) { return (*(F*)data)(args...); };
    };


    /// Obtains a backtrace. Prints an error to stderr if unable to do so.
    [[gnu::noinline]] inline size_t backtrace(
        uintptr_t* buff,
        size_t count) noexcept {
        constexpr uintptr_t FULL_PTR = ~uintptr_t {0};
        SimpleBacktraceHandler handler(buff, count);

        int res = backtrace_simple(
            BACKTRACE_STATE,
            1,
            SimpleBacktraceHandler::callback,
            SimpleBacktraceHandler::error_callback,
            &handler);

        if (res == SimpleBacktraceHandler::OUT_OF_SPACE) {
            std::setbuf(stderr, nullptr);
            fwrite_msg(
                stderr,
                "mem_profile::backtrace: call stack exceeded buffer size "
                "Returning truncated backtrace.\n");
        }

        size_t size = handler.size();
        /// For some reason libbacktrace sets the top of the trace to all ones
        if (size > 0 && buff[size - 1] == FULL_PTR) {
            return size - 1;
        }

        return size;
    }

    inline void print_backtrace() {
        backtrace_full(
            BACKTRACE_STATE,
            0,
            [](void* addr,
               uintptr_t pc,
               char const* filename,
               int lineno,
               char const* func) -> int {
                if (filename && func) {
                    fprintf(stderr, "%s:%i %s\n", filename, lineno, func);
                } else {
                    Dl_info info {};
                    dladdr((void*)(pc + 1), &info);

                    long long obj_base = (long long)info.dli_fbase,
                              sym_addr = (long long)info.dli_saddr;
                    fprintf(
                        stderr,
                        "%s:+%p %s\n",
                        info.dli_fname,
                        (void*)(pc - obj_base + 1),
                        info.dli_sname);
                }
                return 0;
            },
            mem_profile::fatal_error_callback,
            nullptr);
    }
} // namespace mem_profile
#endif
