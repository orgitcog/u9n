#ifndef RESERVOIRCPP_TQDM_TQDM_HPP
#define RESERVOIRCPP_TQDM_TQDM_HPP

#include <iostream>
#include <string>

namespace reservoircpp {
namespace tqdm {

class tqdm {
public:
    tqdm(int total = 0) : total_(total), current_(0) {}
    
    void update(int n = 1) { current_ += n; }
    void set_description(const std::string& desc) { desc_ = desc; }
    
private:
    int total_;
    int current_;
    std::string desc_;
};

} // namespace tqdm
} // namespace reservoircpp

#endif
