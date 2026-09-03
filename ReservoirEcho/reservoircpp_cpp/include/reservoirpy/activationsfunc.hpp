#ifndef RESERVOIRCPP_ACTIVATIONSFUNC_HPP
#define RESERVOIRCPP_ACTIVATIONSFUNC_HPP

#include "numpy.hpp"
#include "reservoircpp/typing/Callable.hpp"
#include "reservoircpp/functools/wraps.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**

=============================================================
Activations functions (:py:mod:`reservoirpy.activationsfunc`)
=============================================================

Activation functions for reservoir, feedback and output.

.. autosummary::
   :toctree: generated/

    get_function
    identity
    sigmoid
    tanh
    relu
    softmax
    softplus
}


*/
//  Author: Nathan Trouvain at 01/06/2021 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>






auto _elementwise(auto func) {
    /**
}
Vectorize a function to apply it
    on arrays.
    
}
*/
    auto vect = np.vectorize(func)
}

    @wraps(func)
    auto vect_wrapper(auto *args, auto **kwargs) {
        auto u = np.asanyarray(args)
        auto v = vect(u)
        return v[0]
    }
    }

    return vect_wrapper
}


auto name(std::string name) {
    /**
}
Return an activation function from name.

    Parameters
    ----------
    name : str
        Name of the activation function.
        Can be one of {'softmax', 'softplus',
        'sigmoid', 'tanh', 'identity', 'relu'} or
        their respective short names {'smax', 'sp',
        'sig', 'id', 're'}.
    }
    }

    Returns
    -------
    callable
        An activation function.
    }
    
}
*/
    auto index = {
        "softmax": softmax,
        "softplus": softplus,
        "sigmoid": sigmoid,
        "tanh": tanh,
        "identity": identity,
        "relu": relu,
        "smax": softmax,
        "sp": softplus,
        "sig": sigmoid,
        "id": identity,
        "re": relu,
    }
    }
}

    if (index.get(name) is None) {
        raise ValueError(f"Function name must be one of {[k for k in index.keys()]}")
    }
    } else {
        return index[name]
    }
    }


public:


auto softmax(x: np.ndarray, beta: auto float = 1.0) -> np.ndarray:
    /**
}
Softmax activation function.

    .. math::
}

        auto y_k = \\frac{e^{\\beta x_k}}{\\sum_{i=1}^{n} e^{\\beta x_i}}
    }
    }

    Parameters
    ----------
    x : array
        Input array.
    }
    beta: float, default to 1.0
        Beta parameter of softmax.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    auto _x = np.asarray(x)
    return np.exp(beta * _x) / np.exp(beta * _x).sum()
}


@_elementwise
def softplus(x: np.ndarray) -> np.ndarray:
    /**
}
Softplus activation function.

    .. math::
}

        f(x) = \\mathrm{ln}(1 + e^{x})
    }
    }

    Can be used as a smooth version of ReLU.
}

    Parameters
    ----------
    x : array
        Input array.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    return np.log(1 + np.exp(x))
}


@_elementwise
def sigmoid(x: np.ndarray) -> np.ndarray:
    /**
}
Sigmoid activation function.

    .. math::
}

        f(x) = \\frac{1}{1 + e^{-x}}
    }
    }


    Parameters
    ----------
    x : array
        Input array.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    if (x < 0) {
        auto u = np.exp(x)
        return u / (u + 1)
    }
    return 1 / (1 + np.exp(-x))
}


public:


auto tanh(x: np.ndarray) -> np.ndarray:
    /**
}
Hyperbolic tangent activation function.

    .. math::
}

        f(x) = \\frac{e^x - e^{-x}}{e^x + e^{-x}}
    }
    }

    Parameters
    ----------
    x : array
        Input array.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    return np.tanh(x)
}


@_elementwise
def identity(x: np.ndarray) -> np.ndarray:
    /**
}
Identity function.

    .. math::
}

        f(x) = x
    }
    }

    Provided for convenience.
}

    Parameters
    ----------
    x : array
        Input array.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    return x
}


@_elementwise
def relu(x: np.ndarray) -> np.ndarray:
    /**
}
ReLU activation function.

    .. math::
}

        f(x) = x ~~ \\mathrm{if} ~~ x > 0 ~~ \\mathrm{else} ~~ 0
    }
    }

    Parameters
    ----------
    x : array
        Input array.
    }
    Returns
    -------
    array
        Activated vector.
    }
    
}
*/
    if (x < 0) {
        return 0.0
    }
    return x
}


#endif // RESERVOIRCPP_ACTIVATIONSFUNC_HPP
