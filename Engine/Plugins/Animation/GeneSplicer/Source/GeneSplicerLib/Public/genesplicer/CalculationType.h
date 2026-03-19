// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace gs4 {

enum class CalculationType {
    Scalar,  ///< scalar implementation
    SSE,  ///< vectorized implementation
    AVX  ///< vectorized implementation
};

}  // namespace gs4
