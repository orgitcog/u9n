// Copyright Epic Games, Inc. All Rights Reserved.

#include "NDIMediaTextureSampleConverter.h"

#include "NDIMediaShaders.h"
#include "NDIMediaTextureSample.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"

namespace UE::NDIMediaTextureSampleConverter
{
	// Reference FMediaTextureResource::GetColorSpaceConversionMatrixForSample.
	FMatrix44f GetColorSpaceConversionMatrixForSample(const TSharedPtr<FNDIMediaTextureSample>& InSample)
	{
		FMatrix44f OutColorSpaceMatrix;
		
		const UE::Color::FColorSpace& Working = UE::Color::FColorSpace::GetWorking();

		if (InSample->GetMediaTextureSampleColorConverter())
		{
			OutColorSpaceMatrix = FMatrix44f::Identity;
		}
		else
		{
			OutColorSpaceMatrix = UE::Color::Transpose<float>(UE::Color::FColorSpaceTransform(InSample->GetSourceColorSpace(), Working));
		}
	
		const float NormalizationFactor = InSample->GetHDRNitsNormalizationFactor();
		if (NormalizationFactor != 1.0f)
		{
			OutColorSpaceMatrix = OutColorSpaceMatrix.ApplyScale(NormalizationFactor);
		}

		return OutColorSpaceMatrix;
	}
}

void FNDIMediaTextureSampleConverter::Setup(const TSharedPtr<FNDIMediaTextureSample>& InSample)
{
	SampleWeak = InSample;
}

uint32 FNDIMediaTextureSampleConverter::GetConverterInfoFlags() const
{
	return ConverterInfoFlags_Default;
}

