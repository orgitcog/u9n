#ifndef RESERVOIRCPP_FUNCTOOLS_WRAPS_HPP
#define RESERVOIRCPP_FUNCTOOLS_WRAPS_HPP

#include <functional>
#include <string>
#include <utility>

namespace reservoircpp {
namespace functools {

/**
 * @brief Function wrapper utilities similar to Python's functools.wraps
 * 
 * Provides utilities for creating decorated functions while preserving
 * metadata from the original function.
 */

// Function metadata container
struct FunctionMeta {
    std::string name;
    std::string doc;
    std::string module;
    
    FunctionMeta() = default;
    FunctionMeta(std::string n, std::string d = "", std::string m = "")
        : name(std::move(n)), doc(std::move(d)), module(std::move(m)) {}
};

// Wrapper that preserves function metadata
template<typename Func>
class WrappedFunction {
public:
    WrappedFunction(Func func, FunctionMeta meta = {})
        : func_(std::move(func)), meta_(std::move(meta)) {}

    template<typename... Args>
    auto operator()(Args&&... args) const 
        -> decltype(std::declval<Func>()(std::forward<Args>(args)...)) {
        return func_(std::forward<Args>(args)...);
    }

    const FunctionMeta& meta() const { return meta_; }
    FunctionMeta& meta() { return meta_; }

    const std::string& name() const { return meta_.name; }
    const std::string& doc() const { return meta_.doc; }

private:
    Func func_;
    FunctionMeta meta_;
};

// Create a wrapped function
template<typename Func>
WrappedFunction<Func> wrap(Func&& func, FunctionMeta meta = {}) {
    return WrappedFunction<Func>(std::forward<Func>(func), std::move(meta));
}

// Decorator pattern helper
template<typename Decorator, typename Func>
auto decorate(Decorator&& decorator, Func&& func) 
    -> decltype(decorator(std::forward<Func>(func))) {
    return decorator(std::forward<Func>(func));
}

// Partial application (like functools.partial)
template<typename Func, typename... BoundArgs>
class Partial {
public:
    Partial(Func func, BoundArgs... args)
        : func_(std::move(func)), bound_args_(std::move(args)...) {}

    template<typename... Args>
    auto operator()(Args&&... args) const {
        return std::apply(
            [this, &args...](const BoundArgs&... bound) {
                return func_(bound..., std::forward<Args>(args)...);
            },
            bound_args_
        );
    }

private:
    Func func_;
    std::tuple<BoundArgs...> bound_args_;
};

// Create partial application
template<typename Func, typename... Args>
Partial<std::decay_t<Func>, std::decay_t<Args>...> partial(Func&& func, Args&&... args) {
    return Partial<std::decay_t<Func>, std::decay_t<Args>...>(
        std::forward<Func>(func), std::forward<Args>(args)...);
}

// Memoization wrapper
template<typename Func>
class Memoized {
public:
    explicit Memoized(Func func) : func_(std::move(func)) {}

    template<typename... Args>
    auto operator()(Args&&... args) {
        // Simple implementation - for production use a proper cache
        return func_(std::forward<Args>(args)...);
    }

private:
    Func func_;
};

template<typename Func>
Memoized<std::decay_t<Func>> memoize(Func&& func) {
    return Memoized<std::decay_t<Func>>(std::forward<Func>(func));
}

} // namespace functools
} // namespace reservoircpp

#endif // RESERVOIRCPP_FUNCTOOLS_WRAPS_HPP
