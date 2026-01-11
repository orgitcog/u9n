// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/GraphEditorSchemaActions.h"
#include "EditorUtils.h"
#include "AnimNextEdGraph.h"
#include "AnimNextEdGraphNode.h"
#include "EdGraphNode_Comment.h"
#include "Editor.h"
#include "Module/AnimNextModule_EditorData.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "EdGraphSchema_K2.h"
#include "GraphEditor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "AnimNextUnitNode.h"
#include "Editor/RigVMEditorStyle.h"


#define LOCTEXT_NAMESPACE "AnimNextSchemaActions"

// *** Base Schema Action ***

const FSlateBrush* FAnimNextSchemaAction::GetIconBrush() const
{
	return FRigVMEditorStyle::Get().GetBrush("RigVM.Unit");
}

const FLinearColor& FAnimNextSchemaAction::GetIconColor() const
{
	return FLinearColor::White;
}

// *** Rig Unit ***

UEdGraphNode* FAnimNextSchemaAction_RigUnit::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
{
	IRigVMClientHost* Host = ParentGraph->GetImplementingOuter<IRigVMClientHost>();
	URigVMEdGraphNode* NewNode = nullptr;
	URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(ParentGraph);

	UEdGraphPin* FromPin = nullptr;
	if (FromPins.Num() > 0)
	{
		FromPin = FromPins[0];
	}
	
	if (Host != nullptr && EdGraph != nullptr)
	{
		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(Cast<UObject>(Host), StructTemplate->GetFName().ToString());
		URigVMController* Controller = Host->GetRigVMClient()->GetController(ParentGraph);

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		FRigVMUnitNodeCreatedContext& UnitNodeCreatedContext = Controller->GetUnitNodeCreatedContext();
		FRigVMUnitNodeCreatedContext::FScope ReasonScope(UnitNodeCreatedContext, ERigVMNodeCreatedReason::NodeSpawner, Host);

		if (URigVMUnitNode* ModelNode = Controller->AddUnitNode(StructTemplate, NodeClass, FRigVMStruct::ExecuteName, FDeprecateSlateVector2D(Location), Name.ToString(), true, true))
		{
			NewNode = Cast<URigVMEdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode)
			{
				if (FromPin)
				{
					NewNode->AutowireNewNode(FromPin);
				}

				Controller->ClearNodeSelection(true, true);
				Controller->SelectNode(ModelNode, true, true, true);
			}
		}
		Controller->CloseUndoBracket();
	}

	return NewNode;
}

// *** Dispatch Factory ***

const FSlateBrush* FAnimNextSchemaAction_DispatchFactory::GetIconBrush() const
{
	return FRigVMEditorStyle::Get().GetBrush("RigVM.Template");
}

UEdGraphNode* FAnimNextSchemaAction_DispatchFactory::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
{
	IRigVMClientHost* Host = ParentGraph->GetImplementingOuter<IRigVMClientHost>();
	if(Host == nullptr)
	{
		return nullptr;
	}

	URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(ParentGraph);
	if(EdGraph == nullptr)
	{
		return nullptr;
	}

	const FRigVMTemplate* Template = FRigVMRegistry::Get().FindTemplate(Notation);
	if (Template == nullptr)
	{
		return nullptr;
	}
	
	URigVMEdGraphNode* NewNode = nullptr;
	UEdGraphPin* FromPin = nullptr;
	if (FromPins.Num() > 0)
	{
		FromPin = FromPins[0];
	}

	const int32 NotationHash = (int32)GetTypeHash(Notation);
	const FString TemplateName = TEXT("RigVMTemplate_") + FString::FromInt(NotationHash);

	FName Name = UE::AnimNext::Editor::FUtils::ValidateName(Cast<UObject>(Host), Template->GetName().ToString());
	URigVMController* Controller = Host->GetRigVMClient()->GetController(ParentGraph);

	Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

	if (URigVMTemplateNode* ModelNode = Controller->AddTemplateNode(Notation, FDeprecateSlateVector2D(Location), Name.ToString(), true, true))
	{
		NewNode = Cast<URigVMEdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));

		if (NewNode)
		{
			if(FromPin)
			{
				NewNode->AutowireNewNode(FromPin);
			}

			Controller->ClearNodeSelection(true, true);
			Controller->SelectNode(ModelNode, true, true, true);
		}

		Controller->CloseUndoBracket();
	}
	else
	{
		Controller->CancelUndoBracket();
	}

	return NewNode;
}

// *** Variable ***

