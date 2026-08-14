// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transform/AnimationEvaluation.h"

#include "Animation/AnimInstance.h"
#include "Components/SceneComponent.h"
#include "CoreGlobals.h"
#include "ConstraintsManager.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"

namespace UE::Anim
{

static bool	bLogNewEvaluation = false;
static FAutoConsoleVariableRef CVarLogNewEvaluation(
	TEXT("Constraints.LogNewEvaluation"),
	bLogNewEvaluation,
	TEXT("Log new constraints' evaluation scheme.")
);

	
FString GetComponentName(const USceneComponent* InSceneComponent)
{
#if WITH_EDITOR
	const AActor* Actor = InSceneComponent->GetOwner();
	return Actor ? Actor->GetActorLabel() : InSceneComponent->GetName();
#else
	return InSceneComponent->GetName();
#endif
}

FAnimationEvaluator::FAnimationEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent)
	: SkeletalMeshComponent(InSkeletalMeshComponent)
{
	if (InSkeletalMeshComponent)
	{
		const FOnBoneTransformsFinalizedMultiCast::FDelegate OnBoneTransformsFinalizedDelegate =
			FOnBoneTransformsFinalizedMultiCast::FDelegate::CreateRaw(this, &FAnimationEvaluator::BoneTransformsFinalized);
		OnBoneTransformsFinalizedHandle = InSkeletalMeshComponent->RegisterOnBoneTransformsFinalizedDelegate(OnBoneTransformsFinalizedDelegate);
		
		if (bLogNewEvaluation)
		{
			UE_LOG(LogTemp, Warning, TEXT("[%p] FAnimationEvaluator register finalize bone from %s"), this, *GetComponentName(InSkeletalMeshComponent));
		}
	}
}

FAnimationEvaluator::~FAnimationEvaluator()
{
	if (OnBoneTransformsFinalizedHandle.IsValid())
	{
		constexpr bool bEvenIfPendingKill = true;
		if (USkeletalMeshComponent* RawSkeletalMeshComponent = SkeletalMeshComponent.Get(bEvenIfPendingKill))
		{
			RawSkeletalMeshComponent->UnregisterOnBoneTransformsFinalizedDelegate(OnBoneTransformsFinalizedHandle);
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("[%p]  FAnimationEvaluator unregister finalize bone from %s"), this, *GetComponentName(RawSkeletalMeshComponent));
			}
		}
		OnBoneTransformsFinalizedHandle.Reset();
	}

	PostEvaluationTasks.Reset();
}
	
void FAnimationEvaluator::Update(const bool bRefreshBoneTransforms)
{
	Context.Clear();
	
	if (bRefreshBoneTransforms)
	{
		RefreshBoneTransforms();
	}
}

bool FAnimationEvaluator::IsValid() const
{
	return SkeletalMeshComponent.IsValid() && Context.SkeletalMesh && !Context.ComponentSpaceTransforms.IsEmpty();
}

FTransform FAnimationEvaluator::GetGlobalTransform(const FName InSocketName) const
{
	USkeletalMeshComponent* RawSkeletalMeshComponent = SkeletalMeshComponent.Get();
	if (!RawSkeletalMeshComponent)
	{
		return FTransform::Identity;
	}
	
	if (IsValid() && InSocketName != NAME_None)
	{
		// TODO socket
		const int32 BoneIndex = Context.SkeletalMesh->GetRefSkeleton().FindBoneIndex(InSocketName);
		if (ensure(BoneIndex != INDEX_NONE && Context.ComponentSpaceTransforms.IsValidIndex(BoneIndex)))
		{
			return Context.ComponentSpaceTransforms[BoneIndex] * RawSkeletalMeshComponent->GetComponentTransform();
		}
	}
	
	return RawSkeletalMeshComponent->GetSocketTransform(InSocketName);
}

void FAnimationEvaluator::AddPostEvaluationTask(const FAnimationEvaluationTask& InTask)
{
	if (USkeletalMeshComponent* RawSkeletalMeshComponent = SkeletalMeshComponent.Get())
	{
		if (InTask.IsValid(RawSkeletalMeshComponent))
		{
			PostEvaluationTasks.FindOrAdd(InTask.Guid, InTask);
		}
	}
}
	
