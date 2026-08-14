#ifndef RESERVOIRCPP_CONTEXTLIB_CONTEXTMANAGER_HPP
#define RESERVOIRCPP_CONTEXTLIB_CONTEXTMANAGER_HPP

#include <functional>
#include <memory>
#include <utility>

namespace reservoircpp {
namespace contextlib {

/**
 * @brief ContextManager class for RAII-style resource management
 * 
 * Provides Python-like context manager semantics using C++ RAII.
 * Supports enter/exit callbacks for resource acquisition and release.
 */
template<typename T>
class ContextManager {
public:
    using enter_func = std::function<T()>;
    using exit_func = std::function<void(T&)>;

    ContextManager(enter_func enter, exit_func exit)
        : enter_(std::move(enter))
        , exit_(std::move(exit))
        , value_()
        , entered_(false) {}

    ~ContextManager() {
        if (entered_) {
            try {
                exit_(value_);
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    // Non-copyable
    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;

    // Movable
    ContextManager(ContextManager&& other) noexcept
        : enter_(std::move(other.enter_))
        , exit_(std::move(other.exit_))
        , value_(std::move(other.value_))
        , entered_(other.entered_) {
        other.entered_ = false;
    }

    ContextManager& operator=(ContextManager&& other) noexcept {
        if (this != &other) {
            if (entered_) {
                exit_(value_);
            }
            enter_ = std::move(other.enter_);
            exit_ = std::move(other.exit_);
            value_ = std::move(other.value_);
            entered_ = other.entered_;
            other.entered_ = false;
        }
        return *this;
    }

    // Enter the context
    T& enter() {
        if (!entered_) {
            value_ = enter_();
            entered_ = true;
        }
        return value_;
    }

    // Exit the context
    void exit() {
        if (entered_) {
            exit_(value_);
            entered_ = false;
        }
    }

    // Access the value
    T& value() { return value_; }
    const T& value() const { return value_; }

    bool is_entered() const { return entered_; }

private:
    enter_func enter_;
    exit_func exit_;
    T value_;
    bool entered_;
};

/**
 * @brief ScopeGuard for simple cleanup actions
 */
class ScopeGuard {
public:
    explicit ScopeGuard(std::function<void()> cleanup)
        : cleanup_(std::move(cleanup))
        , dismissed_(false) {}

    ~ScopeGuard() {
        if (!dismissed_) {
            try {
                cleanup_();
            } catch (...) {
                // Suppress exceptions in destructor
            }
        }
    }

    // Non-copyable
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;

    // Movable
    ScopeGuard(ScopeGuard&& other) noexcept
        : cleanup_(std::move(other.cleanup_))
        , dismissed_(other.dismissed_) {
        other.dismissed_ = true;
    }

    void dismiss() { dismissed_ = true; }

private:
    std::function<void()> cleanup_;
    bool dismissed_;
};

// Helper function to create scope guards
inline ScopeGuard make_scope_guard(std::function<void()> cleanup) {
    return ScopeGuard(std::move(cleanup));
}

/**
 * @brief NullContext for optional context management
 */
class NullContext {
public:
    void* enter() { return nullptr; }
    void exit() {}
};

// Macro for Python-like 'with' statement simulation
#define WITH_CONTEXT(ctx, var) \
    if (auto var = (ctx).enter(); true)

} // namespace contextlib
} // namespace reservoircpp

#endif // RESERVOIRCPP_CONTEXTLIB_CONTEXTMANAGER_HPP