FAnimNextSchemaAction_Variable::FAnimNextSchemaAction_Variable(FName InName, const FAnimNextParamType& InType, const EVariableAccessorChoice InVariableAccessorChoice)
	: Name(InName)
	, VariableAccessorChoice(InVariableAccessorChoice)
{
	if(InType.IsObjectType())
	{
		ObjectPath = InType.GetValueTypeObject()->GetPathName();
	}

	TypeName = InType.ToRigVMTemplateArgument().GetBaseCPPType();

	static const FText VariablesCategory = LOCTEXT("Variables", "Variables");
	static const FTextFormat GetVariableFormat = LOCTEXT("GetVariableFormat", "Get {0}");
	static const FTextFormat SetVariableFormat = LOCTEXT("SetVariableFormat", "Set {0}");

	FText MenuDesc;
	FText ToolTip;

	if(InVariableAccessorChoice == EVariableAccessorChoice::Get)
	{
		MenuDesc = FText::Format(GetVariableFormat, FText::FromName(Name));
		ToolTip = FText::FromString(FString::Printf(TEXT("Get the value of variable %s"), *Name.ToString()));
	}
	else if (InVariableAccessorChoice == EVariableAccessorChoice::Set)
	{
		MenuDesc = FText::Format(SetVariableFormat, FText::FromName(Name));
		ToolTip = FText::FromString(FString::Printf(TEXT("Set the value of variable %s"), *Name.ToString()));
	}
	else
	{
		MenuDesc = FText::FromName(Name);
	}

	UpdateSearchData(MenuDesc, ToolTip, VariablesCategory, FText::GetEmpty());

	FEdGraphPinType PinType = UE::AnimNext::UncookedOnly::FUtils::GetPinTypeFromParamType(InType);
	VariableColor = GetDefault<UEdGraphSchema_K2>()->GetPinTypeColor(PinType);
}

const FSlateBrush* FAnimNextSchemaAction_Variable::GetIconBrush() const
{
	return  FAppStyle::GetBrush(TEXT("Kismet.VariableList.TypeIcon"));
}

const FLinearColor& FAnimNextSchemaAction_Variable::GetIconColor() const
{
	return VariableColor;
}

UEdGraphNode* FAnimNextSchemaAction_Variable::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
{
	const auto AddNodeLambda = [](UEdGraph* ParentGraph, FName InName, const FString& InTypeName, const FString& InObjectPath, const FVector2f& Location, const bool bIsGetter) -> URigVMEdGraphNode*
	{
		IRigVMClientHost* Host = ParentGraph->GetImplementingOuter<IRigVMClientHost>();
		if(Host == nullptr)
		{
			return nullptr;
		}

		URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(ParentGraph);
		if(EdGraph == nullptr)
		{
			return nullptr;
		}

		URigVMEdGraphNode* NewNode = nullptr;

		FString NodeName;

		URigVMController* Controller = Host->GetRigVMClient()->GetController(ParentGraph);
		Controller->OpenUndoBracket(TEXT("Add Variable"));

		if (URigVMNode* ModelNode = Controller->AddVariableNodeFromObjectPath(InName, InTypeName, InObjectPath, bIsGetter, FString(), FDeprecateSlateVector2D(Location), NodeName, true, true))
		{
			for (UEdGraphNode* Node : ParentGraph->Nodes)
			{
				if (URigVMEdGraphNode* RigNode = Cast<URigVMEdGraphNode>(Node))
				{
					if (RigNode->GetModelNodeName() == ModelNode->GetFName())
					{
						NewNode = RigNode;
						break;
					}
				}
			}

			if (NewNode)
			{
				Controller->ClearNodeSelection(true, true);
				Controller->SelectNode(ModelNode, true, true, true);
			}
			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->CancelUndoBracket();
		}

		return NewNode;
	};

	if (VariableAccessorChoice == EVariableAccessorChoice::Deferred)
	{
		FMenuBuilder MenuBuilder(true, NULL);
		const FText SectionText = FText::FromString(FString::Printf(TEXT("Variable %s"), *Name.ToString()));

		MenuBuilder.BeginSection("VariableDropped", SectionText);

		MenuBuilder.AddMenuEntry(
			FText::FromString(FString::Printf(TEXT("Get %s"), *Name.ToString())),
			FText::FromString(FString::Printf(TEXT("Adds a getter node for variable %s"), *Name.ToString())),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([AddNodeLambda, ParentGraph, Location, Name = Name, TypeName = TypeName, ObjectPath = ObjectPath]
				{
					AddNodeLambda(ParentGraph, Name, TypeName, ObjectPath, Location, true);
				}),
				FCanExecuteAction()
			)
		);

		MenuBuilder.AddMenuEntry(
			FText::FromString(FString::Printf(TEXT("Set %s"), *Name.ToString())),
			FText::FromString(FString::Printf(TEXT("Adds a setter node for variable %s"), *Name.ToString())),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([AddNodeLambda, ParentGraph, Location, Name = Name, TypeName = TypeName, ObjectPath = ObjectPath]
				{
					AddNodeLambda(ParentGraph, Name, TypeName, ObjectPath, Location, false);
				}),
				FCanExecuteAction()
			)
		);

		MenuBuilder.EndSection();

		FSlateApplication::Get().PushMenu(
			FSlateApplication::Get().GetInteractiveTopLevelWindows()[0],
			FWidgetPath(),
			MenuBuilder.MakeWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

		return nullptr;
	}
	else
	{
		const bool bIsGetter = VariableAccessorChoice == EVariableAccessorChoice::Get;
		return AddNodeLambda(ParentGraph, Name, TypeName, ObjectPath, Location, bIsGetter);
	}
}