// Reference: FMediaTextureResource::ConvertTextureToOutput
bool FNDIMediaTextureSampleConverter::Convert(FRHICommandListImmediate& InRHICmdList, FTextureRHIRef& InDstTexture, const FConversionHints& InHints)
{
	TSharedPtr<FNDIMediaTextureSample> Sample = SampleWeak.Pin();
	if (!Sample || !Sample->GetBuffer())
	{
		return false;
	}

	if (!UpdateInputTextures(InRHICmdList, Sample))
	{
		return false;
	}
	
	// Initialize the frame size parameter
	FIntPoint FieldSize = Sample->GetDim();
	FIntPoint FrameSize = Sample->bIsProgressive ? FieldSize : FIntPoint(FieldSize.X, FieldSize.Y*2);
	
	// Draw full size quad into render target
	// This needs to happen before we begin to setup the draw call, because on DX11, this might flush the command list more or less randomly
	const float FieldUVOffset = Sample->FieldIndex ? 0.5f/FrameSize.Y : 0.f;
	FBufferRHIRef VertexBuffer = CreateTempMediaVertexBuffer(InRHICmdList, 0.f, 1.f, 0.f-FieldUVOffset, 1.f-FieldUVOffset);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;

	InRHICmdList.Transition(FRHITransitionInfo(InDstTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
	ON_SCOPE_EXIT {InRHICmdList.Transition(FRHITransitionInfo(InDstTexture, ERHIAccess::RTV, ERHIAccess::SRVGraphics));};
	
	FRHIRenderPassInfo RPInfo(InDstTexture.GetReference(), ERenderTargetActions::DontLoad_Store);
	{
		InRHICmdList.BeginRenderPass(RPInfo, TEXT("ConvertMedia(NDI)"));
		ON_SCOPE_EXIT { InRHICmdList.EndRenderPass(); };
		
		InRHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		
		FIntPoint OutputSize(InDstTexture->GetSizeXYZ().X, InDstTexture->GetSizeXYZ().Y);
		InRHICmdList.SetViewport(0, 0, 0.0f, OutputSize.X, OutputSize.Y, 1.0f);
	
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.BlendState = TStaticBlendStateWriteMask<CW_RGBA, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE, CW_NONE>::GetRHI();
		GraphicsPSOInit.PrimitiveType = PT_TriangleStrip;

		// Configure media shaders
		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FMediaShadersVS> VertexShader(ShaderMap);
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GMediaVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();

		// Note: this convert supports only one shader, all others are already supported by media shaders.
		{	// --- Setup UYVA to BGRA Pixel shader ---
			TShaderMapRef<FNDIMediaShaderUYVAtoBGRAPS> ConvertShader(ShaderMap);
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = ConvertShader.GetPixelShader();

			// Ensure the pipeline state is set to the one we've configured
			SetGraphicsPipelineState(InRHICmdList, GraphicsPSOInit, 0);
	
			// set the texture parameter of the conversion shader
			FNDIMediaShaderUYVAtoBGRAPS::FParameters Params(
				SourceYUVTexture,
				SourceAlphaTexture,
				OutputSize,
				Sample->GetSampleToRGBMatrix(),
				Sample->GetEncodingType(),
				UE::NDIMediaTextureSampleConverter::GetColorSpaceConversionMatrixForSample(Sample),
				Sample->GetToneMapMethod());

			SetShaderParametersLegacyPS(InRHICmdList, ConvertShader, Params);
		}
	
		InRHICmdList.SetStreamSource(0, VertexBuffer, 0);

		// Set Viewport to RT size
		InRHICmdList.SetViewport(0, 0, 0.0f, OutputSize.X, OutputSize.Y, 1.0f);

		InRHICmdList.DrawPrimitive(0, 2, 1);
	}
	return true;
}

bool FNDIMediaTextureSampleConverter::UpdateInputTextures(FRHICommandList& InRHICmdList, const TSharedPtr<FNDIMediaTextureSample>& InSample)
{
	const void* Buffer = InSample->GetBuffer();
	
	if (!Buffer)
	{
		return false;
	}
	
	FIntPoint FieldSize = InSample->GetDim();
	FIntPoint FrameSize = InSample->bIsProgressive ? FieldSize : FIntPoint(FieldSize.X, FieldSize.Y*2);

	if (!SourceYUVTexture.IsValid() || FrameSize != PreviousFrameSize)
	{
		// The source YUV texture will be given UYVY data, so make it half-width
		const FRHITextureCreateDesc CreateDesc = FRHITextureCreateDesc::Create2D(TEXT("NDIMediaReceiverInterlacedAlphaSourceTexture"))
			.SetExtent(FieldSize.X / 2, FieldSize.Y)
			.SetFormat(PF_B8G8R8A8)
			.SetNumMips(1)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Dynamic);

		SourceYUVTexture = RHICreateTexture(CreateDesc);
	}

	if (!SourceAlphaTexture.IsValid() || FrameSize != PreviousFrameSize)
	{
		const FRHITextureCreateDesc CreateAlphaDesc = FRHITextureCreateDesc::Create2D(TEXT("NDIMediaReceiverInterlacedAlphaSourceAlphaTexture"))
			.SetExtent(FieldSize.X, FieldSize.Y)
			.SetFormat(PF_A8)
			.SetNumMips(1)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::Dynamic);

		SourceAlphaTexture = RHICreateTexture(CreateAlphaDesc);
	}

	PreviousFrameSize = FrameSize;

	// Update GPU textures from sample buffer
	FUpdateTextureRegion2D YUVRegion(0, 0, 0, 0, FieldSize.X/2, FieldSize.Y);
	RHIUpdateTexture2D(SourceYUVTexture, 0, YUVRegion, InSample->GetStride(), (uint8*&)Buffer);

	// Make sure resource is in SRV mode again
	InRHICmdList.Transition(FRHITransitionInfo(SourceYUVTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	
	FUpdateTextureRegion2D AlphaRegion(0, 0, 0, 0, FieldSize.X, FieldSize.Y);
	RHIUpdateTexture2D(SourceAlphaTexture, 0, AlphaRegion, FrameSize.X, ((uint8*&)Buffer) + FieldSize.Y * InSample->GetStride());

	// Make sure resource is in SRV mode again
	InRHICmdList.Transition(FRHITransitionInfo(SourceAlphaTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));

	return true;
}
