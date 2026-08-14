#ifndef RESERVOIRCPP_TYPING_SEQUENCE_HPP
#define RESERVOIRCPP_TYPING_SEQUENCE_HPP

#include <vector>
#include <deque>
#include <list>
#include <type_traits>
#include <iterator>

namespace reservoircpp {
namespace typing {

/**
 * @brief Sequence concept for iterable containers
 * 
 * Provides type traits and utilities for sequence-like containers
 * in the reservoir computing framework.
 */

// Type trait to check if T is a sequence type
template<typename T, typename = void>
struct is_sequence : std::false_type {};

template<typename T>
struct is_sequence<T, std::void_t<
    typename T::value_type,
    typename T::iterator,
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end()),
    decltype(std::declval<T>().size())
>> : std::true_type {};

template<typename T>
inline constexpr bool is_sequence_v = is_sequence<T>::value;

// Generic Sequence wrapper for uniform interface
template<typename T>
class Sequence {
public:
    using value_type = T;
    using container_type = std::vector<T>;
    using size_type = typename container_type::size_type;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    Sequence() = default;
    
    explicit Sequence(size_type count) : data_(count) {}
    
    Sequence(size_type count, const T& value) : data_(count, value) {}
    
    Sequence(std::initializer_list<T> init) : data_(init) {}
    
    template<typename InputIt>
    Sequence(InputIt first, InputIt last) : data_(first, last) {}

    // Element access
    T& operator[](size_type idx) { return data_[idx]; }
    const T& operator[](size_type idx) const { return data_[idx]; }
    T& at(size_type idx) { return data_.at(idx); }
    const T& at(size_type idx) const { return data_.at(idx); }
    T& front() { return data_.front(); }
    const T& front() const { return data_.front(); }
    T& back() { return data_.back(); }
    const T& back() const { return data_.back(); }
    T* data() noexcept { return data_.data(); }
    const T* data() const noexcept { return data_.data(); }

    // Iterators
    iterator begin() noexcept { return data_.begin(); }
    iterator end() noexcept { return data_.end(); }
    const_iterator begin() const noexcept { return data_.begin(); }
    const_iterator end() const noexcept { return data_.end(); }
    const_iterator cbegin() const noexcept { return data_.cbegin(); }
    const_iterator cend() const noexcept { return data_.cend(); }

    // Capacity
    bool empty() const noexcept { return data_.empty(); }
    size_type size() const noexcept { return data_.size(); }
    void reserve(size_type new_cap) { data_.reserve(new_cap); }

    // Modifiers
    void clear() noexcept { data_.clear(); }
    void push_back(const T& value) { data_.push_back(value); }
    void push_back(T&& value) { data_.push_back(std::move(value)); }
    template<typename... Args>
    void emplace_back(Args&&... args) { data_.emplace_back(std::forward<Args>(args)...); }
    void pop_back() { data_.pop_back(); }
    void resize(size_type count) { data_.resize(count); }
    void resize(size_type count, const T& value) { data_.resize(count, value); }

    // Underlying container access
    container_type& underlying() noexcept { return data_; }
    const container_type& underlying() const noexcept { return data_; }

private:
    container_type data_;
};

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_SEQUENCE_HPP
