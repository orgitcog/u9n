// Copyright Epic Games, Inc. All Rights Reserved.

#include "Resources/TextureShareResourceUtils.h"

#include "RHIStaticStates.h"

#include "RenderingThread.h"
#include "RenderResource.h"
#include "CommonRenderResources.h"
#include "PixelShaderUtils.h"

#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterUtils.h"

#include "ScreenRendering.h"
#include "PostProcess/DrawRectangle.h"

#include "RenderTargetPool.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DECLARE_STATS_GROUP(TEXT("TextureShare"), STATGROUP_TextureShare, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CopyShared"), STAT_TextureShare_CopyShared, STATGROUP_TextureShare);
DECLARE_CYCLE_STAT(TEXT("ResampleTempRTT"), STAT_TextureShare_ResampleTempRTT, STATGROUP_TextureShare);

namespace UE::TextureShare::ResourceUtils
{
	static inline FIntRect GetTextureRect(FRHITexture* InTexture, const FIntRect* InRect = nullptr)
	{
		// InRect must be within the size of InTexture.
		FIntRect OutRect(FIntPoint(EForceInit::ForceInitToZero), InTexture->GetDesc().Extent);
		if (InRect)
		{
			OutRect.Max.X = FMath::Clamp(InRect->Max.X, 0, OutRect.Max.X);
			OutRect.Max.Y = FMath::Clamp(InRect->Max.Y, 0, OutRect.Max.Y);

			OutRect.Min.X = FMath::Clamp(InRect->Min.X, 0, OutRect.Max.X);
			OutRect.Min.Y = FMath::Clamp(InRect->Min.Y, 0, OutRect.Max.X);
		}

		return OutRect;
	}

	/** Returns true if a texture resampling shader is required.
	* @param SrcTexture           - Source texture
	* @param DestTexture          - Dest texture
	* @param SrcTextureColorDesc  - Source texture color information (gamme, etc)
	* @param DestTextureColorDesc - Dest texture color information (gamme, etc)
	* @param SrcTextureRect       - Source texture rect
	* @param DestTextureRect      - Dest texture rect
	*/
	static bool ShouldUseResampleShader(
		FRHITexture* SrcTexture,
		FRHITexture* DestTexture,
		const FTextureShareColorDesc& SrcTextureColorDesc,
		const FTextureShareColorDesc& DestTextureColorDesc,
		const FIntRect& SrcTextureRect,
		const FIntRect& DestTextureRect)
	{
		// Resize on request
		if (SrcTextureRect.Size() != DestTextureRect.Size())
		{
			return true;
		}

		// Format change on request
		const EPixelFormat InSrcFormat = SrcTexture->GetFormat();
		const EPixelFormat InDestFormat = DestTexture->GetFormat();
		if (InSrcFormat != InDestFormat)
		{
			return true;
		}


		// Gamma change on request
		if (SrcTextureColorDesc.ShouldConvertGamma(DestTextureColorDesc))
		{
			return true;
		}

		return false;
	}

