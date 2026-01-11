// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCineAssemblyConfigWindow.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CineAssemblyFactory.h"
#include "CineAssemblyNamingTokens.h"
#include "CineAssemblyToolsStyle.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IContentBrowserSingleton.h"
#include "Interfaces/IPluginManager.h"
#include "IStructureDetailsView.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "ProductionSettings.h"
#include "PropertyEditorModule.h"
#include "UObject/Package.h"
#include "Settings/ContentBrowserSettings.h"
#include "STemplateStringEditableTextBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SCineAssemblyConfigWindow"

namespace UE::CineAssemblyWidgets::Private
{
	TSharedRef<IDetailsView> MakeDetailsView(UCineAssembly* Assembly)
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;

		TSharedRef<IDetailsView> DetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
		DetailsView->SetObject(Assembly, true);

		return DetailsView;
	}
}

void SCineAssemblyConfigWindow::Construct(const FArguments& InArgs, const FString& InCreateAssetPath)
{
	CreateAssetPath = InCreateAssetPath;

	// Create a new transient CineAssembly to configure in the UI.
	CineAssemblyToConfigure = TStrongObjectPtr<UCineAssembly>(NewObject<UCineAssembly>(GetTransientPackage(), NAME_None, RF_Transient));

	UWorld* CurrentWorld = GEditor->GetEditorWorldContext().World();
	CineAssemblyToConfigure->Level = FSoftObjectPath(CurrentWorld);

	const UProductionSettings* const ProductionSettings = GetDefault<UProductionSettings>();
	TOptional<const FCinematicProduction> ActiveProduction = ProductionSettings->GetActiveProduction();
	
	if (ActiveProduction.IsSet())
	{
		CineAssemblyToConfigure->Production = ActiveProduction.GetValue().ProductionID;
		CineAssemblyToConfigure->ProductionName = ActiveProduction.GetValue().ProductionName;
	}

	const FVector2D DefaultWindowSize = FVector2D(1400, 750);

	SWindow::Construct(SWindow::FArguments()
		.Title(LOCTEXT("WindowTitleCreateNew", "Create Cine Assembly"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(DefaultWindowSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						[
							SNew(SSplitter)
								.Orientation(Orient_Horizontal)
								.PhysicalSplitterHandleSize(2.0f)

								+ SSplitter::Slot()
								.Value(0.7f)
								[
									MakeCineTemplatePanel()
								]

								+ SSplitter::Slot()
								.Value(0.3f)
								[
									MakeInfoPanel()
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
				]
		]);
}

SCineAssemblyConfigWindow::~SCineAssemblyConfigWindow()
{
	// Save the UI config settings for whether to display engine/plugin content
	if (UContentBrowserSettings* ContentBrowserSettings = GetMutableDefault<UContentBrowserSettings>())
	{
		const bool bShowEngineContent = ContentBrowserSettings->GetDisplayEngineFolder();
		const bool bShowPluginContent = ContentBrowserSettings->GetDisplayPluginFolders();

		if (GConfig)
		{
			GConfig->SetBool(TEXT("NewCineAssemblyUI"), TEXT("bShowEngineContent"), bShowEngineContent, GEditorPerProjectIni);
			GConfig->SetBool(TEXT("NewCineAssemblyUI"), TEXT("bShowPluginContent"), bShowPluginContent, GEditorPerProjectIni);
		}

		ContentBrowserSettings->SetDisplayEngineFolder(bShowEngineContentCached);
		ContentBrowserSettings->SetDisplayPluginFolders(bShowPluginContentCached);
	}
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeCineTemplatePanel()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");

	// The asset picker will only display Cine Assembly Schema assets
	FAssetPickerConfig Config;
	Config.Filter.ClassPaths.Add(UCineAssemblySchema::StaticClass()->GetClassPathName());
	Config.SelectionMode = ESelectionMode::Single;
	Config.InitialAssetViewType = EAssetViewType::Tile;
	Config.bFocusSearchBoxWhenOpened = false;
	Config.bShowBottomToolbar = false;
	Config.bAllowDragging = false;
	Config.bAllowRename = false;
	Config.bCanShowClasses = false;
	Config.bCanShowFolders = false;
	Config.bForceShowEngineContent = false;
	Config.bForceShowPluginContent = false;
	Config.bAddFilterUI = false;
	Config.bShowPathInColumnView = true;
	Config.OnAssetSelected = FOnAssetSelected::CreateSP(this, &SCineAssemblyConfigWindow::OnSchemaSelected);

	const FString PackageName = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetMountedAssetPath();
	const FString NoSchemaName = TEXT("NoSchema");
	const FString PackagePath = PackageName / NoSchemaName;

	FAssetData NoSchemaAssetData = FAssetData(*PackagePath, *PackageName, *NoSchemaName, FTopLevelAssetPath());

	// Add a fake asset to the list (so that it appears as a tile in the asset picker) that represents a selection of no schema
	Config.OnGetCustomSourceAssets = FOnGetCustomSourceAssets::CreateLambda([NoSchemaAssetData](const FARFilter& Filter, TArray<FAssetData>& OutAssets)
		{
			OutAssets.Add(NoSchemaAssetData);
		});

	Config.InitialAssetSelection = NoSchemaAssetData;

	// The fake NoSchema asset should not display the normal asset tooltip, just a plain text-based tooltip describing what it is
	Config.OnIsAssetValidForCustomToolTip = FOnIsAssetValidForCustomToolTip::CreateLambda([NoSchemaAssetData](FAssetData& AssetData)
		{
			return (AssetData == NoSchemaAssetData) ? true : false;
		});

	Config.OnGetCustomAssetToolTip = FOnGetCustomAssetToolTip::CreateLambda([](FAssetData& AssetData) -> TSharedRef<SToolTip>
		{
			return SNew(SToolTip).Text(LOCTEXT("NoSchemaToolTip", "Create a new assembly with no schema"));
		});

	// Check the UI config settings to determine whether or not to display engine/plugin content by default in this window
	UContentBrowserSettings* ContentBrowserSettings = GetMutableDefault<UContentBrowserSettings>();

	bool bShowEngineContent = true;
	bool bShowPluginContent = true;
	GConfig->GetBool(TEXT("NewCineAssemblyUI"), TEXT("bShowEngineContent"), bShowEngineContent, GEditorPerProjectIni);
	GConfig->GetBool(TEXT("NewCineAssemblyUI"), TEXT("bShowPluginContent"), bShowPluginContent, GEditorPerProjectIni);

	bShowEngineContentCached = ContentBrowserSettings->GetDisplayEngineFolder();
	bShowPluginContentCached = ContentBrowserSettings->GetDisplayPluginFolders();

	ContentBrowserSettings->SetDisplayEngineFolder(bShowEngineContent);
	ContentBrowserSettings->SetDisplayPluginFolders(bShowPluginContent);

	return SNew(SBorder)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
		.Padding(16.0f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(Config)
		];
}

void SCineAssemblyConfigWindow::OnSchemaSelected(const FAssetData& AssetData)
{
	const FString NoSchemaName = TEXT("NoSchema");
	if (AssetData.AssetName == NoSchemaName)
	{
		SelectedSchema = nullptr;
	}
	else if (UCineAssemblySchema* CineAssemblySchema = Cast<UCineAssemblySchema>(AssetData.GetAsset()))
	{
		SelectedSchema = CineAssemblySchema;
	}
	else
	{
		return;
	}

	CineAssemblyToConfigure->ChangeSchema(SelectedSchema);

	// The details view needs to be redrawn to show the new metadata fields from the selected schema
	DetailsView->ForceRefresh();

	// Recreate the hierarchy tree items based on the selected schema
	PopulateHierarchyTree();
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeInfoPanel()
{
	TabSwitcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()
			[ MakeDetailsWidget() ]

		+ SWidgetSwitcher::Slot()
			[ MakeHierarchyWidget() ]

		+ SWidgetSwitcher::Slot()
			[ MakeNotesWidget() ];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SSegmentedControl<int32>)
				.Value(0)
				.OnValueChanged_Lambda([Switcher = TabSwitcher](int32 NewValue)
				{
					Switcher->SetActiveWidgetIndex(NewValue);
				})

				+ SSegmentedControl<int32>::Slot(0)
				.Text(LOCTEXT("DetailsTab", "Details"))
				.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Details").GetIcon())

				+ SSegmentedControl<int32>::Slot(1)
				.Text(LOCTEXT("HierarchyTab", "Hierarchy"))
				.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.FolderClosed").GetIcon())

				+ SSegmentedControl<int32>::Slot(2)
				.Text(LOCTEXT("NotesTab", "Notes"))
				.Icon(FSlateIcon(FCineAssemblyToolsStyle::StyleName, "Icons.Notes").GetIcon())
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		[
			TabSwitcher.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.PanelBackground"))
			.Padding(16.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(0.0f, 0.0f, 8.0f, 0.0f)
						[
							SNew(STextBlock).Text(LOCTEXT("AssemblyNameField", "Assembly Name"))
						]

					+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						.VAlign(VAlign_Center)
						[
							SNew(STemplateStringEditableTextBox)
								.Text_Lambda([this]() -> FText 
									{
										return FText::FromString(CineAssemblyToConfigure->AssemblyName.Template);
									})
								.ResolvedText_Lambda([this]() 
									{ 
										EvaluateTokenString(CineAssemblyToConfigure->AssemblyName);
										return CineAssemblyToConfigure->AssemblyName.Resolved;
									})
								.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
									{
										CineAssemblyToConfigure->Modify();
										CineAssemblyToConfigure->AssemblyName.Template = InText.ToString();
									})
						]
				]
		];
}

void SCineAssemblyConfigWindow::EvaluateTokenString(FTemplateString& StringToEvaluate)
{
	FDateTime CurrentTime = FDateTime::Now();
	if ((CurrentTime - LastTokenUpdateTime).GetSeconds() >= 1.0f)
	{
		StringToEvaluate.Resolved = UCineAssemblyNamingTokens::GetResolvedText(StringToEvaluate.Template, CineAssemblyToConfigure.Get());
		LastTokenUpdateTime = CurrentTime;
	}
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeDetailsWidget()
{
	DetailsView = UE::CineAssemblyWidgets::Private::MakeDetailsView(CineAssemblyToConfigure.Get());

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.RecessedNoBorder"))
				.Padding(16.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
						.Padding(0.0f, 0.0f, 0.0f, 16.0f)
						.AutoHeight()
						[
							SNew(SHorizontalBox)

							+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(0.0f, 0.0f, 8.0f, 0.0f)
								[
									SNew(SImage)
										.Image_Lambda([this]()
											{
												return SelectedSchema ? SelectedSchema->GetThumbnailBrush() : FCineAssemblyToolsStyle::Get().GetBrush("Thumbnails.Schema");
											})
								]

								+ SHorizontalBox::Slot()
								.VAlign(VAlign_Center)
								[
									SNew(SVerticalBox)

									+ SVerticalBox::Slot()
										[
											SNew(STextBlock)
												.Text_Lambda([this]()
													{
														return SelectedSchema ? FText::FromString(SelectedSchema->SchemaName) : LOCTEXT("NoSchemaName", "No Schema");
													})
										]

									+ SVerticalBox::Slot()
										[
											SNew(STextBlock)
												.Text(LOCTEXT("SchemClassName", "Cine Assembly Schema"))
												.ColorAndOpacity(FSlateColor::UseSubduedForeground())
										]
								]
						]

					+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(STextBlock)
								.AutoWrapText(true)
								.Text_Lambda([this]()
									{
										if (SelectedSchema)
										{
											return !SelectedSchema->Description.IsEmpty() ? FText::FromString(SelectedSchema->Description) : LOCTEXT("EmptyDescription", "No description");
										}
										return LOCTEXT("SchemaInstructions", "Choose a schema to use as the base for configuring your Cine Assembly, or proceed with no schema.");
									})
						]
				]
		]

		+ SVerticalBox::Slot()
		.FillContentHeight(1.0f)
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeHierarchyWidget()
{
	HierarchyTreeView = SNew(STreeView<TSharedPtr<FHierarchyTreeItem>>)
		.TreeItemsSource(&HierarchyTreeItems)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SCineAssemblyConfigWindow::OnGenerateTreeRow)
		.OnGetChildren(this, &SCineAssemblyConfigWindow::OnGetChildren);

	// Create the hierarchy tree root
	RootItem = MakeShared<FHierarchyTreeItem>();
	RootItem->Type = FHierarchyTreeItem::EItemType::Folder;

	FTemplateString RootTemplate;
	RootTemplate.Template = TEXT("");
	RootItem->Path = RootTemplate;

	PopulateHierarchyTree();

	// Register a Slate timer that runs at a set frequency to evaluate all of the tokens in the tree view.
	// This will automatically be unregistered when this window is destroyed.
	constexpr float TimerFrequency = 1.0f;
	RegisterActiveTimer(TimerFrequency, FWidgetActiveTimerDelegate::CreateLambda([this](double InCurrentTime, float InDeltaTime) -> EActiveTimerReturnType
		{
			EvaluateHierarchyTokensRecursive(RootItem);
			HierarchyTreeView->RequestTreeRefresh();

			return EActiveTimerReturnType::Continue;
		}));

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(16.0f)
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("HierarchyInstructions", "The following content will be created as defined by the selected Schema."))
				.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.RecessedBackground"))
				.Padding(8.0f)
				[
					HierarchyTreeView.ToSharedRef()
				]
		];
}

