// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * Definition and helpers for debug view modes
 */

#pragma once

#include "CoreMinimal.h"

#define WITH_DEBUG_VIEW_MODES (WITH_EDITOR || !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

struct FSlowTask;
class UMaterialInterface;
enum EShaderPlatform : uint16;
namespace ERHIFeatureLevel { enum Type : int; }
namespace EMaterialQualityLevel { enum Type : uint8; }

/** 
 * Enumeration for different Quad Overdraw visualization mode.
 */
enum EDebugViewShaderMode
{
	DVSM_None,						// No debug view.
	DVSM_ShaderComplexity,			// Default shader complexity viewmode
	DVSM_ShaderComplexityContainedQuadOverhead,	// Show shader complexity with quad overdraw scaling the PS instruction count.
	DVSM_ShaderComplexityBleedingQuadOverhead,	// Show shader complexity with quad overdraw bleeding the PS instruction count over the quad.
	DVSM_QuadComplexity,			// Show quad overdraw only.
	DVSM_PrimitiveDistanceAccuracy,	// Visualize the accuracy of the primitive distance computed for texture streaming.
	DVSM_MeshUVDensityAccuracy,		// Visualize the accuracy of the mesh UV densities computed for texture streaming.
	DVSM_MaterialTextureScaleAccuracy, // Visualize the accuracy of the material texture scales used for texture streaming.
	DVSM_OutputMaterialTextureScales,  // Outputs the material texture scales.
	DVSM_RequiredTextureResolution, // Visualize the accuracy of the streamed texture resolution.
	DVSM_LODColoration,				// Visualize primitive LOD .
	DVSM_VisualizeGPUSkinCache,		// Visualize various properties of Skin Cache.
	DVSM_LWCComplexity,				// Visualize usage of LWC functions in materials.
	DVSM_MAX
};

ENGINE_API const TCHAR* DebugViewShaderModeToString(EDebugViewShaderMode InShaderMode);

#if WITH_DEBUG_VIEW_MODES
/** Returns true if debug view shaders are only ODSC compiled (in editor or cooked builds). Returns false if they are can also be compiled for editor only shader maps as well as ODSC. */
ENGINE_API bool IsDebugViewShaderModeODSCOnly();
/** Returns true if a specific debug view supported for a platform. Can be used as input for shader cooking. */
ENGINE_API bool SupportDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform);
/** Returns true if a specific debug view can be displayed at runtime. */
ENGINE_API bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel);
#else
FORCEINLINE bool IsDebugViewShaderModeODSCOnly() { return false; }
FORCEINLINE bool SupportDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform) { return false; }
FORCEINLINE bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel) { return false; }
#endif

ENGINE_API int32 GetNumActorsInWorld(UWorld* InWorld);
ENGINE_API bool GetUsedMaterialsInWorld(UWorld* InWorld, OUT TSet<UMaterialInterface*>& OutMaterials, FSlowTask* Task);
ENGINE_API bool CompileDebugViewModeShaders(EDebugViewShaderMode Mode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<UMaterialInterface*>& Materials, FSlowTask* ProgressTask);
ENGINE_API bool WaitForShaderCompilation(const FText& Message, FSlowTask* ProgressTask);
