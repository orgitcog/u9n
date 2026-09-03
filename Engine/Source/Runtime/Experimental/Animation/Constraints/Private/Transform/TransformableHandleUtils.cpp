// Copyright Epic Games, Inc. All Rights Reserved.

#include "Transform/TransformableHandleUtils.h"

#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"

namespace TransformableHandleUtils
{

static int32 SkeletalMeshTickingMode = 2;
static bool	bNewEvaluation = false;

static FAutoConsoleVariableRef CVarSkeletalMeshTickingMode(
	TEXT("Constraints.SkeletalMesh.TickingMode"),
	SkeletalMeshTickingMode,
	TEXT( "Constraint skeletal mesh ticking mode ([0, 2] - default: 2):\n" )
		TEXT( "0 - do not tick any related skeletal mesh\n" )
		TEXT( "1 - only tick the constrained skeletal mesh\n")
		TEXT( "2 - tick all the skeletal meshes attached to the constrained actor\n"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
	{
		SkeletalMeshTickingMode = FMath::Clamp(SkeletalMeshTickingMode, 0, 2);
	})
);

static FAutoConsoleVariableRef CVarNewEvaluation(
	TEXT("Constraints.NewEvaluation"),
	bNewEvaluation,
	TEXT("Set constraints new evaluation scheme."),
	FConsoleVariableDelegate::CreateLambda([](IConsoleVariable*)
	{
		UE_LOG(LogTemp, Warning, TEXT("Constraints' new evaluation scheme %s"), bNewEvaluation ? TEXT("enabled.") : TEXT("disabled."));
		SkeletalMeshTickingMode = bNewEvaluation ? 0 : 2;
	})
);

bool SkipTicking()
{
	return bNewEvaluation;
}

void TickDependantComponents(USceneComponent* InComponent)
{
	if (SkeletalMeshTickingMode <= 0 || bNewEvaluation)
	{
		return;
	}
	
	if (!InComponent)
	{
		return;
	}

	if (SkeletalMeshTickingMode == 1)
	{
		return TickSkeletalMeshComponent(Cast<USkeletalMeshComponent>(InComponent));
	}

	static constexpr bool bIncludeFromChildActors = true;

	const AActor* Parent = InComponent->GetOwner();
	while (Parent)
	{
		Parent->ForEachComponent<USkeletalMeshComponent>(bIncludeFromChildActors, &TickSkeletalMeshComponent);
		Parent = Parent->GetAttachParentActor();
	}
}

void TickSkeletalMeshComponent(USkeletalMeshComponent* InSkeletalMeshComponent)
{
	if (!InSkeletalMeshComponent || bNewEvaluation)
	{
		return;
	}

	// avoid re-entrant animation evaluation
	if (InSkeletalMeshComponent->IsPostEvaluatingAnimation())
	{
		return;
	}

	static constexpr float DeltaTime = 0.03f;
	static constexpr bool bNeedsValidRootMotion = false;
	
	InSkeletalMeshComponent->TickAnimation(DeltaTime, bNeedsValidRootMotion);
	InSkeletalMeshComponent->RefreshBoneTransforms();
	InSkeletalMeshComponent->RefreshFollowerComponents();
	InSkeletalMeshComponent->UpdateComponentToWorld();
	InSkeletalMeshComponent->FinalizeBoneTransform();
	InSkeletalMeshComponent->MarkRenderTransformDirty();
	InSkeletalMeshComponent->MarkRenderDynamicDataDirty();
}
	
void MarkComponentForEvaluation(const USceneComponent* InSceneComponent)
{
	if (const USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InSceneComponent))
	{
		UE::Anim::FAnimationEvaluationCache::Get().MarkForEvaluation(SkeletalMeshComponent);
	}
}

const UE::Anim::FAnimationEvaluator& EvaluateComponent(USceneComponent* InSceneComponent)
{
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InSceneComponent))
	{
		return UE::Anim::FAnimationEvaluationCache::Get().GetEvaluator(SkeletalMeshComponent);
	}

	static const UE::Anim::FAnimationEvaluator Invalid(nullptr);
	return Invalid;
}

CONSTRAINTS_API const UE::Anim::FAnimationEvaluator& EvaluateComponent(USceneComponent* InSceneComponent, const UE::Anim::FAnimationEvaluationTask& InTask)
{
	if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(InSceneComponent))
	{
		return UE::Anim::FAnimationEvaluationCache::Get().GetEvaluator(SkeletalMeshComponent, InTask);
	}

	static const UE::Anim::FAnimationEvaluator Invalid(nullptr);
	return Invalid;
}

}