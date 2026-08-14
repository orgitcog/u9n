// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extensions/Text3DEffectExtensionBase.h"
#include "Text3DComponent.h"

EText3DExtensionResult UText3DEffectExtensionBase::PreRendererUpdate(EText3DRendererFlags InFlag)
{
	if (InFlag != EText3DRendererFlags::Layout)
	{
		return EText3DExtensionResult::Active;
	}

	const UText3DComponent* Text3DComponent = GetText3DComponent();

	const uint16 CharacterCount = Text3DComponent->GetCharacterCount();
	for (uint16 Index = 0; Index < CharacterCount; Index++)
	{
		if (GetTargetRange().IsInRange(Index))
		{
			ApplyEffect(Index, CharacterCount);
		}
	}

	return EText3DExtensionResult::Finished;
}

EText3DExtensionResult UText3DEffectExtensionBase::PostRendererUpdate(EText3DRendererFlags InFlag)
{
	return EText3DExtensionResult::Active;
}