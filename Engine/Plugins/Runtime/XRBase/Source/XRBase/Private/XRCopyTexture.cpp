// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRCopyTexture.h"

#include "ClearQuad.h"
#include "CommonRenderResources.h"
#include "GenerateMips.h"
#include "HDRHelper.h"
#include "OculusShaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphResources.h"
#include "ScreenRendering.h"
#include "StereoRenderTargetManager.h"
#include "RHIStaticStates.h"
#include "Modules/ModuleManager.h"

class FDisplayMappingPS : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FDisplayMappingPS, Global);
public:

	class FArraySource : SHADER_PERMUTATION_BOOL("DISPLAY_MAPPING_PS_FROM_ARRAY");
	class FLinearInput : SHADER_PERMUTATION_BOOL("DISPLAY_MAPPING_INPUT_IS_LINEAR");
	using FPermutationDomain = TShaderPermutationDomain<FArraySource, FLinearInput>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	FDisplayMappingPS(const ShaderMetaType::CompiledShaderInitializerType& Initializer) :
		FGlobalShader(Initializer)
	{
		OutputDevice.Bind(Initializer.ParameterMap, TEXT("OutputDevice"));
		OutputGamut.Bind(Initializer.ParameterMap, TEXT("OutputGamut"));
		SceneTexture.Bind(Initializer.ParameterMap, TEXT("SceneTexture"));
		SceneSampler.Bind(Initializer.ParameterMap, TEXT("SceneSampler"));
		TextureToOutputGamutMatrix.Bind(Initializer.ParameterMap, TEXT("TextureToOutputGamutMatrix"));
		ArraySlice.Bind(Initializer.ParameterMap, TEXT("ArraySlice"));
	}
	FDisplayMappingPS() = default;

	static FMatrix44f GamutToXYZMatrix(EDisplayColorGamut ColorGamut)
	{
		static const FMatrix44f sRGB_2_XYZ_MAT(
			FVector3f(	0.4124564, 0.3575761, 0.1804375),
			FVector3f(	0.2126729, 0.7151522, 0.0721750),
			FVector3f(	0.0193339, 0.1191920, 0.9503041),
			FVector3f(	0        ,         0,         0)
		);

		static const FMatrix44f Rec2020_2_XYZ_MAT(
			FVector3f(	0.6369736, 0.1446172, 0.1688585),
			FVector3f(	0.2627066, 0.6779996, 0.0592938),
			FVector3f(	0.0000000, 0.0280728, 1.0608437),
			FVector3f(	0        ,         0,         0)
		);

		static const FMatrix44f P3D65_2_XYZ_MAT(
			FVector3f(	0.4865906, 0.2656683, 0.1981905),
			FVector3f(	0.2289838, 0.6917402, 0.0792762),
			FVector3f(	0.0000000, 0.0451135, 1.0438031),
			FVector3f(	0        ,         0,         0)
		);
		switch (ColorGamut)
		{
		case EDisplayColorGamut::sRGB_D65: return sRGB_2_XYZ_MAT;
		case EDisplayColorGamut::Rec2020_D65: return Rec2020_2_XYZ_MAT;
		case EDisplayColorGamut::DCIP3_D65: return P3D65_2_XYZ_MAT;
		default:
			checkNoEntry();
			return FMatrix44f::Identity;
		}

	}

	static FMatrix44f XYZToGamutMatrix(EDisplayColorGamut ColorGamut)
	{
		static const FMatrix44f XYZ_2_sRGB_MAT(
			FVector3f(	 3.2409699419, -1.5373831776, -0.4986107603),
			FVector3f(	-0.9692436363,  1.8759675015,  0.0415550574),
			FVector3f(	 0.0556300797, -0.2039769589,  1.0569715142),
			FVector3f(	 0           ,             0,             0)
		);

		static const FMatrix44f XYZ_2_Rec2020_MAT(
			FVector3f(1.7166084, -0.3556621, -0.2533601),
			FVector3f(-0.6666829, 1.6164776, 0.0157685),
			FVector3f(0.0176422, -0.0427763, 0.94222867),
			FVector3f(0, 0, 0)
		);

		static const FMatrix44f XYZ_2_P3D65_MAT(
			FVector3f(	 2.4933963, -0.9313459, -0.4026945),
			FVector3f(	-0.8294868,  1.7626597,  0.0236246),
			FVector3f(	 0.0358507, -0.0761827,  0.9570140),
			FVector3f(	 0        ,         0,         0 )
		);

		switch (ColorGamut)
		{
		case EDisplayColorGamut::sRGB_D65: return XYZ_2_sRGB_MAT;
		case EDisplayColorGamut::Rec2020_D65: return XYZ_2_Rec2020_MAT;
		case EDisplayColorGamut::DCIP3_D65: return XYZ_2_P3D65_MAT;
		default:
			checkNoEntry();
			return FMatrix44f::Identity;
		}

	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, EDisplayOutputFormat DisplayOutputFormat, EDisplayColorGamut DisplayColorGamut, EDisplayColorGamut TextureColorGamut, FRHITexture* SceneTextureRHI, FRHISamplerState* SamplerStateRHI, uint32 InArraySlice = 0)
	{
		int32 OutputDeviceValue = (int32)DisplayOutputFormat;
		int32 OutputGamutValue = (int32)DisplayColorGamut;

		SetShaderValue(BatchedParameters, OutputDevice, OutputDeviceValue);
		SetShaderValue(BatchedParameters, OutputGamut, OutputGamutValue);

		const FMatrix44f TextureGamutMatrixToXYZ = GamutToXYZMatrix(TextureColorGamut);
		const FMatrix44f XYZToDisplayMatrix = XYZToGamutMatrix(DisplayColorGamut);
		// note: we use mul(m,v) instead of mul(v,m) in the shaders for color conversions which is why matrix multiplication is reversed compared to what we usually do
		const FMatrix44f CombinedMatrix = XYZToDisplayMatrix * TextureGamutMatrixToXYZ;

		SetShaderValue(BatchedParameters, TextureToOutputGamutMatrix, CombinedMatrix);
		SetTextureParameter(BatchedParameters, SceneTexture, SceneSampler, SamplerStateRHI, SceneTextureRHI);

		SetShaderValue(BatchedParameters, ArraySlice, InArraySlice);
	}

	static const TCHAR* GetSourceFilename()
	{
		return TEXT("/Engine/Private/DisplayMappingPixelShader.usf");
	}

	static const TCHAR* GetFunctionName()
	{
		return TEXT("DisplayMappingPS");
	}

