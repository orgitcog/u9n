// Copyright Epic Games, Inc. All Rights Reserved.

#include "NewAssetContextMenu.h"

#include "ToolMenu.h"
#include "Widgets/SBoxPanel.h"
#include "ToolMenuEntry.h"
#include "Widgets/SOverlay.h"
#include "ToolMenuSection.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Factories/Factory.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "ContentBrowserUtils.h"
#include "Styling/StyleColors.h"
#include "Widgets/SAssetMenuIcon.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

struct FFactoryItem
{
	UFactory& Factory;
	FText DisplayName;

	FFactoryItem(UFactory& InFactory, const FText& InDisplayName)
		: Factory(InFactory)
		, DisplayName(InDisplayName)
	{
	}
};

struct FCategorySubMenuItem
{
	FText Name;
	TArray<FFactoryItem> Factories;
	TMap<FString, TSharedPtr<FCategorySubMenuItem>> Children;

	void SortSubMenus(FCategorySubMenuItem* SubMenu = nullptr)
	{
		if (!SubMenu)
		{
			SubMenu = this;
		}

		// Sort the factories by display name
		SubMenu->Factories.Sort([](const FFactoryItem& A, const FFactoryItem& B) -> bool
		{
			return A.DisplayName.CompareToCaseIgnored(B.DisplayName) < 0;
		});

		for (TPair<FString, TSharedPtr<FCategorySubMenuItem>>& Pair : SubMenu->Children)
		{
			if (Pair.Value.IsValid())
			{
				FCategorySubMenuItem* MenuData = Pair.Value.Get();
				SortSubMenus(MenuData);
			}
		}
	}
};

/** Utility to return the new asset factories from FAssetToolsModule */
static const TArray<UFactory*> GetNewAssetFactories()
{
	QUICK_SCOPE_CYCLE_COUNTER(GetNewAssetFactories);

	static const FName NAME_AssetTools = "AssetTools";
	const IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_AssetTools).Get();

	return AssetTools.GetNewAssetFactories();
}

/**
 * Utility to find the factories (from the set provided by the caller) with a given category.
 * 
 * @param Factories			The factories to look in
 * @param AssetTypeCategory	The category to find factories for
 * @param FindFirstOnly		Returns once the first factory has been found
 */
static TArray<FFactoryItem> FindFactoriesInCategory(const TArray<UFactory*>& Factories, EAssetTypeCategories::Type AssetTypeCategory, bool FindFirstOnly)
{
	QUICK_SCOPE_CYCLE_COUNTER(FindFactoriesInCategory);
	
	TArray<FFactoryItem> FactoriesInThisCategory;

	for (UFactory* Factory : Factories)
	{
		QUICK_SCOPE_CYCLE_COUNTER(GetMenuCategories);
	
		const uint32 FactoryCategories = Factory->GetMenuCategories();
		if (FactoryCategories & AssetTypeCategory)
		{
			FactoriesInThisCategory.Emplace(*Factory, Factory->GetDisplayName());

			if (FindFirstOnly)
			{
				return FactoriesInThisCategory;
			}
		}
	}

	return FactoriesInThisCategory;
}

/** 
 * Utility to find the new assert factories with a given category.
 * 
 * @param AssetTypeCategory	The category to find factories for
 * @param FindFirstOnly		Returns once the first factory has been found
 */
static TArray<FFactoryItem> FindFactoriesInCategory(EAssetTypeCategories::Type AssetTypeCategory, bool FindFirstOnly)
{	
	return FindFactoriesInCategory(GetNewAssetFactories(), AssetTypeCategory, FindFirstOnly);
}

