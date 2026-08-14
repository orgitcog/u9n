#ifndef RESERVOIRCPP_ABC_ABSTRACTMETHOD_HPP
#define RESERVOIRCPP_ABC_ABSTRACTMETHOD_HPP

/**
 * @brief Abstract method marker macros
 * 
 * Provides Python-like @abstractmethod semantics through macros.
 * Use ABSTRACT_METHOD to mark pure virtual methods.
 */

// Macro to mark a method as abstract (pure virtual)
#define ABSTRACT_METHOD = 0

// Macro for abstract property getter
#define ABSTRACT_PROPERTY virtual

// Macro for abstract static method (not truly enforceable in C++)
#define ABSTRACT_STATIC_METHOD static

// Macro for abstract class method
#define ABSTRACT_CLASS_METHOD virtual

namespace reservoircpp {
namespace abc {

/**
 * @brief NotImplementedError exception
 * 
 * Thrown when an abstract method is called without implementation.
 */
class NotImplementedError : public std::runtime_error {
public:
    explicit NotImplementedError(const std::string& method_name)
        : std::runtime_error("Abstract method '" + method_name + "' must be implemented") {}
};

// Helper macro to throw NotImplementedError
#define NOT_IMPLEMENTED(method_name) \
    throw reservoircpp::abc::NotImplementedError(method_name)

} // namespace abc
} // namespace reservoircpp

#endif // RESERVOIRCPP_ABC_ABSTRACTMETHOD_HPP
