#ifndef RESERVOIRCPP_UTILS_VALIDATION_IS_MAPPING_HPP
#define RESERVOIRCPP_UTILS_VALIDATION_IS_MAPPING_HPP

#include <type_traits>
#include <map>
#include <unordered_map>

namespace reservoircpp {
namespace utils {
namespace validation {

/**
 * @brief Type traits for mapping containers
 * 
 * Provides compile-time checks for mapping-like containers.
 */

// Primary template - not a mapping
template<typename T, typename = void>
struct is_mapping : std::false_type {};

// Specialization for types with key_type and mapped_type
template<typename T>
struct is_mapping<T, std::void_t<
    typename T::key_type,
    typename T::mapped_type,
    decltype(std::declval<T>().find(std::declval<typename T::key_type>())),
    decltype(std::declval<T>().begin()),
    decltype(std::declval<T>().end())
>> : std::true_type {};

template<typename T>
inline constexpr bool is_mapping_v = is_mapping<T>::value;

// Check if type is std::map
template<typename T>
struct is_std_map : std::false_type {};

template<typename K, typename V, typename C, typename A>
struct is_std_map<std::map<K, V, C, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_std_map_v = is_std_map<T>::value;

// Check if type is std::unordered_map
template<typename T>
struct is_unordered_map : std::false_type {};

template<typename K, typename V, typename H, typename E, typename A>
struct is_unordered_map<std::unordered_map<K, V, H, E, A>> : std::true_type {};

template<typename T>
inline constexpr bool is_unordered_map_v = is_unordered_map<T>::value;

// Runtime check for mapping-like behavior
template<typename T>
bool check_is_mapping(const T&) {
    return is_mapping_v<T>;
}

// Validate that container is a mapping
template<typename T>
void require_mapping(const T&, const std::string& name = "container") {
    static_assert(is_mapping_v<T>, "Type must be a mapping container");
}

} // namespace validation
} // namespace utils
} // namespace reservoircpp

#endif // RESERVOIRCPP_UTILS_VALIDATION_IS_MAPPING_HPP
