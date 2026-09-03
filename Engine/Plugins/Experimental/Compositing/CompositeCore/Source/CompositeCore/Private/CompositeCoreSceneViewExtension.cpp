// Copyright Epic Games, Inc. All Rights Reserved.

#include "CompositeCoreSceneViewExtension.h"

#include "CompositeCoreModule.h"
#include "Passes/CompositeCorePassDilate.h"
#include "Passes/CompositeCorePassFXAAProxy.h"

#include "CommonRenderResources.h"
#include "Components/PrimitiveComponent.h"
#include "Containers/Set.h"
#include "EngineUtils.h"
#include "Engine/Texture.h"
#include "HDRHelper.h"
#include "PostProcess/PostProcessAA.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Rendering/CustomRenderPass.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "SceneView.h"
#include "ScreenPass.h"
#include "SystemTextures.h"
#include "TextureResource.h"

static TAutoConsoleVariable<int32> CVarCompositeCoreApplyFXAA(
	TEXT("CompositeCore.ApplyFXAA"),
	0,
	TEXT("When enabled, the custom render pass automatically applies FXAA."),
	ECVF_RenderThreadSafe);

class FCompositeCoreCustomRenderPass : public FCustomRenderPassBase
{
public:
	IMPLEMENT_CUSTOM_RENDER_PASS(FCompositeCoreCustomRenderPass);

	FCompositeCoreCustomRenderPass(const FIntPoint& InRenderTargetSize, FCompositeCoreSceneViewExtension* InParentExtension, const FSceneView& InView, const UE::CompositeCore::FBuiltInRenderPassOptions& InOptions)
		: FCustomRenderPassBase(TEXT("CompositeCoreCustomRenderPass"), ERenderMode::DepthAndBasePass, ERenderOutput::SceneColorAndAlpha, InRenderTargetSize)
		, ParentExtension(InParentExtension)
		, ViewId(InView.GetViewKey())
		, ViewFeatureLevel(InView.GetFeatureLevel())
		, Inputs({ InOptions.DilationSize, InOptions.bOpacifyOutput })
	{
		bSceneColorWithTranslucent = true;
	}

	virtual void OnPreRender(FRDGBuilder& GraphBuilder) override
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetSize, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource);
		RenderTargetTexture = GraphBuilder.CreateTexture(TextureDesc, TEXT("CompositeCoreCustomTexture"));
		AddClearRenderTargetPass(GraphBuilder, RenderTargetTexture, FLinearColor::Black, FIntRect(FInt32Point(), RenderTargetSize));
	}

	virtual void OnPostRender(FRDGBuilder& GraphBuilder) override
	{
		const FRDGTextureDesc TextureDesc = FRDGTextureDesc::Create2D(RenderTargetTexture->Desc.Extent, PF_FloatRGBA, FClearValueBinding::Black, TexCreate_UAV | TexCreate_ShaderResource | TexCreate_RenderTargetable);
		FRDGTextureRef Output = GraphBuilder.CreateTexture(TextureDesc, TEXT("CompositeCoreProcessedTexture"));

		UE::CompositeCore::Private::AddDilatePass(GraphBuilder, RenderTargetTexture, Output, ViewFeatureLevel, Inputs);

		ParentExtension->CollectCustomRenderTarget(ViewId, GraphBuilder.ConvertToExternalTexture(Output));
	}

private:

	FCompositeCoreSceneViewExtension* ParentExtension;
	const uint32 ViewId;
	const ERHIFeatureLevel::Type ViewFeatureLevel;
	const UE::CompositeCore::Private::FDilateInputs Inputs;
};

FCompositeCoreSceneViewExtension::FCompositeCoreSceneViewExtension(const FAutoRegister& AutoReg, UWorld* InWorld)
	: FWorldSceneViewExtension(AutoReg, InWorld)
{
}

FCompositeCoreSceneViewExtension::~FCompositeCoreSceneViewExtension() = default;