void SCineAssemblyConfigWindow::PopulateHierarchyTree()
{
	HierarchyTreeItems.Empty();
	HierarchyTreeItems.Add(RootItem);
	RootItem->ChildAssets.Empty();
	RootItem->ChildFolders.Empty();

	auto AddItemsToTree = [this](TArray<FTemplateString>& ItemList, FHierarchyTreeItem::EItemType ItemType)
		{
			// Sort the list so that paths are added to the tree in the proper order
			ItemList.Sort([](const FTemplateString& A, const FTemplateString& B) { return A.Template < B.Template; });

			for (const FTemplateString& ItemName : ItemList)
			{
				const FString ParentPath = FPaths::GetPath(ItemName.Template);

				// Walk the tree until we find an item whose path matches the parent path. The new tree item will be created as one of its children
				TSharedPtr<FHierarchyTreeItem> ParentItem = FindItemAtPathRecursive(RootItem, ParentPath);
				if (ParentItem)
				{
					TSharedPtr<FHierarchyTreeItem> NewItem = MakeShared<FHierarchyTreeItem>();
					NewItem->Type = ItemType;
					NewItem->Path = ItemName;

					if (ItemType == FHierarchyTreeItem::EItemType::Folder)
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

	if (CineAssemblyToConfigure->BaseSchema)
	{
		TArray<FTemplateString> FolderTemplates;
		FolderTemplates.Reserve(CineAssemblyToConfigure->BaseSchema->FoldersToCreate.Num());
		Algo::Transform(CineAssemblyToConfigure->BaseSchema->FoldersToCreate, FolderTemplates, [](const FString& TemplateString)
			{ 
				FTemplateString FolderTemplate;
				FolderTemplate.Template = TemplateString;
				return FolderTemplate;
			});

		AddItemsToTree(FolderTemplates, FHierarchyTreeItem::EItemType::Folder);
		AddItemsToTree(CineAssemblyToConfigure->SubAssemblyNames, FHierarchyTreeItem::EItemType::Asset);

		EvaluateHierarchyTokensRecursive(RootItem);
	}

	HierarchyTreeView->RequestTreeRefresh();
	ExpandTreeRecursive(RootItem);
}

void SCineAssemblyConfigWindow::EvaluateHierarchyTokensRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem)
{
	// Evaluate the token template string for this tree item
	TreeItem->Path.Resolved = UCineAssemblyNamingTokens::GetResolvedText(TreeItem->Path.Template, CineAssemblyToConfigure.Get());

	// Evaluate the tokens for all of the child assets, then resort them alphabetically based on the resolved paths
	for (const TSharedPtr<FHierarchyTreeItem>& Asset : TreeItem->ChildAssets)
	{
		EvaluateHierarchyTokensRecursive(Asset);
	}

	TreeItem->ChildAssets.Sort([](const TSharedPtr<FHierarchyTreeItem>& A, const TSharedPtr<FHierarchyTreeItem>& B) { return A->Path.Resolved.ToString() < B->Path.Resolved.ToString(); });

	// Evaluate the tokens for all of the child folders, then resort them alphabetically based on the resolved paths
	for (const TSharedPtr<FHierarchyTreeItem>& Child : TreeItem->ChildFolders)
	{
		EvaluateHierarchyTokensRecursive(Child);
	}

	TreeItem->ChildFolders.Sort([](const TSharedPtr<FHierarchyTreeItem>& A, const TSharedPtr<FHierarchyTreeItem>& B) { return A->Path.Resolved.ToString() < B->Path.Resolved.ToString(); });
}

void SCineAssemblyConfigWindow::ExpandTreeRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem) const
{
	if (HierarchyTreeView)
	{
		HierarchyTreeView->SetItemExpansion(TreeItem, true);
	}

	for (const TSharedPtr<FHierarchyTreeItem>& ChildItem : TreeItem->ChildFolders)
	{
		ExpandTreeRecursive(ChildItem);
	}
}

TSharedPtr<FHierarchyTreeItem> SCineAssemblyConfigWindow::FindItemAtPathRecursive(TSharedPtr<FHierarchyTreeItem> TreeItem, const FString& Path) const
{
	if (TreeItem->Path.Template.Equals(Path))
	{
		return TreeItem;
	}

	for (const TSharedPtr<FHierarchyTreeItem>& Child : TreeItem->ChildFolders)
	{
		if (const TSharedPtr<FHierarchyTreeItem>& ItemAtPath = FindItemAtPathRecursive(Child, Path))
		{
			return ItemAtPath;
		}
	}

	return nullptr;
}

TSharedRef<ITableRow> SCineAssemblyConfigWindow::OnGenerateTreeRow(TSharedPtr<FHierarchyTreeItem> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SImage> Icon = SNew(SImage);

	if (TreeItem->Type == FHierarchyTreeItem::EItemType::Folder)
	{
		Icon->SetImage(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Folder"));
		Icon->SetColorAndOpacity(FAppStyle::Get().GetSlateColor("ContentBrowser.DefaultFolderColor"));
	}
	else
	{
		Icon->SetImage(FCineAssemblyToolsStyle::Get().GetBrush("Icons.Sequencer"));
		Icon->SetColorAndOpacity(FLinearColor::White);
	}

	return SNew(STableRow<TSharedPtr<FHierarchyTreeItem>>, OwnerTable)
		.Padding(FMargin(8.0f, 2.0f, 8.0f, 0.0f))
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
					SNew(STextBlock)
						.Text_Lambda([this, TreeItem]() -> FText
							{
								return (TreeItem == RootItem) ? LOCTEXT("RootPathName", "Root Folder") : FText::FromString(FPaths::GetPathLeaf(TreeItem->Path.Resolved.ToString()));
							})
				]
		];
}

