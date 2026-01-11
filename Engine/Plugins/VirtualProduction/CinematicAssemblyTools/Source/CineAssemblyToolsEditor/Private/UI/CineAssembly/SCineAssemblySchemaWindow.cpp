// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCineAssemblySchemaWindow.h"

#include "Algo/Contains.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetViewUtils.h"
#include "CineAssemblyNamingTokens.h"
#include "CineAssemblySchemaFactory.h"
#include "CineAssemblyToolsStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IStructureDetailsView.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "NamingTokensEngineSubsystem.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "Settings/ContentBrowserSettings.h"
#include "Styling/StyleColors.h"
#include "UObject/Package.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCineAssemblySchemaWindow"

namespace UE::CineAssemblyTools::Private
{
	auto SortTreeItems = [](const TSharedPtr<FSchemaTreeItem>& A, const TSharedPtr<FSchemaTreeItem>& B) { return A->Path < B->Path; };
};

void SCineAssemblySchemaWindow::Construct(const FArguments& InArgs, const FString& InCreateAssetPath)
{
	// Create a new transient CineAssemblySchema to configure in the UI.
	// If the configuration is successful, this will turn into the persistent object created by the factory.
	SchemaToConfigure = TStrongObjectPtr<UCineAssemblySchema>(NewObject<UCineAssemblySchema>(GetTransientPackage(), NAME_None, RF_Transient));
	Mode = ESchemaConfigMode::CreateNew;

	CreateAssetPath = InCreateAssetPath;

	ChildSlot [ BuildUI() ];
}

void SCineAssemblySchemaWindow::Construct(const FArguments& InArgs, UCineAssemblySchema* InSchema)
{
	SchemaToConfigure = TStrongObjectPtr<UCineAssemblySchema>(InSchema);
	Mode = ESchemaConfigMode::Edit;

	ChildSlot [ BuildUI() ];
}

void SCineAssemblySchemaWindow::Construct(const FArguments& InArgs, FGuid InSchemaGuid)
{
	Mode = ESchemaConfigMode::Edit;

	// The UI will be temporary because no CineAssemblySchema has been found yet
	ChildSlot [ BuildUI() ];

	// If the asset registry is still scanning assets, add a callback to find the schema asset matching the input GUID and update this widget once the scan is finished.
	// Otherwise, we can find the schema asset and update the UI immediately. 
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &SCineAssemblySchemaWindow::FindSchema, InSchemaGuid);
	}
	else
	{
		FindSchema(InSchemaGuid);
	}
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::BuildUI()
{
	// Build a temporary UI to display while waiting for the schema to be loaded
	if (!SchemaToConfigure)
	{
		return SNew(SBorder)
			.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.PanelNoBorder"))
			.Padding(8.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("LoadingSchemaText", "Loading Cine Assembly Schema..."))
			];
	}

	// Initialize the content tree view with the current list of assets and folders saved in the schema
	InitializeContentTree();

	// Check the UI config settings to determine whether or not to display engine/plugin content by default in this window
	UContentBrowserSettings* ContentBrowserSettings = GetMutableDefault<UContentBrowserSettings>();

	bool bShowEngineContent = true;
	bool bShowPluginContent = true;
	GConfig->GetBool(TEXT("NewCineAssemblySchemaUI"), TEXT("bShowEngineContent"), bShowEngineContent, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("NewCineAssemblySchemaUI"), TEXT("bShowPluginContent"), bShowPluginContent, GEditorPerProjectIni);

	bShowEngineContentCached = ContentBrowserSettings->GetDisplayEngineFolder();
	bShowPluginContentCached = ContentBrowserSettings->GetDisplayPluginFolders();

	ContentBrowserSettings->SetDisplayEngineFolder(bShowEngineContent);
	ContentBrowserSettings->SetDisplayPluginFolders(bShowPluginContent);

	return SNew(SBorder)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				[
					SNew(SSplitter)
						.Orientation(Orient_Horizontal)
						.PhysicalSplitterHandleSize(2.0f)

					+ SSplitter::Slot()
						.Value(0.2f)
						[
							MakeMenuPanel()
						]

					+ SSplitter::Slot()
						.Value(0.8f)
						[
							MakeContentPanel()
						]
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SSeparator)
						.Orientation(Orient_Horizontal)
						.Thickness(2.0f)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					MakeButtonsPanel()
				]
		];
}

SCineAssemblySchemaWindow::~SCineAssemblySchemaWindow()
{
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
	}

	// Save the UI config settings for whether to display engine/plugin content
	if (UContentBrowserSettings* ContentBrowserSettings = GetMutableDefault<UContentBrowserSettings>())
	{
		const bool bShowEngineContent = ContentBrowserSettings->GetDisplayEngineFolder();
		const bool bShowPluginContent = ContentBrowserSettings->GetDisplayPluginFolders();
		
		if (GConfig)
		{
			GConfig->SetBool(TEXT("NewCineAssemblySchemaUI"), TEXT("bShowEngineContent"), bShowEngineContent, GEditorPerProjectIni);
			GConfig->SetBool(TEXT("NewCineAssemblySchemaUI"), TEXT("bShowPluginContent"), bShowPluginContent, GEditorPerProjectIni);
		}

		ContentBrowserSettings->SetDisplayEngineFolder(bShowEngineContentCached);
		ContentBrowserSettings->SetDisplayPluginFolders(bShowPluginContentCached);
	}
}

void SCineAssemblySchemaWindow::FindSchema(FGuid SchemaID)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// The only search criterion for the asset search is for an asset with an SchemaID matching the input GUID
	const TMultiMap<FName, FString> TagValues = { { UCineAssemblySchema::SchemaGuidPropertyName, SchemaID.ToString() } };

	TArray<FAssetData> SchemaAssets;
	AssetRegistryModule.Get().GetAssetsByTagValues(TagValues, SchemaAssets);

	// The Schema ID is unique, so at most one asset should ever be found
	if (SchemaAssets.Num() > 0)
	{
		SchemaToConfigure = TStrongObjectPtr<UCineAssemblySchema>(Cast<UCineAssemblySchema>(SchemaAssets[0].GetAsset()));

		// Update the widget's UI
		ChildSlot.DetachWidget();
		ChildSlot.AttachWidget(BuildUI());
	}
}