void FCompositeCoreSceneViewExtension::RegisterPrimitives_GameThread(const TArray<UPrimitiveComponent*>& InPrimitiveComponents)
{
	check(IsInGameThread());

	for (UPrimitiveComponent* InPrimitiveComponent : InPrimitiveComponents)
	{
		if (!IsValid(InPrimitiveComponent))
		{
			continue;
		}

		if (!CompositePrimitives.Contains(InPrimitiveComponent))
		{
			CompositePrimitives.Add(InPrimitiveComponent);
		}

		// The SetHoldout() function makes changes only if the HoldoutState value differs from the current value.
		InPrimitiveComponent->SetHoldout(true);
	}
}

void FCompositeCoreSceneViewExtension::UnregisterPrimitives_GameThread(const TArray<UPrimitiveComponent*>& InPrimitiveComponents)
{
	check(IsInGameThread());

	for (UPrimitiveComponent* InPrimitiveComponent : InPrimitiveComponents)
	{
		if (!IsValid(InPrimitiveComponent))
		{
			continue;
		}

		if (CompositePrimitives.Contains(InPrimitiveComponent))
		{
			CompositePrimitives.Remove(InPrimitiveComponent);
		}

		// The SetHoldout() function makes changes only if the HoldoutState value differs from the current value.
		InPrimitiveComponent->SetHoldout(false);
	}
}

void FCompositeCoreSceneViewExtension::SetPostRenderWork_GameThread(UE::CompositeCore::FPostRenderWork&& InWork)
{
	ENQUEUE_RENDER_COMMAND(CopyCompositeCoreRenderWork)(
		[InWork = MoveTemp(InWork), WeakThis = AsWeak()](FRHICommandListImmediate& RHICmdList) mutable
		{
			TSharedPtr<FCompositeCoreSceneViewExtension> SVE = StaticCastSharedPtr<FCompositeCoreSceneViewExtension>(WeakThis.Pin());
			if (SVE.IsValid())
			{
				SVE->ExternalInputs_RenderThread.Reset();

				for (int32 Index=0; Index < InWork.ExternalInputs.Num(); ++Index)
				{
					const UE::CompositeCore::FExternalTexture& ExternalInput = InWork.ExternalInputs[Index];
					TStrongObjectPtr<UTexture> ExternalTexture = ExternalInput.Texture.Pin();
					
					if (ExternalTexture.IsValid())
					{
						if (FTextureResource* TexResource = ExternalTexture->GetResource())
						{
							const int32 MapIndex = UE::CompositeCore::EXTERNAL_RANGE_START_ID + Index;

							UE::CompositeCore::FExternalRenderTarget& Target = SVE->ExternalInputs_RenderThread.Add(MapIndex);
							Target.RenderTarget = CreateRenderTarget(TexResource->GetTextureRHI(), TEXT("CompositeExternalInput"));
							Target.Metadata = ExternalInput.Metadata;
						}
					}
				}

				SVE->PostRenderWork_RenderThread = MoveTemp(InWork);
			}
		});
}

void FCompositeCoreSceneViewExtension::ResetPostRenderWork_GameThread()
{
	ENQUEUE_RENDER_COMMAND(CopyCompositeCoreRenderWork)(
		[WeakThis = AsWeak()](FRHICommandListImmediate& RHICmdList)
		{
			TSharedPtr<FCompositeCoreSceneViewExtension> SVE = StaticCastSharedPtr<FCompositeCoreSceneViewExtension>(WeakThis.Pin());
			if (SVE.IsValid())
			{
				SVE->PostRenderWork_RenderThread.Reset();
				SVE->ExternalInputs_RenderThread.Reset();
			}
		});
}

void FCompositeCoreSceneViewExtension::SetBuiltInRenderPassOptions_GameThread(const UE::CompositeCore::FBuiltInRenderPassOptions& InOptions)
{
	BuiltInRenderPassOptions = InOptions;
}

void FCompositeCoreSceneViewExtension::ResetBuiltInRenderPassOptions_GameThread()
{
	BuiltInRenderPassOptions.Reset();
}

/* Called by the custom render pass to store its view render target for this frame. */

