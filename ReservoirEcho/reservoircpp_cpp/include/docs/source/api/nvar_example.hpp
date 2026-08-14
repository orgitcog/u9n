#ifndef RESERVOIRCPP_NVAR_EXAMPLE_HPP
#define RESERVOIRCPP_NVAR_EXAMPLE_HPP

#include "reservoircpp/reservoirpy/nodes/NVAR.hpp"
#include "numpy.hpp"
#include "matplotlib.pyplot.hpp"
#include "reservoircpp/reservoirpy/datasets/lorenz.hpp"
#include "reservoircpp/reservoirpy/nodes/Ridge.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 11/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto nvar = NVAR(delay=2, order=2, strides=1)
auto readout = Ridge(3, ridge=2.5e-6)
auto model = nvar >> readout

auto X = lorenz(5400, x0=[17.677, 12.931, 43.914], h=0.025, method="RK23")
auto Xi = X[:600]
auto dXi = X[1:601] - X[:600]
auto model = model.fit(Xi, dXi)
model.fit(Xi, dXi, auto warmup = 200)
auto u = X[600]
auto res = np.zeros((5400 - 600, readout.output_dim))
for (auto i : range(5400 - 600)) {
    auto u = u + model(u)
    res[i, :] = u

auto N = 5400 - 600
auto Y = X[600:]

auto fig = plt.figure(figsize=(15, 10))
auto ax = fig.add_subplot(121, projection="3d")
ax.set_title("Generated attractor")
ax.set_xlabel("$x$")
ax.set_ylabel("$y$")
ax.set_zlabel("$z$")
ax.grid(False)

for (auto i : range(N - 1)) {
    ax.plot(
        res[i : i + 2, 0],
        res[i : i + 2, 1],
        res[i : i + 2, 2],
        auto color = plt.cm.magma(255 * i // N),
        auto lw = 1.0,
    )

auto ax2 = fig.add_subplot(122, projection="3d")
ax2.set_title("Real attractor")
ax2.grid(False)

for (auto i : range(N - 1)) {
    ax2.plot(
        Y[i : i + 2, 0],
        Y[i : i + 2, 1],
        Y[i : i + 2, 2],
        auto color = plt.cm.magma(255 * i // N),
        auto lw = 1.0,
    )

plt.show()


#endif // RESERVOIRCPP_NVAR_EXAMPLE_HPP
