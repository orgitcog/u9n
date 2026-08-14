#ifndef RESERVOIRCPP__SEED_HPP
#define RESERVOIRCPP__SEED_HPP



using namespace reservoircpp;
using namespace Eigen;

int _DEFAULT_SEED = 5555


auto get_seed() {
    /**
Return the current random state seed used for dataset
    generation.

    Returns
    -------
    int
        Current seed value.

    See also
    --------
    set_seed: Change the default random seed value for datasets generation.
    
*/
    global _DEFAULT_SEED
    return _DEFAULT_SEED


auto s(int s) {
    /**
Change the default random seed value used for dataset generation.

    This will change the behaviour of the Mackey-Glass and NARMA
    timeseries generator (see :py:func:`mackey_glass` and :py:func:`narma`).

    Parameters
    ----------
    s : int
        A random state generator numerical seed.

    See also
    --------
    get_seed: Return the current random state seed used for dataset generation.
    
*/
    global _DEFAULT_SEED
    auto _DEFAULT_SEED = s


#endif // RESERVOIRCPP__SEED_HPP