FString SCineAssemblySchemaWindow::GetSchemaName()
{
	if (SchemaToConfigure)
	{
		FString SchemaName;
		SchemaToConfigure->GetName(SchemaName);
		return SchemaName;
	}
	return TEXT("CineAssemblySchema");
}


bool SCineAssemblySchemaWindow::DoesSchemaExistWithName(const FString& SchemaName) const
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> SchemaAssets;
	AssetRegistryModule.Get().GetAssetsByClass(UCineAssemblySchema::StaticClass()->GetClassPathName(), SchemaAssets);

	return Algo::ContainsBy(SchemaAssets, *SchemaName, &FAssetData::AssetName);
}

bool SCineAssemblySchemaWindow::ValidateSchemaName(const FText& InText, FText& OutErrorMessage) const
{
	if (InText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyNameErrorMessage", "Please provide a name for the schema");
		return false;
	}

	// It is valid if the input text matches the schema's current name
	if (SchemaToConfigure->SchemaName == InText.ToString())
	{
		return true;
	}

	if (DoesSchemaExistWithName(InText.ToString()))
	{
		OutErrorMessage = LOCTEXT("DuplicateNameErrorMessage", "A schema with that name already exists");
		return false;
	}

	return FName::IsValidXName(InText.ToString(), INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &OutErrorMessage);
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeMenuPanel()
{
	return SNew(SBorder)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.RecessedBackground"))
		.Padding(8.0f)
		.VAlign(VAlign_Top)
		[
			SNew(SSegmentedControl<int32>)
				.Style(&FCineAssemblyToolsStyle::Get().GetWidgetStyle<FSegmentedControlStyle>("PrimarySegmentedControl"))
				.MaxSegmentsPerLine(1)
				.Value_Lambda([this]()
				{ 
					return MenuTabSwitcher->GetActiveWidgetIndex();
				})
				.OnValueChanged_Lambda([this](int32 NewValue)
				{
					MenuTabSwitcher->SetActiveWidgetIndex(NewValue);
				})

				+ SSegmentedControl<int32>::Slot(0)
				.Text(LOCTEXT("DetailsTab", "Details"))
				.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Details").GetIcon())
				.HAlign(HAlign_Left)

				+ SSegmentedControl<int32>::Slot(1)
				.Text(LOCTEXT("MetadataTab", "Metadata"))
				.Icon(FSlateIcon(FCineAssemblyToolsStyle::StyleName, "Icons.DataAsset").GetIcon())
				.HAlign(HAlign_Left)

				+ SSegmentedControl<int32>::Slot(2)
				.Text(LOCTEXT("HierarchyTab", "Content Hierarchy"))
				.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed").GetIcon())
				.HAlign(HAlign_Left)
		];
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeContentPanel()
{
	MenuTabSwitcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()
		[
			MakeDetailsTabContent()
		]

		+ SWidgetSwitcher::Slot()
		[
			MakeMetadataTabContent()
		]

		+ SWidgetSwitcher::Slot()
		[
			MakeHierarchyTabContent()
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			MenuTabSwitcher.ToSharedRef()
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
				.Padding(16.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.FillWidth(0.8)
						[
							SNullWidget::NullWidget
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("SchemaNameField", "Schema Name"))
						]

					+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.FillWidth(0.2)
						[
							SNew(SEditableTextBox)
								.Text_Lambda([this]() -> FText 
									{ 
										return FText::FromString(SchemaToConfigure->SchemaName); 
									})
								.OnVerifyTextChanged(this, &SCineAssemblySchemaWindow::ValidateSchemaName)
								.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
									{
										SchemaToConfigure->RenameAsset(InText.ToString());
									})
						]
				]
		];
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeButtonsPanel()
{
	if (Mode == ESchemaConfigMode::CreateNew)
	{
		return SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
			.Padding(16.0f)
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
					.MinWidth(118.0f)
					.MaxWidth(118.0f)
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						SNew(SButton)
							.Text(LOCTEXT("CreateAssetButton", "Create Schema"))
							.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
							.HAlign(HAlign_Center)
							.OnClicked(this, &SCineAssemblySchemaWindow::OnCreateAssetClicked)
					]

				+ SHorizontalBox::Slot()
					.MinWidth(118.0f)
					.MaxWidth(118.0f)
					.VAlign(VAlign_Center)
					[
						SNew(SButton)
							.Text(LOCTEXT("CancelButton", "Cancel"))
							.HAlign(HAlign_Center)
							.OnClicked(this, &SCineAssemblySchemaWindow::OnCancelClicked)
					]
			];
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeDetailsWidget(bool bShowMetadata)
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;

	TSharedRef<IDetailsView> DetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);

	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateSP(this, &SCineAssemblySchemaWindow::IsPropertyVisible, bShowMetadata));

	DetailsView->SetObject(SchemaToConfigure.Get(), true);
	DetailsView->OnFinishedChangingProperties().AddSP(this, &SCineAssemblySchemaWindow::OnSchemaPropertiesChanged);

	return DetailsView;
}

bool SCineAssemblySchemaWindow::IsPropertyVisible(const FPropertyAndParent& PropertyAndParent, bool bShowMetadata)
{
	if (PropertyAndParent.Property.GetFName() == GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, AssemblyMetadata))
	{
		return bShowMetadata;
	}
	else if ((PropertyAndParent.ParentProperties.Num() > 0) && (PropertyAndParent.ParentProperties[0]->GetFName() == GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, AssemblyMetadata)))
	{
		return bShowMetadata;
	}
	return !bShowMetadata;
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeDetailsTabContent()
{
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("DetailsTitle", "Schema Details"))
						.Font(FCineAssemblyToolsStyle::Get().GetFontStyle("ProductionWizard.HeadingFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("SchemaDetailsInstruction", "Configure the properties which will be inherited by every Cine Assembly asset created from this schema."))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					MakeDetailsWidget(false)
				]
		];
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeMetadataTabContent()
{
	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("MetadataTitle", "Schema Metadata"))
						.Font(FCineAssemblyToolsStyle::Get().GetFontStyle("ProductionWizard.HeadingFont"))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("SchemaMetadataInstruction", "Configure the metadata that should be associated with Cine Assemblies created from this schema. "
							"For each metadata field, choose the value type, metadata key, and optionally a default value."))
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					MakeDetailsWidget(true)
				]
		];
}