	/** Gets a temporary resource from the pool.
	* @param InOutPoolResources - All temporarily created resources will be added to this array.
	* @param InSize             - desired size of RTT
	* @param InFormat           - desired format
	* @param bIsRTT             - true if resource is RTT
	*
	* @return ptr to the new temporary RHI texture
	*/
	FRHITexture* GetRenderTargetPoolResource_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		TArray<TRefCountPtr<IPooledRenderTarget>>& InOutPoolResources,
		const FIntPoint& InSize,
		const EPixelFormat InFormat,
		const bool bIsRTT)
	{
		const ETextureCreateFlags TargetableFlags = bIsRTT ? TexCreate_RenderTargetable : TexCreate_ShaderResource;
		const FPooledRenderTargetDesc NewResourceDesc = FPooledRenderTargetDesc::Create2DDesc(
				InSize,
				InFormat,
				FClearValueBinding::None,
				TexCreate_None, TargetableFlags, false);


		TRefCountPtr<IPooledRenderTarget> RenderTargetPoolResource;
		GRenderTargetPool.FindFreeElement(RHICmdList, NewResourceDesc, RenderTargetPoolResource, TEXT("TextureShare_ResampleTexture"));

		if (RenderTargetPoolResource.IsValid())
		{
			if (FRHITexture* RHITexture = RenderTargetPoolResource->GetRHI())
			{
				// Maintains an internal link to this resource.
				// It will be released late, after the TS has completed all operations on that resource.
				InOutPoolResources.Add(RenderTargetPoolResource);

				return RHITexture;
			}
		}

		return nullptr;
	}

	/**
	* Resample shaders parameters
	*/
	BEGIN_SHADER_PARAMETER_STRUCT(FScreenResamplePSParameters, )
		SHADER_PARAMETER_TEXTURE(Texture2D, InTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, InTextureSampler)
		SHADER_PARAMETER(float, GammaModifier)
	END_SHADER_PARAMETER_STRUCT()

	/**
	 * A pixel shader for TextureShare resource resampling
	 */
	class FTextureShareScreenResamplePS : public FGlobalShader
	{
	public:
		DECLARE_GLOBAL_SHADER(FTextureShareScreenResamplePS);
		SHADER_USE_PARAMETER_STRUCT(FTextureShareScreenResamplePS, FGlobalShader);

		using FParameters = FScreenResamplePSParameters;
	};

	IMPLEMENT_GLOBAL_SHADER(FTextureShareScreenResamplePS, "/Plugin/TextureShare/Private/TextureShareScreenPixelShader.usf", "Main", SF_Pixel);
};

bool FTextureShareResourceUtils::ResampleCopyTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* SrcTexture,
	FRHITexture* DestTexture,
	const FTextureShareColorDesc& SrcTextureColorDesc,
	const FTextureShareColorDesc& DestTextureColorDesc,
	const FIntRect* SrcTextureRect,
	const FIntRect* DestTextureRect)
{
	using namespace UE::TextureShare::ResourceUtils;

	if (SrcTexture == DestTexture || !SrcTexture || !DestTexture)
	{
		return false;
	}

	const FIntRect SrcRect = GetTextureRect(SrcTexture, SrcTextureRect);
	const FIntRect DestRect = GetTextureRect(DestTexture, DestTextureRect);

	// Implement simple gamma based on pow()
	// Gamma is converted in the shader in a simplified way:
	// 2.2 -> 1 : OutColor = pow(Color, 2.2f);
	// 1 -> 2.2 : OutColor = pow(Color, 1.0f / 2.2f);
	float SimpleGammaConversionValue = 1.f;
	if (SrcTextureColorDesc.ShouldConvertGamma(DestTextureColorDesc))
	{
		SimpleGammaConversionValue = SrcTextureColorDesc.CustomGamma / DestTextureColorDesc.CustomGamma;
	}

	// Texture format mismatch, use a shader to do the copy.
	FRHIRenderPassInfo RPInfo(DestTexture, ERenderTargetActions::Load_Store);
	RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::RTV));
	RHICmdList.BeginRenderPass(RPInfo, TEXT("TextureShare_ResampleTexture"));
	{
		const FIntPoint SrcTextureSize = SrcTexture->GetDesc().Extent;
		const FIntPoint DestTextureSize = DestTexture->GetDesc().Extent;

		RHICmdList.SetViewport(0.f, 0.f, 0.0f, DestTextureSize.X, DestTextureSize.Y, 1.0f);

		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();

		FGlobalShaderMap* ShaderMap = GetGlobalShaderMap(GMaxRHIFeatureLevel);
		TShaderMapRef<FScreenVS> VertexShader(ShaderMap);
		TShaderMapRef<FTextureShareScreenResamplePS> PixelShader(ShaderMap);

		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;

		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		FScreenResamplePSParameters PSParameters;
		{
			PSParameters.InTexture = SrcTexture;
			PSParameters.InTextureSampler = (SrcRect.Size() == DestRect.Size())
				? TStaticSamplerState<SF_Point>::GetRHI()
				: TStaticSamplerState<SF_Bilinear>::GetRHI();

			PSParameters.GammaModifier = SimpleGammaConversionValue;
		}
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PSParameters);

		// Set up vertex uniform parameters for scaling and biasing the rectangle.
		// Note: Use DrawRectangle in the vertex shader to calculate the correct vertex position and uv.
		UE::Renderer::PostProcess::DrawRectangle(
			RHICmdList, VertexShader,
			DestRect.Min.X,    DestRect.Min.Y,
			DestRect.Size().X, DestRect.Size().Y,
			SrcRect.Min.X, SrcRect.Min.Y,
			SrcRect.Size().X, SrcRect.Size().Y,
			DestTextureSize, SrcTextureSize
		);
	}

	RHICmdList.EndRenderPass();
	RHICmdList.Transition(FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::SRVMask));

	return true;
}

