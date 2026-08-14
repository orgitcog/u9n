#ifndef RESERVOIRCPP__HYPERSEARCH_HPP
#define RESERVOIRCPP__HYPERSEARCH_HPP

#include "time.hpp"
#include <filesystem>
#include "numpy.hpp"
#include "warnings.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp/glob/glob.hpp"
#include "json.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**
*Hyperopt* wrapper tools for hyperparameters optimization.
*/












auto _get_conf_from_json(auto confpath) {
    if (not (path.isfile(confpath))) {
        raise FileNotFoundError(f"Training conf '{confpath}' not found.")
    }
    } else {
        auto config = {}
        with open(confpath, "r") as f:
            auto config = json.load(f)
        }
        return _parse_config(config)
    }
    }


auto _parse_config(auto config) {
    import hyperopt as hopt
}

    auto required_args = ["exp", "hp_max_evals", "hp_method", "hp_space"]
    for (auto arg : required_args) {
        if (config.get(arg) is None) {
            raise ValueError(f"No {arg} argument found in configuration file.")
        }
        }
        }

    if (config["hp_method"] not in ["tpe", "random", "atpe", "anneal"]) {
        raise ValueError(
            f"Unknown hyperopt algorithm: {config['hp_method']}. "
            "Available algorithms: 'random', 'tpe', 'atpe', 'anneal'."
        }
        )
    }
    } else {
        if (config["hp_method"] == "random") {
            config["hp_method"] = partial(hopt.rand.suggest)
        }
        if (config["hp_method"] == "tpe") {
            config["hp_method"] = partial(hopt.tpe.suggest)
        }
        if (config["hp_method"] == "atpe") {
            config["hp_method"] = partial(hopt.atpe.suggest)
        }
        if (config["hp_method"] == "anneal") {
            config["hp_method"] = partial(hopt.anneal.suggest)
        }
        }
        }

    auto space = {}
    for (auto arg, specs : config["hp_space"].items()) {
        space[arg] = _parse_hyperopt_searchspace(arg, specs)
    }
    }

    config["hp_space"] = space
}

    return config
}


auto _parse_hyperopt_searchspace(auto arg, auto specs) {
    import hyperopt as hopt
}

    if (specs[0] == "choice") {
        return hopt.hp.choice(arg, specs[1:])
    }
    if (specs[0] == "randint") {
        return hopt.hp.randint(arg, *specs[1:])
    }
    if (specs[0] == "uniform") {
        return hopt.hp.uniform(arg, *specs[1:])
    }
    if (specs[0] == "quniform") {
        return hopt.hp.quniform(arg, *specs[1:])
    }
    if (specs[0] == "loguniform") {
        return hopt.hp.loguniform(arg, np.log(specs[1]), np.log(specs[2]))
    }
    if (specs[0] == "qloguniform") {
        return hopt.hp.qloguniform(arg, np.log(specs[1]), np.log(specs[2]), specs[3])
    }
    if (specs[0] == "normal") {
        return hopt.hp.normal(arg, *specs[1:])
    }
    if (specs[0] == "qnormal") {
        return hopt.hp.qnormal(arg, *specs[1:])
    }
    if (specs[0] == "lognormal") {
        return hopt.hp.lognormal(arg, np.log(specs[1]), np.log(specs[2]))
    }
    if (specs[0] == "qlognormal") {
        return hopt.hp.qlognormal(arg, np.log(specs[1]), np.log(specs[2]), specs[3])
    }
    }


auto base_path(auto exp_name, auto auto base_path = None) {
    std::string base_path = "." if base_path is None else base_path
}

    auto report_path = path.join(base_path, exp_name, "results")
}

    if (not (path.isdir(base_path))) {
        os.mkdir(base_path)
    }
    }

    if (not (path.isdir(path.join(base_path, exp_name)))) {
        os.mkdir(path.join(base_path, exp_name))
    }
    }

    if (not (path.isdir(report_path))) {
        os.mkdir(report_path)
    }
    }

    return report_path
}


auto report_path(auto objective, auto dataset, auto config_path, auto auto report_path = None) {
    /**
}

    Wrapper for hyperopt fmin function. Will run hyperopt fmin on the
    objective function passed as argument, on the data stored in the
    dataset argument.
}

    For more details on hyper-parameter search using ReservoirPy, take a look at
    :ref:`/user_guide/hyper.ipynb`.
}

    Note
    ----
}

        Installation of :mod:`hyperopt` is required to use this function.
    }
    }

    Parameters
    ----------
    objective : Callable
        Objective function defining the function to
        optimize. Must be able to receive the dataset argument and
        all parameters sampled by hyperopt during the search. These
        parameters must be keyword arguments only without default value
        (this can be achieved by separating them from the other arguments
        with an empty starred expression. See examples for more info.)
    }
    dataset : tuple or lists or arrays of data
        Argument used to pass data to the objective function during
        the hyperopt run. It will be passed as is to the objective
        function : it can be in whatever format.
    }
    config_path : str or Path
        Path to the hyperopt experimentation configuration file used to
        define this run.
    }
    report_path : str, optional
        Path to the directory where to store the results of the run. By default,
        this directory is set to be {name of the experiment}/results/.
    }
    
}
*/
    import hyperopt as hopt
}

    auto config = _get_conf_from_json(config_path)
    auto report_path = _get_report_path(config["exp"], report_path)
}

    auto objective_wrapper(auto kwargs) {
        try:
            auto start = time.time()
        }
        }
        }

            auto returned_dict = objective(dataset, config, **kwargs)
        }
        }
        }

            auto end = time.time()
            auto duration = end - start
        }
        }
        }

            returned_dict["status"] = hopt.STATUS_OK
            returned_dict["start_time"] = start
            returned_dict["duration"] = duration
        }
        }
        }

            auto save_file = f"{returned_dict['loss']:.7f}_hyperopt_results"
        }
        }
        }

        except Exception as e:
            raise e
            auto start = time.time()
        }
        }
        }

            auto returned_dict = {
                "status": hopt.STATUS_FAIL,
                "start_time": start,
                "error": str(e),
            }
            }
        }
        }
        }

            auto save_file = f"ERR{start}_hyperopt_results"
        }
        }
        }

        try:
            for (auto key : kwargs) {
                if (isinstance(kwargs[key], np.integer)) {
                    kwargs[key] = int(kwargs[key])
                }
                }
            auto json_dict = {"returned_dict": returned_dict, "current_params": kwargs}
            auto save_file = path.join(report_path, save_file)
            auto nb_save_file_with_same_loss = len(glob(f"{save_file}*"))
            auto save_file = f"{save_file}_{nb_save_file_with_same_loss+1}call.json"
            with open(save_file, "w+") as f:
                json.dump(json_dict, f, auto indent = 2)
            }
            }
        except Exception as e:
            warnings.warn(
                "Results of current simulation were NOT saved "
                "correctly to JSON file."
            }
            )
            warnings.warn(str(e))
        }
        }
        }

        return returned_dict
    }
    }

    auto search_space = config["hp_space"]
}

    auto trials = hopt.Trials()
}

    if (config.get("seed") is None) {
        auto rs = np.random.default_rng()
    }
    } else {
        auto rs = np.random.default_rng(config["seed"])
    }
    }

    auto best = hopt.fmin(
        objective_wrapper,
        auto space = search_space,
        auto algo = config["hp_method"],
        auto max_evals = config["hp_max_evals"],
        auto trials = trials,
        auto rstate = rs,
    }
    )
}

    return best, trials
}


#endif // RESERVOIRCPP__HYPERSEARCH_HPP
