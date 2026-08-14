#ifndef RESERVOIRCPP_TYPE_MAPPEDDATA_HPP
#define RESERVOIRCPP_TYPE_MAPPEDDATA_HPP

#include <memory>
#include <vector>
#include <cstddef>
#include "Shape.hpp"

namespace reservoircpp {
namespace type {

/**
 * @brief MappedData class for efficient data storage with shape information
 * 
 * Provides a container that combines raw data storage with shape metadata,
 * supporting both owned and view semantics for reservoir computing operations.
 */
template<typename T>
class MappedData {
public:
    using value_type = T;
    using size_type = std::size_t;
    using pointer = T*;
    using const_pointer = const T*;
    using reference = T&;
    using const_reference = const T&;

    // Default constructor
    MappedData() : data_(nullptr), size_(0), owned_(false) {}

    // Constructor with shape (allocates memory)
    explicit MappedData(const Shape& shape) 
        : shape_(shape), size_(shape.numel()), owned_(true) {
        if (size_ > 0) {
            storage_ = std::make_unique<T[]>(size_);
            data_ = storage_.get();
        }
    }

    // Constructor from existing data (view, non-owning)
    MappedData(T* data, const Shape& shape) 
        : data_(data), shape_(shape), size_(shape.numel()), owned_(false) {}

    // Constructor from vector (copies data)
    explicit MappedData(const std::vector<T>& vec) 
        : shape_({vec.size()}), size_(vec.size()), owned_(true) {
        storage_ = std::make_unique<T[]>(size_);
        data_ = storage_.get();
        std::copy(vec.begin(), vec.end(), data_);
    }

    // Move constructor
    MappedData(MappedData&& other) noexcept
        : storage_(std::move(other.storage_))
        , data_(other.data_)
        , shape_(std::move(other.shape_))
        , size_(other.size_)
        , owned_(other.owned_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.owned_ = false;
    }

    // Move assignment
    MappedData& operator=(MappedData&& other) noexcept {
        if (this != &other) {
            storage_ = std::move(other.storage_);
            data_ = other.data_;
            shape_ = std::move(other.shape_);
            size_ = other.size_;
            owned_ = other.owned_;
            other.data_ = nullptr;
            other.size_ = 0;
            other.owned_ = false;
        }
        return *this;
    }

    // Copy constructor
    MappedData(const MappedData& other) 
        : shape_(other.shape_), size_(other.size_), owned_(true) {
        if (size_ > 0) {
            storage_ = std::make_unique<T[]>(size_);
            data_ = storage_.get();
            std::copy(other.data_, other.data_ + size_, data_);
        }
    }

    // Copy assignment
    MappedData& operator=(const MappedData& other) {
        if (this != &other) {
            shape_ = other.shape_;
            size_ = other.size_;
            owned_ = true;
            if (size_ > 0) {
                storage_ = std::make_unique<T[]>(size_);
                data_ = storage_.get();
                std::copy(other.data_, other.data_ + size_, data_);
            }
        }
        return *this;
    }

    // Element access
    reference operator[](size_type idx) { return data_[idx]; }
    const_reference operator[](size_type idx) const { return data_[idx]; }

    reference at(size_type idx) {
        if (idx >= size_) throw std::out_of_range("MappedData index out of range");
        return data_[idx];
    }

    const_reference at(size_type idx) const {
        if (idx >= size_) throw std::out_of_range("MappedData index out of range");
        return data_[idx];
    }

    // Data access
    pointer data() noexcept { return data_; }
    const_pointer data() const noexcept { return data_; }

    // Shape access
    const Shape& shape() const noexcept { return shape_; }
    size_type size() const noexcept { return size_; }
    size_type ndim() const noexcept { return shape_.ndim(); }
    bool empty() const noexcept { return size_ == 0; }
    bool is_owned() const noexcept { return owned_; }

    // Reshape (returns view if possible)
    MappedData reshape(const Shape& new_shape) const {
        if (new_shape.numel() != size_) {
            throw std::invalid_argument("Cannot reshape: element count mismatch");
        }
        MappedData result;
        result.data_ = data_;
        result.shape_ = new_shape;
        result.size_ = size_;
        result.owned_ = false;
        return result;
    }

    // Create a copy
    MappedData copy() const {
        return MappedData(*this);
    }

    // Fill with value
    void fill(const T& value) {
        std::fill(data_, data_ + size_, value);
    }

private:
    std::unique_ptr<T[]> storage_;
    pointer data_ = nullptr;
    Shape shape_;
    size_type size_ = 0;
    bool owned_ = false;
};

} // namespace type
} // namespace reservoircpp

#endif // RESERVOIRCPP_TYPE_MAPPEDDATA_HPP
