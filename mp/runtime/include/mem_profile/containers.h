#pragma once

#include <algorithm>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace mp {
template <class T> using opt = std::optional<T>;

/// Simple priority_queue implementation. Supports moving elements out of the
/// queue. Moving elements out of the queue is necessary in order to modify
/// them.
template <class T, class Cmp = std::less<T>> class priority_queue : private Cmp {
    std::vector<T> vec;

    Cmp const& get_cmp() { return *this; }

  public:
    /// Checks if the queue is empty
    [[nodiscard]] bool empty() const noexcept { return vec.empty(); }

    /// Attempts to get the element at the top of the heap. Throws a
    /// std::out_of_range error if none exists
    [[nodiscard]] T const& top() const {
        if (vec.empty()) {
            throw std::out_of_range("Attempted to access top of queue, but queue was empty");
        }

        return vec[0];
    }


    /// Attempts to pop the top element from the heap. Throws an exception
    /// if the queue is empty
    [[nodiscard]] T pop() {
        if (vec.empty()) {
            throw std::out_of_range("Attempted to pop element from empty queue");
        }

        std::pop_heap(vec.begin(), vec.end(), get_cmp());
        T result(std::move(vec.back()));
        vec.pop_back();
        return result;
    }


    /// Pushes a value to the queue
    void push(T&& value) {
        vec.push_back(std::move(value));
        std::push_heap(vec.begin(), vec.end(), get_cmp());
    }

    [[nodiscard]] size_t size() const noexcept { return vec.size(); }
};

    /// Suppose you have some string_view s. s has an unknown lifetime: after
    /// the end of the function call, it could go away. You want to store the
    /// string so that you can hold onto it.
    ///
    /// SVStorage stores strings. You give it a string_view, and it gives you a
    /// new string_view whose lifetime is the same as that of the SVStorage.
    ///
    /// ```cpp
    /// SVStore store;
    ///
    /// std::string_view sv; // Obtained from somewhere
    ///
    /// sv2 = store.add(sv);  // Returns a string_view whose contents are
    ///                       // identical, that points to space inside
    ///                       // the SVStore
    ///
    /// ASSERT_EQ(sv, sv2);               // new one equals old one
    /// ASSERT_NE(sv.data(), sv2.data()); // sv2 points to space inside SVStore
    /// ```
class sv_store {
  public:
    /// size of blocks used if default_block_size is not specified (64KB)
    constexpr static size_t DEFAULT_BLOCK_SIZE = 1 << 16;

  private:
    class block {
        std::unique_ptr<char[]> buff{};
        size_t                  used{};
        size_t                  capacity{};

      public:
        block() = default;

        block(size_t space) : buff(new char[space]), used(0), capacity(space) {}

        [[nodiscard]] size_t available_space() const noexcept { return capacity - used; }

        [[nodiscard]] bool has_space_for(std::string_view sv) const noexcept {
            return sv.size() <= available_space();
        }

        /// Adds a string_view to the block without bounds checking
        /// This should only be used if you *know* the block has enough
        /// space for the string_view
        [[nodiscard]] std::string_view add_unsafe(std::string_view sv) {
            // Return a default-constructed string_view if given one with
            // null data. This check needs to happen anyways, since invoking
            // memcpy with a nullptr is Undefined Behavior
            if (sv.data() == nullptr) {
                return {};
            }
            char* dest = buff.get() + used;
            ::memcpy(dest, sv.data(), sv.size());
            used += sv.size();

            return std::string_view(dest, sv.size());
        }

        /// Attempts to add a string view to the block. Returns a
        /// string_view to the location in the block on success. Otherwise,
        /// returns a nullopt
        [[nodiscard]] opt<std::string_view> try_add(std::string_view sv) {
            if (sv.size() <= available_space()) {
                return add_unsafe(sv);
            }

            return std::nullopt;
        }
    };

    /// Provides comparison operator for two blocks that compares by
    /// availible space Used for constructing things like a priority_queue
    struct cmp_block_space {
        bool operator()(block const& a, block const& b) const noexcept {
            return a.available_space() < b.available_space();
        }
    };

    /// Block with most space at the top of the queue
    /// https://en.cppreference.com/w/cpp/container/priority_queue
    size_t                                 default_block_size = DEFAULT_BLOCK_SIZE;
    priority_queue<block, cmp_block_space> block_queue;

