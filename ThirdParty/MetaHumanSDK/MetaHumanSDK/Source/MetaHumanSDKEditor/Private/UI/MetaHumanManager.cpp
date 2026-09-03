// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanManager.h"

#include "MetaHumanAssetReport.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"
#include "UI/MetaHumanStyleSet.h"
#include "UI/SAssetGroupItemView.h"
#include "UI/SAssetGroupNavigation.h"
#include "Verification/MetaHumanVerificationRuleCollection.h"
#include "Verification/VerifyMetaHumanCharacter.h"
#include "Verification/VerifyMetaHumanGroom.h"
#include "Verification/VerifyMetaHumanOutfitClothing.h"
#include "Verification/VerifyMetaHumanSkeletalClothing.h"
#include "Verification/VerifyMetaHumanPackageSource.h"
#include "Verification/VerifyObjectValid.h"

#include "DesktopPlatformModule.h"
#include "EngineAnalytics.h"
#include "MetaHumanSDKEditor.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenus.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanManager"

namespace UE::MetaHuman
{
/**
 * The main MetaHuman Manager window. For now this is just placeholder content
 */
class SMetaHumanManagerWindow final : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanManagerWindow)
		{
		}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		AnalyticsEvent(TEXT("ManagerShown"));

		SWindow::Construct(
			SWindow::FArguments()
			.Title(LOCTEXT("MetaHumanManagerTitle", "MetaHuman Manager"))
			.SupportsMinimize(true)
			.SupportsMaximize(true)
			.ClientSize(FMetaHumanStyleSet::Get().GetVector("MetaHumanManager.WindowSize"))
			.MinWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.WindowMinWidth"))
			.MinHeight(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.WindowMinHeight"))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MinWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.NavigationWidth"))
				.MaxWidth(FMetaHumanStyleSet::Get().GetFloat("MetaHumanManager.NavigationWidth"))
				.FillContentWidth(0)
				.VAlign(VAlign_Fill)
				[
					SNew(SAssetGroupNavigation)
					.OnNavigate(FOnNavigate::CreateSP(this, &SMetaHumanManagerWindow::SelectItems))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.FillContentWidth(1)
				.VAlign(VAlign_Fill)
				.Padding(FMetaHumanStyleSet::Get().GetMargin("MetaHumanManager.ItemViewPadding"))
				[
					SAssignNew(ItemView, SAssetGroupItemView)
					.OnVerify(FOnVerify::CreateSP(this, &SMetaHumanManagerWindow::VerifyItems))
					.OnPackage(FOnPackage::CreateSP(this, &SMetaHumanManagerWindow::PackageItems))
				]
			]);
	}

private:
	void SelectItems(const TArray<TSharedRef<FMetaHumanAssetDescription>>& SelectedItems) const
	{
		FScopedSlowTask LoadingTask(SelectedItems.Num() * 2, LOCTEXT("UpdatingAssetTask", "Updating asset details..."));
		LoadingTask.MakeDialog();
		for (const TSharedRef<FMetaHumanAssetDescription>& SelectedItem : SelectedItems)
		{
			LoadingTask.EnterProgressFrame();
			// Ensure that all asset info is up to date
			UMetaHumanAssetManager::UpdateAssetDependencies(SelectedItem.Get());
			LoadingTask.EnterProgressFrame();
			UMetaHumanAssetManager::UpdateAssetDetails(SelectedItem.Get());
		}
		ItemView->SetItems(SelectedItems);
	}

	void VerifyItems(const TArray<TSharedRef<FMetaHumanAssetDescription>>& SelectedItems)
	{
		FScopedSlowTask VerifyingTask(SelectedItems.Num(), LOCTEXT("VerificationProgressMessage", "Verifying assets..."));
		VerifyingTask.MakeDialog();
		for (const TSharedRef<FMetaHumanAssetDescription>& SelectedItem : SelectedItems)
		{
			VerifyingTask.EnterProgressFrame();

			UMetaHumanVerificationRuleCollection* VerificationCollection = NewObject<UMetaHumanVerificationRuleCollection>();

			// Common verification tests
			VerificationCollection->AddVerificationRule(NewObject<UVerifyObjectValid>());
			VerificationCollection->AddVerificationRule(NewObject<UVerifyMetaHumanPackageSource>());

			// AssetType-specific verification tests
			if (SelectedItem->AssetType == EMetaHumanAssetType::CharacterAssembly)
			{
				VerificationCollection->AddVerificationRule(NewObject<UVerifyMetaHumanCharacter>());
			}
			if (SelectedItem->AssetType == EMetaHumanAssetType::SkeletalClothing)
			{
				VerificationCollection->AddVerificationRule(NewObject<UVerifyMetaHumanSkeletalClothing>());
			}
			if (SelectedItem->AssetType == EMetaHumanAssetType::OutfitClothing)
			{
				VerificationCollection->AddVerificationRule(NewObject<UVerifyMetaHumanOutfitClothing>());
			}
			if (SelectedItem->AssetType == EMetaHumanAssetType::Groom)
			{
				VerificationCollection->AddVerificationRule(NewObject<UVerifyMetaHumanGroom>());
			}

			// TODO: get actual export options from UI
			FMetaHumansVerificationOptions VerificationOptions{
				false, // bVerbose
				false // bTreatWarningsAsErrors
			};

			TStrongObjectPtr<UMetaHumanAssetReport> LifetimeManager;
			LifetimeManager.Reset(NewObject<UMetaHumanAssetReport>());
			VerificationCollection->ApplyAllRules(SelectedItem->AssetData.GetAsset(), LifetimeManager.Get(), VerificationOptions);
			SelectedItem->VerificationReport =  LifetimeManager.Get();

			// Keep hold of all reports until we close the window.
			Reports.Add(LifetimeManager);
		}

		// Refresh the UI
		ItemView->SetItems(SelectedItems);
	}

	void PackageItems(const TArray<TSharedRef<FMetaHumanAssetDescription>>& SelectedItems)
	{
		if (SelectedItems.IsEmpty())
		{
			return;
		}

		TArray<FString> SelectedFilenames;
		if (!FDesktopPlatformModule::Get()->SaveFileDialog(
			FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()),
			TEXT("Save as MetaHuman Package file..."),
			TEXT(""),
			SelectedItems[0]->Name.ToString(),
			TEXT("MetaHuman Package file (*.mhpkg)|*.mhpkg"),
			EFileDialogFlags::None,
			SelectedFilenames))
		{
			return;
		}

		if (SelectedFilenames.IsEmpty())
		{
			return;
		}

		// Simple progress dialogue. Allows sub items to add progress updates.
		FScopedSlowTask PackagingTask(1, LOCTEXT("PackagingProgressMessage", "Packaging Assets..."));
		PackagingTask.MakeDialog();
		PackagingTask.EnterProgressFrame();

		// Ensure that all asset info is up to date
		TArray<FMetaHumanAssetDescription> ToPackage;
		for (const TSharedRef<FMetaHumanAssetDescription>& SelectedItem : SelectedItems)
		{
			FMetaHumanAssetDescription Item = SelectedItem.Get();
			UMetaHumanAssetManager::UpdateAssetDependencies(Item);
			UMetaHumanAssetManager::UpdateAssetDetails(Item);
			ToPackage.Add(Item);
		}
		UMetaHumanAssetManager::CreateArchive(ToPackage, SelectedFilenames[0]);
		FPlatformProcess::ExploreFolder(*SelectedFilenames[0]);
	}

	TSharedPtr<SAssetGroupItemView> ItemView;

	// Required to stop Reports being GC'd as lifetime management within Slate does not use UE GC-aware pointers
	TArray<TStrongObjectPtr<UMetaHumanAssetReport>> Reports;
};

