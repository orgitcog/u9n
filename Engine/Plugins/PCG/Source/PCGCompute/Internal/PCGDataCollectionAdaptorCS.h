// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GlobalShader.h"
#include "GPUSceneWriter.h"
#include "ShaderParameterStruct.h"

/** [EXPERIMENTAL] Compute shader for updating a data collection buffer with attribute ID remaps and/or element counts.
 * Note: This class is subject to change without deprecation.
 */
class FPCGDataCollectionAdaptorCS : public FGlobalShader
{
public:
	DECLARE_EXPORTED_GLOBAL_SHADER(FPCGDataCollectionAdaptorCS, PCGCOMPUTE_API);
	SHADER_USE_PARAMETER_STRUCT(FPCGDataCollectionAdaptorCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, PCGCOMPUTE_API)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FInt32Vector2>, InSourceToTargetAttributeId)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint32>, InDataElementCounts)
		SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, InDataCollection)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, OutDataCollection)
		SHADER_PARAMETER(uint32, InNumData)
		SHADER_PARAMETER(uint32, InNumRemappedAttributes)
	END_SHADER_PARAMETER_STRUCT()

public:
	static constexpr uint32 NumThreadsPerGroup = 64;

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};
