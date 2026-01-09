// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeParameter.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void UCustomizableObjectNodeParameter::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const FName Type = GetCategory();
	const FName PinName = UEdGraphSchema_CustomizableObject::GetPinCategoryName(Type);
	const FText PinFriendlyName = UEdGraphSchema_CustomizableObject::GetPinCategoryFriendlyName(Type);
	
	UEdGraphPin* ValuePin = CustomCreatePin(EGPD_Output, Type, PinName);
	ValuePin->PinFriendlyName = PinFriendlyName;
	ValuePin->bDefaultValueIsIgnored = true;

	NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
}


void UCustomizableObjectNodeParameter::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (!NamePin.Get())
		{
			NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
		}
	}
}


FText UCustomizableObjectNodeParameter::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	FText NodeTitle;
	const UEdGraphPin* ParamNamePin = NamePin.Get();
	FFormatNamedArguments Args;
	Args.Add(TEXT("ParameterName"), FText::FromString(ParameterName));
	Args.Add(TEXT("Type"), UEdGraphSchema_CustomizableObject::GetPinCategoryFriendlyName(GetCategory()));

	if (TitleType == ENodeTitleType::ListView || (ParamNamePin && ParamNamePin->LinkedTo.Num()))
	{
		NodeTitle = LOCTEXT("ParameterTitle_ListView", "{Type} Parameter");
	}
	else if (TitleType == ENodeTitleType::EditableTitle)
	{
		NodeTitle = LOCTEXT("ParameterTitle_EditableTitle", "{ParameterName}");
	}
	else
	{
		NodeTitle = LOCTEXT("ParameterTitle_Title", "{ParameterName}\n{Type} Parameter");
	}

	return FText::Format(NodeTitle, Args);
}


FLinearColor UCustomizableObjectNodeParameter::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(GetCategory());
}


FText UCustomizableObjectNodeParameter::GetTooltipText() const
{
	return FText::Format(LOCTEXT("Parameter_Tooltip", "Expose a runtime modifiable {0} parameter from the Customizable Object."), FText::FromName(GetCategory()));
}


void UCustomizableObjectNodeParameter::OnRenameNode(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		ParameterName = NewName;
	}
}


bool UCustomizableObjectNodeParameter::GetCanRenameNode() const
{
	const UEdGraphPin* ParamNamePin = NamePin.Get();

	if (ParamNamePin && ParamNamePin->LinkedTo.Num())
	{
		return false;
	}

	return true;
}


void UCustomizableObjectNodeParameter::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == NamePin)
	{
		GetGraph()->NotifyGraphChanged();
	}
}


FString UCustomizableObjectNodeParameter::GetParameterName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
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

	return ParameterName;
}


void UCustomizableObjectNodeParameter::SetParameterName(const FString& Name)
{
	ParameterName = Name;
}

#undef LOCTEXT_NAMESPACE

