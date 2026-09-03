// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "LandscapeComponent.h"
#include "MeshMaterialShader.h"

class ALandscapeProxy;
class FLandscapeAsyncTextureReadback;
class FLandscapeComponentSceneProxy;
class FRDGTexture;
class FTextureRenderTarget2DResource;
class UTextureRenderTarget2D;
using FRDGTextureRef = FRDGTexture*;

namespace UE::Landscape
{
	LANDSCAPE_API bool CanRenderGrassMap(ULandscapeComponent* Component);
	LANDSCAPE_API bool IsRuntimeGrassMapGenerationSupported();
}

// Hacky base class to avoid 8 bytes of padding after the vtable
class FLandscapeGrassWeightExporter_RenderThread_FixLayout
{
public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread_FixLayout() = default;
};

// data also accessible by render thread
class FLandscapeGrassWeightExporter_RenderThread : public FLandscapeGrassWeightExporter_RenderThread_FixLayout
{
	friend class FLandscapeGrassWeightExporter;
	friend class FLandscapeGrassMapsBuilder;

private:
	FLandscapeGrassWeightExporter_RenderThread(const TArray<int32>& InHeightMips, bool bInReadbackToCPU = true);

public:
	virtual ~FLandscapeGrassWeightExporter_RenderThread();

	const FIntPoint& GetTargetSize() const { return TargetSize; }

	/** Renders the component to the given texture. */
	LANDSCAPE_API void RenderLandscapeComponentToTexture_RenderThread(FRDGBuilder& GraphBuilder, FRDGTextureRef OutputTexture);

private:
	struct FComponentInfo
	{
		TObjectPtr<ULandscapeComponent> Component = nullptr;
		TArray<TObjectPtr<ULandscapeGrassType>> RequestedGrassTypes;
		FVector2D ViewOffset = FVector2D::ZeroVector;
		int32 PixelOffsetX = 0;
		FLandscapeComponentSceneProxy* SceneProxy = nullptr;
		int32 NumPasses = 0;
		int32 FirstHeightMipsPassIndex = MAX_int32;

		FComponentInfo(ULandscapeComponent* InComponent, bool bInNeedsGrassmap, bool bInNeedsHeightmap, const TArray<int32>& InHeightMips)
			: Component(InComponent)
			, SceneProxy((FLandscapeComponentSceneProxy*)InComponent->SceneProxy)
		{
			if (bInNeedsGrassmap)
			{
				RequestedGrassTypes = InComponent->GetGrassTypes();
			}
			int32 NumGrassMaps = RequestedGrassTypes.Num();
			if (bInNeedsHeightmap || NumGrassMaps > 0)
			{
				// 2 channels for the heightmap, and one channel for each grass map, packed into 4 channel render targets
				NumPasses += FMath::DivideAndRoundUp(2 /* heightmap */ + NumGrassMaps, 4);
			}

#if WITH_EDITORONLY_DATA
			// since we don't read HeightMips unless we are in editor, there's no reason to add passes for it unless we are in editor
			if (InHeightMips.Num() > 0)
			{
				FirstHeightMipsPassIndex = NumPasses;
				NumPasses += InHeightMips.Num();
			}
#endif // WITH_EDITORONLY_DATA
		}
	};

	FSceneInterface* SceneInterface = nullptr;
	TArray<FComponentInfo, TInlineAllocator<1>> ComponentInfos;
	FIntPoint TargetSize;
	TArray<int32> HeightMips;
	float PassOffsetX;
	FVector ViewOrigin;

	FLandscapeAsyncTextureReadback* AsyncReadbackPtr = nullptr;

	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;

	/** Creates a texture and renders the component to it, and then and triggers a readback of the texture. */
	void RenderLandscapeComponentToTexture_RenderThread(FRHICommandListImmediate& RHICmdList);
};

class FLandscapeGrassWeightExporter : public FLandscapeGrassWeightExporter_RenderThread
{
	friend class FLandscapeGrassMapsBuilder;

private:
	TObjectPtr<ALandscapeProxy> LandscapeProxy;
	int32 ComponentSizeVerts;
	int32 SubsectionSizeQuads;
	int32 NumSubsections;
	TArray<TObjectPtr<ULandscapeGrassType>> GrassTypes;

public:
	LANDSCAPE_API FLandscapeGrassWeightExporter(ALandscapeProxy* InLandscapeProxy, TArrayView<ULandscapeComponent* const> InLandscapeComponents, bool bInNeedsGrassmap = true, bool bInNeedsHeightmap = true, const TArray<int32>& InHeightMips = {}, bool bInRenderImmediately = true, bool bInReadbackToCPU = true);

	// If using the async readback path, check its status and update if needed. Return true when the AsyncReadbackResults are available.
	// You must call this periodically, or the async readback may not complete.
	// bInForceFinish will force the RenderThread to wait until GPU completes the readback, ensuring the readback is completed after the render thread executes the command.
	// NOTE: you may still see false returned, this just means the render thread hasn't executed the command yet.
	bool CheckAndUpdateAsyncReadback(bool& bOutRenderCommandsQueued, const bool bInForceFinish = false);

	// return true if the async readback is complete.  (Does not update the readback state)
	bool IsAsyncReadbackComplete();

	// Fetches the results from the GPU texture and translates them into FLandscapeComponentGrassDatas.
	// If using async readback, requires AsyncReadback to be complete before calling this.
	// bFreeAsyncReadback if true will call FreeAsyncReadback() to free the readback resource (otherwise you must do it manually)
	TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>> FetchResults(bool bFreeAsyncReadback);

private:
	void FreeAsyncReadback();

	// Applies the results using pre-fetched data.
	static void ApplyResults(TMap<ULandscapeComponent*, TUniquePtr<FLandscapeComponentGrassData>, TInlineSetAllocator<1>>& Results);

	// Fetches the results and applies them to the landscape components
	// If using async readback, requires AsyncReadback to be complete before calling this.
	void ApplyResults();

	void CancelAndSelfDestruct();
};

namespace UE::Landscape::Grass
{
	void AddGrassWeightShaderTypes(FMaterialShaderTypes& InOutShaderTypes);
}