class FMetaHumanManagerImpl
{
	static void RegisterMenuItems()
	{
		// Create the MetaHumanManager entry for the main window menu
		if (UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window")))
		{
			WindowMenu->FindOrAddSection("MetaHuman", LOCTEXT("MetaHumanSection", "MetaHuman"), {TEXT("Log"), EToolMenuInsertType::Before})
					.AddMenuEntry(
						MetaHumanManagerMenuItemName,
						MetaHumanManagerName,
						MetaHumanManagerToolTip,
						FSlateIcon(FMetaHumanStyleSet::Get().GetStyleSetName(), "MenuIcon"),
						FUIAction(
							FExecuteAction::CreateLambda([]()
							{
								IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
								if (MainFrameModule.GetParentWindow().IsValid())
								{
									FSlateApplication::Get().AddWindowAsNativeChild(SNew(SMetaHumanManagerWindow), MainFrameModule.GetParentWindow().ToSharedRef());
								}
								else
								{
									FSlateApplication::Get().AddWindow(SNew(SMetaHumanManagerWindow));
								}
							})
						)
					);
		}
	}

	// UI management
	void InitializeStyle()
	{
		FSlateStyleRegistry::RegisterSlateStyle(FMetaHumanStyleSet::Get());
		FSlateApplication::Get().GetRenderer()->ReloadTextureResources();
	}

	void DestroyStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(FMetaHumanStyleSet::Get());
	}

	// UI strings
	static const FText MetaHumanManagerToolTip;
	static const FText MetaHumanManagerName;

	// UI element names
	static const FName MetaHumanManagerMenuItemName;

public:
	void Initialize()
	{
		if (FSlateApplicationBase::IsInitialized() && IPluginManager::Get().FindEnabledPlugin(TEXT("MetaHumanCharacter")) )
		{
			// Register UI entrypoints
			InitializeStyle();
			RegisterMenuItems();
		}
	}

	void Shutdown()
	{
		if (FSlateApplicationBase::IsInitialized())
		{
			// Clean up UI
			DestroyStyle();
		}
	}
};

// Statics:
TUniquePtr<FMetaHumanManagerImpl> FMetaHumanManager::Instance;
const FText FMetaHumanManagerImpl::MetaHumanManagerToolTip = LOCTEXT("MenuTooltip", "Launch MetaHuman Manager");
const FText FMetaHumanManagerImpl::MetaHumanManagerName = LOCTEXT("MenuName", "MetaHuman Manager");

// UI element names
const FName FMetaHumanManagerImpl::MetaHumanManagerMenuItemName = TEXT("OpenMetaHumanManagerTab");


void FMetaHumanManager::Initialize()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FMetaHumanManagerImpl>();
	}
	Instance->Initialize();
}

void FMetaHumanManager::Shutdown()
{
	Instance.Reset();
}
} // namespace UE::MetaHuman

#undef LOCTEXT_NAMESPACE
