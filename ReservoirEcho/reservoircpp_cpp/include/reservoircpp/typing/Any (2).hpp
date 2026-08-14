#ifndef RESERVOIRCPP_TYPING_ANY_HPP
#define RESERVOIRCPP_TYPING_ANY_HPP

#include <any>
#include <typeinfo>
#include <string>
#include <stdexcept>

namespace reservoircpp {
namespace typing {

/**
 * @brief Any class for type-erased value storage
 * 
 * Wraps std::any with additional convenience methods
 * for reservoir computing dynamic parameter handling.
 */
class Any {
public:
    Any() = default;
    
    template<typename T>
    Any(T&& value) : data_(std::forward<T>(value)) {}

    Any(const Any&) = default;
    Any(Any&&) = default;
    Any& operator=(const Any&) = default;
    Any& operator=(Any&&) = default;

    template<typename T>
    Any& operator=(T&& value) {
        data_ = std::forward<T>(value);
        return *this;
    }

    // Type checking
    bool has_value() const noexcept { return data_.has_value(); }
    
    const std::type_info& type() const noexcept { return data_.type(); }

    template<typename T>
    bool is() const noexcept {
        return data_.type() == typeid(T);
    }

    // Value access
    template<typename T>
    T& get() {
        return std::any_cast<T&>(data_);
    }

    template<typename T>
    const T& get() const {
        return std::any_cast<const T&>(data_);
    }

    template<typename T>
    T get_or(const T& default_value) const noexcept {
        try {
            return std::any_cast<T>(data_);
        } catch (const std::bad_any_cast&) {
            return default_value;
        }
    }

    template<typename T>
    T* get_if() noexcept {
        return std::any_cast<T>(&data_);
    }

    template<typename T>
    const T* get_if() const noexcept {
        return std::any_cast<T>(&data_);
    }

    // Modifiers
    void reset() noexcept { data_.reset(); }

    template<typename T, typename... Args>
    T& emplace(Args&&... args) {
        return data_.emplace<T>(std::forward<Args>(args)...);
    }

    void swap(Any& other) noexcept { data_.swap(other.data_); }

    // Underlying access
    std::any& underlying() noexcept { return data_; }
    const std::any& underlying() const noexcept { return data_; }

private:
    std::any data_;
};

// Helper function for type-safe casting
template<typename T>
T any_cast(const Any& any) {
    return std::any_cast<T>(any.underlying());
}

template<typename T>
T any_cast(Any& any) {
    return std::any_cast<T>(any.underlying());
}

template<typename T>
T any_cast(Any&& any) {
    return std::any_cast<T>(std::move(any.underlying()));
}

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_ANY_HPP
