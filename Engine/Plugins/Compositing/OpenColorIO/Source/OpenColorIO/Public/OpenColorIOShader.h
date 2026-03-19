// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenColorIOShader.h: Shader base classes
=============================================================================*/

#pragma once

#include "Containers/ContainersFwd.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "GlobalShader.h"
#endif
#include "OpenColorIOShaderType.h"
#include "OpenColorIOShared.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"

class FTextureResource;
class FRHITexture;

BEGIN_SHADER_PARAMETER_STRUCT(FOpenColorIOPixelShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER(float, Gamma)
	SHADER_PARAMETER(uint32, TransformAlpha)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

namespace OpenColorIOShader
{
	UE_DEPRECATED(5.6, "This limit is now deprecated.")
	static constexpr uint32 MaximumTextureSlots = 8;
}

class FOpenColorIOPixelShader : public FGlobalShader
{
	DECLARE_EXPORTED_SHADER_TYPE(FOpenColorIOPixelShader, OpenColorIO, OPENCOLORIO_API);

public:
	FOpenColorIOPixelShader() = default;
	FOpenColorIOPixelShader(
		const FOpenColorIOShaderType::CompiledShaderInitializerType& Initializer
	);

	MS_ALIGN(SHADER_PARAMETER_STRUCT_ALIGNMENT) struct FParameters
	{
	};
};

BEGIN_SHADER_PARAMETER_STRUCT(FOpenColorIOInvalidShaderParameters, )
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputTextureSampler)
	SHADER_PARAMETER_TEXTURE(Texture2D, MiniFontTexture)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class OPENCOLORIO_API FOpenColorIOInvalidPixelShader : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FOpenColorIOInvalidPixelShader);
	SHADER_USE_PARAMETER_STRUCT(FOpenColorIOInvalidPixelShader, FGlobalShader);

	using FParameters = FOpenColorIOInvalidShaderParameters;
};

UE_DEPRECATED(5.6, "This shader parameter binding function is now deprecated.")
OPENCOLORIO_API bool OpenColorIOBindTextureResources(FOpenColorIOPixelShaderParameters* Parameters, const TSortedMap<int32, FTextureResource*>& InTextureResources);

FRHITexture* OpenColorIOGetMiniFontTexture();