void FCompositeCoreSceneViewExtension::CollectCustomRenderTarget(uint32 InViewId, const TRefCountPtr<IPooledRenderTarget>& InRenderTarget)
{
	CustomRenderTargetPerView_RenderThread.Add(InViewId, InRenderTarget);
}

bool FCompositeCoreSceneViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	bool bIsActive = FWorldSceneViewExtension::IsActiveThisFrame_Internal(Context);
	bIsActive &= !CompositePrimitives.IsEmpty();
	bIsActive &= !IsHDREnabled();

	return bIsActive;
}


//~ Begin ISceneViewExtension Interface

int32 FCompositeCoreSceneViewExtension::GetPriority() const
{
	return GetDefault<UCompositeCorePluginSettings>()->SceneViewExtensionPriority;
}

const UE::CompositeCore::FPostRenderWork& FCompositeCoreSceneViewExtension::GetRenderWork() const
{
	if (PostRenderWork_RenderThread.IsSet())
	{
		return PostRenderWork_RenderThread.GetValue();
	}
	else
	{
		return UE::CompositeCore::FPostRenderWork::GetDefault();
	}
}

void FCompositeCoreSceneViewExtension::SetupViewFamily(FSceneViewFamily& InViewFamily)
{
	// Cleanup invalid primitives.
	for (auto Iter = CompositePrimitives.CreateIterator(); Iter; ++Iter)
	{
		if (!Iter->IsValid())
		{
			Iter.RemoveCurrent();
		}
	}
}

void FCompositeCoreSceneViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
}

void FCompositeCoreSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
	// Disable custom render pass for view families without primitive alpha holdout support.
	if (InViewFamily.EngineShowFlags.AllowPrimitiveAlphaHoldout == 0)
	{
		return;
	}

	const TWeakObjectPtr<UWorld> WorldPtr = GetWorld();
	check(WorldPtr.IsValid());

	for (int32 ViewIndex = 0; ViewIndex < InViewFamily.Views.Num(); ++ViewIndex)
	{
		const FSceneView& InView = *InViewFamily.Views[ViewIndex];

		TSet<FPrimitiveComponentId> CompositeCorePrimitiveIds;
		for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : CompositePrimitives)
		{
			TStrongObjectPtr<UPrimitiveComponent> Primitive = PrimitivePtr.Pin();
			// Collect only those primitives that use the bHoldout flag
			// The user can directly change this flag outside of this VE.
			if (Primitive.IsValid() && Primitive->bHoldout)
			{
				const FPrimitiveComponentId PrimId = Primitive->GetPrimitiveSceneId();

				if (InView.ShowOnlyPrimitives.IsSet())
				{
					if (InView.ShowOnlyPrimitives.GetValue().Contains(PrimId))
					{
						CompositeCorePrimitiveIds.Add(Primitive->GetPrimitiveSceneId());
					}
				}
				else if (!InView.HiddenPrimitives.Contains(PrimId))
				{
					CompositeCorePrimitiveIds.Add(Primitive->GetPrimitiveSceneId());
				}
			}
		}

		if (CompositeCorePrimitiveIds.IsEmpty())
		{
			return;
		}

		UE::CompositeCore::FBuiltInRenderPassOptions RenderPassOptions = BuiltInRenderPassOptions.IsSet() ? *BuiltInRenderPassOptions : UE::CompositeCore::FBuiltInRenderPassOptions{};

		// Create a new custom render pass to render the composite primitive(s)
		FCompositeCoreCustomRenderPass* CustomRenderPass = new FCompositeCoreCustomRenderPass(
			InView.UnscaledViewRect.Size(),
			this,
			InView,
			RenderPassOptions
		);

		FSceneInterface::FCustomRenderPassRendererInput PassInput{};
		PassInput.EngineShowFlags = InViewFamily.EngineShowFlags;
		PassInput.EngineShowFlags.DisableFeaturesForUnlit();
		PassInput.EngineShowFlags.SetTranslucency(true);
		PassInput.EngineShowFlags.SetUnlitViewmode(RenderPassOptions.bEnableUnlitViewmode);
		PassInput.EngineShowFlags.SetAllowPrimitiveAlphaHoldout(false);
		if (RenderPassOptions.ViewUserFlagsOverride.IsSet())
		{
			PassInput.bOverridesPostVolumeUserFlags = true;
			PassInput.PostVolumeUserFlags = RenderPassOptions.ViewUserFlagsOverride.GetValue();
		}
		// Note: Incoming view location is invalid for scene captures
		PassInput.ViewLocation = InView.ViewMatrices.GetViewOrigin();
		PassInput.ViewRotationMatrix = InView.ViewMatrices.GetViewMatrix().RemoveTranslation();
		PassInput.ViewRotationMatrix.RemoveScaling();

		// Note: Projection matrix here is without jitter, GetProjectionNoAAMatrix() is invalid (not yet available).
		PassInput.ProjectionMatrix = InView.ViewMatrices.GetProjectionMatrix();
		PassInput.ViewActor = InView.ViewActor;
		PassInput.ShowOnlyPrimitives = CompositeCorePrimitiveIds;
		PassInput.CustomRenderPass = CustomRenderPass;
		PassInput.bIsSceneCapture = true;

		WorldPtr.Get()->Scene->AddCustomRenderPass(&InViewFamily, PassInput);
	}
}

