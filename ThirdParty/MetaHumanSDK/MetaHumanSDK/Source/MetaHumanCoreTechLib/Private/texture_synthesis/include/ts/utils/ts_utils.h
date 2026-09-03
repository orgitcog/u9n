// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Math.h>
#include <carbon/utils/Timer.h>
#include <carbon/io/NpyFileFormat.h>
#include <carbon/utils/TaskThreadPool.h>
#include <ts/model_data_provider_interface.h>
#include <ts/utils/cached_data_provider.h>
#include <ts/ts_types.h>
#include <Eigen/Dense>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

namespace detail {
    // default lock policy for load_npy_model_data
    struct load_npy_model_data_no_concurrency_policy {

        using lock_t = void*;
        static lock_t lock_shared() {
            return {};
        }
        static lock_t lock_unique() {
            return {};
        }

        static void raise_critical_error() {
        }

        static bool critical_error_signal_set() {
            return false;
        }
    };

    template <typename T>
    Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> ModelDataToEigenMap(const Model_Data& model_data)
    {
        const T* data = model_data.data<T>();
        size_t n_rows = model_data.rows();
        size_t n_cols = model_data.cols();
        const Eigen::Map<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> data_map(data, n_rows, n_cols);
        return data_map;
    }

    template <typename T>
    Eigen::Map<const Eigen::Matrix<Eigen::Vector3<T>, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> ReshapeSquare(Eigen::Ref<const Eigen::VectorX<T>> src)
    {
        const int n_pixels = int(src.size() / 3);
        const int size = (int)sqrt(n_pixels);
        CARBON_ASSERT(src.size() == size * size * 3, "Input vector does not contain 3 channels.");
        return Eigen::Map<const Eigen::Matrix<Eigen::Vector3<T>, -1, -1, Eigen::RowMajor>>((const Eigen::Vector3<T>*)src.data(), size, size);
    }

    template <typename T>
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> UintToFp(const uint8_t* imageData,
                                                                                             const int resolution,
                                                                                             const std::pair<Eigen::Vector3f, Eigen::Vector3f>& range,
                                                                                             TITAN_NAMESPACE::TaskThreadPool* task_thread_pool)
    {
        static_assert(std::is_same<T, uint8_t>::value || std::is_same<T, uint16_t>::value, "Only uint8_t or uint16_t are supported.");
        const T* typed_data = reinterpret_cast<const T*>(imageData);
        Eigen::Map<const Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> in_map(typed_data, resolution, resolution * 3);
        Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> imageFP = in_map.template cast<float>();

        const Eigen::Vector3f& minima = range.first;
        const Eigen::Vector3f& maxima = range.second;
        const Eigen::Vector3f interval = maxima - minima;
        constexpr float scale = std::is_same<T, uint8_t>::value ? (1.0f / 255.0f) : (1.0f / 65535.0f);

        Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> out(resolution, resolution);
        auto convert = [&](int start, int end) {
            for (int i = start; i < end; ++i){
                for (int j = 0; j < resolution; ++j) {
                    const Eigen::Vector3f px = Eigen::Vector3f(imageFP(i, j * 3),
                                                               imageFP(i, j * 3 + 1),
                                                               imageFP(i, j * 3 + 2));
                    out(i, j) = minima + scale * px.cwiseProduct(interval);
                }
            }
        };
        if (task_thread_pool) task_thread_pool->AddTaskRangeAndWait(resolution, convert);
        else convert(0, resolution);   
        return out;
    }

    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImage(const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>& image, const int new_size, const Eigen::Vector3f& offset = Eigen::Vector3f::Zero(), TaskThreadPool* threadPool = nullptr);
    
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ModelDataToImageType(const Model_Data& model_data,
                                                                                                         const std::pair<Eigen::Vector3f, Eigen::Vector3f>& range,
                                                                                                         TITAN_NAMESPACE::TaskThreadPool* task_thread_pool);
    
    bool LoadTSParams(const std::string& ts_params_path, TextureModelParams& model_params, std::vector<CharacterParams>& character_params);
    
    float Interpolate(const std::array<float, 2>& extremes, const float x);
    
    float InterpolatePiecewise(const std::array<float, 2>& extremes, const float x, const float v_ui_chr_0, const float param_baseline);
    
    std::string zfill(const std::string& input, size_t width);

    bool AreFloatsEqual(const float a, const float b);

    bool HasFourTextureColourRangesFormat(const JsonElement& texture_ranges_json);

    bool HasTextureModelParamsFormat(const JsonElement& model_json);

