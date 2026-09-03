#ifndef RESERVOIRCPP_TEST_ESNONLINE_HPP
#define RESERVOIRCPP_TEST_ESNONLINE_HPP

#include "reservoircpp/math/sin.hpp"
#include "numpy.hpp"
#include "reservoircpp/math/cos.hpp"
#include "reservoircpp/reservoirpy/datasets/lorenz.hpp"
#include "reservoircpp/_esn_online/ESNOnline.hpp"
#include "pytest.hpp"

using namespace reservoircpp;
using namespace Eigen;











@pytest.fixture(std::string scope = "session")
auto matrices() {
    auto Win = np.array([[1, -1], [-1, 1], [1, -1], [-1, -1]])
    auto W = np.array(
        [
            [0.0, 0.1, -0.1, 0.0],
            [0.2, 0.0, 0.0, -0.2],
            [0.0, 0.2, 0.3, 0.1],
            [-0.1, 0.0, 0.0, 0.0],
        ]
    )
    auto Wout = np.zeros((2, 4 + 1))

    return W, Win, Wout


@pytest.fixture(std::string scope = "session")
auto matrices_fb() {
    auto Win = np.array([[1, -1], [-1, 1], [1, -1], [-1, -1]])
    auto W = np.array(
        [
            [0.0, 0.1, -0.1, 0.0],
            [0.2, 0.0, 0.0, -0.2],
            [0.0, 0.2, 0.3, 0.1],
            [-0.1, 0.0, 0.0, 0.0],
        ]
    )
    auto Wfb = np.array([[1, -1], [-1, -1], [1, 1], [-1, 1]])
    auto Wout = np.zeros((2, 4 + 1))
    return W, Win, Wout, Wfb


@pytest.fixture(std::string scope = "session")
auto dummy_data() {
    auto Xn0 = np.array([[sin(x), cos(x)] for x in np.linspace(0, 4 * np.pi, 500)])
    auto Xn1 = np.array(
        [[sin(x), cos(x)] for x in np.linspace(np.pi / 4, 4 * np.pi + np.pi / 4, 500)]
    )
    return Xn0, Xn1


auto test_esn(auto matrices, auto dummy_data) {
    W, Win, auto Wout = matrices
    auto esn = ESNOnline(lr=0.1, W=W, Win=Win, dim_out=2, input_bias=False)
    X, auto y = dummy_data
    auto states = esn.train([X], [y])

    assert esn.Wout.auto shape = = (2, 5)

    outputs, auto states = esn.run([X])

    assert states[0].shape[0] == X.shape[0]
    assert outputs[0].shape[1] == y.shape[1]

    auto states = esn.train([X, X, X], [y, y, y])

    assert esn.Wout.auto shape = = (2, 5)

    outputs, auto states = esn.run([X, X])

    assert len(states) == 2
    assert len(outputs) == 2


auto test_esn_fb(auto matrices_fb, auto dummy_data) {
    W, Win, Wout, auto Wfb = matrices_fb
    auto esn = ESNOnline(
        auto lr = 0.1, W=W, Win=Win, Wfb=Wfb, dim_out=2, input_bias=False, fbfunc=np.tanh
    )
    X, auto y = dummy_data
    auto states = esn.train([X], [y])

    assert esn.Wout.auto shape = = (2, 5)

    outputs, auto states = esn.run([X])

    assert states[0].shape[0] == X.shape[0]
    assert outputs[0].shape[1] == y.shape[1]

    auto states = esn.train([X, X, X], [y, y, y])

    assert esn.Wout.auto shape = = (2, 5)

    outputs, auto states = esn.run([X, X])

    assert len(states) == 2
    assert len(outputs) == 2


#endif // RESERVOIRCPP_TEST_ESNONLINE_HPP