bool FTextureShareResourceUtils::DirectCopyTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	FRHITexture* SrcTexture,
	FRHITexture* DestTexture,
	const FIntRect* SrcTextureRect,
	const FIntRect* DestTextureRect)
{
	using namespace UE::TextureShare::ResourceUtils;

	if (SrcTexture == DestTexture || !SrcTexture || !DestTexture)
	{
		return false;
	}

	const FIntRect SrcRect = GetTextureRect(SrcTexture, SrcTextureRect);
	const FIntRect DestRect = GetTextureRect(DestTexture, DestTextureRect);

	const FIntPoint InRectSize = SrcRect.Size();
	// Copy with resolved params
	FRHICopyTextureInfo Params = {};
	Params.Size = FIntVector(InRectSize.X, InRectSize.Y, 0);
	Params.SourcePosition = FIntVector(SrcRect.Min.X, SrcRect.Min.Y, 0);
	Params.DestPosition = FIntVector(DestRect.Min.X, DestRect.Min.Y, 0);

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::RTV, ERHIAccess::CopySrc),
		FRHITransitionInfo(DestTexture, ERHIAccess::Unknown, ERHIAccess::CopyDest),
		});

	RHICmdList.CopyTexture(SrcTexture, DestTexture, Params);

	RHICmdList.Transition({
		FRHITransitionInfo(SrcTexture, ERHIAccess::CopySrc, ERHIAccess::SRVMask),
		FRHITransitionInfo(DestTexture, ERHIAccess::CopyDest, ERHIAccess::SRVMask),
		});

	return true;
}

bool FTextureShareResourceUtils::WriteToShareTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	TArray<TRefCountPtr<IPooledRenderTarget>>& InOutPoolResources,
	FRHITexture* SrcTexture,
	FRHITexture* DestSharedTexture,
	const FTextureShareColorDesc& SrcColorDesc,
	const FTextureShareColorDesc& DestColorDesc,
	const FIntRect* SrcTextureRectPtr)
{
	using namespace UE::TextureShare::ResourceUtils;

	if (SrcTexture == DestSharedTexture || !SrcTexture || !DestSharedTexture)
	{
		return false;
	}

	const FIntRect SrcRect = GetTextureRect(SrcTexture, SrcTextureRectPtr);
	const FIntRect DestRect = GetTextureRect(DestSharedTexture);
	if (ShouldUseResampleShader(SrcTexture, DestSharedTexture, SrcColorDesc, DestColorDesc, SrcRect, DestRect))
	{
		const EPixelFormat DestFormat = DestSharedTexture->GetFormat();
		if (FRHITexture* TemporaryRTT = GetRenderTargetPoolResource_RenderThread(
			RHICmdList,InOutPoolResources, DestRect.Size(), DestFormat, true))
		{
			// Resample source texture to TemporaryRTT
			SCOPE_CYCLE_COUNTER(STAT_TextureShare_ResampleTempRTT);
			ResampleCopyTexture_RenderThread(RHICmdList, SrcTexture, TemporaryRTT, SrcColorDesc, DestColorDesc, &SrcRect, nullptr);

			// Copy TemporaryRTT to shared texture surface
			SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
			DirectCopyTexture_RenderThread(RHICmdList, TemporaryRTT, DestSharedTexture, nullptr, &DestRect);

			return true;
		}

		// Can't allocate temporary texture
		return false;
	}

	// Copy direct to shared texture
	SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
	return DirectCopyTexture_RenderThread(RHICmdList, SrcTexture, DestSharedTexture, &SrcRect, &DestRect);
}

