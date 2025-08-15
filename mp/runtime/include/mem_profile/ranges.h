#pragma once

#include <optional>
#include <type_traits>

namespace mp {
    template <class Impl>
    struct Source : Impl {
        using Impl::next;
    };
    template <class Iter> struct iter_source {
        Iter begin;
        Iter end;
        using value_type = typename std::iterator_traits<Iter>::value_type;
        auto next() -> std::optional<value_type> {
            if (begin == end) {
                return std::nullopt;
            } else {
                auto result = std::optional<value_type> {*begin};
                ++begin;
                return result;
            }
        }
    };
    template <class Iter, auto member> struct member_iter_source {
        Iter begin;
        Iter end;
        using value_type = std::decay_t<decltype((*begin).*member)>;
        auto next() -> std::optional<value_type> {
            if (begin == end) {
                return std::nullopt;
            } else {
                auto result = std::optional<value_type> {(*begin).*member};
                ++begin;
                return result;
            }
        }
    };
    template <class Iter, class F> struct map_iter_source {
        Iter begin;
        Iter end;
        F func;
        using value_type = std::decay_t<decltype(func(*begin))>;

        auto next() -> std::optional<value_type> {
            if (begin == end) {
                return std::nullopt;
            } else {
                auto result = std::optional<value_type> {func(*begin)};
                ++begin;
                return result;
            }
        }
    };

    template <class Range>
    auto make_source(Range& range) {
        using iter_t = decltype(std::begin(range));
        return Source<iter_source<iter_t>>{std::begin(range), std::end(range)};
    }
    template <class Range, class Func>
    auto make_source(Range& range, Func&& func) {
        using iter_t = decltype(std::begin(range));
        return Source<map_iter_source<iter_t, Func>>{std::begin(range),
                                                     std::end(range),
                                                     static_cast<Func&&>(func)};
    }

    template <auto ptr, class Range>
    auto make_source(Range& range) {
        using iter_t = decltype(std::begin(range));
        return Source<member_iter_source<iter_t, ptr>>{std::begin(range), std::end(range)};
    }
} // namespace mp
