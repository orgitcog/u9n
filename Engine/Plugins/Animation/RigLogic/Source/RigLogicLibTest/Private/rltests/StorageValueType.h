// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <cstdint>

#ifdef RL_BUILD_WITH_HALF_FLOATS
    using StorageValueType = std::uint16_t;
#else
    using StorageValueType = float;
#endif