void FCompositeCoreSceneViewExtension::PreRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{
}

void FCompositeCoreSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
}

void FCompositeCoreSceneViewExtension::PrePostProcessPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessingInputs& Inputs)
{
	using namespace UE::CompositeCore;

	TRefCountPtr<IPooledRenderTarget>* CompositeRenderPassPtr = CustomRenderTargetPerView_RenderThread.Find(InView.GetViewKey());

	if ((CompositeRenderPassPtr != nullptr) && CVarCompositeCoreApplyFXAA.GetValueOnRenderThread())
	{
		static FFXAAPassProxy FXAAPassProxy = FFXAAPassProxy(DefaultPassInputDecl);

		// Set the composite render target as the pass input
		FPassInputArray PassInputs;
		FPassInput& PassInput = PassInputs.GetArray().AddDefaulted_GetRef();
		PassInput.Texture = FScreenPassTexture{ GraphBuilder.RegisterExternalTexture(*CompositeRenderPassPtr) };

		// Apply FXAA, with additional fwd/inv display transform passes
		const FPassOutput Output = FXAAPassProxy.Add(GraphBuilder, InView, PassInputs, {});
		const FScreenPassTexture& ScreenTexOutput = Output.Resource.Texture;

		// Extract the result back into the composite render target
		*CompositeRenderPassPtr = GraphBuilder.ConvertToExternalTexture(ScreenTexOutput.Texture);
	}
}

void FCompositeCoreSceneViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass PassId, const FSceneView& InView, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	if (!IsActiveForView(InView))
	{
		return;
	}

	if (GetRenderWork().FramePasses.Contains(PassId))
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FCompositeCoreSceneViewExtension::PostProcessWork_RenderThread, PassId));
	}
}

bool FCompositeCoreSceneViewExtension::IsActiveForView(const FSceneView& InView) const
{
	bool bIsActive = true;
	bIsActive &= static_cast<bool>(InView.Family->EngineShowFlags.AllowPrimitiveAlphaHoldout);
	bIsActive &= CustomRenderTargetPerView_RenderThread.Contains(InView.GetViewKey()) || !ExternalInputs_RenderThread.IsEmpty();

	return bIsActive;
}

