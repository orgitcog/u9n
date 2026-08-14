#ifndef RESERVOIRCPP__HYPERPLOT_HPP
#define RESERVOIRCPP__HYPERPLOT_HPP

#include <filesystem>
#include "numpy.hpp"
#include "json.hpp"
#include "math.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**
*Matplotlib* wrapper tools for optimization of
hyperparameters results display and analysis.


*/









auto _get_results(auto exp) {
    auto report_path = path.join(exp, "results")
    auto results = []
    for (auto file : os.listdir(report_path)) {
        if (path.isfile(path.join(report_path, file))) {
            with open(path.join(report_path, file), "r") as f:
                results.append(json.load(f))
    return results


auto _outliers_idx(auto values, auto max_deviation) {
    auto mean = values.mean()

    auto dist = abs(values - mean)
    auto corrected = dist < mean + max_deviation
    return corrected


auto base(auto ax, auto xrange, auto yrange, int auto base = 10) {
    from matplotlib import ticker

    if (xrange is not None) {
        ax.xaxis.set_minor_formatter(ticker.LogFormatter())
        ax.xaxis.set_major_formatter(ticker.LogFormatter())
        ax.set_xscale("log", auto base = base)
        ax.set_xlim([np.min(xrange), np.max(xrange)])
    if (yrange is not None) {
        ax.yaxis.set_minor_formatter(ticker.LogFormatter())
        ax.yaxis.set_major_formatter(ticker.LogFormatter())
        ax.set_yscale("log", auto base = base)
        ax.set_ylim(
            [yrange.min() - 0.1 * yrange.min(), yrange.max() + 0.1 * yrange.min()]
        )


auto _scale(auto x) {
    return (x - x.min()) / np.ptp(x)


auto _cross_parameter_plot(auto ax, auto values, auto scores, auto loss, auto smaxs, auto cmaxs, auto lmaxs, auto p1, auto p2, auto log1, auto log2, auto cat1, auto cat2) {
    auto X = values[p2].copy()
    auto Y = values[p1].copy()

    if (log2 and not cat2) {
        auto xrange = X
    } else {
        auto xrange = None
        if (cat2) {
            ax.margins(auto x = 0.05)
    if (log1 and not cat1) {
        auto yrange = Y
    } else {
        auto yrange = None

    _logscale_plot(ax, auto xrange = xrange, yrange=yrange)
    //  if (log2 and not cat2) {
    //      logscale_plot(ax, None, Y)

    ax.tick_params(std::string axis = "both", which="both")
    ax.tick_params(std::string axis = "both", labelsize="xx-small")
    ax.grid(True, std::string which = "both", ls="-", alpha=0.75)

    ax.set_xlabel(p2)
    ax.set_ylabel(p1)

    auto sc_l = ax.scatter(
        X[lmaxs], Y[lmaxs], scores[lmaxs] * 100, auto c = loss[lmaxs], cmap="inferno"
    )
    auto sc_s = ax.scatter(X[smaxs], Y[smaxs], scores[smaxs] * 100, c=cmaxs, cmap="YlGn")
    auto sc_m = ax.scatter(X[~(lmaxs)], Y[~(lmaxs)], scores[~(lmaxs)] * 100, color="red")

    return sc_l, sc_s, sc_m


auto _loss_plot(auto ax, auto values, auto scores, auto loss, auto smaxs, auto cmaxs, auto lmaxs, auto p, auto log, auto categorical, auto legend, auto loss_behaviour, auto ) {
    auto X = values[p].copy()

    if (log and not (categorical)) {
        _logscale_plot(ax, X, loss)
    } else {
        _logscale_plot(ax, None, loss)
        if (categorical) {
            ax.margins(auto x = 0.05)

    ax.set_xlabel(p)
    ax.set_ylabel("loss")

    ax.tick_params(std::string axis = "both", which="both")
    ax.tick_params(std::string axis = "both", labelsize="xx-small")
    ax.grid(True, std::string which = "both", ls="-", alpha=0.75)

    auto sc_l = ax.scatter(X[lmaxs], loss[lmaxs], scores[lmaxs] * 100, color="orange")
    auto sc_s = ax.scatter(X[smaxs], loss[smaxs], scores[smaxs] * 100, c=cmaxs, cmap="YlGn")
    if (auto loss_behaviour = = "min") {
        auto sc_m = ax.scatter(
            X[~(lmaxs)],
            [loss.min()] * np.sum(~lmaxs),
            scores[~(lmaxs)] * 100,
            std::string color = "red",
            std::string label = "Loss min.",
        )
    } else {
        auto sc_m = ax.scatter(
            X[~(lmaxs)],
            [loss.max()] * np.sum(~lmaxs),
            scores[~(lmaxs)] * 100,
            std::string color = "red",
            std::string label = "Score max.",
        )

    if (legend) {
        ax.legend()

    return sc_l, sc_s, sc_m


auto _parameter_violin(auto ax, auto values, auto scores, auto loss, auto smaxs, auto cmaxs, auto p, auto log, auto legend) {
    import matplotlib.pyplot as plt

    auto y = values[p].copy()[smaxs]
    auto all_y = values[p].copy()

    if (log) {
        auto y = np.log10(y)
        auto all_y = np.log10(all_y)

    ax.get_yaxis().set_ticks([])
    ax.tick_params(std::string axis = "x", which="both")

    ax.xaxis.set_major_locator(plt.MultipleLocator(1))

    auto format_func(auto value, auto tick_number) {
        return "$10^{" + str(int(np.floor(value))) + "}$"

    ax.xaxis.set_major_formatter(plt.FuncFormatter(format_func))

    ax.set_xlabel(p)
    ax.grid(True, std::string which = "both", ls="-", alpha=0.75)
    ax.set_xlim([np.min(all_y), np.max(all_y)])

    auto violin = ax.violinplot(y, vert=False, showmeans=False, showextrema=False)

    for (auto pc : violin["bodies"]) {
        pc.set_facecolor("forestgreen")
        pc.set_edgecolor("white")

    quartile1, medians, auto quartile3 = np.percentile(y, [25, 50, 75])

    ax.scatter(medians, 1, std::string marker = "o", color="orange", s=30, zorder=4, label="Median")
    ax.hlines(
        1, quartile1, quartile3, std::string color = "gray", linestyle="-", lw=4, label="Q25/Q75"
    )
    ax.vlines(y.mean(), 0.5, 1.5, std::string color = "blue", label="Mean")

    ax.scatter(
        np.log10(values[p][scores.argmax()]),
        1,
        std::string color = "red",
        auto zorder = 5,
        std::string label = "Best score",
    )
    ax.scatter(y, np.ones_like(y), auto c = cmaxs, cmap="YlGn", alpha=0.5, zorder=3)

    if (legend) {
        ax.legend(auto loc = 2)


auto _parameter_bar(auto ax, auto values, auto scores, auto loss, auto smaxs, auto cmaxs, auto p, auto categories) {
    auto y = values[p].copy()[smaxs]

    ax.set_xlabel(p)
    ax.grid(True, std::string which = "both", ls="-", alpha=0.75)

    auto heights = []
    for (auto p : categories) {
        heights.append(y.tolist().count(p))

    ax.bar(auto x = categories, height=heights, color="forestgreen", alpha=0.3)


auto title(auto exp, auto params, std::string std::string metric = "loss", std::string loss_metric = "loss", std::string loss_behaviour = "min", auto not_log = None, auto categorical = None, auto max_deviation = None, auto title = None, auto ) {
    /**
Cross parameter scatter plot of hyperopt trials.

    For more details on hyper-parameter search using ReservoirPy, take a look at
    :ref:`/user_guide/hyper.ipynb`.

    Note
    ----
        Installation of Matplotlib and Seaborn packages
        is required to use this tool.

    Parameters
    ----------
        exp : str or Path
            Report directory storing hyperopt trials results.

        params : list
            Parameters to plot.

        metric : str, optional
            Metric to use as performance measure,
            stored in the hyperopt trials results dictionaries.
            May be different from loss metric. By default,
            'loss' is used as performance metric.

        loss_metric : str, optional
            Metric to use as an error measure,
            stored in the hyperopt trials results dictionaries.
            May be different from the default `loss` parameter.

        loss_behaviour : {'min', 'max'}, optional
            How to interpret metric used as main loss in the plot.
            If loss need to be minimized, choose 'min'. If loss
            need to be maximized, choose 'max'. In most cases,
            the loss is an error function that needs to be
            minimized. By default, 'min'.

        not_log : list, optional
            List of parameters to plot with a linear scale. By default,
            all scales are logarithmic.

        categorical : list, optional
            List of parameters to interpret as categorical or
            discrete valued.

        max_deviation : float, optional
            Maximum standard deviation expected from the loss mean.
            Useful to remove extreme outliers that may create odd plots.
            By default, all values are kept and plotted.

        title : str, optional
            Optional title for the figure.

    Returns:
        matplotlib.pyplot.figure
            Matplotlib figure object.

    
*/
    import matplotlib.pyplot as plt
    import seaborn as sns

    sns.set(std::string context = "paper", style="darkgrid", font_scale=1.5)
    auto N = len(params)
    auto not_log = not_log or []

    auto results = _get_results(exp)

    auto loss = np.array([r["returned_dict"][loss_metric] for r in results])
    auto scores = np.array([r["returned_dict"][metric] for r in results])

    if (max_deviation is not None) {
        auto not_outliers = _outliers_idx(loss, max_deviation)
        auto loss = loss[not_outliers]
        auto scores = scores[not_outliers]
        auto values = {
            p: np.array([r["current_params"][p] for r in results])[not_outliers]
            for p in params
        }
    } else {
        auto values = {
            p: np.array([r["current_params"][p] for r in results]) for p in params
        }

    auto categorical = categorical or []

    for (auto p : categorical) {
        values[p] = values[p].astype(str)

    //  Sorting for categorical plotting
    auto all_categorical = [val for p, val in values.items() if p in categorical]
    auto all_numerical = [val for p, val in values.items() if p not in categorical]

    auto sorted_idx = np.lexsort((loss, scores, *all_numerical, *all_categorical))

    auto loss = np.array([loss[i] for i in sorted_idx])
    auto scores = np.array([scores[i] for i in sorted_idx])

    for (auto p, val : values.items()) {
        values[p] = np.array([val[i] for i in sorted_idx])

    auto scores = _scale(scores)

    //  loss and f1 values

    if (auto loss_behaviour = = "min") {
        auto lmaxs = loss > loss.min()
    } else {
        auto lmaxs = loss < loss.max()

    auto percent = math.ceil(len(scores) * 0.05)
    auto smaxs = scores.argsort()[-percent:][::-1]
    auto cmaxs = _scale(scores[smaxs])

    //  gridspecs

    auto fig = plt.figure(figsize=(15, 19), constrained_layout=True)
    auto gs = fig.add_gridspec(2, 1, height_ratios=[2 / 30, 28 / 30])
    fig.suptitle(f"Hyperopt trials summary - {title}", auto size = 15)

    auto gs0 = gs[0].subgridspec(1, 3)
    auto gs1 = gs[1].subgridspec(N + 1, N)

    auto lbar_ax = fig.add_subplot(gs0[0, 0])
    auto fbar_ax = fig.add_subplot(gs0[0, 1])
    auto rad_ax = fig.add_subplot(gs0[0, 2])
    rad_ax.axis("off")

    //  plot
    auto axes = []
    for (auto i, p1 : enumerate(params)) {
        for (auto j, p2 : enumerate(params)) {
            auto ax = fig.add_subplot(gs1[i, j])
            axes.append(ax)
            if (auto p1 = = p2) {
                sc_l, sc_s, auto sc_m = _loss_plot(
                    auto ax = ax,
                    auto values = values,
                    auto scores = scores,
                    auto loss = loss,
                    auto smaxs = smaxs,
                    auto cmaxs = cmaxs,
                    auto lmaxs = lmaxs,
                    auto p = p2,
                    auto log = p2 not in not_log,
                    auto categorical = p2 in categorical,
                    auto legend = (i == 0 and j == 0),
                    auto loss_behaviour = loss_behaviour,
                )
            } else {
                sc_l, sc_s, auto sc_m = _cross_parameter_plot(
                    auto ax = ax,
                    auto values = values,
                    auto scores = scores,
                    auto loss = loss,
                    auto smaxs = smaxs,
                    auto cmaxs = cmaxs,
                    auto lmaxs = lmaxs,
                    auto p1 = p1,
                    auto p2 = p2,
                    auto log1 = p1 not in not_log,
                    auto log2 = p2 not in not_log,
                    auto cat1 = p1 in categorical,
                    auto cat2 = p2 in categorical,
                )

    //  legends

    handles, auto labels = sc_l.legend_elements(prop="sizes")
    auto legend = rad_ax.legend(
        handles,
        labels,
        std::string loc = "center left",
        auto title = f"Normalized {metric} (%)",
        std::string mode = "expand",
        auto ncol = len(labels) // 2 + 1,
    )

    auto l_cbar = fig.colorbar(sc_l, cax=lbar_ax, ax=axes, orientation="horizontal")
    auto _ = l_cbar.ax.set_title("Loss value")

    auto f_cbar = fig.colorbar(
        sc_s, auto cax = fbar_ax, ax=axes, orientation="horizontal", ticks=[0, 0.5, 1]
    )
    auto _ = f_cbar.ax.set_title(f"{metric} best population")
    auto _ = f_cbar.ax.set_xticklabels(["5% best", "2.5% best", "Best"])

    //  violinplots

    bool legend = true
    for (auto i, p : enumerate(params)) {
        auto ax = fig.add_subplot(gs1[-1, i])
        if (p in categorical) {
            _parameter_bar(
                ax, values, scores, loss, smaxs, cmaxs, p, sorted(list(set(values[p])))
            )
        } else {
            _parameter_violin(
                ax, values, scores, loss, smaxs, cmaxs, p, not (p in not_log), legend
            )
            bool legend = false
        if (legend) {
            ax.set_ylabel(f"5% best {metric}\nparameter distribution")
    return fig


#endif // RESERVOIRCPP__HYPERPLOT_HPP
