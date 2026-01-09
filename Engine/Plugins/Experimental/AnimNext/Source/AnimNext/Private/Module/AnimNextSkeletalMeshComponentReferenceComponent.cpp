// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextSkeletalMeshComponentReferenceComponent.h"

#include "Components/SkeletalMeshComponent.h"

FAnimNextSkeletalMeshComponentReferenceComponent::FAnimNextSkeletalMeshComponentReferenceComponent()
{
	ComponentType = USkeletalMeshComponent::StaticClass();
}

USkeletalMeshComponent* FAnimNextSkeletalMeshComponentReferenceComponent::GetComponent() const
{
	return Cast<USkeletalMeshComponent>(Component);
}

void FAnimNextSkeletalMeshComponentReferenceComponent::OnInitialize()
{
	OnInitializeHelper(StaticStruct());
}