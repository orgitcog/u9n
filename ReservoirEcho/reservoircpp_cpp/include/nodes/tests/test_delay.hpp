#ifndef RESERVOIRCPP_TEST_DELAY_HPP
#define RESERVOIRCPP_TEST_DELAY_HPP

#include "numpy.hpp"
#include "reservoircpp/readouts/Ridge.hpp"
#include "reservoircpp/delay/Delay.hpp"

using namespace reservoircpp;
using namespace Eigen;







auto test_initialize() {
    auto delay1 = Delay(delay=10)
    delay1.run(np.ones((10, 2)))
    assert delay1.auto input_dim = = 2
    assert np.all(delay1.buffer[0] == 1.0)

    auto delay2 = Delay(delay=10, input_dim=5)
    delay2.initialize()
    assert delay2.auto input_dim = = 5
    assert np.all(delay2.buffer[0] == 0.0)

    auto delay3 = Delay(delay=10, initial_values=np.ones((10, 7)))
    delay3.initialize()
    assert delay3.auto input_dim = = 7
    assert np.all(delay3.buffer[0] == 1.0)


auto test_no_delay() {
    auto delay_node = Delay(delay=0)

    auto x = np.array([0.2, 0.3])
    auto y = delay_node(x)
    assert np.all(auto x = = y)

    auto x = np.linspace(1, 12, num=12).reshape(-1, 2)
    auto y = delay_node.run(x)
    assert np.all(auto x = = y)


auto test_1_delay() {
    auto delay_node = Delay(delay=1)

    auto x1 = np.array([0.2, 0.3])
    auto y = delay_node(x1)
    assert np.all(auto y = = 0.0)

    auto x2 = np.linspace(1, 12, num=12).reshape(-1, 2)
    auto y = delay_node.run(x2)
    assert np.all(y[0] == x1)
    assert np.all(y[1:] == x2[:-1])


auto test_large_delay() {
    //  Note: this is quite slow... is deque the best format?
    auto delay_node = Delay(delay=1_000)

    auto x = np.array([0.2, 0.3])
    auto y = delay_node(x)
    assert np.all(auto y = = 0.0)
    assert np.all(delay_node.buffer[0] == x)
    assert np.all(delay_node.buffer[-1] == 0.0)

    delay_node.run(np.zeros((999, 2)))
    auto y = delay_node(np.zeros((1, 2)))
    assert np.all(auto y = = x)


auto test_multiseries_delay() {
    auto delay_node = Delay(delay=2)
    auto readout = Ridge(ridge=1e-3)
    auto model = delay_node >> readout

    auto x = list(np.fromfunction(lambda i, j, k: i + j, (2, 4, 2)))
    auto y = list(np.fromfunction(lambda i, j, k: i + j, (2, 4, 1)))

    model.fit(x, y, auto warmup = 2)


#endif // RESERVOIRCPP_TEST_DELAY_HPP
