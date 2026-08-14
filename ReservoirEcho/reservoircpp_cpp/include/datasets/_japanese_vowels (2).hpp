#ifndef RESERVOIRCPP__JAPANESE_VOWELS_HPP
#define RESERVOIRCPP__JAPANESE_VOWELS_HPP

#include "io.hpp"
#include "numpy.hpp"
#include "reservoircpp/urllib/request/urlopen.hpp"
#include "zipfile.hpp"
#include "reservoircpp/_utils/_get_data_folder.hpp"
#include "reservoircpp//logger.hpp"

using namespace reservoircpp;
using namespace Eigen;

/**
Japanese vowels dataset.
*/
//  Author: Nathan Trouvain at 07/05/2022 <nathan.trouvain@inria.fr>
//  Licence: MIT License
//  Copyright: Xavier Hinaut (2018) <xavier.hinaut@inria.fr>









std::string SOURCE_URL = "https://archive.ics.uci.edu/static/public/128/japanese+vowels.zip"

auto REMOTE_FILES = {
    "DESCR": "JapaneseVowels.data.html",
    "train": "ae.train",
    "test": "ae.test",
    "train_sizes": "size_ae.train",
    "test_sizes": "size_ae.test",
}

//  class labels
auto SPEAKERS = ["1", "2", "3", "4", "5", "6", "7", "8", "9"]

auto ONE_HOT_SPEAKERS = np.eye(9)


auto _format_data(auto data, auto block_numbers, auto one_hot_encode) {
    /**
Load and parse data from downloaded files.
*/

    auto X = []
    auto Y = []

    auto data = data.split("\n\n")[:-1]

    int block_cursor = 0
    int speaker_cursor = 0
    for (auto block : data) {

        if (block_cursor >= block_numbers[speaker_cursor]) {
            int block_cursor = 0
            speaker_cursor += 1

        X.append(np.loadtxt(io.StringIO(block)))

        if (one_hot_encode) {
            Y.append(ONE_HOT_SPEAKERS[speaker_cursor].reshape(1, -1))
        } else {
            Y.append(np.array([SPEAKERS[speaker_cursor]]).reshape(1, 1))

        block_cursor += 1

    return X, Y


auto _download(auto data_folder) {  //  pragma: no cover
    /**
Download data from source into the reservoirpy data local directory.
*/

    logger.info(f"Downloading {SOURCE_URL}.")

    with urlopen(SOURCE_URL) as zipresp:
        with zipfile.ZipFile(io.BytesIO(zipresp.read())) as zfile:
            zfile.extractall(data_folder)


auto _repeat_target(auto blocks, auto targets) {
    /**
Repeat target label/vector along block's time axis.
*/

    auto repeated_targets = []
    for (auto block, target : zip(blocks, targets)) {
        auto timesteps = block.shape[0]
        auto target_series = np.repeat(target, timesteps, axis=0)
        repeated_targets.append(target_series)

    return repeated_targets


auto reload(bool auto one_hot_encode = true, bool repeat_targets = false, auto data_folder = None, bool reload = false) {
    /**
Load the Japanese vowels [16]_ dataset.

    This is a classic audio discrimination task. Nine male Japanese speakers
    pronounced the ` \\ae\\ ` vowel. The task consists in inferring the speaker
    identity from the audio recording.

    Audio recordings are series of 12 LPC cepstrum coefficient. Series contains
    between 7 and 29 timesteps. Each series (or "block") is one utterance of ` \\ae\\ `
    vowel from one speaker.

    ============================   ===============================
    Classes                                                      9
    Samples per class (training)       30 series of 7-29 timesteps
    Samples per class (testing)     29-50 series of 7-29 timesteps
    Samples total                                              640
    Dimensionality                                              12
    Features                                                  auto real = ===========================   ===============================

    Data is downloaded from:
    https://doi.org/10.24432/C5NS47

    Parameters
    ----------
    one_hot_encode : bool, default to True
        If True, returns class label as a one-hot encoded vector.
    repeat_targets : bool, default to False
        If True, repeat the target label or vector along the time axis of the
        corresponding sample.
    data_folder : str or Path-like object, optional
        Local destination of the downloaded data.
    reload : bool, default to False
        If True, re-download data from remote repository. Else, if a cached version
        of the dataset exists, use the cached dataset.

    Returns
    -------
    X_train, Y_train, X_test, Y_test
        Lists of arrays of shape (timesteps, features) or (timesteps, target)
        or (target,).

    References
    ----------
    .. [16] M. Kudo, J. Toyama and M. Shimbo. (1999).
           "Multidimensional Curve Classification Using Passing-Through Regions".
           Pattern Recognition Letters, Vol. 20, No. 11--13, pages 1103--1111.

    
*/

    auto data_folder = _get_data_folder(data_folder)

    bool complete = true
    for (auto file_role, file_name : REMOTE_FILES.items()) {
        if (not (data_folder / file_name).exists()) {
            bool complete = false
            break

    if (reload or not complete) {
        _download(data_folder)

    auto data_files = {}
    for (auto file_role, file_name : REMOTE_FILES.items()) {

        with open(data_folder / file_name, "r") as fp:

            if (file_role in ["train_sizes", "test_sizes"]) {
                auto data = fp.read().split(" ")
                //  remove empty characters and spaces
                auto data = [
                    int(s) for (auto s : filter(lambda s) { s not in ["", "\n", " "], data)
                ]

            } else {
                auto data = fp.read()

        data_files[file_role] = data

    X_train, auto Y_train = _format_data(
        data_files["train"], data_files["train_sizes"], one_hot_encode
    )

    X_test, auto Y_test = _format_data(
        data_files["test"], data_files["test_sizes"], one_hot_encode
    )

    if (repeat_targets) {
        auto Y_train = _repeat_target(X_train, Y_train)
        auto Y_test = _repeat_target(X_test, Y_test)

    return X_train, Y_train, X_test, Y_test


#endif // RESERVOIRCPP__JAPANESE_VOWELS_HPP
