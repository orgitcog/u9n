// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEShaderFillBufferCS.h"

namespace UE::NNERuntimeIREEShader::Internal
{

void FFillBufferCS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(InParameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("THREAD_GROUP_SIZE"), FFillBufferConstants::THREAD_GROUP_SIZE);
}

bool FFillBufferCS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return true;
}



IMPLEMENT_GLOBAL_SHADER(FFillBufferCS, "/Plugin/NNERuntimeIREEShader/NNERuntimeIREEShaderFillBuffer.usf", "Main", SF_Compute);

} // UE::NNERuntimeIREEShader::Internal