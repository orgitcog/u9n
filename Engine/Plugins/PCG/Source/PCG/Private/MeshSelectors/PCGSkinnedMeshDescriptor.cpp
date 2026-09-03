// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshSelectors/PCGSkinnedMeshDescriptor.h"
#include "Components/InstancedSkinnedMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGSkinnedMeshDescriptor)

FPCGSoftSkinnedMeshComponentDescriptor::FPCGSoftSkinnedMeshComponentDescriptor()
: FSoftSkinnedMeshComponentDescriptor()
{
	// Override defaults from base class
	ComponentClass = UInstancedSkinnedMeshComponent::StaticClass();
	Mobility = EComponentMobility::Static;
	// GW-TODO: bGenerateOverlapEvents = 0;
	// GW-TODO: BodyInstance.SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
}

FPCGSoftSkinnedMeshComponentDescriptor::FPCGSoftSkinnedMeshComponentDescriptor(const FSkinnedMeshComponentDescriptor& Other)
	: Super(Other)
{}

void FPCGSoftSkinnedMeshComponentDescriptor::InitFrom(const UInstancedSkinnedMeshComponent* Component, bool bInitBodyInstance)
{
	ComponentTags = Component->ComponentTags;
	Super::InitFrom(Component, bInitBodyInstance);
}

void FPCGSoftSkinnedMeshComponentDescriptor::InitComponent(UInstancedSkinnedMeshComponent* ABMComponent) const
{
	ABMComponent->ComponentTags = ComponentTags;
	Super::InitComponent(ABMComponent);
}

int32 FPCGSoftSkinnedMeshComponentDescriptor::GetOrAddBankIndex(const FSoftAnimBankItem& BankItem)
{
	int32 BankIndex = INDEX_NONE;

	for (int32 ExistingBankIndex = 0; ExistingBankIndex < BankItems.Num(); ++ExistingBankIndex)
	{
		if (BankItems[ExistingBankIndex] == BankItem)
		{
			BankIndex = ExistingBankIndex;
			break;
		}
	}

	if (BankIndex == INDEX_NONE)
	{
		BankIndex = BankItems.Num();
		BankItems.Emplace(BankItem);
	}

	return BankIndex;
}