TSortedMap<int32, UE::CompositeCore::FPassInput> FCompositeCoreSceneViewExtension::CreateExternalTextureMap(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessMaterialInputs& Inputs) const
{
	using namespace UE::CompositeCore;

	TSortedMap<int32, FPassInput> InputOverrides;
	InputOverrides.Reserve(2 + ExternalInputs_RenderThread.Num());

	{
		FPassInput& Resource = InputOverrides.Add(BUILT_IN_EMPTY_ID);
		Resource.Texture = FScreenPassTexture{ GSystemTextures.GetBlackDummy(GraphBuilder) };
	}

	const TRefCountPtr<IPooledRenderTarget>* CompositeRenderPassPtr = CustomRenderTargetPerView_RenderThread.Find(InView.GetViewKey());
	if (CompositeRenderPassPtr != nullptr)
	{
		FPassInput& Resource = InputOverrides.Add(BUILT_IN_CRP_ID);
		Resource.Texture = FScreenPassTexture{ GraphBuilder.RegisterExternalTexture(*CompositeRenderPassPtr) };
		Resource.Metadata.bInvertedAlpha = true;
	}

	for (const auto& Pair : ExternalInputs_RenderThread)
	{
		FPassInput& Resource = InputOverrides.Add(Pair.Key);
		Resource.Texture = FScreenPassTexture{ GraphBuilder.RegisterExternalTexture(Pair.Value.RenderTarget) };
		Resource.Metadata = Pair.Value.Metadata;
	}

	return InputOverrides;
}

void FCompositeCoreSceneViewExtension::UpdateNextPassInputs(
	FRDGBuilder& GraphBuilder,
	const UE::CompositeCore::FPassOutput& InOutput,
	const int32 BindingIndex,
	UE::CompositeCore::FPassInputArray& InOutInputs,
	FExternalTextureMap& InOutExternalTextures
)
{
	using namespace UE::CompositeCore;

	if (InOutput.Override.IsSet())
	{
		const ResourceId& ExternalId = InOutput.Override.GetValue();

		FPassInput* ExternalTexture = InOutExternalTextures.Find(ExternalId);
		if (ensureMsgf(ExternalTexture != nullptr, TEXT("Unexpected missing external texture override as output.")))
		{
			*ExternalTexture = InOutput.Resource;
		}
	}
	else
	{
		// Update input binding for the next pass(es)
		InOutInputs[BindingIndex] = InOutput.Resource;
	}
}

