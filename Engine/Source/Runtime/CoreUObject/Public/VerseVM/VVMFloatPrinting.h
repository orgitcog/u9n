// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "HAL/Platform.h"
#include "VerseVM/VVMFloat.h"

namespace Verse
{
enum class EFloatStringFormat
{
	Legacy, // Reproduces the legacy behavior of Verse's ToString function, which is equivalent to printf("%f", Float).
	ShortestOfFixedOrScientific
};

COREUOBJECT_API void AppendDecimalToString(FUtf8StringBuilderBase& Builder, VFloat Float, EFloatStringFormat Format = EFloatStringFormat::ShortestOfFixedOrScientific);
} // namespace Verse