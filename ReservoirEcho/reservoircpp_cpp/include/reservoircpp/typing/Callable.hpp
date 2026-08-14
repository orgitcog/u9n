#ifndef RESERVOIRCPP_TYPING_CALLABLE_HPP
#define RESERVOIRCPP_TYPING_CALLABLE_HPP

#include <functional>
#include <type_traits>
#include <tuple>

namespace reservoircpp {
namespace typing {

/**
 * @brief Type traits and utilities for callable objects
 */

// Check if T is callable
template<typename T, typename = void>
struct is_callable : std::false_type {};

template<typename T>
struct is_callable<T, std::void_t<decltype(&T::operator())>> : std::true_type {};

template<typename R, typename... Args>
struct is_callable<R(*)(Args...)> : std::true_type {};

template<typename R, typename... Args>
struct is_callable<R(&)(Args...)> : std::true_type {};

template<typename R, typename... Args>
struct is_callable<std::function<R(Args...)>> : std::true_type {};

template<typename T>
inline constexpr bool is_callable_v = is_callable<T>::value;

// Extract return type from callable
template<typename T>
struct callable_return_type;

template<typename R, typename... Args>
struct callable_return_type<R(*)(Args...)> {
    using type = R;
};

template<typename R, typename... Args>
struct callable_return_type<R(&)(Args...)> {
    using type = R;
};

template<typename R, typename... Args>
struct callable_return_type<std::function<R(Args...)>> {
    using type = R;
};

template<typename T>
struct callable_return_type {
private:
    template<typename U>
    static auto test(int) -> decltype(&U::operator(), std::true_type{});
    
    template<typename>
    static std::false_type test(...);
    
    using has_call_op = decltype(test<T>(0));
    
    template<typename U, bool = has_call_op::value>
    struct impl;
    
    template<typename U>
    struct impl<U, true> {
        template<typename C, typename R, typename... Args>
        static R deduce(R(C::*)(Args...) const);
        
        template<typename C, typename R, typename... Args>
        static R deduce(R(C::*)(Args...));
        
        using type = decltype(deduce(&U::operator()));
    };
    
    template<typename U>
    struct impl<U, false> {
        using type = void;
    };
    
public:
    using type = typename impl<T>::type;
};

template<typename T>
using callable_return_type_t = typename callable_return_type<T>::type;

// Generic callable wrapper
template<typename Signature>
using Callable = std::function<Signature>;

// Callable with any return type
template<typename... Args>
using AnyCallable = std::function<void(Args...)>;

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_CALLABLE_HPP
