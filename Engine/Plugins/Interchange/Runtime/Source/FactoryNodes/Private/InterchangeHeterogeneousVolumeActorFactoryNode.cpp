// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeHeterogeneousVolumeActorFactoryNode.h"

#include "Components/HeterogeneousVolumeComponent.h"

UClass* UInterchangeHeterogeneousVolumeActorFactoryNode::GetObjectClass() const
{
#if WITH_ENGINE
	return AHeterogeneousVolume::StaticClass();
#else
	return nullptr;
#endif
}

bool UInterchangeHeterogeneousVolumeActorFactoryNode::GetCustomVolumetricMaterialUid(FString& AttributeValue) const
{
	IMPLEMENT_NODE_ATTRIBUTE_GETTER(MaterialDependency, FString)
}

bool UInterchangeHeterogeneousVolumeActorFactoryNode::SetCustomVolumetricMaterialUid(const FString& AttributeValue)
{
	IMPLEMENT_NODE_ATTRIBUTE_SETTER_NODELEGATE(MaterialDependency, FString)
}
