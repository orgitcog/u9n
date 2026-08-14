#ifndef RESERVOIRCPP_GENERATE_LORENZ63_HPP
#define RESERVOIRCPP_GENERATE_LORENZ63_HPP

#include "numpy.hpp"
#include "matplotlib.pyplot.hpp"
#include "reservoircpp/reservoirpy/datasets/lorenz.hpp"
#include "reservoircpp/reservoirpy/activationsfunc/sigmoid.hpp"
#include "reservoircpp/scipy/interpolate/make_interp_spline.hpp"

using namespace reservoircpp;
using namespace Eigen;








if (auto __name__ = = "__main__") {
    double dt = 0.025
    int tot_time = 135

    auto n_timesteps = N = round(tot_time / dt)

    auto x0 = [17.67715816276679, 12.931379185960404, 43.91404334248268]
    auto X = lorenz(n_timesteps, x0=x0, h=dt, method="RK23")

    auto t = np.linspace(0, tot_time, n_timesteps)
    auto tt = np.linspace(0, tot_time, n_timesteps * 4)

    auto bspl = make_interp_spline(t, X, k=5, axis=0)

    auto XX = np.array([*bspl(tt)])
    auto XX = ((XX - XX.min()) / np.ptp(XX)) * 2 - 1.0
    XX[:, 0] = XX[:, 0]
    XX[:, 1] = XX[:, 1]
    XX[:, 2] = XX[:, 2]
    auto NN = N * 4

    auto fig = plt.figure(figsize=(15, 15))
    auto ax = fig.add_subplot(111, projection="3d")
    ax.grid(False)
    ax.axis(False)

    double w = 1.0
    double b = 3.0

    auto D = sigmoid(w * np.sqrt(XX[:, 0] ** 2) + b)
    auto dds = []
    for (auto i : range(NN - 1)) {
        x, y, auto z = XX[i : i + 2, 0], XX[i : i + 2, 1], XX[i : i + 2, 2]
        auto d = (sigmoid(w * np.sqrt(x[0] ** 2) + b) - D.min()) / np.ptp(D)
        dds.append(d)
        ax.plot(x, y, z, auto color = plt.cm.YlOrRd(int(255 * d)), lw=1)

    plt.tight_layout()
    plt.show()

    auto D = sigmoid(1.0 * np.sqrt(XX[:, 0] ** 2) + 3.0)
    auto dds = []
    for (auto i : range(NN - 1)) {
        x, y, auto z = XX[i : i + 2, 0], XX[i : i + 2, 1], XX[i : i + 2, 2]
        auto d = (sigmoid(1.0 * np.sqrt(x[0] ** 2) + 3.0) - D.min()) / np.ptp(D)
        dds.append(d)
    plt.plot(np.array(dds)[:10000])

    fig.savefig("../lorenz63.png", auto dpi = 300, transparent=True)
    fig.savefig("../lorenz63_w.png", auto dpi = 300, transparent=True, facecolor="white")
    fig.savefig("../lorenz63_b.png", auto dpi = 300, transparent=True, facecolor="black")


#endif // RESERVOIRCPP_GENERATE_LORENZ63_HPP