  public:
    /// Creates an SVStore with a default block size of
    /// SVStore::DEFAULT_BLOCK_SIZE
    sv_store()                = default;
    sv_store(sv_store const&) = delete;
    sv_store(sv_store&&)      = default;

    /// Creates an SVStore with the given default block size. The default
    /// block size determines how frequently an SVStore needs to allocate in
    /// order to accomodate new strings.
    ///
    /// This doesn't allocate anything yet; stuff only gets allocated once
    /// you add in the first string
    sv_store(size_t default_block_size) noexcept : default_block_size(default_block_size) {}

    /// Adds a string_view to the string_view storage. Returns a new
    /// string_view that points to somewhere in the storage. This new
    /// string_view has a lifetime identical to that of the SVStorage
    [[nodiscard]] std::string_view add(std::string_view sv) {
        if (block_queue.empty()) {
            block_queue.push(block(default_block_size));
        }

        if (block_queue.top().has_space_for(sv)) {
            auto block  = block_queue.pop();
            auto result = block.add_unsafe(sv);

            block_queue.push(std::move(block));

            return result;
        }

        /// Make a block with enough space for the string_view
        block block(std::max(sv.size(), default_block_size));
        auto  result = block.add_unsafe(sv);

        /// Blocks can be moved without invalidating any of the string_views
        /// they return
        block_queue.push(std::move(block));

        return result;
    }

    /// Returns number of blocks in the SVStore. Mostly useful for testing
    /// purposes
    size_t block_count() const noexcept { return block_queue.size(); }
};


struct store_result {
    std::string_view view{};
    size_t           id{};
};


    /// You put in a string_view, it returns an ID for the string_view
    /// IDs count up in order (0, 1, 2, ...), so you can recover the original
    /// string_view just by using the [] operator or .at(id).
    ///
    /// Makes an internal copy of the string_view using an SVStore. This object
    /// can be moved or added to without invalidating any string_views you've
    /// obtained from accesing it.
    ///
    /// This is optimized for the case where consecutive strings are likely to
    /// be identical. So, for example:
    ///
    /// ```cpp
    /// OrdIDStore s;
    ///
    /// s.add("a") => 0
    /// s.add("b") => 1
    /// s.add("c") => 2
    /// s.add("c") => 2
    /// s.add("c") => 2
    /// s.add("a") => 3 // Results in new id
    ///
    /// s[0] => "a"
    /// s[1] => "b"
    /// s[2] => "c"
    /// s[3] => "a" // s[0] and s[3] are both valid names for "a"
    /// ```
    /// Adding the same string consecutively multiple times will result in the
    /// same ID as before, but non-consecutive adds of the same string will
    /// result in different IDs.
    ///
    /// When dumping information about the callgraph, we want to avoid
    /// duplication of things like file names and function names. As a result,
    /// we'll store all the file names in an array, and then individual sites in
    /// the call graph will be associated with a file ID.
    ///
    /// Because addresses are sorted before we obtain information about them,
    /// things like file names and function names are likely to be consecutive
    /// for a large number of addresses in a row, hence, it is *mostly* deduped
class ord_id_store : private std::vector<std::string_view> {
    using base = std::vector<std::string_view>;

    std::string_view prev;
    sv_store         store;

  public:
    ord_id_store() : base(1, std::string_view{}), prev(), store() {}
    /// Creates an OrdIDStore with the given initial capacity
    ord_id_store(size_t cap) : ord_id_store() { base::reserve(cap); }
    ord_id_store(ord_id_store const&) = delete;
    ord_id_store(ord_id_store&&)      = default;
    using base::begin;
    using base::capacity;
    using base::end;
    using base::size;
    using base::operator[];
    using base::at;

    /// Adds a string_view. Returns an ID for the string_view.
    ///
    /// It is safe to use string_views obtained from the store even after
    /// adding objects to the store: adding a string_view to the store will
    /// not invalidate any existing string_views that came from the store,
    /// although it will still invalidate the iterators obtained by begin()
    /// and end()
    ///
    /// Always returns 0 for null string_views
    [[nodiscard]] size_t add(std::string_view key) { return save(key).id; }
    [[nodiscard]] size_t add(char const* c_str) {
        if (c_str == nullptr) {
            return save(std::string_view{}).id;
        } else {
            return save(std::string_view(c_str)).id;
        }
    }

