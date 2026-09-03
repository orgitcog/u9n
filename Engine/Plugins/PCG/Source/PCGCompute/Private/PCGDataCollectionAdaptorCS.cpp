// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGDataCollectionAdaptorCS.h"

namespace PCGComputeConstants
{
	// @todo_pcg: These must be kept in sync with the constants in PCGComputeCommon.h, but currently that is an internal file that we cannot reference from this module.
	constexpr int DATA_COLLECTION_HEADER_SIZE_BYTES = 4;
	constexpr int DATA_HEADER_PREAMBLE_SIZE_BYTES = 12;
	constexpr int MAX_NUM_ATTRS = 128;
	constexpr int ATTRIBUTE_HEADER_SIZE_BYTES = 8;
	constexpr int DATA_HEADER_SIZE_BYTES = DATA_HEADER_PREAMBLE_SIZE_BYTES + MAX_NUM_ATTRS * ATTRIBUTE_HEADER_SIZE_BYTES;
}

void FPCGDataCollectionAdaptorCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

	OutEnvironment.SetDefine(TEXT("NUM_THREADS_PER_GROUP"), NumThreadsPerGroup);
	OutEnvironment.SetDefine(TEXT("PCG_DATA_COLLECTION_HEADER_SIZE_BYTES"), PCGComputeConstants::DATA_COLLECTION_HEADER_SIZE_BYTES);
	OutEnvironment.SetDefine(TEXT("PCG_DATA_HEADER_SIZE_BYTES"), PCGComputeConstants::DATA_HEADER_SIZE_BYTES);
	OutEnvironment.SetDefine(TEXT("PCG_ATTRIBUTE_HEADER_SIZE_BYTES"), PCGComputeConstants::ATTRIBUTE_HEADER_SIZE_BYTES);
}

IMPLEMENT_GLOBAL_SHADER(FPCGDataCollectionAdaptorCS, "/PCGComputeShaders/PCGDataCollectionAdaptor.usf", "MainCS", SF_Compute);
