// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeIREEShader.h"

#include "NNERuntimeIREEShaderType.h"

IMPLEMENT_SHADER_TYPE(, FNNERuntimeIREEShader, TEXT("/Plugin/NNERuntimeIREEShader/NNERuntimeIREEShader.usf"), TEXT("__"), SF_Compute)

FNNERuntimeIREEShader::FNNERuntimeIREEShader(const FNNERuntimeIREEShaderType::CompiledShaderInitializerType& Initializer)
	: FShader(Initializer)
{
	const FShaderParametersMetadata& ShaderParametersMetadata = static_cast<const FNNERuntimeIREEShaderType::FParameters*>(Initializer.Parameters)->ShaderParamMetadata;

	Bindings.BindForLegacyShaderParameters(this, Initializer.PermutationId, Initializer.ParameterMap, ShaderParametersMetadata, true);
}