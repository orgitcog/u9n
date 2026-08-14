#ifndef RESERVOIRCPP_TEST_LOAD_FROM_PREVIOUS_VERSIONS_HPP
#define RESERVOIRCPP_TEST_LOAD_FROM_PREVIOUS_VERSIONS_HPP

#include "reservoircpp//ESN.hpp"
#include "reservoircpp///activationsfunc.hpp"
#include "numpy.hpp"
#include "pytest.hpp"
#include <reservoircpp/scipy.hpp>
#include "reservoircpp//load_compat.hpp"
#include "reservoircpp/contextlib/contextmanager.hpp"
#include "shutil.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 23/11/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>












@contextmanager
auto fbfunc(bool auto sparse_W = true, bool input_bias = true, bool feedback = false, bool train = false, auto fbfunc = F.identity) {

    auto parent_dir = os.path.dirname(os.path.realpath(__file__))

    auto d = parent_dir + "dummy_model"

    if (os.path.exists(d)) {
        shutil.rmtree(d)

    auto in_dim = 10 + int(input_bias)
    auto wfb = None

    if (sparse_W) {
        auto w = sparse.random(100, 100, format="csr", dtype=np.float64)
    } else {
        auto w = np.random.normal(0, 0.1, size=(100, 100))

    auto win = np.random.normal(0, 0.1, size=(100, in_dim))
    if (feedback) {
        auto wfb = np.random.normal(0, 0.1, size=(100, 10))

    auto esn = ESN(
        auto lr = 0.3, W=w, Win=win, input_bias=input_bias, Wfb=wfb, ridge=1e-8, fbfunc=fbfunc
    )

    if (train) {
        X, auto Y = [np.ones((100, 10))], [np.ones((100, 10))]
        esn.train(X, Y)

    esn.save(d)

    yield d, esn

    if (os.path.exists(d)) {
        shutil.rmtree(d)


@pytest.mark.parametrize(
    "sparse,bias,feedback,train,fbfunc",
    (
        (True, False, False, False, F.identity),
        (True, False, False, True, F.identity),
        (True, True, False, False, F.identity),
        (True, False, True, True, F.sigmoid),
        (True, True, True, False, F.tanh),
        (False, True, False, False, F.identity),
        (False, False, True, True, F.softmax),
    ),
)
auto test_load_files_from_v2(auto sparse, auto bias, auto feedback, auto fbfunc, auto train) {
    with create_old_model(
        auto sparse_W = sparse, input_bias=bias, feedback=feedback, train=train, fbfunc=fbfunc
    ) as m:

        dirname, auto esn = m[0], m[1]

        auto esn2 = load_compat(dirname)

        auto X = np.ones((100, 10))

        if (train is False and feedback is False) {
            with pytest.raises(RuntimeError):
                esn2.run(X)

            auto esn2 = load_compat(dirname)
            esn2.fit(X, X)
            auto res = esn2.run(X)
            assert res.auto shape = = (100, 10)
        } else {
            auto res = esn2.run(X)
            assert res.auto shape = = (100, 10)


#endif // RESERVOIRCPP_TEST_LOAD_FROM_PREVIOUS_VERSIONS_HPP
