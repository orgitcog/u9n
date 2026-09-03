// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteHelper.h"
#include "Engine/EngineTypes.h"
#include "HAL/IConsoleManager.h"

namespace Nanite
{

void CorrectFallbackSettings(FMeshNaniteSettings& NaniteSettings, int32 NumTris, bool bIsAssembly, bool bIsRayTracing)
{
	static const auto CVarFallbackThreshold = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.Nanite.Builder.FallbackTriangleThreshold"));
	static const auto CVarFallbackTargetAutoRelativeError = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Nanite.Builder.FallbackTargetAutoRelativeError"));
	static const auto CVarRayTracingProxyFallbackTargetAutoRelativeError = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.Nanite.Builder.RayTracingProxy.FallbackTargetAutoRelativeError"));

	const float AutoRelativeError = bIsRayTracing ? CVarRayTracingProxyFallbackTargetAutoRelativeError->GetValueOnAnyThread() : CVarFallbackTargetAutoRelativeError->GetValueOnAnyThread();

	switch (NaniteSettings.FallbackTarget)
	{
		case ENaniteFallbackTarget::Auto:
			NaniteSettings.FallbackPercentTriangles = 1.0f;
			NaniteSettings.FallbackRelativeError = (!bIsAssembly && NumTris <= CVarFallbackThreshold->GetValueOnAnyThread()) ? 0.0f : AutoRelativeError;
			break;
		case ENaniteFallbackTarget::PercentTriangles:
			NaniteSettings.FallbackRelativeError = 0.0f;
			break;
		case ENaniteFallbackTarget::RelativeError:
			NaniteSettings.FallbackPercentTriangles = 1.0f;
			break;
	}
}

}