// *** Add Comment ***

FAnimNextSchemaAction_AddComment::FAnimNextSchemaAction_AddComment()
	: FAnimNextSchemaAction(FText(), LOCTEXT("AddComment", "Add Comment..."), LOCTEXT("AddCommentTooltip", "Create a resizable comment box."))
{
}

UEdGraphNode* FAnimNextSchemaAction_AddComment::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode_Comment* const CommentTemplate = NewObject<UEdGraphNode_Comment>();

	FVector2f SpawnLocation = Location;
	FSlateRect Bounds;

	TSharedPtr<SGraphEditor> GraphEditorPtr = SGraphEditor::FindGraphEditorForGraph(ParentGraph);
	if (GraphEditorPtr.IsValid() && GraphEditorPtr->GetBoundsForSelectedNodes(/*out*/ Bounds, 50.0f))
	{
		CommentTemplate->SetBounds(Bounds);
		SpawnLocation.X = CommentTemplate->NodePosX;
		SpawnLocation.Y = CommentTemplate->NodePosY;
	}

	return FEdGraphSchemaAction_NewNode::SpawnNodeFromTemplate<UEdGraphNode_Comment>(ParentGraph, CommentTemplate, SpawnLocation, bSelectNewNode);
}

const FSlateBrush* FAnimNextSchemaAction_AddComment::GetIconBrush() const
{
	return FAppStyle::Get().GetBrush("Icons.Comment");
}

// *** Graph Function ***

FAnimNextSchemaAction_Function::FAnimNextSchemaAction_Function(const FRigVMGraphFunctionHeader& InReferencedPublicFunctionHeader, const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, const FText& InKeywords)
	: FAnimNextSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
{
	ReferencedPublicFunctionHeader = InReferencedPublicFunctionHeader;
	NodeClass = UAnimNextEdGraphNode::StaticClass();
	bIsLocalFunction = true;
}

FAnimNextSchemaAction_Function::FAnimNextSchemaAction_Function(const URigVMLibraryNode* InFunctionLibraryNode, const FText& InNodeCategory, const FText& InMenuDesc, const FText& InToolTip, const FText& InKeywords)
	: FAnimNextSchemaAction(InNodeCategory, InMenuDesc, InToolTip, InKeywords)
{
	ReferencedPublicFunctionHeader = InFunctionLibraryNode->GetFunctionHeader();
	NodeClass = UAnimNextEdGraphNode::StaticClass();
	bIsLocalFunction = true;
}

const FSlateBrush* FAnimNextSchemaAction_Function::GetIconBrush() const
{
	return FAppStyle::GetBrush("GraphEditor.Function_16x");
}

UEdGraphNode* FAnimNextSchemaAction_Function::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
{
	IRigVMClientHost* Host = ParentGraph->GetImplementingOuter<IRigVMClientHost>();
	URigVMEdGraphNode* NewNode = nullptr;
	URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(ParentGraph);

	UEdGraphPin* FromPin = nullptr;
	if (FromPins.Num() > 0)
	{
		FromPin = FromPins[0];
	}

	if (Host != nullptr && EdGraph != nullptr)
	{
		FName Name = UE::AnimNext::Editor::FUtils::ValidateName(Cast<UObject>(Host), ReferencedPublicFunctionHeader.Name.ToString());
		URigVMController* Controller = EdGraph->GetController();

		Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Node"), *Name.ToString()));

		if (URigVMFunctionReferenceNode* ModelNode = Controller->AddFunctionReferenceNodeFromDescription(ReferencedPublicFunctionHeader, FDeprecateSlateVector2D(Location), Name.ToString(), true, true))
		{
			NewNode = Cast<URigVMEdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
			check(NewNode);

			if (NewNode)
			{
				Controller->ClearNodeSelection(true, true);
				Controller->SelectNode(ModelNode, true, true, true);
			}
			Controller->CloseUndoBracket();
		}
		else
		{
			Controller->CancelUndoBracket();
		}
	}

	return NewNode;
}

#undef LOCTEXT_NAMESPACE
