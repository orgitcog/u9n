# ReservoirCpp

**C++ implementation of ReservoirPy - A Simple and Flexible Library for Reservoir Computing architectures like Echo State Networks (ESN).**

## Overview

ReservoirCpp is a C++ port of the Python library [ReservoirPy](https://github.com/reservoirpy/reservoirpy). It provides a fast and efficient framework for creating and simulating Reservoir Computing architectures, with a focus on Echo State Networks (ESNs).

This implementation converts the Python codebase to C++ to achieve better performance while maintaining the same functionality and API design patterns. The conversion uses a combination of automated transformation (approximately 80% of the code) and manual refinement (the remaining 20%).

## Requirements

- C++17 compatible compiler
- [Eigen](https://eigen.tuxfamily.org) (version 3.3 or higher)
- CMake (version 3.14 or higher)

## Building

```bash
mkdir build
cd build
cmake ..
make
```

## Basic Usage

```cpp
#include <reservoircpp/model.hpp>
#include <reservoircpp/nodes/esn.hpp>
#include <reservoircpp/nodes/readouts/ridge.hpp>

int main() {
    // Create a simple Echo State Network with ridge regression readout
    auto reservoir = reservoircpp::nodes::ESN(100, input_dim=1);
    auto readout = reservoircpp::nodes::readouts::Ridge();
    
    // Connect reservoir to readout
    auto model = reservoircpp::Model({reservoir, readout});
    
    // Train the model
    model.fit(X_train, Y_train);
    
    // Run the model on new data
    auto Y_pred = model.run(X_test);
    
    return 0;
}
```

## Documentation

For detailed documentation, check the [docs](docs/) folder.

## License

This project is licensed under the BSD 3-Clause License - see the LICENSE file for details.