    /// Adds a string_view. Returns an ID for the string_view, and an
    /// updated view whose value is identical to the key, but which points
    /// to memory inside the id_store's internal store
    ///
    /// It is safe to use string_views obtained from the store even after
    /// adding objects to the store: adding a string_view to the store will
    /// not invalidate any existing string_views that came from the store,
    /// although it will still invalidate the iterators obtained by begin()
    /// and end()
    ///
    /// Always returns 0 for null string_views
    [[nodiscard]] store_result save(std::string_view key) {
        if (key.data() == nullptr) {
            return store_result{{}, 0};
        }

        // Shortcut: if we got the same key, just return the prev id
        if (prev == key) {
            return store_result{prev, size() - 1};
        }

        /// Point the key to our internal storage. If we don't do this,
        /// the view could be invalidated at some point after the call
        /// to add()
        key  = store.add(key);
        prev = key;
        base::push_back(key);
        return store_result{key, size() - 1};
    }
    [[nodiscard]] store_result save(char const* c_str) {
        if (c_str == nullptr) {
            return save(std::string_view{});
        } else {
            return save(std::string_view(c_str));
        }
    }

    /// Gets keys ordered by ID
    [[nodiscard]] std::vector<std::string_view> const& keys() const noexcept { return *this; }

    /// Retruns true if the given ID refers to a null string_view
    bool is_null(size_t id) const noexcept { return id == 0; }
};

class unique_id_store : private std::vector<std::string_view> {
    using base = std::vector<std::string_view>;
    sv_store store{};

    struct Key {
        mutable std::string_view key;
    };
    struct key_hash : std::hash<std::string_view> {
        size_t operator()(Key const& key) const noexcept {
            return std::hash<std::string_view>::operator()(key.key);
        }
    };

    std::string_view                             prev{};
    size_t                                       prev_id{};
    std::unordered_map<std::string_view, size_t> ids;

  public:
    unique_id_store() : base(1, std::string_view{}), store(), prev(), prev_id(0), ids() {}
    unique_id_store(size_t cap) : unique_id_store() { base::reserve(cap); }
    using base::begin;
    using base::capacity;
    using base::end;
    using base::size;
    using base::operator[];
    using base::at;


    /// Returns the id for the given string_view, adding it to the store if
    /// necessary.
    ///
    /// std::string_views with data() == nullptr always have an ID of zero
    [[nodiscard]] store_result save(std::string_view key) {
        /// If the string_view is null, just return 0 for the ID
        if (key.data() == nullptr) {
            return store_result{{}, 0};
        }

        /// If we match the previous one (likely), take the fast path and
        /// return the previous ID
        if (prev == key) {
            return store_result{prev, prev_id};
        }

        auto it = ids.find(key);
        if (it == ids.end()) {
            // Update the key so it references a string in the store
            // (extends lifetime of key)
            key       = store.add(key);
            size_t id = size();

            ids[key] = id;
            base::push_back(key);

            prev    = key;
            prev_id = id;
            return store_result{key, id};
        } else {
            // The hash map only contains keys from the store
            // so now key is from the store
            prev      = it->first;
            size_t id = it->second;
            prev_id   = id;
            return store_result{prev, id};
        }
    }
    [[nodiscard]] size_t add(char const* c_str) {
        if (c_str == nullptr) {
            return save(std::string_view{}).id;
        } else {
            return save(std::string_view(c_str)).id;
        }
    }

    [[nodiscard]] size_t       add(std::string_view key) { return save(key).id; }
    [[nodiscard]] store_result save(char const* c_str) {
        if (c_str == nullptr) {
            return save(std::string_view{});
        } else {
            return save(std::string_view(c_str));
        }
    }


    /// Gets keys ordered by ID
    [[nodiscard]] auto keys() const noexcept -> std::vector<std::string_view> const& {
        return *this;
    }

    /// Retruns true if the given ID refers to a null string_view
    bool is_null(size_t id) const noexcept { return id == 0; }
};

using id_store = unique_id_store;
} // namespace mp
