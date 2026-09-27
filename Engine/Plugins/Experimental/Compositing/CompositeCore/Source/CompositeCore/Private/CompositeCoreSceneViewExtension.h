// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneViewExtension.h"

#include "Containers/ContainersFwd.h"
#include "RenderGraphFwd.h"
#include "RendererInterface.h"

#include "CompositeCoreSettings.h"
#include "Passes/CompositeCorePassProxy.h"

class FViewInfo;
struct FScreenPassRenderTarget;
struct FScreenPassTexture;
struct FScreenPassTextureSlice;
class UMaterialInterface;
class UTexture;

enum class EPostProcessMaterialInput : uint32;

namespace UE
{
	namespace CompositeCore
	{
		/** External pooled render target and its accompanying metadata. */
		struct FExternalRenderTarget
		{
			TRefCountPtr<IPooledRenderTarget> RenderTarget = {};
			FResourceMetadata Metadata = {};
		};
	}
}

class FCompositeCoreSceneViewExtension : public FWorldSceneViewExtension
{
public:
	FCompositeCoreSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld);
	~FCompositeCoreSceneViewExtension();

	/** Register primitives for compositing. */
	void RegisterPrimitives_GameThread(const TArray<UPrimitiveComponent*>& InPrimitiveComponents);

	/** Unregister primitives for compositing. */
	void UnregisterPrimitives_GameThread(const TArray<UPrimitiveComponent*>& InPrimitiveComponents);

	/** Set post-processing render work to be enqueued on the render thread. */
	void SetPostRenderWork_GameThread(UE::CompositeCore::FPostRenderWork&& InWork);

	/** Reset post-processing render work. */
	void ResetPostRenderWork_GameThread();

	/** Set options for built-in composite custom render pass. */
	void SetBuiltInRenderPassOptions_GameThread(const UE::CompositeCore::FBuiltInRenderPassOptions& InOptions);
	
	/** Reset default options for built-in composite custom render pass. */
	void ResetBuiltInRenderPassOptions_GameThread();

	/** Called by the custom render pass to store its view render target for this frame. */
	void CollectCustomRenderTarget(uint32 InViewId, const TRefCountPtr<IPooledRenderTarget>& InRenderTarget);

	//~ Begin ISceneViewExtension Interface
	virtual int32 GetPriority() const override;
	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	virtual void PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs) override;
	virtual void SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;
	virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily) override;
	virtual void PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView) override;
	//~ End ISceneViewExtension Interface

protected:
	//~ Begin ISceneViewExtension Interface
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;
	//~ End ISceneViewExtension Interface

private:
	using FExternalTextureMap = TSortedMap<int32, UE::CompositeCore::FPassInput>;

	/** Getter for the frame render work, either from the actor or default. */
	const UE::CompositeCore::FPostRenderWork& GetRenderWork() const;

	/** Active check to avoid work in post-processing. */
	bool IsActiveForView(const FSceneView& InView) const;

	/** Convenience function to create an override map of external textures. */
	FExternalTextureMap CreateExternalTextureMap(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessMaterialInputs& Inputs) const;
	
	/** Convenience function to update the next pass inputs from the last pass' output. */
	void UpdateNextPassInputs(
		FRDGBuilder& GraphBuilder,
		const UE::CompositeCore::FPassOutput& InOutput,
		const int32 BindingIndex,
		UE::CompositeCore::FPassInputArray& InOutInputs,
		FExternalTextureMap& InOutExternalTextures
	);

	/** Recursive pass application. */
	bool ApplyPasses_Recursive(
		FRDGBuilder& GraphBuilder,
		const FSceneView& InView,
		const UE::CompositeCore::FPassInputArray& Inputs,
		const UE::CompositeCore::FPassInputArray& OriginalInputs,
		UE::CompositeCore::FPassContext& PassContext,
		const TArray<const FCompositeCorePassProxy*> InPasses,
		FExternalTextureMap& InOutExternalTextures,
		int32 RecursionLevel,
		UE::CompositeCore::FPassOutput& Output
	);

	/** Callback for processing passes. */
	FScreenPassTexture PostProcessWork_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView,
		const FPostProcessMaterialInputs& Inputs, ISceneViewExtension::EPostProcessingPass InLocation);

	/** Collection of primitives to render as a custom render pass and composite after post-processing. .*/
	TSet<TWeakObjectPtr<UPrimitiveComponent>> CompositePrimitives;

	/** Array of external texture inputs. */
	TMap<UE::CompositeCore::ResourceId, UE::CompositeCore::FExternalRenderTarget> ExternalInputs_RenderThread;

	/** Custom render pass render targets for each active view .*/
	TMap<uint32, TRefCountPtr<IPooledRenderTarget>> CustomRenderTargetPerView_RenderThread;

	/** Custom render work. */
	TOptional<UE::CompositeCore::FPostRenderWork> PostRenderWork_RenderThread;

	/** Built-in composite render pass options. .*/
	TOptional<UE::CompositeCore::FBuiltInRenderPassOptions> BuiltInRenderPassOptions;
};

