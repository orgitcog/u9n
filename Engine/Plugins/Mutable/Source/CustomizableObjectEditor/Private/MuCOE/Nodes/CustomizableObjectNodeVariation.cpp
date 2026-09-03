// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeVariation.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"

class UCustomizableObjectNodeRemapPins;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

void UCustomizableObjectNodeVariation::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.Property)
	{
		ReconstructNode();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}


void UCustomizableObjectNodeVariation::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	const FName Category = GetCategory();
	const bool bIsInputPinArray = IsInputPinArray();

	{
		const FName PinName = FName(UEdGraphSchema_CustomizableObject::GetPinCategoryName(GetCategory()).ToString());
		UEdGraphPin* Pin = CustomCreatePin(EGPD_Output, Category, PinName);
		Pin->PinFriendlyName = UEdGraphSchema_CustomizableObject::GetPinCategoryFriendlyName(GetCategory());
	}
	
	VariationsPins.SetNum(VariationsData.Num());
	VariationTagPins.SetNum(VariationsData.Num());

	for (int32 VariationIndex = VariationsData.Num() - 1; VariationIndex >= 0; --VariationIndex)
	{
		const FName PinName = FName(FString::Printf( TEXT("Variation %d"), VariationIndex));
		UEdGraphPin* VariationPin = CustomCreatePin(EGPD_Input, Category, PinName, bIsInputPinArray);

		FString TagName = GetTagDisplayName(VariationsData[VariationIndex].Tag);
		VariationPin->PinFriendlyName = FText::Format(LOCTEXT("Variation_Pin_FriendlyName", "Variation {0} [{1}]"), VariationIndex, FText::FromString(TagName));
		
		VariationsPins[VariationIndex] = VariationPin;

		FText VariationTagSufix = LOCTEXT("TagSufix", " - Tag");
		const FName TagPinName = FName(PinName.ToString() + VariationTagSufix.ToString());
		UEdGraphPin* VariationTagPin = CustomCreatePin(EGPD_Input, Schema->PC_String, TagPinName);

		VariationTagPins[VariationIndex] = VariationTagPin;
	}

	CustomCreatePin(EGPD_Input, Category, FName(TEXT("Default")), bIsInputPinArray);
}


bool UCustomizableObjectNodeVariation::IsInputPinArray() const
{
	return false;
}


int32 UCustomizableObjectNodeVariation::GetNumVariations() const
{
	return VariationsData.Num();
}


const FCustomizableObjectVariation& UCustomizableObjectNodeVariation::GetVariation(int32 Index) const
{
	return VariationsData[Index];
}


FString UCustomizableObjectNodeVariation::GetVariationTag(int32 Index, TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
{
	if (const UEdGraphPin* TagPin = VariationTagPin(Index))
	{
		if (const UEdGraphPin* ConnectedStringPin = FollowInputPin(*TagPin))
		{
			const UEdGraphPin* SourceStringPin = GraphTraversal::FindIOPinSourceThroughMacroContext(*ConnectedStringPin, MacroContext);

			if (SourceStringPin)
			{
				if (const UCustomizableObjectNodeStaticString* StringNode = Cast<UCustomizableObjectNodeStaticString>(SourceStringPin->GetOwningNode()))
				{
					return StringNode->Value;
				}
			}
		}
	}

	return GetVariation(Index).Tag;
}


UEdGraphPin* UCustomizableObjectNodeVariation::DefaultPin() const
{
	return FindPin(TEXT("Default"));
}


UEdGraphPin* UCustomizableObjectNodeVariation::VariationPin(int32 Index) const
{
	if (VariationsPins.IsValidIndex(Index))
	{
		return VariationsPins[Index].Get();
	}

	return nullptr;
}


UEdGraphPin* UCustomizableObjectNodeVariation::VariationTagPin(int32 Index) const
{
	if (VariationTagPins.IsValidIndex(Index))
	{
		return VariationTagPins[Index].Get();
	}

	return nullptr;
}


FText UCustomizableObjectNodeVariation::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(LOCTEXT("Variation_Node_Title", "{0} Variation"), UEdGraphSchema_CustomizableObject::GetPinCategoryFriendlyName(GetCategory()));
}


FLinearColor UCustomizableObjectNodeVariation::GetNodeTitleColor() const
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();
	return Schema->GetPinTypeColor(GetCategory());
}


FText UCustomizableObjectNodeVariation::GetTooltipText() const
{
	return FText::Format(LOCTEXT("Variation_Tooltip", "Select a {0} depending on what tags are active."), UEdGraphSchema_CustomizableObject::GetPinCategoryFriendlyName(GetCategory()));
}


void UCustomizableObjectNodeVariation::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
{
	Super::BackwardsCompatibleFixup(CustomizableObjectCustomVersion);
	
	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::NodeVariationSerializationIssue)
	{
		ReconstructNode();
	}

	if (CustomizableObjectCustomVersion == FCustomizableObjectCustomVersion::EnableMutableMacrosNewVersion)
	{
		if (VariationTagPins.IsEmpty())
		{
			VariationTagPins.SetNum(VariationsData.Num());

			for (int32 VariationIndex = VariationsData.Num() - 1; VariationIndex >= 0; --VariationIndex)
			{
				const FName PinName = FName(FString::Printf(TEXT("Variation %d"), VariationIndex));
				FText VariationTagSufix = LOCTEXT("TagSufix", " - Tag");
				const FName TagPinName = FName(PinName.ToString() + VariationTagSufix.ToString());
				UEdGraphPin* VariationTagPin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, TagPinName);
				VariationTagPins[VariationIndex] = VariationTagPin;
			}
		}
	}

	// TODO(Max UE-268615): Remove the tag name from the variation pins since now there is a pin with a textbox for it.
}


#undef LOCTEXT_NAMESPACE

