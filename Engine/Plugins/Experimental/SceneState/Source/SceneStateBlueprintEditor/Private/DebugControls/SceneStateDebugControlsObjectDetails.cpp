// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateDebugControlsObjectDetails.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "SceneStateDebugControlsObject.h"
#include "SceneStateEventUtils.h"
#include "SceneStateObject.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SceneStateDebugControlsObjectDetails"

namespace UE::SceneState::Editor
{

void FDebugControlsObjectDetails::CustomizeDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	DebugControls = InDetailBuilder.GetObjectsOfTypeBeingCustomized<USceneStateDebugControlsObject>();
	CustomizeEventDetails(InDetailBuilder);
	CustomizeDebuggedObjectDetails(InDetailBuilder);
}

void FDebugControlsObjectDetails::CustomizeEventDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	EventsHandle = InDetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USceneStateDebugControlsObject, Events));
	EventsHandle->MarkHiddenByCustomization();

	IDetailCategoryBuilder& EventsCategory = InDetailBuilder.EditCategory(TEXT("Events"));
	EventsCategory.AddProperty(EventsHandle);
}

USceneStateObject* FDebugControlsObjectDetails::GetDebuggedObject(int32 InIndex) const
{
	if (!DebugControls.IsValidIndex(InIndex))
	{
		return nullptr;
	}

	USceneStateDebugControlsObject* const DebugControlsObject = DebugControls[InIndex].Get();
	if (!DebugControlsObject)
	{
		return nullptr;
	}

	return DebugControlsObject->DebuggedObjectWeak.Get();
}

void FDebugControlsObjectDetails::CustomizeDebuggedObjectDetails(IDetailLayoutBuilder& InDetailBuilder)
{
	TArray<UObject*> DebuggedObjects;
	DebuggedObjects.Reserve(DebugControls.Num());

	// Gather all the Debugged objects within the debug controls
	Algo::Transform(DebugControls, DebuggedObjects,
		[](const TWeakObjectPtr<USceneStateDebugControlsObject>& InDebugControls)->UObject*
		{
			if (USceneStateDebugControlsObject* DebugControl = InDebugControls.Get())
			{
				return DebugControl->DebuggedObjectWeak.Get();
			}
			return nullptr;
		});

	DebuggedObjects.RemoveAll(
		[](UObject* InObject)
		{
			return !InObject;
		});

	InDetailBuilder
		.EditCategory(TEXT("Debugged Object"))
		.AddExternalObjects(DebuggedObjects, EPropertyLocation::Default,
			FAddPropertyParams()
			.HideRootObjectNode(/*bHideRootObjectNode*/true));
}

} // UE::SceneState::Editor

#undef LOCTEXT_NAMESPACE
