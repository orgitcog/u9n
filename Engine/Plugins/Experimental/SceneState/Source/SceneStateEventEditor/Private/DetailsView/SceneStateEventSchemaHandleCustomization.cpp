// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateEventSchemaHandleCustomization.h"
#include "DetailWidgetRow.h"
#include "Widgets/SSceneStateEventSchemaPicker.h"

namespace UE::SceneState::Editor
{

void FEventSchemaHandleCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& InHeaderRow, IPropertyTypeCustomizationUtils& InCustomizationUtils)
{
	InHeaderRow
		.NameContent()
		[
			InStructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SEventSchemaPicker, InStructPropertyHandle)
		];
}

} // UE::SceneState::Editor