void FAnimationEvaluator::UpdateContext()
{
	if (!SkeletalMeshComponent.IsValid())
	{
		Context.Clear();
		return;
	}

	const bool bEvaluatePostProcessInstance = SkeletalMeshComponent->ShouldEvaluatePostProcessInstance();
	
	Context.SkeletalMesh = SkeletalMeshComponent->GetSkeletalMeshAsset();
	Context.AnimInstance = SkeletalMeshComponent->AnimScriptInstance;
	Context.PostProcessAnimInstance = bEvaluatePostProcessInstance? ToRawPtr(SkeletalMeshComponent->PostProcessAnimInstance): nullptr;
	Context.bDoEvaluation = true;
	Context.bDoInterpolation = false;
	Context.bDuplicateToCacheBones = false;
	Context.bDuplicateToCacheCurve = false;
	Context.bDuplicateToCachedAttributes = false;
	Context.bForceRefPose = false;
}
	
void FAnimationEvaluator::RefreshBoneTransforms()
{
	// see. USkeletalMeshComponent::RefreshBoneTransforms
	if (!SkeletalMeshComponent.IsValid())
	{
		return;
	}

	// avoid re-entrant animation evaluation
	if (SkeletalMeshComponent->IsPostEvaluatingAnimation())
	{
		return;
	}

	constexpr bool bBlockOnTask = true, bPerformPostAnimEvaluation = true;
	if (SkeletalMeshComponent->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation))
	{
		return;
	}

	// disable UpdateRateOptimizations
	using UROGuard = TGuardValue_Bitfield_Cleanup<TFunction<void()>>;
	UROGuard Guard([URORefValue = SkeletalMeshComponent->bEnableUpdateRateOptimizations, this]()
	{
		SkeletalMeshComponent->bEnableUpdateRateOptimizations = URORefValue;
	});
	SkeletalMeshComponent->bEnableUpdateRateOptimizations = false;

	// update context & evaluate
	UpdateContext();
	EvaluateAnimation();
}

void FAnimationEvaluator::EvaluateAnimation()
{
	USkeletalMeshComponent* RawSkeletalMeshComponent = SkeletalMeshComponent.Get();
	if (!RawSkeletalMeshComponent)
	{
		return;
	}
	
	// see USkeletalMeshComponent:DoInstancePreEvaluation()
	{
		if (Context.AnimInstance)
		{
			Context.AnimInstance->PreEvaluateAnimation();
		}

		if (Context.PostProcessAnimInstance)
		{
			Context.PostProcessAnimInstance->PreEvaluateAnimation();
		}
	}
	
	// call USkeletalMeshComponent::TickAnimation() if needed
	{
		bool bShouldTickAnimation = false;
		
		if (Context.AnimInstance && !Context.AnimInstance->NeedsUpdate())
		{
			bShouldTickAnimation = !Context.AnimInstance->GetUpdateCounter().HasEverBeenUpdated();
		}

		const bool bBPEnabled = !RawSkeletalMeshComponent->GetDisablePostProcessBlueprint() && RawSkeletalMeshComponent->ShouldEvaluatePostProcessAnimBP();
		if (bBPEnabled && Context.PostProcessAnimInstance && !Context.PostProcessAnimInstance->NeedsUpdate())
		{
			bShouldTickAnimation |= !Context.PostProcessAnimInstance->GetUpdateCounter().HasEverBeenUpdated();
		}

		if (bShouldTickAnimation)
		{
			// We bypass TickPose() and call TickAnimation directly, so URO doesn't intercept us.
			static constexpr float DeltaTime = 0.0f;
			static constexpr bool bNeedsValidRootMotion = false;
			RawSkeletalMeshComponent->TickAnimation(DeltaTime, bNeedsValidRootMotion);
		}
	}

	// see USkeletalMeshComponent::DoParallelEvaluationTasks_OnGameThread()
	{
		Context.ComponentSpaceTransforms = RawSkeletalMeshComponent->GetEditableComponentSpaceTransforms();
		Context.CachedComponentSpaceTransforms = RawSkeletalMeshComponent->GetCachedComponentSpaceTransforms();
		Context.BoneSpaceTransforms = RawSkeletalMeshComponent->GetBoneSpaceTransforms();
		Context.Curve = RawSkeletalMeshComponent->GetAnimCurves();
		Context.RootBoneTranslation = RawSkeletalMeshComponent->RootBoneTranslation;

		// note that don't use curves and custom attributes here 
	}
		
	// see USkeletalMeshComponent::ParallelAnimationEvaluation()
	{
		RawSkeletalMeshComponent->PerformAnimationProcessing(
		   Context.SkeletalMesh,
		   Context.AnimInstance,
		   Context.bDoEvaluation,
		   Context.bForceRefPose,
		   Context.ComponentSpaceTransforms,
		   Context.BoneSpaceTransforms,
		   Context.RootBoneTranslation,
		   Context.Curve,
		   Context.CustomAttributes);
	}

	// call post evaluation tasks if any
	for (auto PostEvaluationTaskIt = PostEvaluationTasks.CreateIterator(); PostEvaluationTaskIt; ++PostEvaluationTaskIt)
	{
		const FAnimationEvaluationTask& PostEvaluationTask = PostEvaluationTaskIt.Value();
		if (!PostEvaluationTask.IsValid(RawSkeletalMeshComponent))
		{
			PostEvaluationTaskIt.RemoveCurrent();
		}
		else
		{
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("EvaluateTask %s"), *PostEvaluationTask.Guid.ToString() );
			}
			PostEvaluationTask.PostEvaluationFunction();
		}
	}
}