void SCineAssemblyConfigWindow::OnGetChildren(TSharedPtr<FHierarchyTreeItem> TreeItem, TArray<TSharedPtr<FHierarchyTreeItem>>& OutNodes)
{
	// Display all of the child assets first, followed by all of the child folders
	OutNodes.Append(TreeItem->ChildAssets);
	OutNodes.Append(TreeItem->ChildFolders);
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeNotesWidget()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(16.0f)
		.AutoHeight()
		[
			SNew(STextBlock)
				.Text(LOCTEXT("NoteInstructions", "The following notes will be saved with the assembly. This can also be edited later."))
				.AutoWrapText(true)
		]

		+ SVerticalBox::Slot()
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.Background"))
				.Padding(16.0f)
				[
					SNew(SMultiLineEditableText)
						.HintText(LOCTEXT("NoteHintText", "Assembly Notes"))
						.Text_Lambda([this]()
						{ 
							return FText::FromString(CineAssemblyToConfigure->AssemblyNote); 
						})
						.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
						{
							CineAssemblyToConfigure->Modify();
							CineAssemblyToConfigure->AssemblyNote = InText.ToString();
						})
				]
		];
}

TSharedRef<SWidget> SCineAssemblyConfigWindow::MakeButtonsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		.Padding(16.0f)
		.HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 8.0f, 0.0f)
				[
					SNew(SButton)
						.Text(this, &SCineAssemblyConfigWindow::GetCreateButtonText)
						.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
						.HAlign(HAlign_Center)
						.OnClicked(this, &SCineAssemblyConfigWindow::OnCreateAssetClicked)
				]

			+ SHorizontalBox::Slot()
				.MinWidth(118.0f)
				.MaxWidth(118.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
						.Text(LOCTEXT("CancelButton", "Cancel"))
						.HAlign(HAlign_Center)
						.OnClicked(this, &SCineAssemblyConfigWindow::OnCancelClicked)
				]
		];
}

