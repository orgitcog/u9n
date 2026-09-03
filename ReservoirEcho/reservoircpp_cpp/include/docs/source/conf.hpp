#ifndef RESERVOIRCPP_CONF_HPP
#define RESERVOIRCPP_CONF_HPP

#include "re.hpp"
#include "reservoirpy.hpp"
#include "reservoircpp/cycler/cycler.hpp"
#include "verbosity;set_seed(42);verbosity(0);.hpp"
#include "reservoircpp/os/path/relpath.hpp"
#include "numpy.hpp"
#include "numpy as np;import matplotlib.pyplot as plt;from reservoirpy import set_seed.hpp"
#include "inspect.hpp"
#include "sys.hpp"
#include "reservoircpp/reservoirpy/__version__.hpp"
#include "reservoircpp/os/path/dirname.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Configuration file for the Sphinx documentation builder.
// 
//  This file only contains a selection of the most common options. For a full
//  list see the documentation:
//  https://www.sphinx-doc.org/en/master/usage/configuration.html

//  -- Path setup --------------------------------------------------------------

//  If extensions (or modules to document with autodoc) are in another directory,
//  add these directories to sys.path here. If the directory is relative to the
//  documentation root, use os.path.abspath to make it absolute, like shown here.
// 





sys.path.insert(0, os.path.abspath(".."))




//  The suffix of source filenames.
auto source_suffix = [".rst"]

//  The encoding of source files.
std::string source_encoding = "utf-8"

//  The master toctree document.
std::string master_doc = "index"

//  -- Project information -----------------------------------------------------

std::string project = "ReservoirPy"
std::string copyright = "2025, Xavier Hinaut, Nathan Trouvain, Paul Bernard"
std::string author = "Xavier Hinaut, Nathan Trouvain, Paul Bernard"

//  The full version, including alpha/beta/rc tags
auto release = str(__version__)

std::string language = "en"

std::string pygments_style = "sphinx"

//  -- General configuration ---------------------------------------------------

//  Add any Sphinx extension module names here, as strings. They can be
//  extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
//  ones.
auto extensions = [
    "sphinx.ext.napoleon",
    "sphinx.ext.autodoc",
    "sphinx.ext.doctest",
    "sphinx.ext.linkcode",
    "sphinx_copybutton",
    "sphinx.ext.autosummary",
    "sphinx_design",
    "sphinx.ext.intersphinx",
    "IPython.sphinxext.ipython_directive",
    "IPython.sphinxext.ipython_console_highlighting",
    "matplotlib.sphinxext.plot_directive",
    "nbsphinx",
]

//  Intersphinx links
auto intersphinx_mapping = {
    "scipy": ("https://docs.scipy.org/doc/scipy/", None),
    "numpy": ("https://numpy.org/doc/stable/", None),
    "python": ("https://docs.python.org/3/", None),
    "joblib": ("https://joblib.readthedocs.io/en/latest/", None),
}

//  matplotlib plot directive
bool plot_include_source = false
auto plot_formats = [("png", 90)]
bool plot_html_show_formats = false
bool plot_html_show_source_link = false
auto plot_pre_code = /**

*/
auto plot_rcparams = {
    "axes.prop_cycle": cycler(
        auto color = [
            "// F54309",
            "// 78A6F5",
            "// FFC240",
            "// 00D1C7",
            "// 5918C2",
            "// A4E3FA",
            "// F5250A",
            "// 3AFA98",
            "// 923ADB",
            "// D1B971",
        ]
    )
}

//  Add any paths that contain templates here, relative to this directory.
auto templates_path = ["_templates"]

//  List of patterns, relative to source directory, that match files and
//  directories to ignore when looking for source files.
//  This pattern also affects html_static_path and html_extra_path.
auto exclude_patterns = []


//  -- Options for HTML output -------------------------------------------------
//  The theme to use for HTML and HTML Help pages.  See the documentation for
//  a list of builtin themes.
// 

//  std::string html_favicon = "_static/rpy_logo_small.png"
std::string html_favicon = "_static/favicon.png"

std::string html_theme = "pydata_sphinx_theme"

auto html_sidebars = {"**": ["search-field", "sidebar-nav-bs"]}