void FAnimationEvaluator::BoneTransformsFinalized()
{
	if (!IsValid())
	{
		return;
	}

	USkeletalMeshComponent* RawSkeletalMeshComponent = SkeletalMeshComponent.Get();

	bool bFinalized = false;

	// NOTE FAnimationEvaluator could store a TBitArray of bones being requested and only check them instead of checking all of them 
	const TArrayView<const FTransform> BonesTransforms = RawSkeletalMeshComponent->GetComponentSpaceTransforms();
	if (Context.ComponentSpaceTransforms.Num() == BonesTransforms.Num())
	{
		for (int32 Index = 0; Index < BonesTransforms.Num(); ++Index)
		{
			FTransform& ContextBoneTransform = Context.ComponentSpaceTransforms[Index];
			if (!ContextBoneTransform.Equals(BonesTransforms[Index]))
			{
				ContextBoneTransform = BonesTransforms[Index];
				bFinalized = true;
			}
		}
	}

	// call post evaluation tasks if any
	for (auto PostEvaluationTaskIt = PostEvaluationTasks.CreateIterator(); PostEvaluationTaskIt; ++PostEvaluationTaskIt)
	{
		const FAnimationEvaluationTask& PostEvaluationTask = PostEvaluationTaskIt.Value();
		if (!PostEvaluationTask.IsValid(RawSkeletalMeshComponent))
		{
			PostEvaluationTaskIt.RemoveCurrent();
		}
		else
		{
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("FinalizeTask %s"), *PostEvaluationTask.Guid.ToString() );
			}
			PostEvaluationTask.PostEvaluationFunction();
		}
	}
	
	if (bLogNewEvaluation && bFinalized)
	{
		UE_LOG(LogTemp, Warning, TEXT("Finalized %s's evaluator."), *GetComponentName(SkeletalMeshComponent.Get()));
	}
}
	
FAnimationEvaluationCache& FAnimationEvaluationCache::Get()
{
	static FAnimationEvaluationCache AnimationEvaluationCache;
	if (!AnimationEvaluationCache.ConstraintsNotificationHandle.IsValid())
	{
		AnimationEvaluationCache.RegisterNotifications();
	}
	return AnimationEvaluationCache;
}

FAnimationEvaluationCache::~FAnimationEvaluationCache()
{
	UnregisterNotifications();
	PerSkeletalMeshEvaluator.Reset();
}
	
void FAnimationEvaluationCache::MarkForEvaluation(const USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (!InSkeletalMeshComponent)
	{
		return;
	}

	if (FAnimationEvaluator* AnimationEvaluator = PerSkeletalMeshEvaluator.Find(InSkeletalMeshComponent))
	{
		if (AnimationEvaluator->IsValid())
		{
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("Marked %s's evaluator for evaluation."), *GetComponentName(InSkeletalMeshComponent));
			}
			constexpr bool bDoNotRefreshBones = false;
			AnimationEvaluator->Update(bDoNotRefreshBones);
		}
	}
}
	
