// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectChild.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeObjectChild::UCustomizableObjectNodeObjectChild()
	: Super()
{
	bIsBase = false;
}


FText UCustomizableObjectNodeObjectChild::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphPin* ChildNamePin = NamePin.Get();
	FFormatNamedArguments Args;
	Args.Add(TEXT("ObjectName"), FText::FromString(ObjectName));

	if (TitleType == ENodeTitleType::ListView || (ChildNamePin && ChildNamePin->LinkedTo.Num()))
	{
		return FText::Format(LOCTEXT("Child_Object_Title_List", "Child Object"), Args);
	}
	else if (TitleType == ENodeTitleType::EditableTitle)
	{
		return FText::Format(LOCTEXT("Child_Object_Edit", "{ObjectName}"), Args);
	}
	else
	{
		return FText::Format(LOCTEXT("Child_Object_Title", "{ObjectName}\nChild Object"), Args);
	}
}


void UCustomizableObjectNodeObjectChild::PrepareForCopying()
{
	// Overriden to hide parent's class error message
}


bool UCustomizableObjectNodeObjectChild::CanUserDeleteNode() const
{
	return true;
}


bool UCustomizableObjectNodeObjectChild::CanDuplicateNode() const
{
	return true;
}


void UCustomizableObjectNodeObjectChild::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == NamePin)
	{
		GetGraph()->NotifyGraphChanged();
	}
}


bool UCustomizableObjectNodeObjectChild::GetCanRenameNode() const
{
	const UEdGraphPin* ChildNamePin = NamePin.Get();

	if (ChildNamePin && ChildNamePin->LinkedTo.Num())
	{
		return false;
	}

	return true;
}


FText UCustomizableObjectNodeObjectChild::GetTooltipText() const
{
	return LOCTEXT("Child_Object_Tooltip",
		"Defines a customizable object children in the same asset as its parent, to ease the addition of small Customizable Objects directly into\ntheir parents asset. Functionally equivalent to the Base Object Node when it has a parent defined. It can be a children of the root\nobject or of any children, allowing arbitrary nesting of objects. Defines materials that can be added to its parent, modify it, remove\nparts of it or change any of its parameters. Also defines properties for others to use or modify.");

}


void UCustomizableObjectNodeObjectChild::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	Super::AllocateDefaultPins(RemapPins);

	NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, "Name");
}


void UCustomizableObjectNodeObjectChild::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!NamePin.Get())
		{
			NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, "Name");
		}
	}
}


bool UCustomizableObjectNodeObjectChild::IsNodeSupportedInMacros() const
{
	return true;
}


FString UCustomizableObjectNodeObjectChild::GetObjectName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
{
	if (NamePin.Get())
	{
		if (const UEdGraphPin* LinkedPin = FollowInputPin(*NamePin.Get()))
		{
			const UEdGraphPin* StringPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*LinkedPin, MacroContext);

			if (const UCustomizableObjectNodeStaticString* StringNode = StringPin ? Cast<UCustomizableObjectNodeStaticString>(StringPin->GetOwningNode()) : nullptr)
			{
				return StringNode->Value;
			}
		}
	}

	return ObjectName;
}


#undef LOCTEXT_NAMESPACE
