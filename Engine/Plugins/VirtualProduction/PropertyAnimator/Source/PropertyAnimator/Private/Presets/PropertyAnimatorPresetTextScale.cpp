// Copyright Epic Games, Inc. All Rights Reserved.

#include "Presets/PropertyAnimatorPresetTextScale.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Characters/Text3DCharacterBase.h"
#include "Properties/PropertyAnimatorTextResolver.h"
#include "Properties/PropertyAnimatorFloatContext.h"
#include "Text3DComponent.h"

void UPropertyAnimatorPresetTextScale::GetPresetProperties(const AActor* InActor, const UPropertyAnimatorCoreBase* InAnimator, TSet<FPropertyAnimatorCoreData>& OutProperties) const
{
	UText3DComponent* TextComponent = InActor->FindComponentByClass<UText3DComponent>();
	if (!TextComponent)
	{
		return;
	}

	const FName ScalePropertyName = UText3DCharacterBase::GetRelativeScalePropertyName();
	FProperty* ScaleProperty = FindFProperty<FProperty>(UText3DCharacterBase::StaticClass(), ScalePropertyName);
	check(ScaleProperty);

	OutProperties.Add(FPropertyAnimatorCoreData(TextComponent, ScaleProperty, nullptr, UPropertyAnimatorTextResolver::StaticClass()));
}