class SFactoryMenuEntry : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SFactoryMenuEntry)
		: _IconContainerSize(32, 32)
		, _IconSize(28, 28)
	{}
		SLATE_ARGUMENT(FVector2D, IconContainerSize)
		SLATE_ARGUMENT(FVector2D, IconSize)
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	Factory				The factory this menu entry represents
	 */
	void Construct(const FArguments& InArgs, const UFactory* Factory)
	{
		const TSharedPtr<SWidget> IconContainer =
			SNew(SAssetMenuIcon,
				Factory->GetSupportedClass(),
				UE::Editor::ContentBrowser::IsNewStyleEnabled() ? Factory->GetNewAssetIconOverride() : Factory->GetNewAssetThumbnailOverride())
			.IconContainerSize(InArgs._IconContainerSize)
			.IconSize(InArgs._IconSize);

		static const FMargin IconSlotPadding = UE::Editor::ContentBrowser::IsNewStyleEnabled()
			? FMargin(2, 0, 3, 0)	// Consistent with SMenuEntryBlock::BuildMenuEntryWidget, but accounts for icon size that is larger than the default
			: FMargin(0, 0, 0, 1);

		static const FMargin LabelSlotPadding = UE::Editor::ContentBrowser::IsNewStyleEnabled()
			? FMargin(4, 0, 6, 0)	// Consistent with SMenuEntryBlock::BuildMenuEntryWidget
			: FMargin(4, 0, 4, 0);

		// The vertical padding between each menu entry
		static constexpr float VerticalEntryPadding = 4.0f;

		// Represents the default icon size in a menu entry, not the one used in this widget
		static constexpr float DefaultMenuIconSize = 14.0f;

		// Adjust the vertical padding to match the default menu entry spacing as much as possible, while keeping a desired padding minimum of 1px
		static const float VerticalPaddingAdjustment = ((InArgs._IconContainerSize.Y - DefaultMenuIconSize) / 2.0f) - VerticalEntryPadding;

		static const FMargin ChildSlotPadding = UE::Editor::ContentBrowser::IsNewStyleEnabled()
			? FMargin(0, -VerticalPaddingAdjustment, 0, -VerticalPaddingAdjustment) // Offset to align with regular menu entries
			: FMargin(0);

		ChildSlot
		.Padding(ChildSlotPadding)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.Padding(IconSlotPadding)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				IconContainer.ToSharedRef()
			]

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.Padding(LabelSlotPadding)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(UE::Editor::ContentBrowser::IsNewStyleEnabled() ? FMargin(0) : FMargin(0, 0, 0, 1))
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("LevelViewportContextMenu.AssetLabel.Text.Font"))
					.Text(Factory->GetDisplayName())
				]
			]
		];

		SetToolTip(IDocumentation::Get()->CreateToolTip(Factory->GetToolTip(), nullptr, Factory->GetToolTipDocumentationPage(), Factory->GetToolTipDocumentationExcerpt()));
	}
};

