// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ChaosClothAssetEditorModule.h"

#include "ChaosClothAsset/AssetDefinition_ClothAsset.h"
#include "ChaosClothAsset/ChaosClothAssetThumbnailRenderer.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothDataflowConstructionVisualization.h"
#include "ChaosClothAsset/ClothDataflowSimulationVisualization.h"
#include "ChaosClothAsset/ClothSimulationNodeDetailExtender.h"
#include "ChaosClothAsset/ClothEditorCommands.h"
#include "ChaosClothAsset/ClothEditorMode.h"
#include "ChaosClothAsset/ClothEditorOptions.h"
#include "ChaosClothAsset/ClothEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "EditorModeRegistry.h"
#include "Algo/AllOf.h"
#include "ContentBrowserMenuContexts.h"
#include "Dataflow/DataflowEditorModeUILayer.h"
#include "Dataflow/DataflowEditor.h"
#include "ComponentAssetBroker.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "ChaosClothAssetEditorModule"

namespace UE::Chaos::ClothAsset
{
	struct FClothAssetComponentBroker : public IComponentAssetBroker
	{
	public:
		UClass* GetSupportedAssetClass() override
		{
			return UChaosClothAsset::StaticClass();
		}

		virtual bool AssignAssetToComponent(UActorComponent* InComponent, UObject* InAsset) override
		{
			if (UChaosClothComponent* ClothComponent = Cast<UChaosClothComponent>(InComponent))
			{
				if (UChaosClothAsset* ClothAsset = Cast<UChaosClothAsset>(InAsset))
				{
					ClothComponent->SetAsset(ClothAsset);
					return true;
				}
			}
			return false;
		}

		virtual UObject* GetAssetFromComponent(UActorComponent* InComponent) override
		{
			if (UChaosClothComponent* ClothComponent = Cast<UChaosClothComponent>(InComponent))
			{
				return Cast<UChaosClothAsset>(ClothComponent->GetAsset());
			}
			return nullptr;
		}
	};

