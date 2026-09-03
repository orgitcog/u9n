// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"

#include "MuCO/CustomizableObjectCustomVersion.h"
#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObject.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticString.h"

class UCustomizableObjectNodeRemapPins;
struct FPropertyChangedEvent;

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


UCustomizableObjectNodeObjectGroup::UCustomizableObjectNodeObjectGroup()
	: Super()
{
	GroupName = "Unnamed Group";
}

void UCustomizableObjectNodeObjectGroup::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FCustomizableObjectCustomVersion::GUID);

	UEdGraphPin* groupPin = GroupProjectorsPin();
	if (Ar.CustomVer(FCustomizableObjectCustomVersion::GUID) < FCustomizableObjectCustomVersion::GroupProjectorPinTypeAdded
		&& groupPin
		&& groupPin->PinType.PinCategory == UEdGraphSchema_CustomizableObject::PC_Projector)
	{
		groupPin->PinType.PinCategory = UEdGraphSchema_CustomizableObject::PC_GroupProjector;
	}

	LastGroupName = GroupName;
}


void UCustomizableObjectNodeObjectGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = (PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None);
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCustomizableObjectNodeObjectGroup, GroupName))
	{
		for (UEdGraphPin* LinkedPin : FollowOutputPinArray(*GroupPin()))
		{
			UCustomizableObjectNode* Root = CastChecked<UCustomizableObjectNode>(LinkedPin->GetOwningNode());

			if (UCustomizableObjectNodeObject* CurrentRootNode = Cast<UCustomizableObjectNodeObject>(Root))
			{
				if (CurrentRootNode->ParentObject)
				{
					TArray<const UCustomizableObject*> VisitedObjects;
					CurrentRootNode = GraphTraversal::GetFullGraphRootNode(CurrentRootNode->ParentObject, VisitedObjects);
				}
				if (!CurrentRootNode->ParentObject)
				{
					for (FCustomizableObjectState& State : CurrentRootNode->States)
					{
						for (int32 p = 0; p < State.RuntimeParameters.Num(); ++p)
						{
							if (State.RuntimeParameters[p].Equals(LastGroupName))
							{
								State.RuntimeParameters[p]= GroupName;
							}
						}
						if (State.ForcedParameterValues.Contains(GroupName))
						{
							// Forced parameter already contains the NEW name of the parameter. We currently allow inconsistencies while working before compile time, so no warning needed here. When this changes to ID based instead of String based, this would become a warning worthy check (TODO: MTBL-1071).
						}
						else if (State.ForcedParameterValues.Contains(LastGroupName))
						{
							FString LastForcedValue = State.ForcedParameterValues.FindAndRemoveChecked(LastGroupName);
							State.ForcedParameterValues.Emplace(GroupName, LastForcedValue);
						}
					}
				}
			}
		}
	}
	LastGroupName = GroupName;
}


FText UCustomizableObjectNodeObjectGroup::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	const UEdGraphPin* GroupNamePin = NamePin.Get();
	FText NodeTitle;
	FFormatNamedArguments Args;
	Args.Add(TEXT("GroupName"), FText::FromString(GroupName));

	if (TitleType == ENodeTitleType::ListView || (GroupNamePin && GroupNamePin->LinkedTo.Num()))
	{
		NodeTitle = LOCTEXT("Object_Group", "Object Group");
	}
	else if(TitleType == ENodeTitleType::EditableTitle)
	{
		NodeTitle = LOCTEXT("ObjectGroup_Edit", "{GroupName}");
	}
	else
	{
		NodeTitle = LOCTEXT("Group_Object_Title", "{GroupName}\nObject Group");
	}

	return FText::Format(NodeTitle, Args);
}


FLinearColor UCustomizableObjectNodeObjectGroup::GetNodeTitleColor() const
{
	return UEdGraphSchema_CustomizableObject::GetPinTypeColor(UEdGraphSchema_CustomizableObject::PC_Object);
}


FText UCustomizableObjectNodeObjectGroup::GetTooltipText() const
{
	return LOCTEXT("Grpup_Object_Tooltip",
	"Define one or multiple parameters that are a collection of Customizable Objects that share a mutual relationship: they either are\nexclusive from each other, at most one of them can be active, or at least one of them has to be, or any combination of them can be\nenabled, or they define materials that will always be shown together.");
}


void UCustomizableObjectNodeObjectGroup::OnRenameNode(const FString& NewName)
{
	if (!NewName.IsEmpty())
	{
		GroupName = NewName;
	}
}


bool UCustomizableObjectNodeObjectGroup::GetCanRenameNode() const
{
	const UEdGraphPin* GroupNamePin = NamePin.Get();

	if (GroupNamePin && GroupNamePin->LinkedTo.Num())
	{
		return false;
	}

	return true;
}


void UCustomizableObjectNodeObjectGroup::PinConnectionListChanged(UEdGraphPin* Pin)
{
	if (Pin == NamePin)
	{
		GetGraph()->NotifyGraphChanged();
	}
}


void UCustomizableObjectNodeObjectGroup::AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins)
{
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UEdGraphPin* ObjectsPin = CustomCreatePin(EGPD_Input, Schema->PC_Object, FName("Objects"), true);
	ObjectsPin->bDefaultValueIsIgnored = true;

	UEdGraphPin* ProjectorsPin = CustomCreatePin(EGPD_Input, Schema->PC_GroupProjector, FName("Projectors"), true);
	ObjectsPin->bDefaultValueIsIgnored = true;

	CustomCreatePin(EGPD_Output, Schema->PC_Object, FName("Group"));

	NamePin = CustomCreatePin(EGPD_Input, UEdGraphSchema_CustomizableObject::PC_String, FName("Name"));
}


bool UCustomizableObjectNodeObjectGroup::IsSingleOutputNode() const
{
	return true;
}


void UCustomizableObjectNodeObjectGroup::BackwardsCompatibleFixup(int32 CustomizableObjectCustomVersion)
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


UEdGraphPin* UCustomizableObjectNodeObjectGroup::ObjectsPin() const
{
	return FindPin(TEXT("Objects"));
}


UEdGraphPin* UCustomizableObjectNodeObjectGroup::GroupProjectorsPin() const
{
	return FindPin(TEXT("Projectors"));
}


UEdGraphPin* UCustomizableObjectNodeObjectGroup::GroupPin() const
{
	return FindPin(TEXT("Group"));
}


FString UCustomizableObjectNodeObjectGroup::GetGroupName(TArray<const UCustomizableObjectNodeMacroInstance*>* MacroContext) const
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

	return GroupName;
}


void UCustomizableObjectNodeObjectGroup::SetGroupName(const FString& Name)
{
	GroupName = Name;
}

#undef LOCTEXT_NAMESPACE