private:
	LAYOUT_FIELD(FShaderParameter, OutputDevice);
	LAYOUT_FIELD(FShaderParameter, OutputGamut);
	LAYOUT_FIELD(FShaderParameter, TextureToOutputGamutMatrix);
	LAYOUT_FIELD(FShaderParameter, ArraySlice);
	LAYOUT_FIELD(FShaderResourceParameter, SceneTexture);
	LAYOUT_FIELD(FShaderResourceParameter, SceneSampler);
};

IMPLEMENT_SHADER_TYPE(, FDisplayMappingPS, TEXT("/Engine/Private/DisplayMappingPixelShader.usf"), TEXT("DisplayMappingPS"), SF_Pixel);

void FXRCopyTextureOptions::SetDisplayMappingOptions(IStereoRenderTargetManager* HDRManager)
{
	bSrcIsLinear = false;
	bSrcSupportsHDR = false;
	bDstSupportsHDR = false;
	SrcColorGamut = EDisplayColorGamut::sRGB_D65;
	DstColorGamut = EDisplayColorGamut::sRGB_D65;
	SrcDisplayFormat = EDisplayOutputFormat::SDR_ExplicitGammaMapping;
	DstDisplayFormat = EDisplayOutputFormat::SDR_ExplicitGammaMapping;

	bool bHasSrcFormats = HDRManager && HDRManager->HDRGetMetaDataForStereo(SrcDisplayFormat, SrcColorGamut, bSrcSupportsHDR);
	HDRGetMetaData(DstDisplayFormat, DstColorGamut, bDstSupportsHDR, FVector2D(0, 0), FVector2D(0, 0), nullptr);

	if (bHasSrcFormats && (DstDisplayFormat != SrcDisplayFormat || DstColorGamut != SrcColorGamut || bDstSupportsHDR != bSrcSupportsHDR))
	{
		bNeedsDisplayMapping = true;
	}

	// In Android Vulkan preview, when the sRGB swapchain texture is sampled, the data is converted to linear and written to the RGBA10A2_UNORM texture.
	// However, D3D interprets integer-valued display formats as containing sRGB data, so we need to convert the linear data back to sRGB.
	if (!IsMobileHDR() && IsMobilePlatform(ShaderPlatform) && IsSimulatedPlatform(ShaderPlatform))
	{
		bNeedsDisplayMapping = true;
		DstDisplayFormat = EDisplayOutputFormat::SDR_sRGB;
		bSrcIsLinear = true;
	}

	bNeedsDisplayMapping &= IsFeatureLevelSupported(ShaderPlatform, ERHIFeatureLevel::ES3_1);
}

BEGIN_SHADER_PARAMETER_STRUCT(FXRCopyTexturePass, )
	RDG_TEXTURE_ACCESS(SrcTexture, ERHIAccess::SRVGraphics)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

