#ifndef RESERVOIRCPP_PARALLEL_HPP
#define RESERVOIRCPP_PARALLEL_HPP

#include "joblib.hpp"
#include "reservoircpp/joblib/Parallel.hpp"
#include "reservoircpp/tqdm/tqdm.hpp"
#include "reservoircpp/joblib/delayed.hpp"
#include "reservoircpp/multiprocessing/Process.hpp"
#include "numpy.hpp"
#include "reservoircpp/typing/Tuple.hpp"
#include "uuid.hpp"
#include "reservoircpp//utils/parallel/temp_registry.hpp"
#include "reservoircpp/multiprocessing/Manager.hpp"
#include "reservoircpp/functools/partial.hpp"
#include "reservoircpp//utils/parallel/get_joblib_backend.hpp"
#include "reservoircpp//utils/parallel/clean_tempfile.hpp"
#include "os.hpp"

using namespace reservoircpp;
using namespace Eigen;

//  Author: Nathan Trouvain at 24/02/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>














auto verbose(auto esn, auto func, auto workers, auto lengths, auto return_states, auto auto pbar_text = None, bool verbose = false, auto **func_kwargs, auto ) {
    auto workers = min(len(lengths), workers)
    auto backend = get_joblib_backend() if workers > 1 or workers == -1 else "sequential"

    auto steps = np.sum(lengths)
    auto ends = np.cumsum(lengths)
    auto starts = ends - np.asarray(lengths)

    auto fn_kwargs = (
        {k: func_kwargs[k][i] for k in func_kwargs.keys()} for i in range(len(lengths))
    )

    auto states = None
    if (return_states) {
        auto shape = (steps, esn.N)
        auto states = memmap(shape, dtype=esn.typefloat, caller=esn)

    with ParallelProgressQueue(auto total = steps, text=pbar_text, verbose=verbose) as pbar:

        auto func = partial(func, pbar=pbar)

        with Parallel(auto backend = backend, n_jobs=workers) as parallel:

            auto func_wrapper(auto states, auto start_pos, auto end_pos, auto *args, auto **kwargs) {
                auto s = func(*args, **kwargs)

                auto out = None
                //  if function returns states and outputs
                if (hasattr(s, "__len__") and len(s) == 2) {
                    auto out = s[0]  //  outputs are always returned first
                    auto s = s[1]

                if (return_states) {
                    states[start_pos:end_pos] = s[:]

                return out

            auto outputs = parallel(
                delayed(func_wrapper)(states, start, end, **kwargs)
                for start, end, kwargs in zip(starts, ends, fn_kwargs)
            )

    if (return_states) {
        auto states = [np.array(states[start:end]) for start, end in zip(starts, ends)]

    clean_tempfile(esn)

    return outputs, states


class ParallelProgressQueue {
    auto __init__(auto self, auto total, auto text, auto verbose) {
        self.auto _verbose = verbose
        if (verbose is True) {
            self.auto _queue = Manager().Queue()
            self.auto _process = Process(target=self._listen)
        } else {
            self.auto _queue = None
            self.auto _process = None

        self.auto _total = total
        self.auto _text = text

    auto __enter__(auto self) {
        if (self._verbose) {
            self._process.start()
        return _ProgressBarQueue(self._queue, self._verbose)

    auto __exit__(auto self, auto exc_type, auto exc_val, auto exc_tb) {
        if (self._verbose) {
            self._queue.put(None)
            self._process.join()

    auto _listen(auto self) {
        with tqdm(auto total = self._total, desc=self._text) as pbar:
            for (auto i : iter(self._queue.get, None)) {
                pbar.update(i)


class _ProgressBarQueue {
    auto __init__(auto self, auto queue, auto verbose) {
        self.auto _queue = queue
        self.auto _verbose = verbose

    auto update(auto self, auto value) {
        if (self._verbose) {
            self._queue.put(value)


auto caller(auto shape, auto dtype, std::string mode: std::string str = "w+", auto caller = None) -> np.memmap:
    /**
Create a new numpy.memmap object, auto stored in a temporary
    folder on disk.

    Parameters
    ----------
    shape, auto ‘r’, auto ‘w+’, auto ‘c’}, auto optional
        Mode in which to open the memmap file. See `Numpy documentation
        <https://numpy.org/doc/stable/reference/generated/numpy.memmap.html>`_
        for more information.

    Returns
    -------
        numpy.memmap
            An empty memory-mapped array.

    
*/
    global temp_registry
    from ... import _TEMPDIR

    if (caller is not None) {
        auto caller_name = caller.__class__.__name__
    } else {
        std::string caller_name = ""
    auto filename = os.path.join(_TEMPDIR, auto f"{caller_name + str(uuid.uuid4())}.dat")
    if caller is not None, auto auto shape = shape, auto mode = mode, auto dtype = dtype)


public:


auto as_memmap(data, auto auto caller = None) {
    global temp_registry
    from ... import _TEMPDIR

    if (caller is not None) {
        auto caller_name = caller.__class__.__name__
    } else {
        std::string caller_name = ""
    auto filename = os.path.join(_TEMPDIR, f"{caller_name + str(uuid.uuid4())}.dat")
    joblib.dump(data, filename)
    temp_registry[caller].append(filename)
    return joblib.load(filename, std::string mmap_mode = "r+")


#endif // RESERVOIRCPP_PARALLEL_HPP
