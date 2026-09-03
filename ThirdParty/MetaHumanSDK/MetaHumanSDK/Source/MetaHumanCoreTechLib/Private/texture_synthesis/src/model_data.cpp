// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/model_data_provider_interface.h>

#include <carbon/Common.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

TITAN_NAMESPACE::ts::Model_Data Model_Data::row_view(int32_t row) const
{
    CARBON_ASSERT(row < _rows, "Row index out of range");
    return Model_Data(
        1,
        _cols,
        _channels,
        _word_size,
        _data + (row * _cols * _channels * _word_size));
}

TITAN_NAMESPACE::ts::Model_Data Model_Data::row_view_as_image(int32_t row, int32_t image_resolution) const
{
    CARBON_ASSERT(row < _rows, "Row index out of range");
    return Model_Data(
        image_resolution,
        image_resolution,
        3,
        _word_size,
        _data + (row * _cols * _channels * _word_size));
}

bool Model_Data::reshape(int32_t rows, int32_t cols, int32_t channels)
{
    if (_rows * _cols * _channels != rows * cols * channels)
    {
        return false;
    }
    _rows = rows;
    _cols = cols;
    _channels = channels;
    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
