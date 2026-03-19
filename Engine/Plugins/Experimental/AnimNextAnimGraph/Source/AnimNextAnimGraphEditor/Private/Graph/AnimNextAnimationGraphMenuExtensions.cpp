// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimationGraphMenuExtensions.h"

#include "AnimGraphUncookedOnlyUtils.h"
#include "AnimNextEdGraphNode.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "ToolMenus.h"
#include "UncookedOnlyUtils.h"
#include "EdGraph/EdGraphNode.h"
#include "Framework/Application/SlateApplication.h"
#include "Graph/RigDecorator_AnimNextCppTrait.h"
#include "Graph/RigUnit_AnimNextRunAnimationGraph_v1.h"
#include "Graph/RigUnit_AnimNextRunAnimationGraph_v2.h"
#include "Graph/RigUnit_AnimNextTraitStack.h"
#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "TraitCore/TraitRegistry.h"
#include "AnimNextController.h"
#include "AnimNextTraitStackUnitNode.h"
#include "AnimNextRigVMAssetEditorData.h"
#include "RigVMModel/RigVMClient.h"

#define LOCTEXT_NAMESPACE "FAnimationGraphMenuExtensions"

namespace UE::AnimNext::Editor::Private
{
	static const FLazyName VariablesTraitBaseName = TEXT("Variables");
}

namespace UE::AnimNext::Editor
{

void FAnimationGraphMenuExtensions::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(TEXT("FAnimNextAnimationGraphItemDetails"));
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("GraphEditor.GraphNodeContextMenu.AnimNextEdGraphNode");
	if (Menu == nullptr)
	{
		return;
	}