void FNewAssetContextMenu::MakeContextMenu(
	UToolMenu* Menu,
	const TArray<FName>& InSelectedAssetPaths,
	const FOnImportAssetRequested& InOnImportAssetRequested,
	const FOnNewAssetRequested& InOnNewAssetRequested
	)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_ContentBrowser_MakeNewAssetContextMenu);
	
	if (InSelectedAssetPaths.Num() == 0)
	{
		return;
	}

	static const FName NAME_AssetTools = "AssetTools";
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(NAME_AssetTools);

	// Ensure we can modify assets at these paths
	{
		TArray<FString> SelectedAssetPathStrs;
		for (const FName& SelectedPath : InSelectedAssetPaths)
		{
			SelectedAssetPathStrs.Add(SelectedPath.ToString());
		}

		if (!AssetToolsModule.Get().AllPassWritableFolderFilter(SelectedAssetPathStrs))
		{
			return;
		}
	}

	const FCanExecuteAction CanExecuteAssetActionsDelegate = FCanExecuteAction::CreateLambda([NumSelectedAssetPaths = InSelectedAssetPaths.Num()]()
	{
		// We can execute asset actions when we only have a single asset path selected
		return NumSelectedAssetPaths == 1;
	});

	const FName FirstSelectedPath = (InSelectedAssetPaths.Num() > 0) ? InSelectedAssetPaths[0] : FName();

	// Import
	if (InOnImportAssetRequested.IsBound() && !FirstSelectedPath.IsNone())
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_ContentBrowser_ImportSection);
	
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("ContentBrowserGetContent");
			Section.AddMenuEntry(
				"ImportAsset",
				LOCTEXT("ImportAsset", "Import to Current Folder"),
				LOCTEXT("ImportAssetTooltip_NewAsset", "Imports an asset from file to this folder."),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Import"),
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteImportAsset, InOnImportAssetRequested, FirstSelectedPath),
					CanExecuteAssetActionsDelegate
					)
				).InsertPosition = FToolMenuInsert(NAME_None, EToolMenuInsertType::First);
		}
	}


	if (InOnNewAssetRequested.IsBound())
	{
		// Add Create Section
		if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
		{
			FToolMenuSection& Section = Menu->AddSection("ContentBrowserNewAsset", LOCTEXT("CreateAssetsMenuHeading", "Create"));
		}

		// Add Basic Asset
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ContentBrowser_BasicSection);

			const FText CreateBasicAssetSectionLabel =
				UE::Editor::ContentBrowser::IsNewStyleEnabled()
				? FText::GetEmpty()
				: LOCTEXT("CreateBasicAssetsMenuHeading", "Create Basic Asset");

			FToolMenuSection& Section = Menu->AddSection("ContentBrowserNewBasicAsset", CreateBasicAssetSectionLabel);

			if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
			{
				// When a section label is empty, it has no visual representation, so manually insert a separator
				Section.AddSeparator(NAME_None);
			}

			CreateNewAssetMenuCategory(
				Menu,
				"ContentBrowserNewBasicAsset",
				EAssetTypeCategories::Basic,
				FirstSelectedPath,
				InOnNewAssetRequested,
				CanExecuteAssetActionsDelegate
				);


		}

		// Add Advanced Asset
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_ContentBrowser_AdvancedSection);

			const FText CreateAdvancedAssetSectionLabel =
				UE::Editor::ContentBrowser::IsNewStyleEnabled()
				? FText::GetEmpty()
				: LOCTEXT("CreateAdvancedAssetsMenuHeading", "Create Advanced Asset");

			FToolMenuSection& Section = Menu->AddSection("ContentBrowserNewAdvancedAsset", CreateAdvancedAssetSectionLabel);

			if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
			{
				// When a section label is empty, it has no visual representation, so manually insert a separator
				Section.AddSeparator(NAME_None);
			}

			TArray<FAdvancedAssetCategory> AdvancedAssetCategories;
			AssetToolsModule.Get().GetAllAdvancedAssetCategories(AdvancedAssetCategories);
			AdvancedAssetCategories.Sort([](const FAdvancedAssetCategory& A, const FAdvancedAssetCategory& B) {
				return (A.CategoryName.CompareToCaseIgnored(B.CategoryName) < 0);
			});

			const IAssetTools& AssetTools = AssetToolsModule.Get();
			const TArray<UFactory*> NewAssetFactories = AssetTools.GetNewAssetFactories();

			for (const FAdvancedAssetCategory& AdvancedAssetCategory : AdvancedAssetCategories)
			{
				const bool FindFirstOnly = true;
				TArray<FFactoryItem> Factories = FindFactoriesInCategory(NewAssetFactories, AdvancedAssetCategory.CategoryType, FindFirstOnly);
				if (Factories.Num() > 0)
				{
					FToolMenuEntry& SubMenuEntry =
							Section.AddSubMenu(
							NAME_None,
							AdvancedAssetCategory.CategoryName,
							FText::GetEmpty(),
							FNewToolMenuDelegate::CreateStatic(
								&FNewAssetContextMenu::CreateNewAssetMenuCategory,
								FName("Section"),
								AdvancedAssetCategory.CategoryType,
								FirstSelectedPath,
								InOnNewAssetRequested,
								FCanExecuteAction() // We handle this at this level, rather than at the sub-menu item level
							),
							FUIAction(
								FExecuteAction(),
								CanExecuteAssetActionsDelegate
							),
							EUserInterfaceActionType::Button
						);

					SubMenuEntry.SubMenuData.Style.StyleName = "ContentBrowser.AddNewMenu";
				}
			}
		}
	}
}

void FNewAssetContextMenu::CreateNewAssetMenuCategory(UToolMenu* Menu, FName SectionName, EAssetTypeCategories::Type AssetTypeCategory, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction)
{
	// Find UFactory classes that can create new objects in this category.
	const bool FindFirstOnly = false;
	TArray<FFactoryItem> FactoriesInThisCategory = FindFactoriesInCategory(AssetTypeCategory, FindFirstOnly);
	if (FactoriesInThisCategory.Num() == 0)
	{
		return;
	}

	TSharedPtr<FCategorySubMenuItem> ParentMenuData = MakeShared<FCategorySubMenuItem>();
	for (FFactoryItem& Item : FactoriesInThisCategory)
	{
		FCategorySubMenuItem* SubMenu = ParentMenuData.Get();
		const TArray<FText>& CategoryNames = Item.Factory.GetMenuCategorySubMenus();
		for (FText CategoryName : CategoryNames)
		{
			const FString SourceString = CategoryName.BuildSourceString();
			if (TSharedPtr<FCategorySubMenuItem> SubMenuData = SubMenu->Children.FindRef(SourceString))
			{
				check(SubMenuData.IsValid());
				SubMenu = SubMenuData.Get();
			}
			else
			{
				TSharedPtr<FCategorySubMenuItem> NewSubMenu = MakeShared<FCategorySubMenuItem>();
				NewSubMenu->Name = CategoryName;
				SubMenu->Children.Add(SourceString, NewSubMenu);
				SubMenu = NewSubMenu.Get();
			}
		}
		SubMenu->Factories.Add(Item);
	}
	ParentMenuData->SortSubMenus();

	CreateNewAssetMenus(Menu, SectionName, ParentMenuData, InPath, InOnNewAssetRequested, InCanExecuteAction);
}

