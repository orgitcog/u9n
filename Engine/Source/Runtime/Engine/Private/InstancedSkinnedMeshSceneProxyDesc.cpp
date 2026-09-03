// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstancedSkinnedMeshSceneProxyDesc.h"
#include "Components/InstancedSkinnedMeshComponent.h"

FInstancedSkinnedMeshSceneProxyDesc::FInstancedSkinnedMeshSceneProxyDesc(const UInstancedSkinnedMeshComponent* InComponent)
{
	InitializeFromInstancedSkinnedMeshComponent(InComponent);
}

void FInstancedSkinnedMeshSceneProxyDesc::InitializeFromInstancedSkinnedMeshComponent(const UInstancedSkinnedMeshComponent* InComponent)
{
	InitializeFromSkinnedMeshComponent(InComponent);

	AnimationMinScreenSize = InComponent->AnimationMinScreenSize;
	InstanceMinDrawDistance = InComponent->InstanceMinDrawDistance;
	InstanceStartCullDistance = InComponent->InstanceStartCullDistance;
	InstanceEndCullDistance = InComponent->InstanceEndCullDistance;
#if WITH_EDITOR
	SelectedInstances = InComponent->SelectedInstances;
#endif

	AnimBankItems = InComponent->AnimBankItems;

	InstanceDataSceneProxy = InComponent->GetInstanceDataSceneProxy();
	check(!GetScene() || InstanceDataSceneProxy);
}