FText SCineAssemblyConfigWindow::GetCreateButtonText() const
{
	if (SelectedSchema && !SelectedSchema->SchemaName.IsEmpty())
	{
		return FText::Format(LOCTEXT("CreateAssetButtonWithSchema", "Create {0}"), FText::FromString(SelectedSchema->SchemaName));
	}
	return LOCTEXT("CreateAssetButton", "Create Assembly");
}

FReply SCineAssemblyConfigWindow::OnCreateAssetClicked()
{
	UCineAssemblyFactory::CreateConfiguredAssembly(CineAssemblyToConfigure.Get(), CreateAssetPath);

	RequestDestroyWindow();
	return FReply::Handled();
}

FReply SCineAssemblyConfigWindow::OnCancelClicked()
{
	RequestDestroyWindow();
	return FReply::Handled();
}

void SCineAssemblyEditWidget::Construct(const FArguments& InArgs, UCineAssembly* InAssembly)
{
	CineAssembly = InAssembly;
	ChildSlot [ BuildUI() ];
}

void SCineAssemblyEditWidget::Construct(const FArguments& InArgs, FGuid InAssemblyGuid)
{
	// The UI will be temporary because no CineAssembly has been found yet
	ChildSlot [ BuildUI() ];

	// If the asset registry is still scanning assets, add a callback to find the assembly asset matching the input GUID and update this widget once the scan is finished.
	// Otherwise, we can find the assembly asset and update the UI immediately. 
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddSP(this, &SCineAssemblyEditWidget::FindAssembly, InAssemblyGuid);
	}
	else
	{
		FindAssembly(InAssemblyGuid);
	}
}

