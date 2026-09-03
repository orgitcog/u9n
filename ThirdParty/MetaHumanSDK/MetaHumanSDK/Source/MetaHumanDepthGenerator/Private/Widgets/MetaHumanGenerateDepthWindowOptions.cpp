// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanGenerateDepthWindowOptions.h"

#include "MetaHumanDepthGeneratorModule.h"

#include "Modules/ModuleManager.h"

const FString UMetaHumanGenerateDepthWindowOptions::ImageSequenceDirectoryName = TEXT("GeneratedDepthData");

#if WITH_EDITOR
void UMetaHumanGenerateDepthWindowOptions::PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	MinDistance = FMath::Clamp(MinDistance, 0.0, MaxDistance);
}
#endif