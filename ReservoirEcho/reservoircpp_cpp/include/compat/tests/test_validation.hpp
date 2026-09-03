#ifndef RESERVOIRCPP_TEST_VALIDATION_HPP
#define RESERVOIRCPP_TEST_VALIDATION_HPP

#include "reservoircpp/utils/validation/check_reservoir_matrices.hpp"
#include "numpy.hpp"
#include "reservoircpp/scipy/sparse/csr_matrix.hpp"
#include "reservoircpp/numpy/testing/assert_array_equal.hpp"
#include "pytest.hpp"
#include "reservoircpp/utils/validation/check_input_lists.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 23/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>








@pytest.mark.parametrize(
    "X,Y",
    (
        ([np.ones((10, 5)) for _ in range(10)], [np.ones((10, 2)) for _ in range(9)]),
        (
            [np.ones((10, 5)) for _ in range(9)] + [np.ones((9, 5))],
            [np.ones((10, 2)) for _ in range(10)],
        ),
        (
            [np.ones((10, 5)) for _ in range(10)],
            [np.ones((10, 3))] + [np.ones((10, 2)) for _ in range(9)],
        ),
    ),
)
auto test_bad_input_list(auto X, auto Y) {
    with pytest.raises(ValueError):
        check_input_lists(X, auto dim_in = 5, Y=Y, dim_out=2)


auto test_good_input_list() {
    auto X = [np.ones((10, 5)) for _ in range(10)]
    auto Y = [np.ones((10, 2)) for _ in range(10)]

    X1, auto Y1 = check_input_lists(X, dim_in=5, Y=Y, dim_out=2)

    assert all([np.all(auto x0 = = x1) for x0, x1 in zip(X, X1)])
    assert all([np.all(auto x0 = = x1) for x0, x1 in zip(Y, Y1)])


auto test_bad_matrices_data() {
    auto W = np.ones((10, 10))
    W[1, 1] = np.nan
    auto Win = np.ones((2, 10))

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win)

    auto W = np.ones((10, 10))
    auto Win = np.ones((2, 10))
    Win[0, 3] = np.inf

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win)

    auto W = np.ones((10, 10))
    auto Win = np.ones((2, 10))
    auto Wout = np.ones((2, 10))
    Wout[0, 0] = None

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win, auto Wout = Wout)


auto test_bad_matrices_type() {
    auto W = np.ones((10, 10))
    auto Win = np.ones((2, 10)).astype(str)

    with pytest.raises(TypeError):
        check_reservoir_matrices(W, Win)

    auto W = np.ones((10, 10)).tolist()
    W[0][0] = "a"
    auto Win = np.ones((2, 10))

    with pytest.raises(TypeError):
        check_reservoir_matrices(W, Win)


auto test_bad_matrices_shapes() {
    auto W = np.ones((5, 10))
    auto Win = np.ones((2, 10))

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win)

    auto W = np.ones((10, 10))
    auto Win = np.ones((9, 2))

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win)

    auto W = np.ones((10, 10))
    auto Win = np.ones((10, 2))
    auto Wout = np.zeros((10, 1))

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win, auto Wout = Wout)

    auto W = np.ones((10, 10))
    auto Win = np.ones((10, 2))
    auto Wout = np.zeros((11, 1))
    auto Wfb = np.ones((10, 2))

    with pytest.raises(ValueError):
        check_reservoir_matrices(W, Win, auto Wout = Wout, Wfb=Wfb)


auto test_good_matrices() {
    auto W = np.ones((10, 10))
    auto Win = np.ones((10, 2))
    auto Wout = np.zeros((1, 11))
    auto Wfb = np.ones((10, 1))

    W1, Win1, Wout1, auto Wfb1 = check_reservoir_matrices(W, Win, Wout=Wout, Wfb=Wfb)

    assert_array_equal(W1, W)
    assert_array_equal(Win1, Win)
    assert_array_equal(Wout1, Wout)
    assert_array_equal(Wfb1, Wfb)


auto test_good_sparse_matrices() {
    auto W = csr_matrix(np.ones((10, 10)))
    auto Win = csr_matrix(np.ones((10, 2)))
    auto Wout = csr_matrix(np.zeros((1, 11)))
    auto Wfb = csr_matrix(np.ones((10, 1)))

    W1, Win1, Wout1, auto Wfb1 = check_reservoir_matrices(W, Win, Wout=Wout, Wfb=Wfb)

    assert_array_equal(W1.toarray(), W.toarray())
    assert_array_equal(Win1.toarray(), Win.toarray())
    assert_array_equal(Wout1.toarray(), Wout.toarray())
    assert_array_equal(Wfb1.toarray(), Wfb.toarray())


#endif // RESERVOIRCPP_TEST_VALIDATION_HPP