SCineAssemblyEditWidget::~SCineAssemblyEditWidget()
{
	if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry"))
	{
		AssetRegistryModule->Get().OnFilesLoaded().RemoveAll(this);
	}
}

TSharedRef<SWidget> SCineAssemblyEditWidget::BuildUI()
{
	// Build a temporary UI to display while waiting for the assembly to be loaded
	if (!CineAssembly)
	{
		return SNew(SBorder)
			.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.PanelNoBorder"))
			.Padding(8.0f)
			[
				SNew(STextBlock).Text(LOCTEXT("LoadingAssemblyText", "Loading Cine Assembly..."))
			];
	}

	TSharedRef<IDetailsView> DetailsView = UE::CineAssemblyWidgets::Private::MakeDetailsView(CineAssembly);
	DetailsView->SetIsCustomRowVisibleDelegate(FIsCustomRowVisible::CreateSP(this, &SCineAssemblyEditWidget::IsCustomRowVisible));
	DetailsView->ForceRefresh();

	TabSwitcher = SNew(SWidgetSwitcher)
		+ SWidgetSwitcher::Slot()
		[MakeOverviewWidget()]

		+ SWidgetSwitcher::Slot()
		[DetailsView];

	return SNew(SBorder)
		.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.PanelNoBorder"))
		.Padding(8.0f)
		[
			SNew(SScrollBox)
				.Orientation(Orient_Vertical)

			+ SScrollBox::Slot()
				.AutoSize()
				.HAlign(HAlign_Center)
				.Padding(0.0f, 0.0f, 0.0f, 8.0f)
				[
					SNew(SSegmentedControl<int32>)
						.Value(0)
						.OnValueChanged_Lambda([Switcher = TabSwitcher](int32 NewValue)
							{
								Switcher->SetActiveWidgetIndex(NewValue);
							})

						+ SSegmentedControl<int32>::Slot(0)
						.Text(LOCTEXT("OverviewTab", "Overview"))
						.Icon(FSlateIcon(FCineAssemblyToolsStyle::StyleName, "Icons.Animation").GetIcon())

						+ SSegmentedControl<int32>::Slot(1)
						.Text(LOCTEXT("DetailsTab", "Details"))
						.Icon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Details").GetIcon())
				]

			+ SScrollBox::Slot()
				.FillSize(1.0f)
				.HAlign(HAlign_Fill)
				[
					TabSwitcher.ToSharedRef()
				]
		];
}

