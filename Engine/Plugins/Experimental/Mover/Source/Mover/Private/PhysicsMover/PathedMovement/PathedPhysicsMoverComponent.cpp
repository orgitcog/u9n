// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsMover/PathedMovement/PathedPhysicsMoverComponent.h"

#include "Backends/MoverPathedPhysicsLiaison.h"
#include "PhysicsMover/PathedMovement/PathedMovementMode.h"
#include "PhysicsMover/PathedMovement/PathedPhysicsDebugDrawComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PathedPhysicsMoverComponent)

UPathedPhysicsMoverComponent::UPathedPhysicsMoverComponent()
{
	MovementModes.Add(TEXT("Default"), CreateDefaultSubobject<UPathedPhysicsMovementMode>(TEXT("DefaultPath")));
	StartingMovementMode = TEXT("Default");
	
	BackendClass = UMoverPathedPhysicsLiaisonComponent::StaticClass();
	bSupportsKinematicBasedMovement = false;

#if ENABLE_DRAW_DEBUG
	DebugDrawComp = CreateDefaultSubobject<UPathedPhysicsDebugDrawComponent>(TEXT("PathDebugDraw")/*, true*/);
	
	// Since the debug draw comp relies on data in this component, and this component will often need to be re-registered following the construction script,
	// we want registration of the debug draw comp to match (and follow) the registration state of this component
	DebugDrawComp->bAutoRegister = false;
	
	// DebugDrawComp->SetFlags(RF_Transactional);
	// if (GetOwner())
	// {
	// 	DebugDrawComp->SetupAttachment(GetOwner()->GetDefaultAttachComponent());
	// }
#endif
}

void UPathedPhysicsMoverComponent::OnRegister()
{
	Super::OnRegister();

	for (TPair<FName, TObjectPtr<UBaseMovementMode>>& NameModePair : MovementModes)
	{
		if (UPathedPhysicsMovementMode* PathedMode = Cast<UPathedPhysicsMovementMode>(NameModePair.Value))
		{
			PathedMode->InitializePath();
		}
	}
	
#if ENABLE_DRAW_DEBUG
	DebugDrawComp->RegisterComponent();
#endif
}

void UPathedPhysicsMoverComponent::OnUnregister()
{
	Super::OnUnregister();

#if ENABLE_DRAW_DEBUG
	DebugDrawComp->UnregisterComponent();
#endif
}

void UPathedPhysicsMoverComponent::InitializeComponent()
{
	Super::InitializeComponent();

	//@todo DanH: Log error about invalid backend class (better would be to make it impossible to choose the wrong one)
	PathedPhysicsLiaison = Cast<UMoverPathedPhysicsLiaisonComponent>(BackendLiaisonComp.GetObject());

	if (PathedPhysicsLiaison)
	{
		PathedPhysicsLiaison->SetPathOrigin(GetUpdatedComponentTransform());
		PathedPhysicsLiaison->SetIsMoving(bAutoMoveOnSpawn, MovementStartDelay);
		PathedPhysicsLiaison->SetPlaybackBehavior(DefaultPlaybackBehavior);
	}
}

void UPathedPhysicsMoverComponent::UninitializeComponent()
{
	Super::UninitializeComponent();
	PathedPhysicsLiaison = nullptr;
}

#if WITH_EDITOR
void UPathedPhysicsMoverComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

#if ENABLE_DRAW_DEBUG
	// DebugDrawComp->MarkRenderStateDirty();
#endif
}
#endif

bool UPathedPhysicsMoverComponent::IsInReverse() const
{
	return PathedPhysicsLiaison && PathedPhysicsLiaison->IsInReverse();
}

void UPathedPhysicsMoverComponent::SetPlaybackDirection(bool bPlayForward)
{
	if (PathedPhysicsLiaison)
	{
		PathedPhysicsLiaison->SetPlaybackDirection(bPlayForward);
	}
}

bool UPathedPhysicsMoverComponent::IsMoving() const
{
	return PathedPhysicsLiaison && PathedPhysicsLiaison->IsMoving();
}

void UPathedPhysicsMoverComponent::SetIsMoving(bool bShouldMove)
{
	if (PathedPhysicsLiaison)
	{
		PathedPhysicsLiaison->SetIsMoving(bShouldMove);
	}
}

// EPathedPhysicsPlaybackBehavior UPathedPhysicsMoverComponent::GetPlaybackBehavior() const
// {
// 	return PathedPhysicsLiaison ? PathedPhysicsLiaison->GetPlaybackBehavior() : EPathedPhysicsPlaybackBehavior::OneShot;
// }

void UPathedPhysicsMoverComponent::SetDefaultPlaybackBehavior(EPathedPhysicsPlaybackBehavior PlaybackBehavior)
{
	//@todo DanH: Change the default behavior here, but don't necessarily change the behavior if the mode has overridden it
	DefaultPlaybackBehavior = PlaybackBehavior;
}

bool UPathedPhysicsMoverComponent::IsJointEnabled() const
{
	return PathedPhysicsLiaison && PathedPhysicsLiaison->IsJointEnabled();
}

void UPathedPhysicsMoverComponent::SetPathOriginTransform(const FTransform& NewPathOrigin)
{
	if (PathedPhysicsLiaison)
	{
		PathedPhysicsLiaison->SetPathOrigin(NewPathOrigin);
	}
}

const FTransform& UPathedPhysicsMoverComponent::GetPathOriginTransform() const
{
	// The liaison isn't established in editor worlds, where it's safe to assume the actor location is the path origin
	return PathedPhysicsLiaison ? PathedPhysicsLiaison->GetPathOrigin() : GetOwner()->GetActorTransform();
}

void UPathedPhysicsMoverComponent::NotifyIsMovingChanged(bool bIsMoving)
{
	//@todo DanH: Fire an event about it
}
