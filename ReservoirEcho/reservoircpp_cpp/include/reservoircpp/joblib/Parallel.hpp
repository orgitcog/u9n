#ifndef RESERVOIRCPP_JOBLIB_PARALLEL_HPP
#define RESERVOIRCPP_JOBLIB_PARALLEL_HPP

#include <functional>
#include <vector>
#include <thread>
#include <future>

namespace reservoircpp {
namespace joblib {

template<typename T>
class Parallel {
public:
    Parallel(int n_jobs = -1) : n_jobs_(n_jobs) {}
    
    template<typename Func, typename... Args>
    auto operator()(Func&& func, Args&&... args) {
        return func(std::forward<Args>(args)...);
    }
    
private:
    int n_jobs_;
};

template<typename Func>
auto delayed(Func&& func) {
    return std::forward<Func>(func);
}

} // namespace joblib
} // namespace reservoircpp

#endif