TSharedRef<SWidget> SCineAssemblySchemaWindow::MakeHierarchyTabContent()
{
	TreeView = SNew(STreeView<TSharedPtr<FSchemaTreeItem>>)
		.TreeItemsSource(&TreeItems)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SCineAssemblySchemaWindow::OnGenerateTreeRow)
		.OnGetChildren(this, &SCineAssemblySchemaWindow::OnGetChildren)
		.OnItemsRebuilt(this, &SCineAssemblySchemaWindow::OnTreeItemsRebuilt)
		.OnContextMenuOpening(this, &SCineAssemblySchemaWindow::MakeContentTreeContextMenu)
		.OnKeyDownHandler(this, &SCineAssemblySchemaWindow::OnTreeViewKeyDown);

	ExpandTreeRecursive(RootItem);

	TSharedRef<SWidget> ContentHierarchyWidget = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SButton)
				.ContentPadding(FMargin(2.0f))
				.OnClicked(this, &SCineAssemblySchemaWindow::OnAddFolderClicked)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
								.ColorAndOpacity(FStyleColors::AccentGreen)
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock).Text(LOCTEXT("AddFolderButton", "Add Folder"))
						]
		]		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.RecessedBackground"))
				[
					TreeView.ToSharedRef()
				]
		];

	TSharedRef<SWidget> AssetListWidget = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Left)
		.Padding(0.0f, 0.0f, 0.0f, 8.0f)
		[
			SNew(SButton)
				.ContentPadding(FMargin(2.0f))
				.OnClicked(this, &SCineAssemblySchemaWindow::OnAddAssetClicked)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(0.0f, 0.0f, 4.0f, 0.0f)
						[
							SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
								.ColorAndOpacity(FStyleColors::AccentGreen)
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock).Text(LOCTEXT("AddAssetButton", "Add Asset"))
						]
		]		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.RecessedBackground"))
				[
					SAssignNew(AssetListView, SListView<TSharedPtr<FString>>)
						.ListItemsSource(&AssetListItems)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SCineAssemblySchemaWindow::OnGenerateAssetRow)
						.OnItemsRebuilt(this, &SCineAssemblySchemaWindow::OnAssetListRebuilt)
						.OnContextMenuOpening(this, &SCineAssemblySchemaWindow::MakeAssetListContextMenu)
						.OnMouseButtonDoubleClick(this, &SCineAssemblySchemaWindow::EnterEditMode)
						.OnKeyDownHandler(this, &SCineAssemblySchemaWindow::OnAssetListKeyDown)
				]
		];

	return SNew(SBorder)
		.Padding(16.0f)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ContentHierarchyTitle", "Content Hierarchy"))
						.Font(FCineAssemblyToolsStyle::Get().GetFontStyle("ProductionWizard.HeadingFont"))
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 0.0f, 0.0f, 24.0f)
				[
					SNew(STextBlock)
						.AutoWrapText(true)
						.Text(LOCTEXT("AssetListInstructions", "Add new named subsequences to the list on the left, then drag and drop them to a location in the folder tree on the right. "
							"When a new Cine Assembly is created using this Schema, the subsequences will automatically be created and added as tracks to the Assembly."))
				]

			+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.FillWidth(0.5f)
						.Padding(0.0f, 0.0f, 16.0f, 0.0f)
						[
							AssetListWidget
						]

					+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SSeparator)
								.Orientation(Orient_Vertical)
								.Thickness(2.0f)
						]

					+ SHorizontalBox::Slot()
						.FillWidth(0.5f)
						.Padding(16.0f, 0.0f, 0.0f, 0.0f)
						[
							ContentHierarchyWidget
						]
				]
		];
}

FReply SCineAssemblySchemaWindow::OnAddAssetClicked()
{
	// Add a new empty string to the list, which will be renamed by the user
	AssetListItems.Add(MakeShared<FString>(TEXT("")));
	AssetListView->RequestListRefresh();
	return FReply::Handled();
}

FString SCineAssemblySchemaWindow::MakeUniqueFolderPath(TSharedPtr<FSchemaTreeItem> InItem)
{
	// This implementation is based on a similar utility in AssetTools for creating a unique asset name.
	const FString BaseName = InItem->Path / TEXT("NewFolder");

	// Find the index in the string of the last non-numeric character
	int32 CharIndex = BaseName.Len() - 1;
	while (CharIndex >= 0 && BaseName[CharIndex] >= TEXT('0') && BaseName[CharIndex] <= TEXT('9'))
	{
		--CharIndex;
	}

	// Trim the numeric characters off the end of the BaseName string, but remember the integer that was trimmed off to increment and append to the output
	int32 IntSuffix = 1;
	FString TrimmedBaseName = BaseName;
	if (CharIndex >= 0 && CharIndex < BaseName.Len() - 1)
	{
		TrimmedBaseName = BaseName.Left(CharIndex + 1);

		const FString TrailingInteger = BaseName.RightChop(CharIndex + 1);
		IntSuffix = FCString::Atoi(*TrailingInteger) + 1;
	}

	FString WorkingName = TrimmedBaseName;

	while (Algo::ContainsBy(InItem->ChildFolders, WorkingName, &FSchemaTreeItem::Path))
	{
		WorkingName = FString::Printf(TEXT("%s%d"), *TrimmedBaseName, IntSuffix);
		IntSuffix++;
	}

	return WorkingName;
}

