// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphWorkspaceAssetUserData.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ISettingsModule.h"
#include "AnimNextAnimGraphSettings.h"
#include "EdGraphUtilities.h"
#include "IAnimNextEditorModule.h"
#include "IWorkspaceEditorModule.h"
#include "PropertyEditorModule.h"
#include "TraitStackEditor.h"
#include "AnimGraphUncookedOnlyUtils.h"
#include "Entries/AnimNextAnimationGraphEntry.h"
#include "Features/IModularFeatures.h"
#include "Framework/Docking/LayoutExtender.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNextAnimationGraphItemDetails.h"
#include "Graph/AnimNextAnimationGraphMenuExtensions.h"
#include "Graph/AnimNextAnimationGraph_EditorData.h"
#include "Graph/AnimNextGraphPanelNodeFactory.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"
#include "Graph/TraitEditorTabSummoner.h"
#include "Traits/AnimNextCallFunctionSharedDataDetails.h"
#include "Traits/CallFunction.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "IWorkspaceEditor.h"
#include "Graph/AnimGraphEditorSchemaActions.h"
#include "Graph/AnimNextAnimationGraphSchema.h"
#include "Common/SActionMenu.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"
#include "AnimNextTraitStackUnitNode.h"
#include "RigVMCore/RigVMFunction.h"
#include "EditorUtils.h"
#include "Graph/AnimNextAnimGraph.h"
#include "Graph/AnimNextGraphDetails.h"
#include "Graph/PostProcessAnimationCustomization.h"
#include "Module/AnimNextEventGraphSchema.h"
#include "RewindDebugger/AnimNextAnimGraphTraceModule.h"
#include "RewindDebugger/EvaluationProgramTrack.h"
#include "RewindDebugger/SequenceInfoTrack.h"
#include "PersonaModule.h"
#include "Developer/TraceServices/Public/TraceServices/ModuleService.h"

#define LOCTEXT_NAMESPACE "FAnimNextAnimGraphEditorModule"

namespace
{
	FAnimNextAnimGraphTraceModule GAnimNextAnimGraphTraceModule;
	UE::AnimNextEditor::FEvaluationProgramTrackCreator GAnimNextModulesTrackCreator;
	UE::AnimNextEditor::FSequenceInfoTrackCreator GSequenceInfoTrackCreator;
}

