// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayBehaviorSmartObjectBehaviorDefinition.h"
#include "GameplayBehaviorConfig.h"

#if WITH_EDITOR
void UGameplayBehaviorSmartObjectBehaviorDefinition::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	if (GameplayBehaviorConfig)
	{
		OutDeps.Add(GameplayBehaviorConfig);
	}
}
#endif //WITH_EDITOR
