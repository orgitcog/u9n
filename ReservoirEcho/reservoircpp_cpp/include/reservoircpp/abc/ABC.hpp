#ifndef RESERVOIRCPP_ABC_ABC_HPP
#define RESERVOIRCPP_ABC_ABC_HPP

#include <type_traits>

namespace reservoircpp {
namespace abc {

/**
 * @brief Abstract Base Class marker
 * 
 * Provides Python ABC-like semantics for C++ abstract classes.
 * Classes inheriting from ABC are intended to be abstract interfaces.
 */
class ABC {
public:
    virtual ~ABC() = default;

protected:
    ABC() = default;
    ABC(const ABC&) = default;
    ABC(ABC&&) = default;
    ABC& operator=(const ABC&) = default;
    ABC& operator=(ABC&&) = default;
};

// Type trait to check if a class derives from ABC
template<typename T>
struct is_abstract_base : std::is_base_of<ABC, T> {};

template<typename T>
inline constexpr bool is_abstract_base_v = is_abstract_base<T>::value;

/**
 * @brief ABCMeta marker for metaclass-like behavior
 * 
 * Classes using ABCMeta are expected to define abstract methods
 * that must be implemented by derived classes.
 */
class ABCMeta : public ABC {
public:
    virtual ~ABCMeta() = default;

protected:
    ABCMeta() = default;
};

} // namespace abc
} // namespace reservoircpp

#endif // RESERVOIRCPP_ABC_ABC_HPP
