// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphPinModuleEvent.h"
#include "SModuleEventPicker.h"

namespace UE::AnimNext::Editor
{

void SGraphPinModuleEvent::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinModuleEvent::GetDefaultValueWidget()
{
	TArray<UObject*> ContextObjects;
	ContextObjects.Add(GraphPinObj->GetOwningNode());
	return SNew(SModuleEventPicker)
		.ContextObjects(ContextObjects)
		.InitiallySelectedEvent(FName(GraphPinObj->GetDefaultAsString()))
		.OnEventPicked_Lambda([this](FName InEventName)
		{
			GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, InEventName.ToString());
		})
		.OnGetSelectedEvent_Lambda([this]()
		{
			return FName(GraphPinObj->GetDefaultAsString());
		});
}

}