	Menu->AddDynamicSection(TEXT("AnimNextEdGraphNode"), FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
	{
		UGraphNodeContextMenuContext* Context = InMenu->FindContext<UGraphNodeContextMenuContext>();
		if(Context == nullptr)
		{
			return;
		}

		const UAnimNextEdGraphNode* AnimNextEdGraphNode = Cast<UAnimNextEdGraphNode>(Context->Node);
		if(AnimNextEdGraphNode == nullptr)
		{
			return;
		}

		URigVMNode* ModelNode = AnimNextEdGraphNode->GetModelNode();
		if(ModelNode == nullptr)
		{
			return;
		}

		auto IsRunGraphNode = [](URigVMNode* InModelNode)
		{
			if (const URigVMUnitNode* VMNode = Cast<URigVMUnitNode>(InModelNode))
			{
				const UScriptStruct* ScriptStruct = VMNode->GetScriptStruct();
				return ScriptStruct == FRigUnit_AnimNextRunAnimationGraph_v1::StaticStruct() || ScriptStruct == FRigUnit_AnimNextRunAnimationGraph_v2::StaticStruct();
			}

			return false;
		};
		
		if (UncookedOnly::FAnimGraphUtils::IsTraitStackNode(ModelNode))
		{
			FToolMenuSection& Section = InMenu->AddSection("AnimNextTraitNodeActions", LOCTEXT("AnimNextTraitNodeActionsMenuHeader", "Traits"));

			auto BuildAddTraitContextMenu = [AnimNextEdGraphNode, ModelNode](UToolMenu* InSubMenu)
			{
				const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();
				TArray<const FTrait*> Traits = TraitRegistry.GetTraits();

				URigVMController* VMController = AnimNextEdGraphNode->GetController();

				const UScriptStruct* CppDecoratorStruct = FRigDecorator_AnimNextCppDecorator::StaticStruct();

				for (const FTrait* Trait : Traits)
				{
					UScriptStruct* ScriptStruct = Trait->GetTraitSharedDataStruct();
			
					FString DefaultValue;
					{
						const FRigDecorator_AnimNextCppDecorator DefaultCppDecoratorStructInstance;
						FRigDecorator_AnimNextCppDecorator CppDecoratorStructInstance;
						CppDecoratorStructInstance.DecoratorSharedDataStruct = ScriptStruct;
			
						if (!CppDecoratorStructInstance.CanBeAddedToNode(ModelNode, nullptr))
						{
							continue;	// This trait isn't supported on this node
						}

						FRigDecorator_AnimNextCppDecorator::StaticStruct()->ExportText(DefaultValue, &CppDecoratorStructInstance, &DefaultCppDecoratorStructInstance, nullptr, PPF_SerializedAsImportText, nullptr);
					}
			
					FString DisplayNameMetadata;
					ScriptStruct->GetStringMetaDataHierarchical(FRigVMStruct::DisplayNameMetaName, &DisplayNameMetadata);
					const FString DisplayName = DisplayNameMetadata.IsEmpty() ? Trait->GetTraitName() : DisplayNameMetadata;
					const FText ToolTip = ScriptStruct->GetToolTipText();
			
					FToolMenuEntry TraitEntry = FToolMenuEntry::InitMenuEntry(
						*Trait->GetTraitName(),
						FText::FromString(DisplayName),
						ToolTip,
						FSlateIcon(),
						FUIAction(FExecuteAction::CreateLambda(
							[Trait, VMController, ModelNode, CppDecoratorStruct, DefaultValue, DisplayName]()
							{
								VMController->AddTrait(
									ModelNode->GetFName(),
									*CppDecoratorStruct->GetPathName(),
									*DisplayName,
									DefaultValue, INDEX_NONE, true, true);
							})
						)
					);

					InSubMenu->AddMenuEntry(NAME_None, TraitEntry);
				}
			};
			
			Section.AddSubMenu(
				TEXT("AddTraitMenu"),
				LOCTEXT("AddTraitMenu", "Add Trait"),
				LOCTEXT("AddTraitMenuTooltip", "Add the chosen trait to currently selected node"),
				FNewToolMenuDelegate::CreateLambda(BuildAddTraitContextMenu));
		}
		else if(IsRunGraphNode(ModelNode))
		{
			FToolMenuSection& Section = InMenu->AddSection("AnimNextRunAnimGraphNodeActions", LOCTEXT("AnimNextAnimGraphNodeActionsMenuHeader", "Animation Graph"));
	
			URigVMController* VMController = AnimNextEdGraphNode->GetController();
			URigVMPin* VMPin = Context->Pin != nullptr ? AnimNextEdGraphNode->FindModelPinFromGraphPin(Context->Pin) : nullptr;
	
			if(VMPin != nullptr && ModelNode->FindTrait(VMPin))
			{
				Section.AddMenuEntry(
					TEXT("RemoveExposedVariables"),
					LOCTEXT("RemoveExposedVariablesMenu", "Remove Exposed Variables"),
					LOCTEXT("RemoveExposeVariablesMenuTooltip", "Remove the exposed variable trait from this node"),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([VMController, ModelNode, Name = VMPin->GetFName()]()
						{
							VMController->RemoveTrait(ModelNode->GetFName(), Name, true, true);
						})
					));
			}
			else
			{
				auto BuildExposeVariablesContextMenu = [AnimNextEdGraphNode, ModelNode](UToolMenu* InSubMenu)
				{
					URigVMController* VMController = AnimNextEdGraphNode->GetController();

					FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

					FAssetPickerConfig AssetPickerConfig;
					AssetPickerConfig.Filter.ClassPaths.Add(UAnimNextDataInterface::StaticClass()->GetClassPathName());
					AssetPickerConfig.Filter.bRecursiveClasses = true;
					AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
					AssetPickerConfig.AssetShowWarningText = LOCTEXT("NoAssetsWithPublicVariablesMessage", "No animation graphs with public variables found");
					AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([VMController, ModelNode](const FAssetData& InAssetData)
					{
						FSlateApplication::Get().DismissAllMenus();

						FString DefaultValue;
						FRigVMTrait_AnimNextPublicVariables DefaultTrait;
						FRigVMTrait_AnimNextPublicVariables NewTrait;
						UAnimNextDataInterface* Asset = CastChecked<UAnimNextDataInterface>(InAssetData.GetAsset());
						NewTrait.InternalAsset = Asset;
						const FInstancedPropertyBag& PublicVariableDefaults = Asset->GetPublicVariableDefaults();
						TConstArrayView<FPropertyBagPropertyDesc> Descs = PublicVariableDefaults.GetPropertyBagStruct()->GetPropertyDescs();
						NewTrait.InternalVariableNames.Reserve(Descs.Num());
						for(const FPropertyBagPropertyDesc& Desc : Descs)
						{
							NewTrait.InternalVariableNames.Add(Desc.Name);
						}
						FRigVMTrait_AnimNextPublicVariables::StaticStruct()->ExportText(DefaultValue, &NewTrait, &DefaultTrait, nullptr, PPF_SerializedAsImportText, nullptr);

						const FName ValidTraitName = URigVMSchema::GetUniqueName(Private::VariablesTraitBaseName, [ModelNode](const FName& InName)
						{
							return ModelNode->FindPin(InName.ToString()) == nullptr;
						}, false, false);

						VMController->AddTrait(ModelNode->GetFName(), *FRigVMTrait_AnimNextPublicVariables::StaticStruct()->GetPathName(), ValidTraitName, DefaultValue, INDEX_NONE, true, true);
					});

					AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateLambda([](const FAssetData& InAssetData)
					{
						// Filter to only show assets with public variables
						FAnimNextAssetRegistryExports Exports;
						if(UE::AnimNext::UncookedOnly::FUtils::GetExportedVariablesForAsset(InAssetData, Exports))
						{
							for(const FAnimNextAssetRegistryExportedVariable& Export : Exports.Variables)
							{
								if((Export.GetFlags() & EAnimNextExportedVariableFlags::Public) != EAnimNextExportedVariableFlags::NoFlags)
								{
									return false;
								}
							}
						}
						return true;
					});

					FToolMenuEntry Entry = FToolMenuEntry::InitWidget(
						TEXT("AnimationGraphPicker"),
						SNew(SBox)
						.WidthOverride(300.0f)
						.HeightOverride(400.0f)
						[
							ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
						],
						FText::GetEmpty(),
						true,
						false,
						false,
						LOCTEXT("AnimationGraphPickerTooltip", "Choose an animation graph with public variables to expose")
					);

					InSubMenu->AddMenuEntry(NAME_None, Entry);
				};
				
				Section.AddSubMenu(
					TEXT("ExposeVariables"),
					LOCTEXT("ExposeVariablesMenu", "Expose Variables"),
					LOCTEXT("ExposeVariablesMenuTooltip", "Expose the variables of a selected animation graph as pins on this node"),
					FNewToolMenuDelegate::CreateLambda(BuildExposeVariablesContextMenu));
			}
		}

		if (UAnimNextTraitStackUnitNode* AnimNextUnitNode = Cast<UAnimNextTraitStackUnitNode>(ModelNode))
		{
			AddManifestSection(InMenu, AnimNextEdGraphNode, ModelNode);
		}
	}));
}

