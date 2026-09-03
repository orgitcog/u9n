#ifndef RESERVOIRCPP_TYPING_ITERATOR_HPP
#define RESERVOIRCPP_TYPING_ITERATOR_HPP

#include <iterator>
#include <type_traits>

namespace reservoircpp {
namespace typing {

/**
 * @brief Iterator utilities for reservoir computing operations
 */

// Type trait to check if T is an iterator
template<typename T, typename = void>
struct is_iterator : std::false_type {};

template<typename T>
struct is_iterator<T, std::void_t<
    typename std::iterator_traits<T>::iterator_category,
    typename std::iterator_traits<T>::value_type
>> : std::true_type {};

template<typename T>
inline constexpr bool is_iterator_v = is_iterator<T>::value;

// Range wrapper for begin/end pairs
template<typename Iterator>
class Range {
public:
    using iterator = Iterator;
    using value_type = typename std::iterator_traits<Iterator>::value_type;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;

    Range(Iterator begin, Iterator end) : begin_(begin), end_(end) {}

    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    
    bool empty() const { return begin_ == end_; }
    
    difference_type size() const { 
        return std::distance(begin_, end_); 
    }

private:
    Iterator begin_;
    Iterator end_;
};

// Helper function to create ranges
template<typename Iterator>
Range<Iterator> make_range(Iterator begin, Iterator end) {
    return Range<Iterator>(begin, end);
}

// Enumerate iterator for Python-like enumeration
template<typename Iterator>
class EnumerateIterator {
public:
    using value_type = std::pair<std::size_t, typename std::iterator_traits<Iterator>::reference>;
    using difference_type = typename std::iterator_traits<Iterator>::difference_type;
    using pointer = void;
    using reference = value_type;
    using iterator_category = std::forward_iterator_tag;

    EnumerateIterator(Iterator it, std::size_t index = 0) 
        : it_(it), index_(index) {}

    reference operator*() const {
        return {index_, *it_};
    }

    EnumerateIterator& operator++() {
        ++it_;
        ++index_;
        return *this;
    }

    EnumerateIterator operator++(int) {
        auto tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const EnumerateIterator& other) const {
        return it_ == other.it_;
    }

    bool operator!=(const EnumerateIterator& other) const {
        return !(*this == other);
    }

private:
    Iterator it_;
    std::size_t index_;
};

// Enumerate range wrapper
template<typename Container>
class Enumerate {
public:
    using iterator = EnumerateIterator<typename Container::iterator>;
    using const_iterator = EnumerateIterator<typename Container::const_iterator>;

    explicit Enumerate(Container& container) : container_(container) {}

    iterator begin() { return iterator(container_.begin(), 0); }
    iterator end() { return iterator(container_.end(), container_.size()); }
    const_iterator begin() const { return const_iterator(container_.begin(), 0); }
    const_iterator end() const { return const_iterator(container_.end(), container_.size()); }

private:
    Container& container_;
};

// Helper function for enumeration
template<typename Container>
Enumerate<Container> enumerate(Container& container) {
    return Enumerate<Container>(container);
}

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_ITERATOR_HPP