    bool HasCharacterParamsFormat(const JsonElement& chr_json);
}

//
// load npy model data and allocate a Model_Data instance for it
//
// If the data is loaded as cached the result might not be available immediately and it is the caller's responsibility to
// check (and potentially wait) for the data to be available by checking ts::Model_Data::is_available()
//
// ConcurrencyPolicy is a policy that dictates how locking and error signals are handled and is a choice left to the implementor
// (of IModel_Data_Provider for example).
//
// ERROR HANDLING
//  * Any file open or load issue is considered "critical" but what this means is delegated to the ConcurrencyPolicy implementation
//  * No exceptions are thrown, but an empty Model_Data instance is returned on error
//
// key is a hash value or Model_Data::nullkey if the data should NOT be cached
//
template<typename T = float, typename ConcurrencyPolicy = detail::load_npy_model_data_no_concurrency_policy>
inline CachedModelData load_npy_model_data(const std::string& npy_path, CachedModelData::key_t key = CachedModelData::nullkey)
{
    if (!std::filesystem::exists(npy_path)) {
        CARBON_CRITICAL("ERROR: Model file doesn't exist: " + npy_path);
    }

    FILE* pFile = nullptr;

#ifdef _MSC_VER
    if (fopen_s(&pFile, npy_path.c_str(), "rb") == 0 && pFile != nullptr) {
#else
    pFile = fopen(npy_path.c_str(), "rb");
    if (pFile) {
#endif

        TITAN_NAMESPACE::npy::NPYHeader header;
        TITAN_NAMESPACE::npy::LoadNPYRawHeader(pFile, header);

        const std::vector<int>& shape = header.m_shape;

        if (shape.size() > 2) {
            ConcurrencyPolicy::raise_critical_error();
            fclose(pFile);
            CARBON_CRITICAL("Only 1D and 2D arrays are supported.");
        }

        int n_rows = static_cast<int>(shape[0]);
        int n_cols = 1;
        if (shape.size() == 2) {
            n_cols = static_cast<int>(shape[1]);
        }
        if (sizeof(T) != header.DataTypeSize())
        {
            ConcurrencyPolicy::raise_critical_error();
            fclose(pFile);
            std::cout << "sizeof(T) = " << sizeof(T) << "  header.DataTypeSize() = " << header.DataTypeSize() << std::endl;
            CARBON_CRITICAL("Mismatching type.");
        }

        static_assert(std::is_same<T, float>::value || std::is_same<T, uint16_t>::value || std::is_same<T, uint8_t>::value, "Matrix element type not implemented!");

        ts::CachedModelData result;
        if (key != CachedModelData::nullkey)
        {
            auto lock = ConcurrencyPolicy::lock_unique();
            (void)lock;
            // check that this hasn't been cached by another thread in the meantime
            result = CachedModelData::get_cached(key);
            if (!result) {
                result = CachedModelData::allocate_cached(key, n_cols, n_rows, sizeof(T), 1);
            }
            else {
                // another thread is loading the data so we'll just close the file and return
                // NOTE: the caller must wait for the data to become available before use by checking Model_Data.is_available()
                fclose(pFile);
                return result;
            }
        }
        else {
            auto lock = ConcurrencyPolicy::lock_unique();
            (void)lock;
            result = CachedModelData::allocate(n_cols, n_rows, sizeof(T), 1);
        }

        if(fread(result.data<void*>(), 1, result.allocation_size(), pFile) != result.allocation_size()) {
            // wether this is a eof or an actual error matters less at this point, we need to signal that the read has failed regardless
            ConcurrencyPolicy::raise_critical_error();
            fclose(pFile);
            CARBON_CRITICAL("Failure to read NPY data");
        }

        // this ignores nullkey entries
        result.make_available();

        fclose(pFile);
        return result;

// This helps with code parsing
#ifdef _MSC_VER
    }
#else
    }
#endif

    // file not found or can't be opened; treat as critical
    ConcurrencyPolicy::raise_critical_error();
    CARBON_CRITICAL("Failure to read NPY data");
}

template<class T, int vec_size>
std::string vector_to_str(const Eigen::Vector<T, vec_size>& vec, const bool reverse = false)
{
    std::string result;

    if (reverse) {
        result += std::to_string(vec(vec_size - 1));
        for (int i = vec_size - 2; i >= 0; --i)
            result += ", " + std::to_string(vec[i]);
    } else {
        result += std::to_string(vec(0));
        for (int i = 1; i < vec_size; ++i)
            result += ", " + std::to_string(vec[i]);
    }
    return result;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
