// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RendererInterface.h"
#include "DecalRenderingCommon.h"

class FDeferredDecalProxy;
class FMaterial;
class FMaterialRenderProxy;
class FScene;
class FViewInfo;
class FDecalVisibilityTaskData;

class FShader;
class FShaderMapPointerTable;
template<typename ShaderType, typename PointerTableType> class TShaderRefBase;
template<typename ShaderType> using TShaderRef = TShaderRefBase<ShaderType, FShaderMapPointerTable>;

/**
 * Compact deferred decal data for rendering.
 */
struct FVisibleDecal
{
	FVisibleDecal(const FDeferredDecalProxy& InDecalProxy, float InConservativeRadius, float InFadeAlpha, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel);

	const FMaterialRenderProxy* MaterialProxy;
	uintptr_t Component;
	uint32 SortOrder;
	FDecalBlendDesc BlendDesc;
	float ConservativeRadius;
	float FadeAlpha;
	float InvFadeDuration;
	float InvFadeInDuration;
	float FadeStartDelayNormalized;
	float FadeInStartDelayNormalized;
	FLinearColor DecalColor;
	FTransform ComponentTrans;
	FBox BoxBounds;
};

using FVisibleDecalList = TArray<FVisibleDecal, SceneRenderingAllocator>;
using FRelevantDecalList = TArray<const FVisibleDecal*, SceneRenderingAllocator>;

class FDecalVisibilityViewPacket
{
public:
	FDecalVisibilityViewPacket(const FDecalVisibilityTaskData& InTaskData, const FScene& Scene, const FViewInfo& InView);

	~FDecalVisibilityViewPacket()
	{
		check(bFinishCalled);
		check(AllTasksEvent.IsCompleted());
	}

	TConstArrayView<FVisibleDecal> FinishVisibleDecals();
	TConstArrayView<const FVisibleDecal*> FinishRelevantDecals(EDecalRenderStage Stage);

	void Finish()
	{
		bFinishCalled = true;
		AllTasksEvent.Trigger();
		AllTasksEvent.Wait();
	}

	bool HasStage(EDecalRenderStage Stage) const
	{
		return RelevantDecalsMap.Contains(Stage);
	}

private:
	const FDecalVisibilityTaskData& TaskData;
	const FViewInfo& View;

	struct FVisibleDecals
	{
		FVisibleDecalList List;
		UE::Tasks::FTask Task;

	} VisibleDecals;

	struct FRelevantDecals
	{
		FRelevantDecalList List;
		UE::Tasks::FTask Task;
	};

	TMap<EDecalRenderStage, FRelevantDecals> RelevantDecalsMap;
	UE::Tasks::FTaskEvent AllTasksEvent{ UE_SOURCE_LOCATION };
	bool bFinishCalled = false;
};

class FDecalVisibilityTaskData
{
public:
	static FDecalVisibilityTaskData* Launch(FRDGBuilder& GraphBuilder, const FScene& Scene, TConstArrayView<FViewInfo> Views);

	TConstArrayView<FVisibleDecal> FinishVisibleDecals(int32 ViewIndex)
	{
		return ViewPackets[ViewIndex].FinishVisibleDecals();
	}

	TConstArrayView<const FVisibleDecal*> FinishRelevantDecals(int32 ViewIndex, EDecalRenderStage Stage)
	{
		return ViewPackets[ViewIndex].FinishRelevantDecals(Stage);
	}

	bool HasStage(int32 ViewIndex, EDecalRenderStage Stage) const
	{
		return ViewPackets[ViewIndex].HasStage(Stage);
	}

	void Finish()
	{
		for (FDecalVisibilityViewPacket& ViewPacket : ViewPackets)
		{
			ViewPacket.Finish();
		}
	}

	bool IsDBufferEnabled() const
	{
		return bDBufferEnabled;
	}

	bool IsGBufferEnabled() const
	{
		return bGBufferEnabled;
	}

private:
	FDecalVisibilityTaskData(const FScene& Scene, TConstArrayView<FViewInfo> Views, bool bInDBufferEnabled, bool bInGBufferEnabled);

	const bool bDBufferEnabled;
	const bool bGBufferEnabled;

	TArray<FDecalVisibilityViewPacket, FRDGArrayAllocator> ViewPackets;

	friend class FDecalVisibilityViewPacket;
	RDG_FRIEND_ALLOCATOR_FRIEND(FDecalVisibilityTaskData);
};

/**
 * Shared deferred decal functionality.
 */
namespace DecalRendering
{
	float GetDecalFadeScreenSizeMultiplier();
	float CalculateDecalFadeAlpha(float DecalFadeScreenSize, const FMatrix& ComponentToWorldMatrix, const FViewInfo& View, float FadeMultiplier);
	FMatrix ComputeComponentToClipMatrix(const FViewInfo& View, const FMatrix& DecalComponentToWorld);
	void SetVertexShaderOnly(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, const FViewInfo& View, const FMatrix& FrustumComponentToClip);
	void SortDecalList(FRelevantDecalList& Decals);
	FVisibleDecalList BuildVisibleDecalList(TConstArrayView<FDeferredDecalProxy*> Decals, const FViewInfo& View);
	FRelevantDecalList BuildRelevantDecalList(TConstArrayView<FVisibleDecal> Decals, EDecalRenderStage DecalRenderStage);
	bool HasRelevantDecals(TConstArrayView<FVisibleDecal> Decals, EDecalRenderStage DecalRenderStage);
	bool GetShaders(ERHIFeatureLevel::Type FeatureLevel, const FMaterial& Material, EDecalRenderStage DecalRenderStage, TShaderRef<FShader>& OutVertexShader, TShaderRef<FShader>& OutPixelShader);
	bool SetupShaderState(ERHIFeatureLevel::Type FeatureLevel, const FMaterial& Material, EDecalRenderStage DecalRenderStage, FBoundShaderStateInput& OutBoundShaderState);
	void SetShader(FRHICommandList& RHICmdList, FGraphicsPipelineStateInitializer& GraphicsPSOInit, uint32 StencilRef, const FViewInfo& View, const FVisibleDecal& DecalData, EDecalRenderStage DecalRenderStage, const FMatrix& FrustumComponentToClip, const FScene* Scene = nullptr);
};