void AddXRCopyTexturePass(FRDGBuilder& GraphBuilder, FRDGEventName&& Name, FRDGTextureRef SrcTexture, FIntRect SrcRect,
									FRDGTextureRef DstTexture, FIntRect DstRect, const FXRCopyTextureOptions& Options)
{
	bool bIsArrayCopy = DstTexture->Desc.Dimension == ETextureDimension::Texture2DArray && SrcTexture->Desc.Dimension == ETextureDimension::Texture2DArray;
	int TotalSlices = bIsArrayCopy ? FMath::Min(DstTexture->Desc.ArraySize, SrcTexture->Desc.ArraySize) : 1;

	bool bSrcHasMips = SrcTexture->Desc.NumMips > 1;
	int TotalMips = (Options.bOutputMipChain && bSrcHasMips) ? DstTexture->Desc.NumMips : 1;
	for (int ArraySlice = 0; ArraySlice < TotalSlices; ArraySlice++)
	{
		for (int MipLevel = 0; MipLevel < TotalMips; ++MipLevel)
		{
			FXRCopyTexturePass *Params = GraphBuilder.AllocParameters<FXRCopyTexturePass>();
			Params->SrcTexture = SrcTexture;
			Params->RenderTargets[0] = FRenderTargetBinding(DstTexture, Options.LoadAction, MipLevel, ArraySlice);
			GraphBuilder.AddPass(MoveTemp(Name), Params, ERDGPassFlags::Raster,
				[SrcTexture, SrcRect, DstTexture, DstRect, Options, ArraySlice, MipLevel](FRHICommandListImmediate& RHICmdList)
				{
					PRAGMA_DISABLE_DEPRECATION_WARNINGS
					XRCopyTexture_InRenderPass(RHICmdList, SrcTexture->GetRHI(), SrcRect, DstTexture->GetRHI(), DstRect, Options, ArraySlice, MipLevel);
					PRAGMA_ENABLE_DEPRECATION_WARNINGS
				});
		}
	}

	if (Options.bOutputMipChain && DstTexture->Desc.NumMips > 1 && !bSrcHasMips)
	{
		FGenerateMips::Execute(GraphBuilder, Options.FeatureLevel, DstTexture);
	}
}

