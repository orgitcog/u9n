// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectMacroLibrary/CustomizableObjectMacroLibrary.h"

#include "MuCOE/CustomizableObjectGraph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTunnel.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectMacroLibrary"


UCustomizableObjectMacro* UCustomizableObjectMacroLibrary::AddMacro()
{
	UCustomizableObjectMacro* NewMacro = nullptr;

	FName BaseName = "NewMacro";
	FName MacroName = MakeUniqueObjectName(this, UCustomizableObjectMacro::StaticClass(), FName(BaseName));
	
	NewMacro = NewObject<UCustomizableObjectMacro>(this, MacroName, RF_Transactional | RF_Public);
	UCustomizableObjectGraph* NewGraph = NewObject<UCustomizableObjectGraph>(NewMacro, NAME_None, RF_Transactional);

	NewGraph->AddEssentialGraphNodes();

	NewMacro->Graph = NewGraph;
	NewMacro->Name = MacroName;
	Macros.Add(NewMacro);

	return NewMacro;
}


void UCustomizableObjectMacroLibrary::RemoveMacro(UCustomizableObjectMacro* MacroToRemove)
{
	if (Macros.Contains(MacroToRemove))
	{
		Macros.Remove(MacroToRemove);
	}
}


UCustomizableObjectMacroInputOutput* UCustomizableObjectMacro::AddVariable(ECOMacroIOType VarType)
{
	const UEdGraphSchema_CustomizableObject* Schema = Cast<UEdGraphSchema_CustomizableObject>(Graph->GetSchema());
	check(Schema);

	FName BaseName = "NewVar";
	FName BaseType = Schema->PC_Mesh;
	FName VariableName = MakeUniqueObjectName(this, UCustomizableObjectMacroInputOutput::StaticClass(), FName(BaseName));

	UCustomizableObjectMacroInputOutput* NewVariable = NewObject<UCustomizableObjectMacroInputOutput>(this, VariableName, RF_Transactional);
	NewVariable->Type = VarType;
	NewVariable->PinCategoryType = BaseType;
	NewVariable->Name = VariableName;
	NewVariable->UniqueId = FGuid::NewGuid();

	InputOutputs.Add(NewVariable);

	return NewVariable;
}


void UCustomizableObjectMacro::RemoveVariable(UCustomizableObjectMacroInputOutput* Variable)
{
	if (InputOutputs.Contains(Variable))
	{
		InputOutputs.Remove(Variable);
	}
}

UCustomizableObjectNodeTunnel* UCustomizableObjectMacro::GetIONode(ECOMacroIOType Type) const
{
	UCustomizableObjectNodeTunnel* OutNode = nullptr;

	if (Graph)
	{
		TArray<UCustomizableObjectNodeTunnel*> IONodes;
		Graph->GetNodesOfClass<UCustomizableObjectNodeTunnel>(IONodes);
		check(IONodes.Num() == 2);

		for (UCustomizableObjectNodeTunnel* IONode : IONodes)
		{
			if (IONode->bIsInputNode && Type == ECOMacroIOType::COMVT_Input)
			{
				OutNode = IONode;
				break;
			}
			else if(!IONode->bIsInputNode && Type == ECOMacroIOType::COMVT_Output)
			{
				OutNode = IONode;
				break;
			}
		}
	}

	return OutNode;
}

#undef LOCTEXT_NAMESPACE
