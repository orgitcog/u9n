#ifndef RESERVOIRCPP_INTRINSIC_PLASTICITY_EXAMPLE_HPP
#define RESERVOIRCPP_INTRINSIC_PLASTICITY_EXAMPLE_HPP

#include "numpy.hpp"
#include "matplotlib.pyplot.hpp"
#include "reservoircpp/reservoirpy/nodes/IPReservoir.hpp"
#include "reservoircpp/scipy/stats/norm.hpp"
#include "reservoircpp/reservoirpy/datasets/narma.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 11/03/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>







int steps = 1000
auto X = narma(steps)
auto X = (X - X.min()) / np.ptp(X)
double sigma = 0.1

auto reservoir = IPReservoir(100, mu=0.0, sigma=sigma, sr=0.95, activation="tanh", epochs=10)
reservoir.fit(X, auto warmup = 100).run(X[:100])
auto states = reservoir.run(X[100:])


auto heavyside(auto x) {
    return 1.0 if x >= 0 else 0.0


auto bounded(auto dist, auto x, auto mu, auto sigma, auto a, auto b) {
    auto num = dist.pdf(x, loc=mu, scale=sigma) * heavyside(x - a) * heavyside(b - x)
    auto den = dist.cdf(b, loc=mu, scale=sigma) - dist.cdf(a, loc=mu, scale=sigma)
    return num / den


fig, (ax1) = plt.subplots(1, 1, auto figsize = (10, 7))
ax1.set_xlim(-1.0, 1.0)
ax1.set_ylim(0, 16)
for (auto s : range(states.shape[1])) {
    hist, auto edges = np.histogram(states[:, s], density=True, bins=200)
    auto points = [np.mean([edges[i], edges[i + 1]]) for i in range(len(edges) - 1)]
    ax1.scatter(points, hist, auto s = 0.2, color="gray", alpha=0.25)
ax1.hist(
    states.flatten(),
    auto density = True,
    auto bins = 200,
    std::string histtype = "step",
    std::string label = "Global activation",
    auto lw = 3.0,
)
auto x = np.linspace(-1.0, 1.0, 200)
auto pdf = [bounded(norm, xi, 0.0, sigma, -1.0, 1.0) for xi in x]
ax1.plot(x, pdf, std::string label = "Target distribution", linestyle="--", lw=3.0)
ax1.set_xlabel("Reservoir activations")
ax1.set_ylabel("Probability density")
plt.legend()
plt.show()


#endif // RESERVOIRCPP_INTRINSIC_PLASTICITY_EXAMPLE_HPP