bool SCineAssemblyEditWidget::IsCustomRowVisible(FName RowName, FName ParentName)
{
	if (RowName == GET_MEMBER_NAME_CHECKED(UCineAssembly, SubAssemblyNames))
	{
		return false;
	}
	return true;
}

void SCineAssemblyEditWidget::FindAssembly(FGuid AssemblyID)
{
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");

	// The only search criterion for the asset search is for an asset with an AssemblyID matching the input GUID
	const TMultiMap<FName, FString> TagValues = { { UCineAssembly::AssemblyGuidPropertyName, AssemblyID.ToString() } };

	TArray<FAssetData> AssemblyAssets;
	AssetRegistryModule.Get().GetAssetsByTagValues(TagValues, AssemblyAssets);

	// The Assembly ID is unique, so at most one asset should ever be found
	if (AssemblyAssets.Num() > 0)
	{
		CineAssembly = Cast<UCineAssembly>(AssemblyAssets[0].GetAsset());

		// Update the widget's UI
		ChildSlot.DetachWidget();
		ChildSlot.AttachWidget(BuildUI());
	}
}

FString SCineAssemblyEditWidget::GetAssemblyName()
{
	if (CineAssembly)
	{
		FString AssemblyName;
		CineAssembly->GetName(AssemblyName);
		return AssemblyName;
	}
	return TEXT("CineAssembly");
}