bool FTextureShareResourceUtils::ReadFromShareTexture_RenderThread(
	FRHICommandListImmediate& RHICmdList,
	TArray<TRefCountPtr<IPooledRenderTarget>>& InOutPoolResources,
	FRHITexture* SrcSharedTexture,
	FRHITexture* DestTexture,
	const FTextureShareColorDesc& SrcColorDesc,
	const FTextureShareColorDesc& DestColorDesc,
	const FIntRect* DestTextureRectPtr)
{
	using namespace UE::TextureShare::ResourceUtils;

	if (SrcSharedTexture == DestTexture || !SrcSharedTexture || !DestTexture)
	{
		return false;
	}

	const FIntRect SrcRect = GetTextureRect(SrcSharedTexture);
	const FIntRect DestRect = GetTextureRect(DestTexture, DestTextureRectPtr);
	if (ShouldUseResampleShader(SrcSharedTexture, DestTexture, SrcColorDesc, DestColorDesc, SrcRect, DestRect))
	{
		const EPixelFormat SrcFormat = SrcSharedTexture->GetFormat();
		const EPixelFormat DestFormat = DestTexture->GetFormat();

		// Create a temporary SRV texture since the shared resource is not srv.
		if (FRHITexture* SrcTextureSRV = GetRenderTargetPoolResource_RenderThread(RHICmdList, InOutPoolResources, SrcRect.Size(), SrcFormat, false))
		{
			// Copy direct from shared texture to SrcTextureSRV (received shared texture has only flag TexCreate_ResolveTargetable, not shader resource)
			SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
			DirectCopyTexture_RenderThread(RHICmdList, SrcSharedTexture, SrcTextureSRV, &SrcRect, nullptr);

			// If possible, use the dest texture directly instead of using a temporary RTT from the pool.
			const bool bDestTextureRenderTargetable = EnumHasAnyFlags(DestTexture->GetDesc().Flags, TexCreate_RenderTargetable);
			if (bDestTextureRenderTargetable)
			{
				// Resample source texture to DestTexture
				SCOPE_CYCLE_COUNTER(STAT_TextureShare_ResampleTempRTT);
				ResampleCopyTexture_RenderThread(RHICmdList, SrcTextureSRV, DestTexture, SrcColorDesc, DestColorDesc, nullptr, &DestRect);

				return true;
			}

			// Create a temporary RTT texture
			if (FRHITexture* TemporaryRTT = GetRenderTargetPoolResource_RenderThread(RHICmdList, InOutPoolResources, DestRect.Size(), DestFormat, true))
			{
				// Resample source texture to TemporaryRTT
				SCOPE_CYCLE_COUNTER(STAT_TextureShare_ResampleTempRTT);
				ResampleCopyTexture_RenderThread(RHICmdList, SrcTextureSRV, TemporaryRTT, SrcColorDesc, DestColorDesc, nullptr, nullptr);

				// Copy TemporaryRTT to Destination
				DirectCopyTexture_RenderThread(RHICmdList, TemporaryRTT, DestTexture, nullptr, &DestRect);

				return true;
			}
		}
	}

	// Copy direct to shared texture
	SCOPE_CYCLE_COUNTER(STAT_TextureShare_CopyShared);
	return DirectCopyTexture_RenderThread(RHICmdList, SrcSharedTexture, DestTexture, &SrcRect, &DestRect);
}
