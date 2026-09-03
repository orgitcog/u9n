// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/ActorModifierAttachmentBaseModifier.h"

#include "GameFramework/Actor.h"

void UActorModifierAttachmentBaseModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);
	
	InMetadata.SetCompatibilityRule([](const AActor* InActor)->bool
	{
		return InActor && InActor->FindComponentByClass<USceneComponent>();		
	});
}

void UActorModifierAttachmentBaseModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FActorModifierSceneTreeUpdateExtension>(this);
}