void XRCopyTexture_InRenderPass(FRHICommandListImmediate& RHICmdList, FRHITexture* SrcTexture, FIntRect SrcRect, FRHITexture* DstTexture,
	FIntRect DstRect, const FXRCopyTextureOptions& Options, uint32 ArraySlice, uint32 MipLevel)
{
	const FIntRect MipDstRect(DstRect.Min.X >> MipLevel, DstRect.Min.Y >> MipLevel,
		(DstRect.Max.X + (1U<<MipLevel) - 1U) >> MipLevel, (DstRect.Max.Y + (1U<<MipLevel) - 1U) >> MipLevel);
	const FIntPoint TargetSize(MipDstRect.Width(), MipDstRect.Height());
	// Do these calculations as floating point to get exact bounds
	const float MipScale = 1.0f / static_cast<float>(1U << MipLevel);
	const float ViewportWidthFractional = DstRect.Width() * MipScale;
	const float ViewportHeightFractional = DstRect.Height() * MipScale;
	const float ViewportSubpixelOffsetX = DstRect.Min.X * MipScale - MipDstRect.Min.X;
	const float ViewportSubpixelOffsetY = DstRect.Min.Y * MipScale - MipDstRect.Min.Y;

	const float SrcTextureWidth = SrcTexture->GetSizeX();
	const float SrcTextureHeight = SrcTexture->GetSizeY();
	float U = 0.f, V = 0.f, USize = 1.f, VSize = 1.f;
	if (SrcRect.IsEmpty())
	{
		SrcRect.Min.X = 0;
		SrcRect.Min.Y = 0;
		SrcRect.Max.X = SrcTextureWidth;
		SrcRect.Max.Y = SrcTextureHeight;
	}
	else
	{
		U = SrcRect.Min.X / SrcTextureWidth;
		V = SrcRect.Min.Y / SrcTextureHeight;
		USize = SrcRect.Width() / SrcTextureWidth;
		VSize = SrcRect.Height() / SrcTextureHeight;
	}

	if (Options.bClearBlack || Options.BlendMod == EXRCopyTextureBlendModifier::Opaque || Options.BlendMod == EXRCopyTextureBlendModifier::InvertAlpha)
	{
		const FIntRect ClearRect(0, 0, FMath::Max(1U, DstTexture->GetSizeX() >> MipLevel), FMath::Max(1U, DstTexture->GetSizeY() >> MipLevel));
		RHICmdList.SetViewport(ClearRect.Min.X, ClearRect.Min.Y, 0, ClearRect.Max.X, ClearRect.Max.Y, 1.0f);

		if (Options.bClearBlack)
		{
			DrawClearQuad(RHICmdList, FLinearColor::Black);
		}
		else
		{
			// For opaque or invert alpha texture copies, we want to make sure alpha is initialized to 1.0f
			DrawClearQuadAlpha(RHICmdList, 1.0f);
		}
	}

	if (TargetSize.X == 0 || TargetSize.Y == 0)
	{
		return;
	}

	RHICmdList.SetViewport(MipDstRect.Min.X, MipDstRect.Min.Y, 0, MipDstRect.Max.X, MipDstRect.Max.Y, 1.0f);

	FGraphicsPipelineStateInitializer GraphicsPSOInit;
	RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

	// We need to differentiate between types of layers: opaque, unpremultiplied alpha (regular texture copy) and premultiplied alpha (emulation texture)
	switch (Options.BlendMod)
	{
		case EXRCopyTextureBlendModifier::Opaque:
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGB>::GetRHI();
			break;
		case EXRCopyTextureBlendModifier::TransparentAlphaPassthrough:
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA>::GetRHI();
			break;
		case EXRCopyTextureBlendModifier::PremultipliedAlphaBlend:
			// Because StereoLayerRender actually enables alpha blending as it composites the layers into the emulation texture
			// the color values for the emulation swapchain are PREMULTIPLIED ALPHA. That means we don't want to multiply alpha again!
			// So we can just do SourceColor * 1.0f + DestColor (1 - SourceAlpha)
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_One, BF_InverseSourceAlpha>::GetRHI();
			break;
		case EXRCopyTextureBlendModifier::InvertAlpha:
			// write RGBA, RGB = src.rgb * 1 + dst.rgb * 0, A = src.a * 0 + dst.a * (1 - src.a)
			// Note dst.a has been cleared to 1.0 above
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_Zero, BO_Add, BF_Zero, BF_InverseSourceAlpha>::GetRHI();
			break;
		default:
			checkf(false, TEXT("Unsupported copy modifier"));
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			break;
	}

	GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
	GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
	GraphicsPSOInit.PrimitiveType = PT_TriangleList;

	FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(Options.ShaderPlatform);

	TShaderMapRef<FScreenVS> VertexShader(ShaderMap);

	TShaderRef<FGlobalShader> PixelShader;
	TShaderRef<FDisplayMappingPS> DisplayMappingPS;
	TShaderRef<FScreenPS> ScreenPS;
	TShaderRef<FScreenPSArraySlice> ScreenPSArraySlice;

	bool bIsArraySource = SrcTexture->GetDesc().IsTextureArray();

	if (Options.bNeedsDisplayMapping)
	{
		FDisplayMappingPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FDisplayMappingPS::FArraySource>(bIsArraySource);
		PermutationVector.Set<FDisplayMappingPS::FLinearInput>(Options.bSrcIsLinear);

		TShaderMapRef<FDisplayMappingPS> DisplayMappingPSRef(ShaderMap, PermutationVector);

		DisplayMappingPS = DisplayMappingPSRef;
		PixelShader = DisplayMappingPSRef;
	}
	else
	{
		if (LIKELY(!bIsArraySource))
		{
			TShaderMapRef<FScreenPS> ScreenPSRef(ShaderMap);
			ScreenPS = ScreenPSRef;
			PixelShader = ScreenPSRef;
		}
		else
		{
			TShaderMapRef<FScreenPSArraySlice> ScreenPSRef(ShaderMap);
			ScreenPSArraySlice = ScreenPSRef;
			PixelShader = ScreenPSRef;
		}
	}

	GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
	GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
	GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

	SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

	const bool bSameSize = DstRect.Size() == SrcRect.Size();
	FRHISamplerState* const PixelSampler = bSameSize ? TStaticSamplerState<SF_Point>::GetRHI() : TStaticSamplerState<SF_Bilinear>::GetRHI();

	if (ScreenPS.IsValid())
	{
		SetShaderParametersLegacyPS(RHICmdList, ScreenPS, PixelSampler, SrcTexture);
	}
	else if (ScreenPSArraySlice.IsValid())
	{
		SetShaderParametersLegacyPS(RHICmdList, ScreenPSArraySlice, PixelSampler, SrcTexture, ArraySlice);
	}
	else if (DisplayMappingPS.IsValid())
	{
		SetShaderParametersLegacyPS(RHICmdList, DisplayMappingPS, Options.DstDisplayFormat, Options.DstColorGamut, Options.SrcColorGamut, SrcTexture, PixelSampler, ArraySlice);
	}

	IRendererModule& RendererModule = FModuleManager::GetModuleChecked<IRendererModule>(FName("Renderer"));
	RendererModule.DrawRectangle(
		RHICmdList,
		ViewportSubpixelOffsetX, ViewportSubpixelOffsetY,
		ViewportWidthFractional, ViewportHeightFractional,
		U, V,
		USize, VSize,
		TargetSize,
		FIntPoint(1, 1),
		VertexShader,
		EDRF_Default);
}
