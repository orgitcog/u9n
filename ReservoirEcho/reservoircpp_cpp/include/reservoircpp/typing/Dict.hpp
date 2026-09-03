#ifndef RESERVOIRCPP_TYPING_DICT_HPP
#define RESERVOIRCPP_TYPING_DICT_HPP

#include <unordered_map>
#include <map>
#include <string>
#include <stdexcept>
#include <optional>

namespace reservoircpp {
namespace typing {

/**
 * @brief Dict class providing Python-like dictionary semantics
 * 
 * Wraps std::unordered_map with additional convenience methods
 * for reservoir computing parameter management.
 */
template<typename Key, typename Value, typename Hash = std::hash<Key>>
class Dict {
public:
    using container_type = std::unordered_map<Key, Value, Hash>;
    using key_type = Key;
    using mapped_type = Value;
    using value_type = typename container_type::value_type;
    using size_type = typename container_type::size_type;
    using iterator = typename container_type::iterator;
    using const_iterator = typename container_type::const_iterator;

    Dict() = default;
    
    Dict(std::initializer_list<value_type> init) : data_(init) {}

    // Element access
    Value& operator[](const Key& key) { return data_[key]; }
    
    Value& at(const Key& key) { return data_.at(key); }
    const Value& at(const Key& key) const { return data_.at(key); }

    // Get with default value (Python-like)
    Value get(const Key& key, const Value& default_value = Value{}) const {
        auto it = data_.find(key);
        return (it != data_.end()) ? it->second : default_value;
    }

    // Get optional (C++ style)
    std::optional<Value> get_optional(const Key& key) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    // Lookup
    bool contains(const Key& key) const {
        return data_.find(key) != data_.end();
    }

    iterator find(const Key& key) { return data_.find(key); }
    const_iterator find(const Key& key) const { return data_.find(key); }

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

    // Modifiers
    void clear() noexcept { data_.clear(); }
    
    std::pair<iterator, bool> insert(const value_type& value) {
        return data_.insert(value);
    }

    template<typename... Args>
    std::pair<iterator, bool> emplace(Args&&... args) {
        return data_.emplace(std::forward<Args>(args)...);
    }

    size_type erase(const Key& key) { return data_.erase(key); }

    // Python-like update (merge)
    void update(const Dict& other) {
        for (const auto& [key, value] : other) {
            data_[key] = value;
        }
    }

    // Pop with default
    Value pop(const Key& key, const Value& default_value = Value{}) {
        auto it = data_.find(key);
        if (it != data_.end()) {
            Value result = std::move(it->second);
            data_.erase(it);
            return result;
        }
        return default_value;
    }

    // Underlying container access
    container_type& underlying() noexcept { return data_; }
    const container_type& underlying() const noexcept { return data_; }

private:
    container_type data_;
};

// Convenience alias for string-keyed dictionaries
template<typename Value>
using StringDict = Dict<std::string, Value>;

} // namespace typing
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPING_DICT_HPP
