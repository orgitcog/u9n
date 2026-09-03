// Copyright Epic Games, Inc. All Rights Reserved.

#include "Module/AnimNextModuleInstanceComponent.h"

void FAnimNextModuleInstanceComponent::Initialize(FAnimNextModuleInstance& InOwnerInstance)
{
	OwnerInstance = &InOwnerInstance;

	OnInitialize();
}

void FAnimNextModuleInstanceComponent::Uninitialize()
{
	OnUninitialize();

	OwnerInstance = nullptr;
}