void FAnimationGraphMenuExtensions::UnregisterMenus()
{
	if(UToolMenus* ToolMenus = UToolMenus::Get())
	{
		ToolMenus->UnregisterOwnerByName("FAnimNextAnimationGraphItemDetails");
	}
}

void FAnimationGraphMenuExtensions::AddManifestSection(UToolMenu* InMenu, const UAnimNextEdGraphNode* AnimNextEdGraphNode, URigVMNode* ModelNode)
{
	FToolMenuSection& Section = InMenu->AddSection("AnimNextManifestNodeActions", LOCTEXT("AnimNextManifestNodeActionsMenuHeader", "Manifest"));

	const TWeakObjectPtr<const UAnimNextEdGraphNode> EdNodeWeak = AnimNextEdGraphNode;

	if (!UE::AnimNext::UncookedOnly::FAnimGraphUtils::IsExposedToManifest(ModelNode))
	{
		Section.AddMenuEntry(
			"AddTraitToManifest",
			LOCTEXT("AddTraitToManifest", "Add to Manifest"),
			LOCTEXT("AddTraitToManifest_Tooltip", "Adds this Trait Stack to the Manifest."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([EdNodeWeak]()
				{
					if (const TStrongObjectPtr<const UAnimNextEdGraphNode> EdGraphNode = EdNodeWeak.Pin())
					{
						if (URigVMNode* ModelNode = EdGraphNode->GetModelNode())
						{
							UAnimNextController* Controller = CastChecked<UAnimNextController>(EdGraphNode->GetController());

							if (ensure(UE::AnimNext::UncookedOnly::FAnimGraphUtils::IsExposedToManifest(ModelNode) == false))
							{
								FRigVMControllerCompileBracketScope CompileScope(Controller);

								Controller->AddNodeToManifest(ModelNode, true, true);
								
								if (UAnimNextRigVMAssetEditorData* EditorData = EdGraphNode->GetGraph()->GetTypedOuter<UAnimNextRigVMAssetEditorData>())
								{
									UE::AnimNext::UncookedOnly::FAnimGraphUtils::RequestVMAutoRecompile(EditorData);  // required to force an asset tags update
								}
							}
						}								
					}
				}))
			);
	}
	else
	{
		Section.AddMenuEntry(
			"RemoveTraitFromManifest",
			LOCTEXT("RemoveTraitFromManifest", "Remove from Manifest"),
			LOCTEXT("RemoveTraitFromManifest_Tooltip", "Remvoes this Trait Stack from the Manifest."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([EdNodeWeak]()
				{
					if (const TStrongObjectPtr<const UAnimNextEdGraphNode> EdGraphNode = EdNodeWeak.Pin())
					{
						UAnimNextController* Controller = CastChecked<UAnimNextController>(EdGraphNode->GetController());

						FRigVMControllerCompileBracketScope CompileScope(Controller);

						Controller->RemoveNodeFromManifest(EdGraphNode->GetModelNode(), true, true);
						
						if (UAnimNextRigVMAssetEditorData* EditorData = EdGraphNode->GetGraph()->GetTypedOuter<UAnimNextRigVMAssetEditorData>())
						{
							UE::AnimNext::UncookedOnly::FAnimGraphUtils::RequestVMAutoRecompile(EditorData); // required to force an asset tags update
						}
					}
				}))
		);
	}
}

}

#undef LOCTEXT_NAMESPACE