FReply SCineAssemblySchemaWindow::OnAddFolderClicked()
{
	TSharedPtr<FSchemaTreeItem> NewFolder = MakeShared<FSchemaTreeItem>();
	NewFolder->Type = FSchemaTreeItem::EItemType::Folder;

	// Get the parent item for the new folder being added (this can be the root folder if no parent is currently selected)
	// The TreeView uses single selection mode, so at most one item can ever be selected by the user
	TSharedPtr<FSchemaTreeItem> SelectedTreeItem = RootItem;

	TArray<TSharedPtr<FSchemaTreeItem>> SelectedNodes = TreeView->GetSelectedItems();
	if (SelectedNodes.Num() == 1)
	{
		SelectedTreeItem = SelectedNodes[0];
	}

	const FString ParentPath = SelectedTreeItem->Path;
	NewFolder->Path = MakeUniqueFolderPath(SelectedTreeItem);
	NewFolder->Parent = SelectedTreeItem;

	SelectedTreeItem->ChildFolders.Add(NewFolder);

	// Sort the children alphabetically to maintain a good ordering with the new folder
	SelectedTreeItem->ChildFolders.Sort(UE::CineAssemblyTools::Private::SortTreeItems);

	// Save a reference to this item so that when the tree is rebuilt, we can immediately start editing its name
	MostRecentlyAddedItem = NewFolder;

	UpdateFolderList();

	TreeView->SetItemExpansion(SelectedTreeItem, true);
	TreeView->RequestTreeRefresh();

	return FReply::Handled();
}

TSharedRef<ITableRow> SCineAssemblySchemaWindow::OnGenerateAssetRow(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SSchemaAssetTableRow> Row = SNew(SSchemaAssetTableRow, OwnerTable)
		.ShowSelection(true)
		.Padding(FMargin(4.0f, 2.0f))
		.OnDragDetected(this, &SCineAssemblySchemaWindow::OnAssetRowDragDetected);

	// Store a reference to the editable textblock in the row to easily set it to edit mode for renaming
	Row->TextBlock = SNew(SInlineEditableTextBlock)
		.Text_Lambda([InItem]() 
		{ 
			return FText::FromString(*InItem.Get()); 
		})
		.OnVerifyTextChanged(this, &SCineAssemblySchemaWindow::ValidateAssetName)
		.OnTextCommitted_Lambda([this, InItem](const FText& InText, ETextCommit::Type InCommitType) 
		{ 
			*InItem = InText.ToString();
		});

	Row->SetContent(SNew(SBorder)
		.Padding(8.0f)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SImage).Image(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Sequencer"))
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					Row->TextBlock.ToSharedRef()
				]

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage).Image(FCineAssemblyToolsStyle::Get().GetBrush("Icons.DragHandle"))
				]
		]);

	return Row;
}

void SCineAssemblySchemaWindow::OnAssetListRebuilt()
{
	if (AssetListView && AssetListView->GetItems().Num() > 0)
	{
		// Only trigger edit mode on the last item in the asset list, and only if it is an empty string so that it can be renamed
		TSharedPtr<FString> LastItem = AssetListView->GetItems().Last();
		if (LastItem && LastItem->IsEmpty())
		{
			*LastItem = TEXT("NewAsset");
			EnterEditMode(LastItem);
		}
	}
}

TSharedPtr<SWidget> SCineAssemblySchemaWindow::MakeAssetListContextMenu()
{
	// The ListView uses single selection mode, so at most one item can ever be selected by the user
	TArray<TSharedPtr<FString>> SelectedItems = AssetListView->GetSelectedItems();
	if (SelectedItems.Num() == 1)
	{
		constexpr bool bCloseAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterMenuSelection, nullptr);

		TSharedPtr<FString>& SelectedItem = SelectedItems[0];

		MenuBuilder.AddMenuEntry(
			LOCTEXT("RenameAsset", "Rename"),
			LOCTEXT("RenameAssetToolTip", "Rename"),
			FSlateIcon(FCineAssemblyToolsStyle::StyleName, "Icons.AssetNaming"),
			FUIAction(FExecuteAction::CreateSP(this, &SCineAssemblySchemaWindow::EnterEditMode, SelectedItem)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		MenuBuilder.AddMenuEntry(
			LOCTEXT("DeleteAssetFromList", "Delete"),
			LOCTEXT("DeleteAssetFromListToolTip", "Delete"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
			FUIAction(FExecuteAction::CreateSP(this, &SCineAssemblySchemaWindow::DeleteAssetItem, SelectedItem)),
			NAME_None,
			EUserInterfaceActionType::Button
		);

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

FReply SCineAssemblySchemaWindow::OnAssetListKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		// The list view uses single selection mode, so at most one item can ever be selected by the user
		TArray<TSharedPtr<FString>> SelectedItems = AssetListView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			const TSharedPtr<FString>& SelectedItem = SelectedItems[0];
			DeleteAssetItem(SelectedItem);
		}
	}

	return FReply::Handled();
}

TSharedRef<ITableRow> SCineAssemblySchemaWindow::OnGenerateTreeRow(TSharedPtr<FSchemaTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SImage> Icon = SNew(SImage);

	if (TreeItem->Type == FSchemaTreeItem::EItemType::Folder)
	{
		Icon->SetImage(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Folder"));
		Icon->SetColorAndOpacity(FAppStyle::Get().GetSlateColor("ContentBrowser.DefaultFolderColor"));
	}
	else
	{
		Icon->SetImage(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Sequencer"));
		Icon->SetColorAndOpacity(FLinearColor::White);
	}

	return SNew(STableRow<TSharedPtr<FSchemaTreeItem>>, OwnerTable)
		.ShowSelection(true)
		.Padding(FMargin(8.0f, 2.0f, 8.0f, 0.0f))
		.OnCanAcceptDrop_Lambda([](const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedPtr<FSchemaTreeItem> InItem)
			{
				// Only folder items can accept drops, and only onto to th item (not above or below)
				return (InItem->Type == FSchemaTreeItem::EItemType::Folder) ? EItemDropZone::OntoItem : TOptional<EItemDropZone>();
			})
		.OnAcceptDrop(this, &SCineAssemblySchemaWindow::OnTreeRowAcceptDrop)
		.OnDragDetected(this, &SCineAssemblySchemaWindow::OnTreeRowDragDetected)
		.Content()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 2.0f, 0.0f)
				[
					Icon.ToSharedRef()
				]

			+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SAssignNew(TreeItem->NameWidget, SInlineEditableTextBlock)
						.Text_Lambda([this, TreeItem]() -> FText
							{
								return (TreeItem == RootItem) ? LOCTEXT("RootPathName", "Root Folder") : FText::FromString(FPaths::GetPathLeaf(TreeItem->Path));
							})
						.IsReadOnly_Lambda([this, TreeItem]() -> bool
							{
								return (TreeItem == RootItem);
							})
						.OnVerifyTextChanged_Lambda([this, TreeItem](const FText& InText, FText& OutErrorMessage) -> bool
							{
								if (TreeItem->Type == FSchemaTreeItem::EItemType::Folder)
								{
									return ValidateFolderName(InText, OutErrorMessage, TreeItem);
								}
								return ValidateAssetName(InText, OutErrorMessage);
							})
						.OnTextCommitted(this, &SCineAssemblySchemaWindow::OnTreeItemTextCommitted, TreeItem)
				]
		];
}

