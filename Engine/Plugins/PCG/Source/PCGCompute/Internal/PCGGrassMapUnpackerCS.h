// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

/** Compute shader for unpacking grass map textures rendered by FLandscapeGrassWeightExporter.
 * Note: This class is subject to change without deprecation.
 */
class FPCGGrassMapUnpackerCS : public FGlobalShader
{
public:
	static constexpr uint32 ThreadGroupDim = 8;
	static constexpr uint32 MaxNumLandscapeComponents = 64;

public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FPCGGrassMapUnpackerCS, PCGCOMPUTE_API);
	SHADER_USE_PARAMETER_STRUCT(FPCGGrassMapUnpackerCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, PCGCOMPUTE_API)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float4>, InPackedGrassMaps)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, OutUnpackedGrassMaps)
		SHADER_PARAMETER_ARRAY(FIntVector4, InLinearTileIndexToComponentIndex, [MaxNumLandscapeComponents])
		SHADER_PARAMETER(uint32, InNumTilesX)
		SHADER_PARAMETER(uint32, InLandscapeComponentResolution)
		SHADER_PARAMETER(uint32, InNumGrassMapPasses)
		SHADER_PARAMETER(FUintVector2, InOutputResolution)
	END_SHADER_PARAMETER_STRUCT()

public:
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
