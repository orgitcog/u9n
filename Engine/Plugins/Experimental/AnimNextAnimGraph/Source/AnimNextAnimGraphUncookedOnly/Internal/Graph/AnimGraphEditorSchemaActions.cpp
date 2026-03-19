// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphEditorSchemaActions.h"
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
#include "UncookedOnlyUtils.h"
#include "AnimNextController.h"
#include "AnimGraphUncookedOnlyUtils.h"
#include "Editor/RigVMEditorTools.h"
#include "Exporters/Exporter.h"
#include "UnrealExporter.h"
#include "AnimNextUnitNode.h"
#include "PersonaModule.h"
#include "ScopedTransaction.h"
#include "Modules/ModuleManager.h"
#include "RigVMFunctions/Execution/RigVMFunction_UserDefinedEvent.h"
#include "Widgets/Input/STextEntryPopup.h"

#define LOCTEXT_NAMESPACE "AnimNextAnimGraphSchemaActions"

namespace Private
{
static const FText ManifestMenuElementPrefix(LOCTEXT("ManifestMenuElementCategoryPrefix", "(M) - {0}"));
}

// --- Manifest Node ---

FAnimNextSchemaAction_AddManifestNode::FAnimNextSchemaAction_AddManifestNode(const FAnimNextAssetRegistryManifestNode& InManifestNodeData, const FText& InKeywords)
	: FAnimNextSchemaAction(FText::Format(Private::ManifestMenuElementPrefix, FText::FromString(InManifestNodeData.NodeCategory)), FText::FromString(InManifestNodeData.MenuDesc), FText::FromString(InManifestNodeData.ToolTip), InKeywords)
	, ModelGraph(InManifestNodeData.ModelGraph)
	, NodeName(InManifestNodeData.NodeName)
{
}

UEdGraphNode* FAnimNextSchemaAction_AddManifestNode::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
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
		if (UAnimNextController* Controller = Cast<UAnimNextController>(EdGraph->GetController()))
		{
			FName ValidName = UE::AnimNext::Editor::FUtils::ValidateName(Cast<UObject>(Host), NodeName);

			Controller->OpenUndoBracket(FString::Printf(TEXT("Add '%s' Manifest Node"), *ValidName.ToString()));

			const URigVMGraph* SourceGraph = ModelGraph.LoadSynchronous();
			check(SourceGraph != nullptr);
			URigVMNode* SourceNode = SourceGraph->FindNode(NodeName);

			FStringOutputDevice Archive;
			const FExportObjectInnerContext Context;

			UExporter::ExportToOutputDevice(&Context, SourceNode, NULL, Archive, TEXT("copy"), 0, PPF_ExportsNotFullyQualified | PPF_Copy | PPF_Delimited, false, SourceNode->GetOuter());

			const FString NodeData = MoveTemp(Archive);

			TArray<FName> NodeNamesCreated = UE::RigVM::Editor::Tools::ImportNodesFromText(FDeprecateSlateVector2D(Location), NodeData, Controller, EdGraph->GetModel(), Host->GetLocalFunctionLibrary(), Host->GetRigVMGraphFunctionHost(), true, true);

			TArray<URigVMNode*> ModelNodes;
			for (const FName& Name : NodeNamesCreated)
			{
				URigVMNode* Node = EdGraph->GetModel()->FindNodeByName(Name);
				if (Node == nullptr)
				{
					return nullptr;
				}
				ModelNodes.AddUnique(Node);
			}
			if (URigVMNode* ModelNode = ModelNodes.Num() > 0 ? ModelNodes[0] : nullptr)
			{
				NewNode = Cast<URigVMEdGraphNode>(EdGraph->FindNodeForModelNodeName(ModelNode->GetFName()));
				check(NewNode);

				if (NewNode)
				{
					Controller->RemoveNodeFromManifest(ModelNode, false, true);
				}
				Controller->CloseUndoBracket();
			}
			else
			{
				Controller->CancelUndoBracket();
			}
		}
	}

	return NewNode;
}

FAnimNextSchemaAction_NotifyEvent::FAnimNextSchemaAction_NotifyEvent()
	: FAnimNextSchemaAction(LOCTEXT("NotifiesCategory", "Notifies"), LOCTEXT("AddNotifyEventLabel", "Add Notify Event..."), LOCTEXT("AddNotifyEventTooltip", "Add a custom event node to handle a named notify event"))
{
}

UEdGraphNode* FAnimNextSchemaAction_NotifyEvent::PerformAction(UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2f& Location, bool bSelectNewNode)
{
	URigVMEdGraph* EdGraph = Cast<URigVMEdGraph>(ParentGraph);
	URigVMController* Controller = Cast<URigVMController>(EdGraph->GetController());
	if(Controller == nullptr)
	{
		return nullptr;
	}

	FMenuBuilder MenuBuilder(true, nullptr);
	MenuBuilder.BeginSection("AddNotify", LOCTEXT("AddNotifyEventSection", "Add Notify Event"));

	auto CreateNotifyEventWithName = [Controller, Location](FName InNotifyEventName)
	{
		if(!Controller->GetAllEventNames().Contains(InNotifyEventName))
		{
			Controller->OpenUndoBracket(LOCTEXT("AddNotifyEventTransaction", "Add Notify Event").ToString());
			URigVMUnitNode* CustomEventNode = Controller->AddUnitNode(FRigVMFunction_UserDefinedEvent::StaticStruct(), FRigVMStruct::ExecuteName, FDeprecateSlateVector2D(Location));
			Controller->SetPinDefaultValue(CustomEventNode->FindPin(GET_MEMBER_NAME_STRING_CHECKED(FRigVMFunction_UserDefinedEvent, EventName))->GetPinPath(), InNotifyEventName.ToString(), true, true, true, true, true);
			Controller->CloseUndoBracket();
		}
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddNewNotifyEventLabel", "Add New Notify Event..."),
		LOCTEXT("AddNewNotifyEventTooltip", "Add a new notify event as a custom event"),
		FSlateIcon(),
		FUIAction(FExecuteAction::CreateLambda([CreateNotifyEventWithName]()
		{
			// Show dialog to enter new track name
			TSharedRef<STextEntryPopup> TextEntry =
				SNew(STextEntryPopup)
				.Label(LOCTEXT("NewNotifyLabel", "Notify Name"))
				.OnTextCommitted_Lambda([CreateNotifyEventWithName](const FText& InText, ETextCommit::Type InCommitType)
				{
					FSlateApplication::Get().DismissAllMenus();
					FName NotifyName = *InText.ToString();
					CreateNotifyEventWithName(NotifyName);
				});

			// Show dialog to enter new event name
			FSlateApplication::Get().PushMenu(
				FSlateApplication::Get().GetInteractiveTopLevelWindows()[0],
				FWidgetPath(),
				TextEntry,
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect( FPopupTransitionEffect::TypeInPopup));
		})),
		NAME_None,
		EUserInterfaceActionType::Button);
	
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	TSharedRef<SWidget> NotifyPickerWidget = PersonaModule.CreateSkeletonNotifyPicker(FOnNotifyPicked::CreateLambda([CreateNotifyEventWithName](FName InNotifyName)
	{
		FSlateApplication::Get().DismissAllMenus();
		CreateNotifyEventWithName(InNotifyName);
	}));

	MenuBuilder.AddWidget(
		SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(400.0f)
		[
			NotifyPickerWidget
		],
		FText::GetEmpty(),
		true, false);

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

#undef LOCTEXT_NAMESPACE
