// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"

struct FMeshNaniteSettings;

namespace Nanite
{
	MESHBUILDERCOMMON_API void CorrectFallbackSettings(FMeshNaniteSettings& NaniteSettings, int32 NumTris, bool bIsAssembly, bool bIsRayTracing);
}
