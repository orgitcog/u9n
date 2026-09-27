// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Shader.h"
#include "ShaderParameters.h"
#include "NNERuntimeIREEShaderType.h"
#include "NNERuntimeIREEShaderShared.h"

class NNERUNTIMEIREESHADER_API FNNERuntimeIREEShader : public FShader
{
	DECLARE_SHADER_TYPE(FNNERuntimeIREEShader, NNERuntimeIREE);

public:
	FNNERuntimeIREEShader() = default;
	FNNERuntimeIREEShader(const FNNERuntimeIREEShaderType::CompiledShaderInitializerType & Initializer);

	MS_ALIGN(SHADER_PARAMETER_STRUCT_ALIGNMENT) struct FParameters
	{
	};
};