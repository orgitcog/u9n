#pragma once
/**
 * @file generator.hpp
 * @brief C++20 coroutine generator for lazy pattern matching
 *
 * Provides a generator<T> type that yields values lazily,
 * perfect for pattern matching where we may not need all results.
 */

#include <coroutine>
#include <exception>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace opencog {

/**
 * @brief A lazy generator using C++20 coroutines
 *
 * Usage:
 *   generator<int> count(int max) {
 *       for (int i = 0; i < max; ++i) {
 *           co_yield i;
 *       }
 *   }
 *
 *   for (int x : count(10)) {
 *       std::cout << x << "\n";
 *   }
 */
template<typename T>
class generator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> current_value;
        std::exception_ptr exception;

        generator get_return_object() {
            return generator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }

        void return_void() {}

        void unhandled_exception() {
            exception = std::current_exception();
        }

        template<typename U>
        std::suspend_never await_transform(U&&) = delete;  // Disallow co_await
    };

    // Iterator support
    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        iterator() = default;
        explicit iterator(handle_type handle) : handle_(handle) {}

        iterator& operator++() {
            handle_.resume();
            if (handle_.done()) {
                if (handle_.promise().exception) {
                    std::rethrow_exception(handle_.promise().exception);
                }
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++*this;
            return tmp;
        }

        [[nodiscard]] const T& operator*() const {
            return *handle_.promise().current_value;
        }

        [[nodiscard]] const T* operator->() const {
            return &*handle_.promise().current_value;
        }

        [[nodiscard]] bool operator==(const iterator& other) const {
            // Both are end iterators, or both point to done handles
            if (!handle_ && !other.handle_) return true;
            if (!handle_ || !other.handle_) {
                // One is end, check if other is done
                auto h = handle_ ? handle_ : other.handle_;
                return h.done();
            }
            return handle_ == other.handle_;
        }

        [[nodiscard]] bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        handle_type handle_;
    };

    // Constructors
    generator() = default;

    explicit generator(handle_type handle) : handle_(handle) {}

    generator(generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    generator& operator=(generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~generator() {
        if (handle_) handle_.destroy();
    }

    generator(const generator&) = delete;
    generator& operator=(const generator&) = delete;

    // Range interface
    [[nodiscard]] iterator begin() {
        if (handle_) {
            handle_.resume();
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
        }
        return iterator{handle_};
    }

    [[nodiscard]] iterator end() {
        return iterator{};
    }

    // Check if there are more values
    [[nodiscard]] bool has_next() const {
        return handle_ && !handle_.done();
    }

    // Get next value (advances the generator)
    [[nodiscard]] std::optional<T> next() {
        if (!handle_ || handle_.done()) return std::nullopt;

        handle_.resume();
        if (handle_.done()) {
            if (handle_.promise().exception) {
                std::rethrow_exception(handle_.promise().exception);
            }
            return std::nullopt;
        }
        return handle_.promise().current_value;
    }

private:
    handle_type handle_;
};

/**
 * @brief Recursive generator that can yield from sub-generators
 *
 * Allows patterns like:
 *   recursive_generator<int> tree_traverse(Node* n) {
 *       co_yield n->value;
 *       for (auto* child : n->children) {
 *           co_yield tree_traverse(child);  // Yield from sub-generator
 *       }
 *   }
 */
template<typename T>
class recursive_generator {
public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;

    struct promise_type {
        std::optional<T> current_value;
        std::exception_ptr exception;
        std::coroutine_handle<> inner_handle;

        recursive_generator get_return_object() {
            return recursive_generator{handle_type::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct final_awaiter {
            bool await_ready() noexcept { return false; }
            std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                auto& promise = h.promise();
                if (promise.inner_handle) {
                    return promise.inner_handle;
                }
                return std::noop_coroutine();
            }
            void await_resume() noexcept {}
        };

        final_awaiter final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(T value) {
            current_value = std::move(value);
            return {};
        }

        // Yield from another generator
        auto yield_value(recursive_generator<T> inner) {
            struct awaiter {
                recursive_generator<T> inner;
                std::optional<T>* outer_value;

                bool await_ready() noexcept { return false; }

                std::coroutine_handle<> await_suspend(handle_type h) noexcept {
                    inner.handle_.promise().inner_handle = h;
                    return inner.handle_;
                }

                void await_resume() noexcept {}
            };
            return awaiter{std::move(inner), &current_value};
        }

        void return_void() {}

        void unhandled_exception() {
            exception = std::current_exception();
        }
    };

    recursive_generator() = default;
    explicit recursive_generator(handle_type handle) : handle_(handle) {}

    recursive_generator(recursive_generator&& other) noexcept : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    recursive_generator& operator=(recursive_generator&& other) noexcept {
        if (this != &other) {
            if (handle_) handle_.destroy();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~recursive_generator() {
        if (handle_) handle_.destroy();
    }

    recursive_generator(const recursive_generator&) = delete;
    recursive_generator& operator=(const recursive_generator&) = delete;

    class iterator {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;

        iterator() = default;
        explicit iterator(handle_type handle) : handle_(handle) {}

        iterator& operator++() {
            handle_.resume();
            return *this;
        }

        [[nodiscard]] const T& operator*() const {
            return *handle_.promise().current_value;
        }

        [[nodiscard]] bool operator==(const iterator& other) const {
            if (!handle_ && !other.handle_) return true;
            if (!handle_ || !other.handle_) {
                auto h = handle_ ? handle_ : other.handle_;
                return h.done();
            }
            return handle_ == other.handle_;
        }

        [[nodiscard]] bool operator!=(const iterator& other) const {
            return !(*this == other);
        }

    private:
        handle_type handle_;
    };

    [[nodiscard]] iterator begin() {
        if (handle_) handle_.resume();
        return iterator{handle_};
    }

    [[nodiscard]] iterator end() {
        return iterator{};
    }

private:
    handle_type handle_;
};

} // namespace opencog
