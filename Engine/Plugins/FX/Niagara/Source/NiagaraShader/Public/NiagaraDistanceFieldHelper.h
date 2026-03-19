// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


class FRDGBuilder;
class FGlobalDistanceFieldParameterData;
class FGlobalDistanceFieldParameters2;

namespace FNiagaraDistanceFieldHelper
{
	NIAGARASHADER_API void SetGlobalDistanceFieldParameters(const FGlobalDistanceFieldParameterData* OptionalParameterData, FGlobalDistanceFieldParameters2& ShaderParameters);
}