	void FChaosClothAssetEditorModule::RegisterMenus()
	{
		// Allows cleanup when module unloads.
		FToolMenuOwnerScoped OwnerScoped(this);

		// Enable opening ChaosClothAssets in the Dataflow Editor via the Content Browser context menu
		// (Note: this should be temporary until the Dataflow Editor becomes *the* editor for ChaosClothAssets)

		UToolMenu* const ClothContextMenu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.ChaosClothAsset");
		FToolMenuSection& Section = ClothContextMenu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry("OpenInDataflowEditor", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section)
		{
			// We'll need to get the target assets out of the context
			if (const UContentBrowserAssetContextMenuContext* const Context = Section.FindContext<UContentBrowserAssetContextMenuContext>())
			{
				// We are deliberately not using Context->GetSelectedObjects() here to avoid triggering a load from right clicking
				// an asset in the content browser.
				const bool bAllSelectedAssetsAreCloth = Algo::AllOf(Context->SelectedAssets, [](const FAssetData& Asset)
				{
					return Asset.AssetClassPath == UChaosClothAsset::StaticClass()->GetClassPathName();
				});

				if (bAllSelectedAssetsAreCloth)
				{
					UDataflowEditorUISubsystem* const DataflowEditorSubsystem = GEditor->GetEditorSubsystem<UDataflowEditorUISubsystem>();
					check(DataflowEditorSubsystem);

					const TSharedPtr<class FUICommandList> CommandListToBind = MakeShared<FUICommandList>();

					CommandListToBind->MapAction(
						FChaosClothAssetEditorCommands::Get().OpenClothAssetInDataflowEditor,
						FExecuteAction::CreateWeakLambda(DataflowEditorSubsystem, [Context, DataflowEditorSubsystem]()
						{
							// When we actually do want to open the editor, trigger the load to get the objects
							TArray<TObjectPtr<UObject>> AssetsToEdit;
							AssetsToEdit.Append(Context->LoadSelectedObjects<UObject>());

							UDataflowEditor* const AssetEditor = NewObject<UDataflowEditor>(DataflowEditorSubsystem, NAME_None, RF_Transient);
							AssetEditor->RegisterToolCategories({"General", "Cloth"});

							// Macke sure the cloth asset has a Dataflow asset
							UChaosClothAsset* const ClothAsset = CastChecked<UChaosClothAsset>(AssetsToEdit[0]);
							if (!ClothAsset->GetDataflow())
							{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
								if (UDataflow* const NewDataflowAsset = Cast<UDataflow>(UAssetDefinition_ClothAsset::NewOrOpenDataflowAsset(ClothAsset)))
								{
									ClothAsset->SetDataflow(NewDataflowAsset);
								}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
							}

							const TSubclassOf<AActor> PreviewActorClass = StaticLoadClass(AActor::StaticClass(), nullptr,
								TEXT("/ChaosClothAssetEditor/BP_ClothPreview.BP_ClothPreview_C"), nullptr, LOAD_None, nullptr);

							AssetEditor->Initialize({AssetsToEdit[0]}, PreviewActorClass);
						}),
						FCanExecuteAction::CreateWeakLambda(Context, [Context]() { return Context->bCanBeModified; }));

					const TAttribute<FText> ToolTipOverride = Context->bCanBeModified ? TAttribute<FText>() : LOCTEXT("ReadOnlyAssetWarning", "The selected asset(s) are read-only and cannot be edited.");
					Section.AddMenuEntryWithCommandList(FChaosClothAssetEditorCommands::Get().OpenClothAssetInDataflowEditor,
						CommandListToBind,
						TAttribute<FText>(),
						ToolTipOverride,
						FSlateIcon());		// TODO: If DataflowEditorStyle.h was public we could do this: FSlateIcon(FDataflowEditorStyle::Get().GetStyleSetName(), "ClassThumbnail.Dataflow")
				}
			}
		}));
	}

	void FChaosClothAssetEditorModule::StartupModule()
	{
		FChaosClothAssetEditorStyle::Get(); // Causes the constructor to be called

		FChaosClothAssetEditorCommands::Register();

		// Menus need to be registered in a callback to make sure the system is ready for them.
		StartupCallbackDelegateHandle = UToolMenus::RegisterStartupCallback(
			FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FChaosClothAssetEditorModule::RegisterMenus));

		Dataflow::FDataflowConstructionVisualizationRegistry::GetInstance().RegisterVisualization(MakeUnique<FClothDataflowConstructionVisualization>());
		Dataflow::FDataflowSimulationVisualizationRegistry::GetInstance().RegisterVisualization(MakeUnique<FClothDataflowSimulationVisualization>());
		Dataflow::FDataflowNodeDetailExtensionRegistry::GetInstance().RegisterExtension(MakeUnique<FClothSimulationNodeDetailExtender>());

		if (IConsoleVariable* const Var = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ChaosCloth.EnableDataflowEditor")))	// declared in AssetDefinition_ClothAsset.cpp
		{
			if (UChaosClothEditorOptions* const Options = UChaosClothEditorOptions::StaticClass()->GetDefaultObject<UChaosClothEditorOptions>())
			{
				Var->Set(Options->bClothAssetsOpenInDataflowEditor);

				OnCVarChangedDelegateHandle = Var->OnChangedDelegate().AddLambda([Options](IConsoleVariable* Variable)
				{
					Options->bClothAssetsOpenInDataflowEditor = Variable->GetBool();
					Options->SaveConfig();
				});
			}
		}

		ClothAssetComponentBroker = MakeShareable(new FClothAssetComponentBroker);
		FComponentAssetBrokerage::RegisterBroker(ClothAssetComponentBroker, UChaosClothComponent::StaticClass(), true, true);

		UThumbnailManager::Get().RegisterCustomRenderer(UChaosClothAsset::StaticClass(), UChaosClothAssetThumbnailRenderer::StaticClass());
	}

	void FChaosClothAssetEditorModule::ShutdownModule()
	{
		if (UObjectInitialized())
		{
			FComponentAssetBrokerage::UnregisterBroker(ClothAssetComponentBroker);
		}

		if (IConsoleVariable* const Var = IConsoleManager::Get().FindConsoleVariable(TEXT("p.ChaosCloth.EnableDataflowEditor")))
		{
			Var->OnChangedDelegate().Remove(OnCVarChangedDelegateHandle);
		}

		Dataflow::FDataflowNodeDetailExtensionRegistry::GetInstance().DeregisterExtension(FClothSimulationNodeDetailExtender::Name);
		Dataflow::FDataflowConstructionVisualizationRegistry::GetInstance().DeregisterVisualization(FClothDataflowConstructionVisualization::Name);
		Dataflow::FDataflowSimulationVisualizationRegistry::GetInstance().DeregisterVisualization(FClothDataflowSimulationVisualization::Name);

		UToolMenus::UnRegisterStartupCallback(StartupCallbackDelegateHandle);

		FChaosClothAssetEditorCommands::Unregister();

		FEditorModeRegistry::Get().UnregisterMode(UChaosClothAssetEditorMode::EM_ChaosClothAssetEditorModeId);
	}

} // namespace UE::Chaos::ClothAsset

IMPLEMENT_MODULE(UE::Chaos::ClothAsset::FChaosClothAssetEditorModule, ChaosClothAssetEditor)

#undef LOCTEXT_NAMESPACE
