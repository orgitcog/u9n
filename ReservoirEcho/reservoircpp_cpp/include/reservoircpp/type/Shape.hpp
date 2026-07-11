#ifndef RESERVOIRCPP_TYPE_SHAPE_HPP
#define RESERVOIRCPP_TYPE_SHAPE_HPP

#include <vector>
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <numeric>
#include <functional>
#include <algorithm>  // std::reverse, std::accumulate (GCC 16+ compatibility)

namespace reservoircpp {
namespace type {

/**
 * @brief Shape class for multi-dimensional array dimensions
 * 
 * Represents the shape of tensors and arrays in the reservoir computing framework.
 * Provides utilities for shape manipulation, broadcasting, and validation.
 */
class Shape {
public:
    using size_type = std::size_t;
    using value_type = size_type;
    using container_type = std::vector<size_type>;
    using iterator = container_type::iterator;
    using const_iterator = container_type::const_iterator;

    // Constructors
    Shape() = default;
    
    Shape(std::initializer_list<size_type> dims) : dims_(dims) {}
    
    explicit Shape(const std::vector<size_type>& dims) : dims_(dims) {}
    
    explicit Shape(std::vector<size_type>&& dims) : dims_(std::move(dims)) {}
    
    template<typename InputIt>
    Shape(InputIt first, InputIt last) : dims_(first, last) {}

    // Dimension access
    size_type ndim() const noexcept { return dims_.size(); }
    
    size_type operator[](size_type idx) const { return dims_[idx]; }
    
    size_type& operator[](size_type idx) { return dims_[idx]; }
    
    size_type at(size_type idx) const { return dims_.at(idx); }
    
    size_type& at(size_type idx) { return dims_.at(idx); }

    // Total element count
    size_type numel() const {
        if (dims_.empty()) return 0;
        return std::accumulate(dims_.begin(), dims_.end(), 
                               size_type(1), std::multiplies<size_type>());
    }

    // Iterators
    iterator begin() noexcept { return dims_.begin(); }
    iterator end() noexcept { return dims_.end(); }
    const_iterator begin() const noexcept { return dims_.begin(); }
    const_iterator end() const noexcept { return dims_.end(); }
    const_iterator cbegin() const noexcept { return dims_.cbegin(); }
    const_iterator cend() const noexcept { return dims_.cend(); }

    // Comparison
    bool operator==(const Shape& other) const { return dims_ == other.dims_; }
    bool operator!=(const Shape& other) const { return dims_ != other.dims_; }

    // Shape manipulation
    Shape flatten() const {
        return Shape({numel()});
    }

    Shape reshape(const std::vector<size_type>& new_dims) const {
        Shape result(new_dims);
        if (result.numel() != numel()) {
            throw std::invalid_argument("Cannot reshape: element count mismatch");
        }
        return result;
    }

    // Broadcasting support
    static Shape broadcast(const Shape& a, const Shape& b) {
        std::vector<size_type> result;
        auto it_a = a.dims_.rbegin();
        auto it_b = b.dims_.rbegin();
        
        while (it_a != a.dims_.rend() || it_b != b.dims_.rend()) {
            size_type dim_a = (it_a != a.dims_.rend()) ? *it_a++ : 1;
            size_type dim_b = (it_b != b.dims_.rend()) ? *it_b++ : 1;
            
            if (dim_a != dim_b && dim_a != 1 && dim_b != 1) {
                throw std::invalid_argument("Shapes are not broadcastable");
            }
            result.push_back(std::max(dim_a, dim_b));
        }
        
        std::reverse(result.begin(), result.end());
        return Shape(std::move(result));
    }

    // Data access
    const container_type& data() const noexcept { return dims_; }
    container_type& data() noexcept { return dims_; }

    bool empty() const noexcept { return dims_.empty(); }

private:
    container_type dims_;
};

} // namespace type
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPE_SHAPE_HPP
