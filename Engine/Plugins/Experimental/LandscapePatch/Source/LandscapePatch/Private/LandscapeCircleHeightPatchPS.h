// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

/**
 * Shader that applies a circle height patch to a landscape heightmap.
 */
class FLandscapeCircleHeightPatchPSBase : public FGlobalShader
{
public:
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InSourceTexture) // Our input texture
		// Offset of the source heightmap relative to the 0,0 location in the destination heightmap, because
		// the source is likely to be a copied region from some inner part of the destination. This is basically
		// a SourceHeightmapToDestinationHeightmap coordinate transformation, except that it is always a simple 
		// integer translation.
		SHADER_PARAMETER(FIntPoint, InSourceTextureOffset)
		SHADER_PARAMETER(FVector3f, InCenter)
		SHADER_PARAMETER(float, InRadius)
		SHADER_PARAMETER(float, InFalloff)
		RENDER_TARGET_BINDING_SLOTS() // Holds our output
	END_SHADER_PARAMETER_STRUCT()

	FLandscapeCircleHeightPatchPSBase() {}
	FLandscapeCircleHeightPatchPSBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
};

template <bool bPerformBlending = false>
class FLandscapeCircleHeightPatchPS : public FLandscapeCircleHeightPatchPSBase
{
	DECLARE_GLOBAL_SHADER(FLandscapeCircleHeightPatchPS);
	SHADER_USE_PARAMETER_STRUCT(FLandscapeCircleHeightPatchPS, FLandscapeCircleHeightPatchPSBase);

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);

	static void AddToRenderGraph(FRDGEventName&& InRDGEventName, FRDGBuilder& GraphBuilder, FLandscapeCircleHeightPatchPSBase::FParameters* InParameters, const FIntRect& DestinationBounds);
};

/**
 * Shader that applies a circle patch to a landscape visibility layer.
 */
template <bool bPerformBlending = false>
class FLandscapeCircleVisibilityPatchPS : public FLandscapeCircleHeightPatchPSBase
{
	DECLARE_GLOBAL_SHADER(FLandscapeCircleVisibilityPatchPS);
	SHADER_USE_PARAMETER_STRUCT(FLandscapeCircleVisibilityPatchPS, FLandscapeCircleHeightPatchPSBase);

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	static void AddToRenderGraph(FRDGEventName&& InRDGEventName, FRDGBuilder& GraphBuilder, FLandscapeCircleHeightPatchPSBase::FParameters* InParameters, const FIntRect& DestinationBounds);
};