void FNewAssetContextMenu::CreateNewAssetMenus(UToolMenu* Menu, FName SectionName, TSharedPtr<FCategorySubMenuItem> SubMenuData, FName InPath, FOnNewAssetRequested InOnNewAssetRequested, FCanExecuteAction InCanExecuteAction)
{
	QUICK_SCOPE_CYCLE_COUNTER(CreateNewAssetMenus);

	FToolMenuSection& Section = Menu->FindOrAddSection(SectionName);
	for (const FFactoryItem& FactoryItem : SubMenuData->Factories)
	{
		TWeakObjectPtr<UClass> WeakFactoryClass = FactoryItem.Factory.GetClass();

		FName AssetTypeName;

		if (UClass* SupportedClass = FactoryItem.Factory.GetSupportedClass())
		{
			AssetTypeName = SupportedClass->GetFName();
		}

		if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
		{
			static constexpr uint32 IconContainerSize = 24;
			static constexpr uint32 IconSize = 16;

			FToolMenuEntry& Entry = Section.AddEntry(
				FToolMenuEntry::InitMenuEntry(
				NAME_None,
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteNewAsset, InOnNewAssetRequested, InPath, WeakFactoryClass),
					InCanExecuteAction
				),
			SNew(SFactoryMenuEntry, &FactoryItem.Factory)
				.IconContainerSize(FVector2D(IconContainerSize, IconContainerSize))
				.IconSize(FVector2D(IconSize, IconSize))
				.AddMetaData<FTagMetaData>(FTagMetaData(AssetTypeName)))
			);

			Entry.SubMenuData.Style.StyleName = "ContentBrowser.AddNewMenu";
		}
		else
		{
			Section.AddEntry(
				FToolMenuEntry::InitMenuEntry(
				NAME_None,
				FUIAction(
					FExecuteAction::CreateStatic(&FNewAssetContextMenu::ExecuteNewAsset, InOnNewAssetRequested, InPath, WeakFactoryClass),
					InCanExecuteAction
				),
			SNew(SFactoryMenuEntry, &FactoryItem.Factory)
				.AddMetaData<FTagMetaData>(FTagMetaData(AssetTypeName)))
			);
		}
	}

	if (SubMenuData->Children.Num() == 0)
	{
		return;
	}

	Section.AddSeparator(NAME_None);

	TArray<TSharedPtr<FCategorySubMenuItem>> SortedMenus;
	SubMenuData->Children.GenerateValueArray(SortedMenus);
	SortedMenus.Sort([](const TSharedPtr<FCategorySubMenuItem>& A, const TSharedPtr<FCategorySubMenuItem>& B) -> bool
	{
		return A->Name.CompareToCaseIgnored(B->Name) < 0;
	});

	for (TSharedPtr<FCategorySubMenuItem>& ChildMenuData : SortedMenus)
	{
		check(ChildMenuData.IsValid());

		FToolMenuEntry& Entry = Section.AddSubMenu(
			NAME_None,
			ChildMenuData->Name,
			FText::GetEmpty(),
			FNewToolMenuDelegate::CreateStatic(
				&FNewAssetContextMenu::CreateNewAssetMenus,
				FName("Section"),
				ChildMenuData,
				InPath,
				InOnNewAssetRequested,
				InCanExecuteAction
			),
			FUIAction(
				FExecuteAction(),
				InCanExecuteAction
			),
			EUserInterfaceActionType::Button
		);

		if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
		{
			Entry.SubMenuData.Style.StyleName = "ContentBrowser.AddNewMenu";
		}
	}
}

void FNewAssetContextMenu::ExecuteImportAsset(FOnImportAssetRequested InOnInportAssetRequested, FName InPath)
{
	InOnInportAssetRequested.ExecuteIfBound(InPath);
}

void FNewAssetContextMenu::ExecuteNewAsset(FOnNewAssetRequested InOnNewAssetRequested, FName InPath, TWeakObjectPtr<UClass> FactoryClass)
{
	if (ensure(FactoryClass.IsValid()) && ensure(!InPath.IsNone()))
	{
		InOnNewAssetRequested.ExecuteIfBound(InPath, FactoryClass);
	}
}

#undef LOCTEXT_NAMESPACE
