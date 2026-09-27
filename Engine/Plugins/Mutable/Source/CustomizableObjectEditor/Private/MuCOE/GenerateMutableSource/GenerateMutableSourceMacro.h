// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/GraphTraversal.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSource.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMacroInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"

template<class T>
mu::Ptr<T> GenerateMutableSourceMacro(const UEdGraphPin& Pin, FMutableGraphGenerationContext& GenerationContext, const TFunction<mu::Ptr<T>(const UEdGraphPin*, FMutableGraphGenerationContext&)>& GenerationFunction)
{
	mu::Ptr<T> Result;
	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin.GetOwningNode());

	if (const UCustomizableObjectNodeMacroInstance* TypedNodeMacro = Cast<UCustomizableObjectNodeMacroInstance>(Node))
	{
		if (const UEdGraphPin* OutputPin = TypedNodeMacro->GetMacroIOPin(ECOMacroIOType::COMVT_Output, Pin.PinName))
		{
			if (const UEdGraphPin* FollowPin = FollowInputPin(*OutputPin))
			{
				GenerationContext.MacroNodesStack.Push(TypedNodeMacro);
				Result = GenerationFunction(FollowPin, GenerationContext);
				GenerationContext.MacroNodesStack.Pop();
			}
			else
			{
				FText Msg = FText::Format(LOCTEXT("MacroInstanceError_PinNotLinked", "Macro Output node Pin {0} not linked."), FText::FromName(Pin.PinName));
				GenerationContext.Log(Msg, Node);
			}
		}
		else
		{
			FText Msg = FText::Format(LOCTEXT("MacroInstanceError_PinNameNotFound", "Macro Output node does not contain a pin with name {0}."), FText::FromName(Pin.PinName));
			GenerationContext.Log(Msg, Node);
		}
	}

	else if (const UCustomizableObjectNodeTunnel* TypedNodeTunnel = Cast<UCustomizableObjectNodeTunnel>(Node))
	{
		check(TypedNodeTunnel->bIsInputNode);
		check(GenerationContext.MacroNodesStack.Num());

		const UCustomizableObjectNodeMacroInstance* MacroInstanceNode = GenerationContext.MacroNodesStack.Pop();
		check(MacroInstanceNode);

		if (const UEdGraphPin* InputPin = MacroInstanceNode->FindPin(Pin.PinName, EEdGraphPinDirection::EGPD_Input))
		{
			if (const UEdGraphPin* FollowPin = FollowInputPin(*InputPin))
			{
				Result = GenerationFunction(FollowPin, GenerationContext);
			}
		}
		else
		{
			FText Msg = FText::Format(LOCTEXT("MacroTunnelError_PinNameNotFound", "Macro Instance Node does not contain a pin with name {0}."), FText::FromName(Pin.PinName));
			GenerationContext.Log(Msg, Node);
		}

		// Push the Macro again even if the result is null
		GenerationContext.MacroNodesStack.Push(MacroInstanceNode);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