void SCineAssemblySchemaWindow::OnGetChildren(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<TSharedPtr<FSchemaTreeItem>>& OutNodes)
{
	// Display all of the child assets first, followed by all of the child folders
	OutNodes.Append(TreeItem->ChildAssets);
	OutNodes.Append(TreeItem->ChildFolders);
}

void SCineAssemblySchemaWindow::OnTreeItemsRebuilt()
{
	// Upon regenerating the tree view, allow the user to immediately interact with the name widget of the newly added folder in order to rename it
	if (MostRecentlyAddedItem && MostRecentlyAddedItem->NameWidget)
	{
		FSlateApplication::Get().SetKeyboardFocus(MostRecentlyAddedItem->NameWidget.ToSharedRef());
		MostRecentlyAddedItem->NameWidget->EnterEditingMode();
		MostRecentlyAddedItem.Reset();
	}
}

TSharedPtr<SWidget> SCineAssemblySchemaWindow::MakeContentTreeContextMenu()
{
	// The TreeView uses single selection mode, so at most one item can ever be selected by the user
	TArray<TSharedPtr<FSchemaTreeItem>> SelectedNodes = TreeView->GetSelectedItems();
	if (SelectedNodes.Num() == 1)
	{
		constexpr bool bCloseAfterMenuSelection = true;
		FMenuBuilder MenuBuilder(bCloseAfterMenuSelection, nullptr);

		TSharedPtr<FSchemaTreeItem>& SelectedTreeItem = SelectedNodes[0];

		if (SelectedTreeItem->Type == FSchemaTreeItem::EItemType::Folder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddChildFolderAction", "Add Child Folder"),
				LOCTEXT("AddChildFolderTooltip", "Add Child Folder"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Plus"),
				FUIAction(FExecuteAction::CreateLambda([this]() { OnAddFolderClicked(); })),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}

		if ((SelectedTreeItem != RootItem) && (SelectedTreeItem != TopLevelAssemblyItem))
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("RenameAction", "Rename"),
				LOCTEXT("RenameActionToolTip", "Rename"),
				FSlateIcon(FCineAssemblyToolsStyle::StyleName, "Icons.AssetNaming"),
				FUIAction(
					FExecuteAction::CreateLambda([SelectedTreeItem]()
						{
							FSlateApplication::Get().SetKeyboardFocus(SelectedTreeItem->NameWidget.ToSharedRef());
							SelectedTreeItem->NameWidget->EnterEditingMode();
						})),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry(
				LOCTEXT("DeleteAction", "Delete"),
				LOCTEXT("DeleteActionToolTip", "Delete"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction(FExecuteAction::CreateSP(this, &SCineAssemblySchemaWindow::DeleteTreeItem, SelectedTreeItem)),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}

		return MenuBuilder.MakeWidget();
	}

	return SNullWidget::NullWidget;
}

FReply SCineAssemblySchemaWindow::OnTreeViewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete)
	{
		// The TreeView uses single selection mode, so at most one item can ever be selected by the user
		TArray<TSharedPtr<FSchemaTreeItem>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			const TSharedPtr<FSchemaTreeItem>& SelectedItem = SelectedItems[0];
			if ((SelectedItem != RootItem) && (SelectedItem != TopLevelAssemblyItem))
			{
				DeleteTreeItem(SelectedItem);
			}
		}
	}

	return FReply::Handled();
}

void SCineAssemblySchemaWindow::InitializeContentTree()
{
	// Create the content tree root
	RootItem = MakeShared<FSchemaTreeItem>();
	RootItem->Type = FSchemaTreeItem::EItemType::Folder;
	RootItem->Path = TEXT("");

	TreeItems.Add(RootItem);

	auto AddItemsToTree = [this](TArray<FString>& ItemList, FSchemaTreeItem::EItemType ItemType)
		{
			// Sort the list so that paths are added to the tree in the proper order
			ItemList.Sort();

			for (const FString& ItemName : ItemList)
			{
				const FString ParentPath = FPaths::GetPath(ItemName);

				// Walk the tree until we find an item whose path matches the parent path. The new tree item will be created as one of its children
				TSharedPtr<FSchemaTreeItem> ParentItem = FindItemAtPathRecursive(RootItem, ParentPath);
				if (ParentItem)
				{
					TSharedPtr<FSchemaTreeItem> NewItem = MakeShared<FSchemaTreeItem>();
					NewItem->Type = ItemType;
					NewItem->Path = ItemName;
					NewItem->Parent = ParentItem;

					if (ItemType == FSchemaTreeItem::EItemType::Folder)
					{
						ParentItem->ChildFolders.Add(NewItem);
					}
					else
					{
						ParentItem->ChildAssets.Add(NewItem);
					}
				}
			}
		};

	AddItemsToTree(SchemaToConfigure->FoldersToCreate, FSchemaTreeItem::EItemType::Folder);
	AddItemsToTree(SchemaToConfigure->SubsequencesToCreate, FSchemaTreeItem::EItemType::Asset);

	// Add the top-level assembly node
	TSharedPtr<FSchemaTreeItem> ParentItem = FindItemAtPathRecursive(RootItem, SchemaToConfigure->DefaultAssemblyPath);
	if (ParentItem)
	{
		TopLevelAssemblyItem = MakeShared<FSchemaTreeItem>();
		TopLevelAssemblyItem->Type = FSchemaTreeItem::EItemType::Asset;
		TopLevelAssemblyItem->Path = TEXT("{assembly}");
		TopLevelAssemblyItem->Parent = ParentItem;

		ParentItem->ChildAssets.Add(TopLevelAssemblyItem);
	}
}

