// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Defs.h>
#include <ts/ts_types.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

// Thin wrapper for accessing matrix/vector representations of the texture synthesis model data
// Layout is [rows, cols, channels] of word_size bytes
// Model_Data objects *DO NOT* assume ownership of the underlying buffer
struct Model_Data
{   
    Model_Data()
            : _rows(0),
            _cols(0),
            _word_size(0),
            _channels(0),
            _data(nullptr)
        {}
    
    Model_Data(int32_t rows, int32_t cols, int32_t channels, int32_t word_size, uint8_t* data)
            : _rows(rows)
            , _cols(cols)
            , _word_size(word_size)
            , _channels(channels)
            , _data(data)
        {}

    Model_Data(const Model_Data& rhs) = default;

    constexpr operator bool() const 
    {
        return _data != nullptr;
    }

    constexpr int32_t cols() const 
    {
        return _cols;
    }

    constexpr int32_t rows() const 
    {
        return _rows;
    }

    constexpr int32_t word_size() const 
    {
        return _word_size;
    }

    constexpr int32_t channels() const 
    {
        return _channels;
    }

    template<typename T>
    constexpr const T* data() const 
    {
        return reinterpret_cast<const T*>(_data);
    }

    bool reshape(int32_t rows, int32_t cols, int32_t channels);

    // Returns a Model_Data object with the data of single row
    [[nodiscard]] Model_Data row_view(int32_t row) const;

    // Returns a Model_Data object with the data of single row but reshaped as a 3 channel square image (image_resolution x image_resolution x 3)
    [[nodiscard]] Model_Data row_view_as_image(int32_t row, int32_t image_resolution) const;

protected:
    int32_t _rows;
    int32_t _cols;
    int32_t _word_size;
    int32_t _channels;
    uint8_t* _data;
};

/*
 * Storage abstraction for TS model data
 * Implement this to support different storage formats and access types
*/
struct IModel_Data_Provider 
{
    // Load a texture image with the required parameters.
    // The texture images represent different kinds of data, depending on the parameters and
    // on the texture type. This is as follows:
    // - For albedo maps (since we need to syntesize these):
    //       LF textures:
    //           map_id = 0 (neutral)  -> LF neutral complement
    //           map_id > 0 (animated) -> LF animated delta
    //       HF textures:
    //           map_id = 0 (neutral)  -> neutral HF
    //           map_id > 0 (animated) -> HF animated delta
    // - For normal and cavity maps (since we need to directly select those):
    //       LF textures not present
    //       HF textures:
    //           map_id = 0 (neutral)  -> full neutral texture
    //           map_id > 0 (animated) -> full animated delta (normals only)
    [[nodiscard]] virtual Model_Data load(TextureType texture_type, Frequency frequency, int map_id, int HFIndex) = 0;
    
    // Load data types dependent on texture type. Our only case: LF models
    [[nodiscard]] virtual Model_Data load(TextureType texture_type) = 0;
    
    // Load data types which are independent of other parameters (uv mask, pca matrices, v_to_vui_map)
    [[nodiscard]] virtual Model_Data load(Data_Type type) = 0;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