bool FCompositeCoreSceneViewExtension::ApplyPasses_Recursive(
	FRDGBuilder& GraphBuilder,
	const FSceneView& InView,
	const UE::CompositeCore::FPassInputArray& Inputs,
	const UE::CompositeCore::FPassInputArray& OriginalInputs,
	UE::CompositeCore::FPassContext& PassContext,
	const TArray<const FCompositeCorePassProxy*> InPasses,
	FExternalTextureMap& ExternalTextures,
	int32 RecursionLevel,
	UE::CompositeCore::FPassOutput& Output)
{
	using namespace UE::CompositeCore;

	if (InPasses.IsEmpty())
	{
		return false;
	}

	// Default pass inputs
	FPassInputArray BasePassInputs = Inputs;
	
	// Iterate over all passes
	for (int32 PassIndex = 0; PassIndex < InPasses.Num(); ++PassIndex)
	{
		const FCompositeCorePassProxy* Pass = InPasses[PassIndex];
		
		// Output override only applies to the (base) last pass
		const bool bIsLastPass = (PassIndex == InPasses.Num() - 1) && (RecursionLevel == 0);

		// Update inputs for the current pass
		FPassInputArray PassInputs = BasePassInputs;

		// Iterate over declared pass inputs
		for (int32 InputIndex = 0; InputIndex < Pass->GetNumDeclaredInputs(); ++InputIndex)
		{
			const TArray<const FCompositeCorePassProxy*>* SubPasses = Pass->GetSubPasses(InputIndex);
			if (SubPasses)
			{
				// Recursively apply sub-passes per input index, automatically overriding inputs.
				UE::CompositeCore::FPassOutput SubPassOutput;
				if (ApplyPasses_Recursive(GraphBuilder, InView, PassInputs, OriginalInputs, PassContext, *SubPasses, ExternalTextures, RecursionLevel + 1, SubPassOutput))
				{
					UpdateNextPassInputs(GraphBuilder, SubPassOutput, InputIndex, PassInputs, ExternalTextures);
				}
			}
			else
			{
				// No sub-pass input override, fetch the regular input index
				const FPassInputDecl& DeclaredInput = Pass->GetDeclaredInput(InputIndex);

				// If an internal texture resource is expected, connect the current input index with either the original or current bass pass input.
				if (DeclaredInput.IsType<FPassInternalResourceDesc>())
				{
					const FPassInternalResourceDesc& Desc = DeclaredInput.Get<FPassInternalResourceDesc>();
					const int32 DeclaredInputIndex = Desc.Index;

					if (Desc.bOriginalCopyBeforePasses)
					{
						if (ensureMsgf(OriginalInputs.IsValidIndex(DeclaredInputIndex), TEXT("Invalid internal input: %d"), DeclaredInputIndex))
						{
							PassInputs[InputIndex] = OriginalInputs[DeclaredInputIndex];
						}
					}
					else
					{
						if (ensureMsgf(BasePassInputs.IsValidIndex(DeclaredInputIndex), TEXT("Invalid internal input: %d"), DeclaredInputIndex))
						{
							PassInputs[InputIndex] = BasePassInputs[DeclaredInputIndex];
						}
					}
				}
				// If an external texture resource is expected, connect the current input index with the resolved identifier
				else if (DeclaredInput.IsType<FPassExternalResourceDesc>())
				{
					const FPassExternalResourceDesc& Desc = DeclaredInput.Get<FPassExternalResourceDesc>();
					const ResourceId DeclaredExternalId = Desc.Id;

					const FPassInput* SliceInput = ExternalTextures.Find(DeclaredExternalId);
					if (ensureMsgf(SliceInput, TEXT("Invalid external input: %d"), DeclaredExternalId))
					{
						PassInputs[InputIndex] = *SliceInput;
					}
				}
				else
				{
					checkNoEntry();
				}
			}
		}

		if (bIsLastPass)
		{
			// Invert alpha when writing back to scene color
			PassContext.bOutputSceneColor = true;
		}
		else
		{
			// Only apply the output override on the last pass
			PassInputs.OverrideOutput = FScreenPassRenderTarget{};
		}

		// Register pass and update output
		Output = Pass->Add(GraphBuilder, InView, PassInputs, PassContext);

		UpdateNextPassInputs(GraphBuilder, Output, 0, BasePassInputs, ExternalTextures);
	}

	return true;
}

FScreenPassTexture FCompositeCoreSceneViewExtension::PostProcessWork_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& InView, const FPostProcessMaterialInputs& Inputs, ISceneViewExtension::EPostProcessingPass InLocation)
{
	using namespace UE::CompositeCore;

	FExternalTextureMap ExternalTextures = CreateExternalTextureMap(GraphBuilder, InView, Inputs);
	const TArray<const FCompositeCorePassProxy*>& Passes = GetRenderWork().FramePasses[InLocation];

	FPassContext PassContext;
	PassContext.SceneTextures = Inputs.SceneTextures;
	PassContext.OutputViewRect = Inputs.GetInput(EPostProcessMaterialInput::SceneColor).ViewRect;
	PassContext.Location = InLocation;
	PassContext.bOutputSceneColor = false;

	FPassInputArray ResolvedInputs(GraphBuilder, InView, Inputs, InLocation);
	FPassOutput Output;
	
	constexpr int32 RecursionLevel = 0;
	ApplyPasses_Recursive(GraphBuilder, InView, ResolvedInputs, ResolvedInputs, PassContext, Passes, ExternalTextures, RecursionLevel, Output);

	if (Output.Resource.Texture.IsValid() && !Output.Override.IsSet())
	{
		return MoveTemp(Output.Resource.Texture);
	}
	else
	{
		return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder);
	}
}

void FCompositeCoreSceneViewExtension::PostRenderViewFamily_RenderThread(FRDGBuilder& GraphBuilder, FSceneViewFamily& InViewFamily)
{

}

void FCompositeCoreSceneViewExtension::PostRenderView_RenderThread(FRDGBuilder& GraphBuilder, FSceneView& InView)
{
	CustomRenderTargetPerView_RenderThread.Remove(InView.GetViewKey());
}

