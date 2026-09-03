// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "RenderGraphFwd.h"
#include "MeshPassProcessor.h"

#if UE_ENABLE_DEBUG_DRAWING

struct FSkinnedMeshPrimitive
{
	FPersistentPrimitiveIndex Index;
	uint32 BoneCount = 0;
	uint32 InstanceCount = 0;
};

class FSkinnedMeshDebugViewExtension : public FSceneViewExtensionBase
{
public:
	FSkinnedMeshDebugViewExtension(const FAutoRegister& AutoRegister);

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& View, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

	FScreenPassTexture PostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const struct FPostProcessMaterialInputs& InOutInputs);

	void RenderSkeletons(class FRDGBuilder& GraphBuilder, const class FSceneView& View, const struct FScreenPassRenderTarget& Output);

private:
	inline static TSharedPtr<FSkinnedMeshDebugViewExtension, ESPMode::ThreadSafe> Instance;

	struct FStaticConstructor
	{
		FStaticConstructor()
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]()
			{ 
				Instance = FSceneViewExtensions::NewExtension<FSkinnedMeshDebugViewExtension>();
			});
			FCoreDelegates::OnEnginePreExit.AddLambda([]()
			{ 
				Instance = nullptr;
			});
		}
	};

	static inline FStaticConstructor StaticConstructor;
};

#endif