namespace UE::AnimNext::Editor
{

class FAnimNextAnimGraphEditorModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		// Register settings
		ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
		SettingsModule.RegisterSettings("Project", "Plugins", "AnimNextAnimGraph",
			LOCTEXT("SettingsName", "UAF Anim Graph"),
			LOCTEXT("SettingsDescription", "Configure options for UAF animation graphs."),
			GetMutableDefault<UAnimNextAnimGraphSettings>());

		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout(
			FAnimNextCallFunctionSharedData::StaticStruct()->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FCallFunctionSharedDataDetails>(); }));
		PropertyModule.RegisterCustomPropertyTypeLayout(
					FAnimNextAnimGraph::StaticStruct()->GetFName(),
					FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<FAnimNextGraphDetails>(); }));
					
		PropertyModule.RegisterCustomPropertyTypeLayout(
							FAnimNextSequenceTraceInfo::StaticStruct()->GetFName(),
							FOnGetPropertyTypeCustomizationInstance::CreateLambda([] { return MakeShared<AnimNextEditor::FAnimNextSequenceTraceInfoCustomization>(); }));					
					
		IModularFeatures::Get().RegisterModularFeature(TraceServices::ModuleFeatureName, &GAnimNextAnimGraphTraceModule);
		IModularFeatures::Get().RegisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, &GAnimNextModulesTrackCreator);
		IModularFeatures::Get().RegisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, &GSequenceInfoTrackCreator);

		Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<Workspace::IWorkspaceEditorModule>("WorkspaceEditor");

		// --- AnimNextAnimationGraph ---
		Workspace::FObjectDocumentArgs AnimNextAnimationGraphDocumentArgs(
			Workspace::FOnRedirectWorkspaceContext::CreateLambda([](UObject* InObject) -> UObject*
			{
				URigVMEdGraph* EdGraph = nullptr;

				UAnimNextAnimationGraph* AnimationGraph = CastChecked<UAnimNextAnimationGraph>(InObject);
				UAnimNextAnimationGraph_EditorData* EditorData = UncookedOnly::FUtils::GetEditorData<UAnimNextAnimationGraph_EditorData>(AnimationGraph);

				UAnimNextAnimationGraphEntry* AnimationGraphEntry = Cast<UAnimNextAnimationGraphEntry>(EditorData->FindEntry(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint));
				// Redirect to the inner graph
				if (ensure(AnimationGraphEntry))
				{
					EdGraph = AnimationGraphEntry->GetEdGraph();
				}
				return EdGraph;
			}));

		WorkspaceEditorModule.RegisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNextAnimGraph.AnimNextAnimationGraph")), AnimNextAnimationGraphDocumentArgs);
		
		WorkspaceEditorModule.OnRegisterTabsForEditor().AddLambda([](FWorkflowAllowedTabSet& TabFactories, const TSharedRef<FTabManager>& InTabManager, TSharedPtr<UE::Workspace::IWorkspaceEditor> InEditorPtr)
		{
			TSharedRef<FTraitEditorTabSummoner> TraitEditorTabSummoner = MakeShared<FTraitEditorTabSummoner>(InEditorPtr);
			TabFactories.RegisterFactory(TraitEditorTabSummoner);
			TraitEditorTabSummoner->RegisterTabSpawner(InTabManager, nullptr);
		});
		
		WorkspaceEditorModule.OnExtendTabs().AddLambda([](FLayoutExtender& InLayoutExtender, TSharedPtr<UE::Workspace::IWorkspaceEditor> InEditorPtr)
		{
			FTabManager::FTab TraitEditorTab(FTabId(TraitEditorTabName), ETabState::ClosedTab);
			InLayoutExtender.ExtendLayout(FTabId(Workspace::WorkspaceTabs::TopRightDocumentArea), ELayoutExtensionPosition::After, TraitEditorTab);
		});
		
		const TSharedPtr<FAnimNextAnimationGraphItemDetails> AssetItemDetails = MakeShared<FAnimNextAnimationGraphItemDetails>();
		WorkspaceEditorModule.RegisterWorkspaceItemDetails(Workspace::FOutlinerItemDetailsId(FAnimNextAnimationGraphOutlinerData::StaticStruct()->GetFName()), AssetItemDetails);

		IAnimNextEditorModule& AnimNextEditorModule = FModuleManager::LoadModuleChecked<IAnimNextEditorModule>("AnimNextEditor");
		AnimNextEditorModule.AddWorkspaceSupportedAssetClass(UAnimNextAnimationGraph::StaticClass()->GetClassPathName());
		CollectMenuActionsDelegateHandle = AnimNextEditorModule.RegisterGraphMenuActionsProvider(IAnimNextEditorModule::FOnCollectGraphMenuActionsDelegate::CreateStatic(CollectContextMenuActions));

		TraitStackEditor = MakeShared<FTraitStackEditor>();
		IModularFeatures::Get().RegisterModularFeature(ITraitStackEditor::ModularFeatureName, TraitStackEditor.Get());

		AnimNextGraphPanelNodeFactory = MakeShared<FAnimNextGraphPanelNodeFactory>();
		FEdGraphUtilities::RegisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);

		FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");
		TArray<FPersonaModule::FOnCustomizeMeshDetails>& CustomizeMeshDetailsDelegates = PersonaModule.GetCustomizeMeshDetailsDelegates();
		CustomizeMeshDetailsDelegates.Add(FPersonaModule::FOnCustomizeMeshDetails::CreateStatic(&FPostProcessAnimationCustomization::OnCustomizeMeshDetails));

		FAnimationGraphMenuExtensions::RegisterMenus();
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "AnimNextAnimGraph");
		}
		
		IModularFeatures::Get().UnregisterModularFeature(TraceServices::ModuleFeatureName, &GAnimNextAnimGraphTraceModule);
		IModularFeatures::Get().UnregisterModularFeature(RewindDebugger::IRewindDebuggerTrackCreator::ModularFeatureName, &GAnimNextModulesTrackCreator);

		if(FModuleManager::Get().IsModuleLoaded("WorkspaceEditor"))
		{
			Workspace::IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::LoadModuleChecked<Workspace::IWorkspaceEditorModule>("WorkspaceEditor");
			WorkspaceEditorModule.UnregisterObjectDocumentType(FTopLevelAssetPath(TEXT("/Script/AnimNextAnimGraph.AnimNextAnimationGraph")));
			if(UObjectInitialized())
			{
				WorkspaceEditorModule.UnregisterWorkspaceItemDetails(Workspace::FOutlinerItemDetailsId(FAnimNextAnimationGraphOutlinerData::StaticStruct()->GetFName()));
			}
		}

		if(FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout("AnimNextCallFunctionSharedData");
			PropertyModule.UnregisterCustomClassLayout("AnimNextAnimGraph");
		}

		IModularFeatures::Get().UnregisterModularFeature(ITraitStackEditor::ModularFeatureName, TraitStackEditor.Get());
		TraitStackEditor.Reset();

		if (IAnimNextEditorModule* AnimNextEditorModule = FModuleManager::GetModulePtr<IAnimNextEditorModule>("AnimNextEditor"))
		{
			AnimNextEditorModule->UnregisterGraphMenuActionsProvider(CollectMenuActionsDelegateHandle);
		}

		FEdGraphUtilities::UnregisterVisualNodeFactory(AnimNextGraphPanelNodeFactory);
		
		FAnimationGraphMenuExtensions::UnregisterMenus();
	}

	static void CollectContextMenuActions(const TWeakPtr<UE::Workspace::IWorkspaceEditor>& InWorkspaceEditorWeak, FGraphContextMenuBuilder& InContexMenuBuilder, const FActionMenuContextData& ActionMenuContextData)
	{
		if (const URigVMEdGraph* RigVMEdGraph = Cast<URigVMEdGraph>(InContexMenuBuilder.CurrentGraph))
		{
			if (RigVMEdGraph->GetModel()->GetSchemaClass() == UAnimNextAnimationGraphSchema::StaticClass())
			{
				TArray<FAnimNextAssetRegistryExports> ManifestExports;
				UE::AnimNext::UncookedOnly::FAnimGraphUtils::GetExportedManifestNodesFromAssetRegistry(ManifestExports);

				TArray<FAssetData> WorkspaceAssets;
				if (TSharedPtr<UE::Workspace::IWorkspaceEditor> WorkspaceEditor = InWorkspaceEditorWeak.Pin())
				{
					WorkspaceEditor->GetAssets(WorkspaceAssets);
				}

				for (const FAnimNextAssetRegistryExports& ManifestExport : ManifestExports)
				{
					for (const FAnimNextAssetRegistryManifestNode& ManifestNodeData : ManifestExport.ManifestNodes)
					{
						bool bIncludeManifestNode = ActionMenuContextData.bShowGlobalManifestNodes;

						if (!ActionMenuContextData.bShowGlobalManifestNodes)
						{
							if (WorkspaceAssets.Num() > 0)
							{
								for (const FAssetData& WorkspaceAsset : WorkspaceAssets)
								{
									if (WorkspaceAsset.PackageName != ManifestNodeData.ModelGraph.GetLongPackageName())
									{
										continue;
									}
									bIncludeManifestNode = true;
								}
							}
						}

						if (bIncludeManifestNode)
						{
							InContexMenuBuilder.AddAction(MakeShared<FAnimNextSchemaAction_AddManifestNode>(ManifestNodeData));
						}
					}
				}
			}
			else if (RigVMEdGraph->GetModel()->GetSchemaClass() == UAnimNextEventGraphSchema::StaticClass())
			{
				InContexMenuBuilder.AddAction(MakeShared<FAnimNextSchemaAction_NotifyEvent>());
			}
		}

		// Add trait stack using a custom RigUnit node class
		UScriptStruct* Struct = FRigUnit_AnimNextTraitStack::StaticStruct();
		const FString FunctionName = FString::Printf(TEXT("%s::%s"), *Struct->GetStructCPPName(), *FRigVMStruct::ExecuteName.ToString());
		const FRigVMFunction* Function = FRigVMRegistry::Get().FindFunction(*FunctionName);
		if (ensure(Function != nullptr))
		{
			if (ActionMenuContextData.RigVMSchema != nullptr && ActionMenuContextData.RigVMSchema->SupportsUnitFunction(ActionMenuContextData.RigVMController, Function))
			{
				UE::AnimNext::Editor::FUtils::AddSchemaRigUnitAction(UAnimNextTraitStackUnitNode::StaticClass(), Struct, *Function, InContexMenuBuilder);
			}
		}
	}

	/** Graph context menu collect actions delegate handle */
	FDelegateHandle CollectMenuActionsDelegateHandle;

	/** Trait stack editor modular feature */
	TSharedPtr<FTraitStackEditor> TraitStackEditor;

	/** Node factory for the AnimNext graph */
	TSharedPtr<FAnimNextGraphPanelNodeFactory> AnimNextGraphPanelNodeFactory;
};

} // end namespace

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::AnimNext::Editor::FAnimNextAnimGraphEditorModule, AnimNextAnimGraphEditor);