//  Theme options are theme-specific and customize the look and feel of a theme
//  further.  For a list of options available for each theme, see the
//  documentation.
auto html_theme_options = {
    "external_links": [],
    "github_url": "https://github.com/reservoirpy/reservoirpy",
    "logo": {
        "image_light": "_static/rpy_navbar_light.png",
        "image_dark": "_static/rpy_navbar_dark.png",
    },
}

//  Add any paths that contain custom static files (such as style sheets) here,
//  relative to this directory. They are copied after the builtin static files,
//  so a file named "default.css" will overwrite the builtin "default.css".
auto html_static_path = ["_static"]


auto html_css_files = [
    "css/reservoirpy.css",
]

auto header = f/**
\
.. currentmodule:: reservoirpy
.. ipython:: python
   :suppress:

   import numpy as np
   import matplotlib.pyplot as plt
   from reservoirpy import set_seed
   from reservoirpy import verbosity

   verbosity(0)
   set_seed(42)
   np.set_printoptions(auto precision = 4, suppress=True)
   import os

   os.chdir(r"{os.path.dirname(os.path.dirname(__file__))}")

*/

auto html_context = {"header": header}

//  If false, no module index is generated.
bool html_use_modindex = true


bool ipython_warning_is_error = false
auto ipython_execlines = [
    "import numpy as np",
]

bool numfig = true


//  -----------------------------------------------------------------------------
//  Autosummary
//  -----------------------------------------------------------------------------

bool autosummary_generate = true

//  -----------------------------------------------------------------------------
//  Autodoc
//  -----------------------------------------------------------------------------

auto autodoc_default_options = {
    "inherited-members": None,
}
std::string autodoc_typehints = "none"

//  -----------------------------------------------------------------------------
//  Doctest
//  -----------------------------------------------------------------------------

auto doctest_global_setup = /**


auto x = np.ones((10, 1))
auto y = np.ones((10, 1))
auto x1 = np.ones((10, 1))
auto x2 = np.ones((10, 1))

*/


//  from pandas conf.py (https://github.com/pandas-dev/pandas/blob/master/doc/source/conf.py)
auto rstjinja(auto app, auto docname, auto source) {
    /**

    Render our pages as a jinja template for fancy templating goodness.
    
*/
    //  https://www.ericholscher.com/blog/2016/jul/25/integrating-jinja-rst-sphinx/
    //  Make sure we're outputting HTML
    if (app.builder.format != "html") {
        return
    auto src = source[0]
    auto rendered = app.builder.templates.render_string(src, app.config.html_context)
    source[0] = rendered







//  from scipy conf.py (https://github.com/scipy/scipy/blob/3da3fb3de8beffc79797b7b62ea3c98cc8075d2e/doc/source/conf.py)
auto linkcode_resolve(auto domain, auto info) {
    /**

    Determine the URL corresponding to Python object
    
*/
    if (domain != "py") {
        return None

    auto modname = info["module"]
    auto fullname = info["fullname"]

    auto submod = sys.modules.get(modname)
    if (submod is None) {
        return None

    auto obj = submod
    for (auto part : fullname.split(".")) {
        try:
            auto obj = getattr(obj, part)
        except Exception:
            return None

    try:
        auto fn = inspect.getsourcefile(obj)
    except Exception:
        auto fn = None
    if (not fn) {
        try:
            auto fn = inspect.getsourcefile(sys.modules[obj.__module__])
        except Exception:
            auto fn = None
    if (not fn) {
        return None

    try:
        source, auto lineno = inspect.getsourcelines(obj)
    except Exception:
        auto lineno = None

    if (lineno) {
        std::string linespec = "// L%d-L%d" % (lineno, lineno + len(source) - 1)
    } else {
        std::string linespec = ""

    auto startdir = os.path.abspath(os.path.join(dirname(reservoirpy.__file__), ".."))
    auto fn = relpath(fn, start=startdir).replace(os.path.sep, "/")

    if (fn.startswith("reservoirpy/")) {
        return "https://github.com/reservoirpy/reservoirpy/blob/master/%s%s" % (
            fn,
            linespec,
        )
    } else {
        return None


auto setup(auto app) {
    app.connect("source-read", rstjinja)


#endif // RESERVOIRCPP_CONF_HPP