void SCineAssemblySchemaWindow::ExpandTreeRecursive(TSharedPtr<FSchemaTreeItem> TreeItem) const
{
	TreeView->SetItemExpansion(TreeItem, true);

	for (const TSharedPtr<FSchemaTreeItem>& ChildItem : TreeItem->ChildFolders)
	{
		ExpandTreeRecursive(ChildItem);
	}
}

void SCineAssemblySchemaWindow::GetFolderListRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<FString>& FolderList) const
{
	for (const TSharedPtr<FSchemaTreeItem>& Child : TreeItem->ChildFolders)
	{
		FolderList.Add(Child->Path);
		GetFolderListRecursive(Child, FolderList);
	}
}

void SCineAssemblySchemaWindow::GetAssetListRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, TArray<FString>& AssetPathList) const
{
	for (const TSharedPtr<FSchemaTreeItem>& Asset : TreeItem->ChildAssets)
	{
		if (Asset != TopLevelAssemblyItem)
		{
			AssetPathList.Add(Asset->Path);
		}
	}

	for (const TSharedPtr<FSchemaTreeItem>& Child : TreeItem->ChildFolders)
	{
		GetAssetListRecursive(Child, AssetPathList);
	}
}

TSharedPtr<FSchemaTreeItem> SCineAssemblySchemaWindow::FindItemAtPathRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, const FString& Path) const
{
	if (TreeItem->Path.Equals(Path))
	{
		return TreeItem;
	}

	for (const TSharedPtr<FSchemaTreeItem>& Child : TreeItem->ChildFolders)
	{
		if (const TSharedPtr<FSchemaTreeItem>& ItemAtPath = FindItemAtPathRecursive(Child, Path))
		{
			return ItemAtPath;
		}
	}

	return nullptr;
}

void SCineAssemblySchemaWindow::SetChildrenPathRecursive(TSharedPtr<FSchemaTreeItem> TreeItem, const FString& NewPath)
{
	for (TSharedPtr<FSchemaTreeItem>& Asset : TreeItem->ChildAssets)
	{
		const FString OldAssetName = FPaths::GetPathLeaf(Asset->Path);

		const FString NewAssetPath = NewPath / OldAssetName;
		Asset->Path = NewAssetPath;
	}

	for (TSharedPtr<FSchemaTreeItem>& Child : TreeItem->ChildFolders)
	{
		const FString OldChildFolderName = FPaths::GetPathLeaf(Child->Path);

		const FString NewChildPath = NewPath / OldChildFolderName;
		Child->Path = NewChildPath;

		SetChildrenPathRecursive(Child, NewChildPath);
	}
}

void SCineAssemblySchemaWindow::EnterEditMode(TSharedPtr<FString> ItemToRename)
{
	// Get the row for the input item and put its textblock in edit mode so the user can rename the item
	if (TSharedPtr<SSchemaAssetTableRow> Widget = StaticCastSharedPtr<SSchemaAssetTableRow>(AssetListView->WidgetFromItem(ItemToRename)))
	{
		if (Widget->TextBlock)
		{
			FSlateApplication::Get().SetKeyboardFocus(Widget->TextBlock.ToSharedRef());
			Widget->TextBlock->EnterEditingMode();
		}
	}
}

void SCineAssemblySchemaWindow::OnTreeItemTextCommitted(const FText& InText, ETextCommit::Type InCommitType, TSharedPtr<FSchemaTreeItem> TreeItem)
{
	// Early-out if the name has not actually changed
	const FString OldPath = TreeItem->Path;
	const FString OldName = FPaths::GetPathLeaf(OldPath);
	if (OldName.Equals(InText.ToString()))
	{
		return;
	}

	const FString NewPath = FPaths::GetPath(OldPath) / InText.ToString();
	TreeItem->Path = NewPath;

	if (TreeItem->Type == FSchemaTreeItem::EItemType::Folder)
	{
		// If this is a folder item, update the path of all of its children (recursively)
		SetChildrenPathRecursive(TreeItem, NewPath);

		TreeItem->Parent->ChildFolders.Sort(UE::CineAssemblyTools::Private::SortTreeItems);
	}
	else
	{
		TreeItem->Parent->ChildAssets.Sort(UE::CineAssemblyTools::Private::SortTreeItems);
	}

	UpdateFolderList();
	UpdateAssetList();

	TreeView->RequestTreeRefresh();
}

bool SCineAssemblySchemaWindow::ValidateAssetName(const FText& InText, FText& OutErrorMessage) const
{
	// An empty name is invalid
	if (InText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyAssetNameErrorMessage", "Please provide a name for the asset");
		return false;
	}

	// Ensure that the name does not contain any characters that would be invalid for an asset name
	// This matches the validation that would happen if the user was renaming an asset in the content browser
	FString InvalidCharacters = INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS;

	// These characters are actually valid, because we want to support naming tokens
	InvalidCharacters = InvalidCharacters.Replace(TEXT("{}"), TEXT(""));
	InvalidCharacters = InvalidCharacters.Replace(TEXT(":"), TEXT(""));

	return FName::IsValidXName(InText.ToString(), InvalidCharacters, &OutErrorMessage);
}

