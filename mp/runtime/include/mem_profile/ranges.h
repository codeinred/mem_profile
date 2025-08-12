#pragma once

#include <optional>
#include <type_traits>

namespace mem_profile {
    template <class Impl>
    struct Source : Impl {
        using Impl::next;
    };
    template <class Iter>
    struct IterSource {
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
    template <class Iter, auto member>
    struct MemberIterSource {
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
    template <class Iter, class F>
    struct MapIterSource {
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
        return Source<IterSource<iter_t>> {std::begin(range), std::end(range)};
    }
    template <class Range, class Func>
    auto make_source(Range& range, Func&& func) {
        using iter_t = decltype(std::begin(range));
        return Source<MapIterSource<iter_t, Func>> {
            std::begin(range),
            std::end(range),
            static_cast<Func&&>(func)};
    }

    template <auto ptr, class Range>
    auto make_source(Range& range) {
        using iter_t = decltype(std::begin(range));
        return Source<MemberIterSource<iter_t, ptr>> {
            std::begin(range),
            std::end(range)};
    }
} // namespace mem_profile
