// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDIMediaShaders.h"

#include "RHIStaticStates.h"

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNDIMediaUYVYAConvertUB, )
	SHADER_PARAMETER(uint32, InputWidth)
	SHADER_PARAMETER(uint32, InputHeight)
	SHADER_PARAMETER(uint32, OutputWidth)
	SHADER_PARAMETER(uint32, OutputHeight)
	SHADER_PARAMETER(FMatrix44f, ColorTransform)
	SHADER_PARAMETER(FMatrix44f, CSTransform)
	SHADER_PARAMETER(uint32, EOTF)
	SHADER_PARAMETER(uint32, ToneMapMethod)
	SHADER_PARAMETER_TEXTURE(Texture2D, YUVTexture)
	SHADER_PARAMETER_TEXTURE(Texture2D, AlphaTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, SamplerB)
	SHADER_PARAMETER_SAMPLER(SamplerState, SamplerT)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FNDIMediaUYVYAConvertUB, "NDIMediaUYVYAConvertUB");

IMPLEMENT_GLOBAL_SHADER(FNDIMediaShaderUYVAtoBGRAPS, "/Plugin/NDIMedia/Private/NDIMediaShaders.usf", "NDIMediaUYVYAConvertPS", SF_Pixel);

void FNDIMediaShaderUYVAtoBGRAPS::SetParameters(FRHIBatchedShaderParameters& InBatchedParameters, const FParameters& InParameters)
{
	FNDIMediaUYVYAConvertUB UB;
	{
		UB.InputWidth = InParameters.YUVTexture->GetSizeX();
		UB.InputHeight = InParameters.YUVTexture->GetSizeY();
		UB.OutputWidth = InParameters.OutputSize.X;
		UB.OutputHeight = InParameters.OutputSize.Y;
		UB.ColorTransform = InParameters.ColorTransform;
		UB.CSTransform = InParameters.CSTransform;
		UB.EOTF = static_cast<uint32>(InParameters.Encoding);
		UB.ToneMapMethod = static_cast<uint32>(InParameters.ToneMapMethod);
		UB.YUVTexture = InParameters.YUVTexture;
		UB.AlphaTexture = InParameters.AlphaTexture;
		UB.SamplerB = TStaticSamplerState<SF_Bilinear>::GetRHI();
		UB.SamplerT = TStaticSamplerState<SF_Trilinear>::GetRHI();
	}

	TUniformBufferRef<FNDIMediaUYVYAConvertUB> Data = TUniformBufferRef<FNDIMediaUYVYAConvertUB>::CreateUniformBufferImmediate(UB, UniformBuffer_SingleFrame);
	SetUniformBufferParameter(InBatchedParameters, GetUniformBufferParameter<FNDIMediaUYVYAConvertUB>(), Data);
}