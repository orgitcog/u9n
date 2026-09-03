// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/ActorModifierEditorSplinePathModifierDetailCustomization.h"

#include "Cloner/Customizations/CEEditorClonerCustomActorPickerNodeBuilder.h"
#include "Components/SplineComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Modifiers/ActorModifierSplinePathModifier.h"

void FActorModifierEditorSplinePathModifierDetailCustomization::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TSharedRef<IPropertyHandle> SplinePropertyHandle = InDetailBuilder.GetProperty(UActorModifierSplinePathModifier::GetSplineActorWeakPropertyName(), UActorModifierSplinePathModifier::StaticClass());

	if (!SplinePropertyHandle->IsValidHandle())
	{
		return;
	}

	SplinePropertyHandle->MarkHiddenByCustomization();

	IDetailCategoryBuilder& SplineCategoryBuilder = InDetailBuilder.EditCategory(SplinePropertyHandle->GetDefaultCategoryName(), SplinePropertyHandle->GetDefaultCategoryText());

	SplineCategoryBuilder.AddCustomBuilder(MakeShared<FCEEditorClonerCustomActorPickerNodeBuilder>(
		SplinePropertyHandle
		, FOnShouldFilterActor::CreateStatic(&FActorModifierEditorSplinePathModifierDetailCustomization::OnFilterSplineActor))
	);
}

bool FActorModifierEditorSplinePathModifierDetailCustomization::OnFilterSplineActor(const AActor* InActor)
{
	if (!IsValid(InActor))
	{
		return false;
	}

	return !!InActor->FindComponentByClass<USplineComponent>();
}
