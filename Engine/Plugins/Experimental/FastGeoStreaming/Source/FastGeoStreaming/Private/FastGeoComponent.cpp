// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastGeoComponent.h"
#include "FastGeoComponentCluster.h"
#include "FastGeoContainer.h"
#include "FastGeoWorldSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "SceneInterface.h"

const FFastGeoElementType FFastGeoComponent::Type(&IFastGeoElement::Type);

FFastGeoComponent::FFastGeoComponent(int32 InComponentIndex, FFastGeoElementType InType)
	: Super(InType)
	, ComponentIndex(InComponentIndex)
	, Owner(nullptr)
{
}

#if WITH_EDITOR
void FFastGeoComponent::InitializeFromComponent(UActorComponent* InComponent)
{
}

UClass* FFastGeoComponent::GetEditorProxyClass() const
{
	return UFastGeoComponentEditorProxy::StaticClass();
}
#endif

FArchive& operator<<(FArchive& Ar, FFastGeoComponent& Component)
{
	Component.Serialize(Ar);
	return Ar;
}

void FFastGeoComponent::SetOwnerComponentCluster(FFastGeoComponentCluster* InOwner)
{
	check(InOwner);
	Owner = InOwner;
}

FFastGeoComponentCluster* FFastGeoComponent::GetOwnerComponentCluster() const
{
	return Owner;
}

UFastGeoContainer* FFastGeoComponent::GetOwnerContainer() const
{
	check(GetOwnerComponentCluster());
	return GetOwnerComponentCluster()->GetOwnerContainer();
}

UWorld* FFastGeoComponent::GetWorld() const
{
	check(GetOwnerContainer());
	check(GetOwnerContainer()->GetLevel());

	return GetOwnerContainer()->GetLevel()->OwningWorld;
}

bool FFastGeoComponent::IsRegistered() const
{
	return GetOwnerContainer()->IsRegistered();
}

FLinearColor FFastGeoComponent::GetDebugColor() const
{
	return UFastGeoWorldSubsystem::IsEnableDebugView() ? FLinearColor::Blue : FLinearColor::White;
}

void FFastGeoComponent::Serialize(FArchive& Ar)
{
	Ar << ComponentIndex;
}

void FFastGeoComponent::OnAsyncCreatePhysicsState()
{
	check(IsCollisionEnabled());
	check(PhysicsStateCreation == EPhysicsStateCreation::NotCreated);
	PhysicsStateCreation = EPhysicsStateCreation::Creating;
}

void FFastGeoComponent::OnAsyncCreatePhysicsStateEnd_GameThread()
{
	check(PhysicsStateCreation == EPhysicsStateCreation::Creating);
	PhysicsStateCreation = EPhysicsStateCreation::Created;
}

void FFastGeoComponent::OnAsyncDestroyPhysicsStateBegin_GameThread()
{
	check(PhysicsStateCreation == EPhysicsStateCreation::Created);
	PhysicsStateCreation = EPhysicsStateCreation::Destroying;
}

void FFastGeoComponent::OnAsyncDestroyPhysicsState()
{
	check(IsCollisionEnabled());
}

void FFastGeoComponent::OnAsyncDestroyPhysicsStateEnd_GameThread()
{
	check(PhysicsStateCreation == EPhysicsStateCreation::Destroying);
	PhysicsStateCreation = EPhysicsStateCreation::NotCreated;
}
