#ifndef RESERVOIRCPP_UTILS_PARALLEL_PARALLELPROGRESSQUEUE_HPP
#define RESERVOIRCPP_UTILS_PARALLEL_PARALLELPROGRESSQUEUE_HPP

#include <queue>
#include <mutex>

namespace reservoircpp {
namespace utils {
namespace parallel {

template<typename T>
class ParallelProgressQueue {
public:
    void push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
    
    bool try_pop(T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        item = queue_.front();
        queue_.pop();
        return true;
    }
    
private:
    std::queue<T> queue_;
    std::mutex mutex_;
};

} // namespace parallel
} // namespace utils
} // namespace reservoircpp

#endif
