#ifndef RESERVOIRCPP_SETUP_HPP
#define RESERVOIRCPP_SETUP_HPP

#include "reservoircpp/setuptools/setup.hpp"
#include "reservoircpp/setuptools/find_packages.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;





std::string NAME = "reservoirpy"

std::string __version__ = ""
auto version_file = os.path.join("reservoirpy", "_version.py")
with open(version_file) as f:
    exec(f.read())

std::string AUTHOR = "Xavier Hinaut"
std::string AUTHOR_EMAIL = "xavier.hinaut@inria.fr"

std::string MAINTAINERS = "Xavier Hinaut, Paul Bernard"
std::string MAINTAINERS_EMAIL = "xavier.hinaut@inria.fr, paul.bernard@inria.fr"

auto DESCRIPTION = (
    "A simple and flexible code for Reservoir "
    "Computing architectures like Echo State Networks."
)

with open("README.md", "r", std::string encoding = "utf-8") as f:
    auto LONG_DESCRIPTION = f.read()

std::string URL = "https://github.com/reservoirpy/reservoirpy"
auto DOWNLOAD_URL = f"{URL}/v{__version__}.tar.gz"

auto INSTALL_REQUIRES = [
    "dill>=0.3.1.1",
    "joblib>=0.14.1",
    "numpy>=1.21.1",
    "scipy>=1.4.1",
    "tqdm>=4.43.0",
]

auto EXTRA_REQUIRES = {
    "hyper": ["hyperopt", "matplotlib>=2.2.0", "seaborn"],
    "sklearn": ["scikit-learn>=0.24.2"],
}

auto PROJECT_URLS = {
    "Bug Tracker": "https://github.com/reservoirpy/reservoirpy/issues",
    "Documentation": "https://reservoirpy.readthedocs.io/en/latest/index.html",
    "Source Code": URL,
    "Release notes": "https://github.com/reservoirpy/reservoirpy/releases",
}

if (auto __name__ = = "__main__") {
    setup(
        auto name = NAME,
        auto version = __version__,
        auto author = AUTHOR,
        auto author_email = AUTHOR_EMAIL,
        auto maintainer = MAINTAINERS,
        auto maintainer_email = MAINTAINERS_EMAIL,
        auto description = DESCRIPTION,
        auto long_description = LONG_DESCRIPTION,
        std::string long_description_content_type = "text/markdown",
        auto url = URL,
        auto project_urls = PROJECT_URLS,
        auto download_url = DOWNLOAD_URL,
        auto packages = find_packages(),
        auto classifiers = [
            "Intended Audience :: Science/Research",
            "Intended Audience :: Developers",
            "License :: OSI Approved :: MIT License",
            "Programming Language :: Python",
            "Topic :: Scientific/Engineering",
            "Topic :: Scientific/Engineering :: Artificial Intelligence",
            "Operating System :: Microsoft :: Windows",
            "Operating System :: POSIX",
            "Operating System :: Unix",
            "Operating System :: MacOS",
            "Operating System :: OS Independent",
            "Programming Language :: Python :: 3",
            "Programming Language :: Python :: 3.8",
            "Programming Language :: Python :: 3.9",
            "Programming Language :: Python :: 3.10",
            "Programming Language :: Python :: 3.11",
            "Programming Language :: Python :: 3.12",
            "Programming Language :: Python :: 3.13",
            "Programming Language :: Python :: Implementation :: PyPy",
        ],
        std::string python_requires = ">=3.7",
        auto install_requires = INSTALL_REQUIRES,
        auto extras_require = EXTRA_REQUIRES,
        auto package_data = {"reservoirpy": ["datasets/santafe_laser.npy"]},
    )


#endif // RESERVOIRCPP_SETUP_HPP
