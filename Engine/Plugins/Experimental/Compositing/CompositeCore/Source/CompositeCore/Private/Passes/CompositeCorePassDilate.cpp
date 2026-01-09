// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositeCorePassDilate.h"

#include "RHIFeatureLevel.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "PixelShaderUtils.h"

DECLARE_GPU_STAT_NAMED(FCompositeCoreDilate, TEXT("CompositeCore.Dilate"));

class FCompositeCoreDilateShader : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompositeCoreDilateShader);
	SHADER_USE_PARAMETER_STRUCT(FCompositeCoreDilateShader, FGlobalShader);

	class FDilationSize : SHADER_PERMUTATION_INT("DILATION_SIZE", 3);
	using FPermutationDomain = TShaderPermutationDomain<FDilationSize>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWOutputTexture)
		SHADER_PARAMETER(FIntPoint, Dimensions)
		SHADER_PARAMETER(uint32, bOpacifyOutput)
	END_SHADER_PARAMETER_STRUCT()

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("TILE_SIZE"), ThreadGroupSize);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}

	static const uint32 ThreadGroupSize = 16;
};
IMPLEMENT_GLOBAL_SHADER(FCompositeCoreDilateShader, "/Plugin/CompositeCore/Private/CompositeCoreDilate.usf", "MainCS", SF_Compute);

namespace UE
{
	namespace CompositeCore
	{
		namespace Private
		{
			void AddDilatePass(FRDGBuilder& GraphBuilder, FRDGTextureRef Input, FRDGTextureRef Output, ERHIFeatureLevel::Type FeatureLevel, const FDilateInputs& PassInputs)
			{
				RDG_EVENT_SCOPE_STAT(GraphBuilder, FCompositeCoreDilate, "CompositeCore.Dilate");
				RDG_GPU_STAT_SCOPE(GraphBuilder, FCompositeCoreDilate);

				FGlobalShaderMap* GlobalShaderMap = GetGlobalShaderMap(FeatureLevel);
				const FIntPoint TextureSize = Input->Desc.Extent;

				FCompositeCoreDilateShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompositeCoreDilateShader::FParameters>();
				PassParameters->InputTexture = Input;
				PassParameters->RWOutputTexture = GraphBuilder.CreateUAV(Output);
				PassParameters->Dimensions = TextureSize;
				PassParameters->bOpacifyOutput = PassInputs.bOpacifyOutput;

				FCompositeCoreDilateShader::FPermutationDomain PermutationVector;
				PermutationVector.Set<FCompositeCoreDilateShader::FDilationSize>(FMath::Clamp(PassInputs.DilationSize, 0, 2));

				TShaderMapRef<FCompositeCoreDilateShader> ComputeShader(GlobalShaderMap, PermutationVector);
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("CompositeCore.Dilate (%dx%d)", TextureSize.X, TextureSize.Y),
					GSupportsEfficientAsyncCompute ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute,
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount(TextureSize, FCompositeCoreDilateShader::ThreadGroupSize)
				);
			}

			FRDGTextureDesc GetPostProcessingDesc(FRDGTextureDesc InDesc, EPixelFormat InOutputFormat)
			{
				FRDGTextureDesc OutputDesc = InDesc;
				OutputDesc.Dimension = ETextureDimension::Texture2D;
				OutputDesc.ArraySize = 1;

				OutputDesc.Reset();
				if (InOutputFormat != PF_Unknown)
				{
					OutputDesc.Format = InOutputFormat;
				}
				OutputDesc.ClearValue = FClearValueBinding(FLinearColor::Black);
				OutputDesc.Flags &= (~ETextureCreateFlags::FastVRAM);

				return OutputDesc;
			}
		}
	}
}