bool SCineAssemblyEditWidget::HasRenderedThumbnail()
{
	const FObjectThumbnail* ObjectThumbnail = nullptr;
	if (CineAssembly)
	{
		const FName FullAssetName = *CineAssembly->GetFullName();

		FThumbnailMap ThumbnailMap;
		ThumbnailTools::ConditionallyLoadThumbnailsForObjects({ FullAssetName }, ThumbnailMap);
		ObjectThumbnail = ThumbnailMap.Find(FullAssetName);
	}

	const bool bHasRenderedThumbnail = ObjectThumbnail && !ObjectThumbnail->IsEmpty();
	return bHasRenderedThumbnail;
}

TSharedRef<SWidget> SCineAssemblyEditWidget::MakeOverviewWidget()
{
	const UCineAssemblySchema* Schema = CineAssembly->GetSchema();

	auto GetVisbility = [Schema]() -> EVisibility
		{
			return Schema ? EVisibility::Visible : EVisibility::Collapsed;
		};

	TSharedRef<FAssetThumbnail> AssetThumbnail = MakeShared<FAssetThumbnail>(CineAssembly, 256, 256, UThumbnailManager::Get().GetSharedThumbnailPool());
	FAssetThumbnailConfig ThumbnailConfig;
	ThumbnailConfig.AssetTypeColorOverride = FLinearColor::Transparent;

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, 16.0f)
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("ProductionWizard.RecessedBackground"))
				.Padding(4.0f)
				[
					SNew(SVerticalBox)

					+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 8.0f, 0.0f, 8.0f)
						.HAlign(HAlign_Center)
						[
							SNew(SHorizontalBox)

								+ SHorizontalBox::Slot()
								.AutoWidth()
								[
									AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
								]

								+ SHorizontalBox::Slot()
								.Padding(16.0f, 0.0, 0.0, 0.0f)
								.VAlign(VAlign_Center)
								.FillContentWidth(1.0f)
								[
									SNew(STextBlock)
										.Text(LOCTEXT("ThumbnailHintText", "This assembly does not currently have a preview thumbnail. Open this asset in Sequencer and save it to render a preview to display here."))
										.AutoWrapText(true)
										.Visibility_Lambda([this]() { return HasRenderedThumbnail() ? EVisibility::Collapsed : EVisibility::Visible; })
								]
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock).Text_Lambda([this]() {	return FText::FromString(GetAssemblyName()); })
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						.HAlign(HAlign_Center)
						.Padding(0.0f, 0.0f, 0.0f, 8.0f)
						[
							SNew(STextBlock)
								.Text_Lambda([Schema]() { return Schema ? FText::FromString(Schema->SchemaName) : LOCTEXT("NoSchemaName", "No Schema"); })
								.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
				]
		]

		+ SVerticalBox::Slot()
		.MinHeight(300.0f)
		.FillContentHeight(1.0f)
		[
			SNew(SBorder)
				.BorderImage(FCineAssemblyToolsStyle::Get().GetBrush("Borders.Background"))
				.Padding(16.0f)
				[
					SNew(SMultiLineEditableText)
						.HintText(LOCTEXT("NoteHintText", "Assembly Notes"))
						.Text_Lambda([this]()
							{
								return FText::FromString(CineAssembly->AssemblyNote);
							})
						.OnTextCommitted_Lambda([this](const FText& InText, ETextCommit::Type InCommitType)
							{
								if (CineAssembly->AssemblyNote != InText.ToString())
								{
									CineAssembly->Modify();
									CineAssembly->AssemblyNote = InText.ToString();
								}
							})
				]
		];
}

#undef LOCTEXT_NAMESPACE