bool SCineAssemblySchemaWindow::ValidateFolderName(const FText& InText, FText& OutErrorMessage, TSharedPtr<FSchemaTreeItem> TreeItem) const
{
	// Check for empty text string
	if (InText.IsEmpty())
	{
		OutErrorMessage = LOCTEXT("EmptyFolderNameErrorMessage", "Please provide a name for this folder");
		return false;
	}

	// These characters are actually valid, because we want to support naming tokens
	const FString FolderNameWithoutTokenChars = InText.ToString().Replace(TEXT(":"), TEXT(""));

	// Check for invalid characters
	if (!AssetViewUtils::IsValidFolderName(FolderNameWithoutTokenChars, OutErrorMessage))
	{
		return false;
	}

	// Check for duplicate folder names
	const FString ParentPath = FPaths::GetPath(TreeItem->Path);
	TSharedPtr<FSchemaTreeItem> FoundItem = FindItemAtPathRecursive(RootItem, ParentPath / InText.ToString());

	if (FoundItem && FoundItem != TreeItem)
	{
		OutErrorMessage = LOCTEXT("DuplicateNameErrorMessage", "A folder already exists at this location with this name");
		return false;
	}

	return true;
}

void SCineAssemblySchemaWindow::DeleteAssetItem(TSharedPtr<FString> InItem)
{
	const int32 IndexToRemove = AssetListItems.IndexOfByKey(InItem);
	if (AssetListItems.IsValidIndex(IndexToRemove))
	{
		AssetListItems.RemoveAt(IndexToRemove);
	}
	AssetListView->RequestListRefresh();
}

void SCineAssemblySchemaWindow::DeleteTreeItem(TSharedPtr<FSchemaTreeItem> TreeItem)
{
	if (TreeItem->Type == FSchemaTreeItem::EItemType::Folder)
	{
		if (ContainsRecursive(TreeItem, TopLevelAssemblyItem))
		{
			TreeItem->Parent->ChildAssets.Add(TopLevelAssemblyItem);
			TopLevelAssemblyItem->Parent = TreeItem->Parent;
		}

		TreeItem->Parent->ChildFolders.Remove(TreeItem);
	}
	else
	{
		TreeItem->Parent->ChildAssets.Remove(TreeItem);
	}

	UpdateFolderList();
	UpdateAssetList();

	TreeView->RequestTreeRefresh();
}

bool SCineAssemblySchemaWindow::ContainsRecursive(TSharedPtr<FSchemaTreeItem> InParent, TSharedPtr<FSchemaTreeItem> InItem)
{
	if (InParent->ChildAssets.Contains(InItem))
	{
		return true;
	}

	bool bFoundRecursively = false;
	for (TSharedPtr<FSchemaTreeItem> ChildFolder : InParent->ChildFolders)
	{
		bFoundRecursively |= ContainsRecursive(ChildFolder, InItem);
	}

	return bFoundRecursively;
}

void SCineAssemblySchemaWindow::OnSchemaPropertiesChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAssemblyMetadataDesc, Key))
	{
		UNamingTokensEngineSubsystem* NamingTokensSubsystem = GEngine->GetEngineSubsystem<UNamingTokensEngineSubsystem>();
		UCineAssemblyNamingTokens* CineAssemblyNamingTokens = Cast<UCineAssemblyNamingTokens>(NamingTokensSubsystem->GetNamingTokens(UCineAssemblyNamingTokens::TokenNamespace));

		for (const FAssemblyMetadataDesc& MetadataDesc : SchemaToConfigure->AssemblyMetadata)
		{
			CineAssemblyNamingTokens->AddMetadataToken(MetadataDesc.Key);
		}
	}
}

FReply SCineAssemblySchemaWindow::OnCreateAssetClicked()
{
	// Prevent the user from finishing creating the schema if there are still assets in the list view that have not been placed in the content hierarchy
	if (AssetListItems.Num() > 0)
	{
		const FText DialogMessage = LOCTEXT("RemainingAssetDialog", "The Content Hierarchy tab contains named subsequences that have not yet been placed in the folder tree. "
			"Please drag and drop the remaining subsequences, or delete them from the asset list if they are not needed.");
		FMessageDialog::Open(EAppMsgType::Ok, DialogMessage);

		return FReply::Unhandled();
	}

	UCineAssemblySchemaFactory::CreateConfiguredSchema(SchemaToConfigure.Get(), CreateAssetPath);

	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}

FReply SCineAssemblySchemaWindow::OnCancelClicked()
{
	TSharedPtr<SWindow> ContainingWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
	if (ContainingWindow.IsValid())
	{
		ContainingWindow->RequestDestroyWindow();
	}

	return FReply::Handled();
}

void SCineAssemblySchemaWindow::UpdateFolderList()
{
	// Update the schema's folder list
	const TArray<FString> CachedFolderList = SchemaToConfigure->FoldersToCreate;

	TArray<FString> FolderList;
	GetFolderListRecursive(RootItem, FolderList);

	if (FolderList != CachedFolderList)
	{
		SchemaToConfigure->Modify();
		SchemaToConfigure->FoldersToCreate = FolderList;
	}
}

void SCineAssemblySchemaWindow::UpdateAssetList()
{
	// Update the schema's asset list
	const TArray<FString> CachedAssetList = SchemaToConfigure->SubsequencesToCreate;

	TArray<FString> AssetList;
	GetAssetListRecursive(RootItem, AssetList);

	SchemaToConfigure->DefaultAssemblyPath = TopLevelAssemblyItem->Parent->Path;

	if (AssetList != CachedAssetList)
	{
		SchemaToConfigure->Modify();
		SchemaToConfigure->SubsequencesToCreate = AssetList;
	}
}

