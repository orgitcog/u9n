#ifndef RESERVOIRCPP_UTILS_PROGRESS_HPP
#define RESERVOIRCPP_UTILS_PROGRESS_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <functional>

namespace reservoircpp {
namespace utils {

/**
 * @brief Progress tracking utilities for long-running operations
 * 
 * Provides progress bars and status updates for reservoir training.
 */
class ProgressBar {
public:
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    ProgressBar(std::size_t total, const std::string& desc = "", 
                std::size_t width = 50, bool enabled = true)
        : total_(total)
        , current_(0)
        , desc_(desc)
        , width_(width)
        , enabled_(enabled)
        , start_time_(clock::now()) {}

    void update(std::size_t n = 1) {
        current_ += n;
        if (enabled_) {
            display();
        }
    }

    void set(std::size_t n) {
        current_ = n;
        if (enabled_) {
            display();
        }
    }

    void finish() {
        current_ = total_;
        if (enabled_) {
            display();
            std::cout << std::endl;
        }
    }

    void display() const {
        if (!enabled_ || total_ == 0) return;

        double progress = static_cast<double>(current_) / total_;
        std::size_t filled = static_cast<std::size_t>(progress * width_);

        auto elapsed = clock::now() - start_time_;
        auto elapsed_sec = std::chrono::duration<double>(elapsed).count();
        
        double rate = (elapsed_sec > 0) ? current_ / elapsed_sec : 0;
        double eta = (rate > 0 && current_ < total_) ? 
                     (total_ - current_) / rate : 0;

        std::cout << "\r";
        if (!desc_.empty()) {
            std::cout << desc_ << ": ";
        }
        
        std::cout << "[";
        for (std::size_t i = 0; i < width_; ++i) {
            if (i < filled) {
                std::cout << "=";
            } else if (i == filled) {
                std::cout << ">";
            } else {
                std::cout << " ";
            }
        }
        std::cout << "] ";
        
        std::cout << std::fixed << std::setprecision(1);
        std::cout << (progress * 100) << "% ";
        std::cout << current_ << "/" << total_ << " ";
        std::cout << "[" << format_time(elapsed_sec) << "<" << format_time(eta) << ", ";
        std::cout << std::setprecision(2) << rate << " it/s]";
        std::cout << std::flush;
    }

    std::size_t current() const { return current_; }
    std::size_t total() const { return total_; }
    bool is_complete() const { return current_ >= total_; }

private:
    std::size_t total_;
    std::size_t current_;
    std::string desc_;
    std::size_t width_;
    bool enabled_;
    time_point start_time_;

    static std::string format_time(double seconds) {
        if (seconds < 60) {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(0) << seconds << "s";
            return oss.str();
        } else if (seconds < 3600) {
            int mins = static_cast<int>(seconds) / 60;
            int secs = static_cast<int>(seconds) % 60;
            std::ostringstream oss;
            oss << mins << ":" << std::setw(2) << std::setfill('0') << secs;
            return oss.str();
        } else {
            int hours = static_cast<int>(seconds) / 3600;
            int mins = (static_cast<int>(seconds) % 3600) / 60;
            std::ostringstream oss;
            oss << hours << ":" << std::setw(2) << std::setfill('0') << mins;
            return oss.str();
        }
    }
};

/**
 * @brief TQDM-like progress iterator wrapper
 */
template<typename Container>
class TqdmIterator {
public:
    TqdmIterator(Container& container, const std::string& desc = "", bool enabled = true)
        : container_(container)
        , progress_(container.size(), desc, 50, enabled) {}

    class Iterator {
    public:
        using inner_iterator = typename Container::iterator;
        using value_type = typename Container::value_type;
        using reference = typename Container::reference;
        using pointer = typename Container::pointer;
        using difference_type = typename Container::difference_type;
        using iterator_category = std::forward_iterator_tag;

        Iterator(inner_iterator it, ProgressBar* progress)
            : it_(it), progress_(progress) {}

        reference operator*() { return *it_; }
        pointer operator->() { return &(*it_); }

        Iterator& operator++() {
            ++it_;
            if (progress_) progress_->update();
            return *this;
        }

        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const Iterator& other) const { return it_ == other.it_; }
        bool operator!=(const Iterator& other) const { return it_ != other.it_; }

    private:
        inner_iterator it_;
        ProgressBar* progress_;
    };

    Iterator begin() { return Iterator(container_.begin(), &progress_); }
    Iterator end() { return Iterator(container_.end(), nullptr); }

    void finish() { progress_.finish(); }

private:
    Container& container_;
    ProgressBar progress_;
};

// Helper function to create tqdm-like iterators
template<typename Container>
TqdmIterator<Container> tqdm(Container& container, const std::string& desc = "") {
    return TqdmIterator<Container>(container, desc);
}

} // namespace utils
} // namespace reservoircpp

#endif // RESERVOIRCPP_UTILS_PROGRESS_HPP