const FAnimationEvaluator& FAnimationEvaluationCache::GetEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (!InSkeletalMeshComponent)
	{
		static const FAnimationEvaluator Invalid(nullptr);
		return Invalid;
	}
	
	constexpr bool bRefreshBones = true;

	if (FAnimationEvaluator* Found = PerSkeletalMeshEvaluator.Find(InSkeletalMeshComponent))
	{
		if (!Found->IsValid())
		{
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("Update %s's evaluator for evaluation."), *GetComponentName(InSkeletalMeshComponent));
			}
			Found->Update(bRefreshBones);
		}
		return *Found;
	}
	
	FAnimationEvaluator& NewEvaluator = PerSkeletalMeshEvaluator.Emplace(InSkeletalMeshComponent, InSkeletalMeshComponent);
	NewEvaluator.Update(bRefreshBones);

	if (bLogNewEvaluation)
	{
		UE_LOG(LogTemp, Warning, TEXT("Create new evaluator for %s."), *GetComponentName(InSkeletalMeshComponent));
	}
	
	return NewEvaluator;
}

const FAnimationEvaluator& FAnimationEvaluationCache::GetEvaluator(USkeletalMeshComponent* InSkeletalMeshComponent,
	const FAnimationEvaluationTask& InTask)
{
	if (!InSkeletalMeshComponent)
	{
		static const FAnimationEvaluator Invalid(nullptr);
		return Invalid;
	}
	
	constexpr bool bRefreshBones = true;

	if (FAnimationEvaluator* Found = PerSkeletalMeshEvaluator.Find(InSkeletalMeshComponent))
	{
		Found->AddPostEvaluationTask(InTask);
		if (!Found->IsValid())
		{
			if (bLogNewEvaluation)
			{
				UE_LOG(LogTemp, Warning, TEXT("Update %s's evaluator for evaluation."), *GetComponentName(InSkeletalMeshComponent));
			}
			Found->Update(bRefreshBones);
		}
		return *Found;
	}
	
	FAnimationEvaluator& NewEvaluator = PerSkeletalMeshEvaluator.Emplace(InSkeletalMeshComponent, InSkeletalMeshComponent);
	NewEvaluator.AddPostEvaluationTask(InTask);
	NewEvaluator.Update(bRefreshBones);

	if (bLogNewEvaluation)
	{
		UE_LOG(LogTemp, Warning, TEXT("Create new evaluator for %s."), *GetComponentName(InSkeletalMeshComponent));
	}
	
	return NewEvaluator;
}

void FAnimationEvaluationCache::RegisterNotifications()
{
	FConstraintsManagerController& Controller = FConstraintsManagerController::Get(nullptr);
	ConstraintsNotificationHandle =
		Controller.GetNotifyDelegate().AddLambda([this](EConstraintsManagerNotifyType InNotifyType, UObject* InObject)
	{
		if (InNotifyType == EConstraintsManagerNotifyType::GraphUpdated)
		{
			PerSkeletalMeshEvaluator.Reset();
		}
	});
}

void FAnimationEvaluationCache::UnregisterNotifications()
{
	if (ConstraintsNotificationHandle.IsValid())
	{
		FConstraintsManagerController& Controller = FConstraintsManagerController::Get(nullptr);
		Controller.GetNotifyDelegate().Remove(ConstraintsNotificationHandle);
		ConstraintsNotificationHandle.Reset();
	}
}

void MarkComponentForEvaluation(const USceneComponent* InSceneComponent)
{
	if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InSceneComponent))
	{
		FAnimationEvaluationCache::Get().MarkForEvaluation(SkeletalMeshComponent);
	}
}

const FAnimationEvaluator& EvaluateComponent(USceneComponent* InSceneComponent)
{
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InSceneComponent))
	{
		return FAnimationEvaluationCache::Get().GetEvaluator(SkeletalMeshComponent);
	}

	static const FAnimationEvaluator Invalid(nullptr);
	return Invalid;
}

}