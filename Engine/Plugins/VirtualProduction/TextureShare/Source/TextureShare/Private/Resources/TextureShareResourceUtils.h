// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "RendererInterface.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"

#include "Containers/TextureShareContainers.h"

/**
 * TextureShare resources helpers
 * 
 * Implements resample shaders that can modify a texture in many ways: size, format, color gamma, etc.
 */
class FTextureShareResourceUtils
{
public:
	/** Read the shared resoure to the texture.
	* @param InOutPoolResources - All temporarily created resources will be added to this array.
	* @param SrcTexture         - Src texture
	* @param DestSharedTexture  - Dest texture (shared resource)
	* @param SrcColorDesc       - Source texture color information (gamma, etc)
	* @param DestColorDesc      - Dest texture color information (gamma, etc)
	* @param SrcTextureRectPtr  - Src texture region (nullptr if the whole texture is used)
	* 
	* @return - true if any RHI command has been used.
	*/
	static bool WriteToShareTexture_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		TArray<TRefCountPtr<IPooledRenderTarget>>& InOutPoolResources,
		FRHITexture* SrcTexture,
		FRHITexture* DestSharedTexture,
		const FTextureShareColorDesc& SrcColorDesc,
		const FTextureShareColorDesc& DestColorDesc,
		const FIntRect* SrcTextureRectPtr = nullptr);

	/** Write the shared resource to the texture
	* @param InOutPoolResources - All temporarily created resources will be added to this array.
	* @param SrcSharedTexture   - Src texture
	* @param DestTexture  - Dest texture (shared resource)
	* @param SrcColorDesc       - Source texture color information (gamma, etc)
	* @param DestColorDesc      - Dest texture color information (gamma, etc)
	* @param DestTextureRectPtr - Src texture region (nullptr if the whole texture is used)
	* 
	* @return - true if any RHI command has been used.
	*/
	static bool ReadFromShareTexture_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		TArray<TRefCountPtr<IPooledRenderTarget>>& InOutPoolResources,
		FRHITexture* SrcSharedTexture,
		FRHITexture* DestTexture,
		const FTextureShareColorDesc& SrcColorDesc,
		const FTextureShareColorDesc& DestColorDesc,
		const FIntRect* DestTextureRectPtr = nullptr);

private:
	/** Copying a texture region.
	* @param SrcTexture      - Src texture
	* @param DestTexture     - Dest texture
	* @param SrcTextureRect  - Src texture region
	* @param DestTextureRect - Dest texture region
	* 
	* @return - true if RHI commands were used to copy the texture.
	*/
	static bool DirectCopyTexture_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		FRHITexture* SrcTexture,
		FRHITexture* DestTexture,
		const FIntRect* SrcTextureRect,
		const FIntRect* DestTextureRect);

	/**
	* Copies src-texture to dest-texture with changes in size, texture format and color.
	* 
	* @param SrcTexture           - Src texture
	* @param DestTexture          - Dest texture
	* @param SrcTextureColorDesc  - Source texture color information (gamme, etc)
	* @param DestTextureColorDesc - Dest texture color information (gamme, etc)
	* @param SrcTextureRect       - Src texture region
	* @param DestTextureRect      - Dest texture region
	* 
	* @return - true if RHI commands were used to copy the texture.
	*/
	static bool ResampleCopyTexture_RenderThread(
		FRHICommandListImmediate& RHICmdList,
		FRHITexture* SrcTexture,
		FRHITexture* DestTexture,
		const FTextureShareColorDesc& SrcTextureColorDesc,
		const FTextureShareColorDesc& DestTextureColorDesc,
		const FIntRect* SrcTextureRect,
		const FIntRect* DestTextureRect);
};