FReply SCineAssemblySchemaWindow::OnAssetRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		// The ListView uses single selection mode, so at most one item can ever be selected by the user
		TArray<TSharedPtr<FString>> SelectedItems = AssetListView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			const TSharedPtr<FString>& SelectedItem = SelectedItems[0];
			TSharedRef<FSchemaAssetDragDrop> Operation = FSchemaAssetDragDrop::New(*SelectedItem);

			Operation->OnDropNotHandled.BindLambda([this](const FString& AssetName)
				{
					// If the asset item is not dropped in a valid place, restore it to the asset list
					AssetListItems.Add(MakeShared<FString>(AssetName));
					AssetListView->RequestListRefresh();
				});

			// Remove this item from the asset list while it is being dragged
			AssetListItems.Remove(SelectedItems[0]);
			AssetListView->RequestListRefresh();

			return FReply::Handled().BeginDragDrop(Operation);
		}
	}

	return FReply::Unhandled();
}

FReply SCineAssemblySchemaWindow::OnTreeRowDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TArray<TSharedPtr<FSchemaTreeItem>> SelectedTreeItems = TreeView->GetSelectedItems();
		if (SelectedTreeItems.Num() == 1)
		{
			TSharedPtr<FSchemaTreeItem> SelectedTreeItem = SelectedTreeItems[0];
		
			if (SelectedTreeItem == RootItem)
			{
				return FReply::Handled();
			}
			
			TSharedRef<FSchemaAssetDragDrop> Operation = FSchemaAssetDragDrop::New(FPaths::GetPathLeaf(SelectedTreeItem->Path));
			Operation->SourceTreeItem = SelectedTreeItem;

			if (SelectedTreeItem->Type == FSchemaTreeItem::EItemType::Asset)
			{
				Operation->OnDropNotHandled.BindLambda([this, SelectedTreeItem](const FString& AssetName)
					{
						// If the tree item is not dropped in a valid place, restore it to its original place in the tree
						SelectedTreeItem->Parent->ChildAssets.Add(SelectedTreeItem);
						TreeView->RequestTreeRefresh();
					});

				// Remove this item from the tree while it is being dragged
				SelectedTreeItem->Parent->ChildAssets.Remove(SelectedTreeItem);
			}
			else
			{
				Operation->OnDropNotHandled.BindLambda([this, SelectedTreeItem](const FString& AssetName)
					{
						// If the tree item is not dropped in a valid place, restore it to its original place in the tree
						SelectedTreeItem->Parent->ChildFolders.Add(SelectedTreeItem);
						TreeView->RequestTreeRefresh();
					});

				// Remove this item from the tree while it is being dragged
				SelectedTreeItem->Parent->ChildFolders.Remove(SelectedTreeItem);
			}

			TreeView->RequestTreeRefresh();
			return FReply::Handled().BeginDragDrop(Operation);
		}
	}

	return FReply::Unhandled();
}

FReply SCineAssemblySchemaWindow::OnTreeRowAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, TSharedPtr<FSchemaTreeItem> InItem)
{
	TSharedPtr<FSchemaAssetDragDrop> Operation = InDragDropEvent.GetOperationAs<FSchemaAssetDragDrop>();
	if (Operation.IsValid())
	{
		TSharedPtr<FSchemaTreeItem> NewItem;
		if (Operation->SourceTreeItem)
		{
			NewItem = Operation->SourceTreeItem;
		}
		else
		{
			// This must be a new item from the asset list, so we need to make a new tree item for it
			NewItem = MakeShared<FSchemaTreeItem>();
			NewItem->Type = FSchemaTreeItem::EItemType::Asset;
		}

		// If an item with the same name already exists in the drop location, do not handle the drop (the dragged item will be reset to its original location)
		if (NewItem->Type == FSchemaTreeItem::EItemType::Asset)
		{
			if (Algo::ContainsBy(InItem->ChildAssets, InItem->Path / Operation->ItemName, &FSchemaTreeItem::Path))
			{
				return FReply::Unhandled();
			}
		}
		else if (NewItem->Type == FSchemaTreeItem::EItemType::Folder)
		{
			if (Algo::ContainsBy(InItem->ChildFolders, InItem->Path / Operation->ItemName, &FSchemaTreeItem::Path))
			{
				return FReply::Unhandled();
			}
		}

		const FString ParentPath = InItem->Path;
		NewItem->Path = ParentPath / Operation->ItemName;

		NewItem->Parent = InItem;

		if (NewItem->Type == FSchemaTreeItem::EItemType::Asset)
		{
			InItem->ChildAssets.Add(NewItem);
			InItem->ChildAssets.Sort(UE::CineAssemblyTools::Private::SortTreeItems);
		}
		else
		{
			InItem->ChildFolders.Add(NewItem);

			SetChildrenPathRecursive(NewItem, NewItem->Path);
			InItem->ChildFolders.Sort(UE::CineAssemblyTools::Private::SortTreeItems);
		}

		UpdateFolderList();
		UpdateAssetList();

		TreeView->SetItemExpansion(InItem, true);
		TreeView->RequestTreeRefresh();
	}

	return FReply::Handled();
}

TSharedRef<FSchemaAssetDragDrop> FSchemaAssetDragDrop::New(const FString& InName)
{
	TSharedRef<FSchemaAssetDragDrop> DragDropOp = MakeShared<FSchemaAssetDragDrop>();
	DragDropOp->ItemName = InName;
	DragDropOp->MouseCursor = EMouseCursor::GrabHandClosed;
	DragDropOp->Construct();

	return DragDropOp;
}

TSharedPtr<SWidget> FSchemaAssetDragDrop::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Padding(8.0f)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SImage).Image(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Sequencer"))
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(FText::FromString(ItemName))
				]
		];
}

void FSchemaAssetDragDrop::OnDragged(const class FDragDropEvent& DragDropEvent)
{
	if (CursorDecoratorWindow.IsValid())
	{
		CursorDecoratorWindow->MoveWindowTo(DragDropEvent.GetScreenSpacePosition() - (CursorDecoratorWindow->GetSizeInScreen() * FVector2f(0.0f, 0.5f)));
	}
}

void FSchemaAssetDragDrop::OnDrop(bool bDropWasHandled, const FPointerEvent& MouseEvent)
{
	if (!bDropWasHandled)
	{
		OnDropNotHandled.ExecuteIfBound(ItemName);
	}
}

#undef LOCTEXT_NAMESPACE
