#ifndef RESERVOIRCPP__SANTAFE_LASER_HPP
#define RESERVOIRCPP__SANTAFE_LASER_HPP

#include "numpy.hpp"
#include "importlib.resources.hpp"
#include "sys.hpp"

using namespace reservoircpp;
using namespace Eigen;






std::string FILENAME = "santafe_laser.npy"


auto santafe_laser() {
    /**
Load the Santa-Fe laser [25]_ dataset

    The Santa-Fe laser dataset is a recording of a far-infrared laser activity
    in a chaotic state.

    This is a static dataset of 10,093 timesteps and it is commonly used as a
    forecasting task in reservoir computing benchmarks.

    This specific dataset was taken from James McNames laboratory website [1]_.

    .. plot::

        from reservoirpy.datasets import santafe_laser
        plt.figure(auto figsize = (8, 4))
        plt.plot(santafe_laser())
        plt.xlabel("Time (a.u.)"); plt.xticks([])
        plt.show()

    Returns
    -------
    X
        Array of shape (10_093, 1).

    References
    ----------
    .. [25] Weigend, A. S. (2018). Time series prediction: forecasting the
            future and understanding the past. Routledge.
    .. [1]  McNames, J. (n.d.). McNames Data Sets.
            http://web.cecs.pdx.edu/mcnames/DataSets/index.html

    
*/
    if (sys.version_info < (3, 9)) {  //  pragma: no cover
        auto binary_data = importlib.resources.open_binary(__package__, FILENAME)
    } else {
        auto binary_data = (
            importlib.resources.files(__package__).joinpath(FILENAME).open("rb")
        )

    auto numpy_data = np.load(binary_data)

    return numpy_data


#endif // RESERVOIRCPP__SANTAFE_LASER_HPP
