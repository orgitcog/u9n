#ifndef RESERVOIRCPP_TYPING_UNION_HPP
#define RESERVOIRCPP_TYPING_UNION_HPP

#include <variant>
#include <type_traits>

namespace reservoircpp {
namespace typing {

/**
 * @brief Union type alias for std::variant
 * 
 * Provides Python-like Union type semantics using C++ variant.
 */
template<typename... Types>
using Union = std::variant<Types...>;

// Helper to check if variant holds a specific type
template<typename T, typename... Types>
bool holds(const Union<Types...>& v) {
    return std::holds_alternative<T>(v);
}

// Safe get with default
template<typename T, typename... Types>
T get_or(const Union<Types...>& v, const T& default_value) {
    if (auto* ptr = std::get_if<T>(&v)) {
        return *ptr;
    }
    return default_value;
}

// Visitor helper
template<typename... Fs>
struct Overloaded : Fs... { using Fs::operator()...; };

template<typename... Fs>
Overloaded(Fs...) -> Overloaded<Fs...>;

// Visit helper
template<typename Variant, typename... Visitors>
decltype(auto) match(Variant&& v, Visitors&&... visitors) {
    return std::visit(Overloaded{std::forward<Visitors>(visitors)...}, 
                      std::forward<Variant>(v));
}

// Type trait to check if T is in Union
template<typename T, typename Variant>
struct is_in_union;

template<typename T, typename... Types>
struct is_in_union<T, Union<Types...>> 
    : std::disjunction<std::is_same<T, Types>...> {};

template<typename T, typename Variant>
inline constexpr bool is_in_union_v = is_in_union<T, Variant>::value;

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_UNION_HPP
