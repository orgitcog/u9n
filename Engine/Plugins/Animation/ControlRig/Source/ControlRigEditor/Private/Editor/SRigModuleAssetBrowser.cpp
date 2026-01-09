// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigModuleAssetBrowser.h"

#include "AssetDefinitionRegistry.h"
#include "ContentBrowserModule.h"
#include "ControlRigBlueprint.h"
#include "IContentBrowserSingleton.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/AssetRegistryTagsContext.h"

#include "ControlRigEditor.h"
#include "FrontendFilterBase.h"
#include "Editor/RigVMEditorTools.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SRigVMVariantWidget.h"

#define LOCTEXT_NAMESPACE "RigModuleAssetBrowser"

namespace UE::Editor::ContentBrowser
{
	static bool IsNewStyleEnabled()
	{
		static bool bIsNewStyleEnabled = [&]()
		{
			if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("ContentBrowser.EnableNewStyle")))
			{
				ensureAlwaysMsgf(!EnumHasAnyFlags(CVar->GetFlags(), ECVF_Default), TEXT("The CVar should have already been set from commandline, @see: UnrealEdGlobals.cpp, UE::Editor::ContentBrowser::EnableContentBrowserNewStyleCVarRegistration."));
				return CVar->GetBool();
			}
			return false;
		}();

		return bIsNewStyleEnabled;
	}
}

typedef UE::RigVM::Editor::Tools::FFilterByAssetTag FRigVMFilterTag; 

void SRigModuleAssetBrowser::Construct(
	const FArguments& InArgs,
	TSharedRef<IControlRigBaseEditor> InEditor)
{
	ControlRigEditor = InEditor;

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SAssignNew(AssetBrowserBox, SBox)
		]
	];

	RefreshView();
}

void SRigModuleAssetBrowser::RefreshView()
{
	FAssetPickerConfig AssetPickerConfig;
	
	// setup filtering
	AssetPickerConfig.Filter.ClassPaths.Add(UControlRigBlueprint::StaticClass()->GetClassPathName());
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::Tile;
	AssetPickerConfig.bAddFilterUI = true;
	AssetPickerConfig.bShowPathInColumnView = true;
	AssetPickerConfig.bShowTypeInColumnView = true;
	AssetPickerConfig.OnShouldFilterAsset = FOnShouldFilterAsset::CreateSP(this, &SRigModuleAssetBrowser::OnShouldFilterAsset);
	AssetPickerConfig.DefaultFilterMenuExpansion = EAssetTypeCategories::Blueprint;
	AssetPickerConfig.OnGetAssetContextMenu = FOnGetAssetContextMenu::CreateSP(this, &SRigModuleAssetBrowser::OnGetAssetContextMenu);
	AssetPickerConfig.GetCurrentSelectionDelegates.Add(&GetCurrentSelectionDelegate);
	AssetPickerConfig.bAllowNullSelection = false;
	AssetPickerConfig.bFocusSearchBoxWhenOpened = false;
	AssetPickerConfig.bAllowDragging = true;
	AssetPickerConfig.bAllowRename = false;
	AssetPickerConfig.bForceShowPluginContent = true;
	AssetPickerConfig.bForceShowEngineContent = true;
	AssetPickerConfig.InitialThumbnailSize = EThumbnailSize::Small;
	AssetPickerConfig.OnGetCustomAssetToolTip = FOnGetCustomAssetToolTip::CreateSP(this, &SRigModuleAssetBrowser::CreateCustomAssetToolTip);

	// hide all asset registry columns by default (we only really want the name and path)
	UObject* DefaultControlRigBlueprint = UControlRigBlueprint::StaticClass()->GetDefaultObject();
	FAssetRegistryTagsContextData Context(DefaultControlRigBlueprint, EAssetRegistryTagsCaller::Uncategorized);
	DefaultControlRigBlueprint->GetAssetRegistryTags(Context);
	for (TPair<FName, UObject::FAssetRegistryTag>& AssetRegistryTagPair : Context.Tags)
	{
		AssetPickerConfig.HiddenColumnNames.Add(AssetRegistryTagPair.Value.Name.ToString());
	}

	// Also hide the type column by default (but allow users to enable it, so don't use bShowTypeInColumnView)
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Class"));
	AssetPickerConfig.HiddenColumnNames.Add(TEXT("Has Virtualized Data"));

	// allow to open the rigs directly on double click
	AssetPickerConfig.OnAssetDoubleClicked = FOnAssetDoubleClicked::CreateSP(this, &SRigModuleAssetBrowser::OnAssetDoubleClicked);

	TSharedRef<FFrontendFilterCategory>	ControlRigFilterCategory = MakeShared<FFrontendFilterCategory>(LOCTEXT("ControlRigFilterCategoryName", "Control Rig Tags"), LOCTEXT("ControlRigFilterCategoryToolTip", "Filter ControlRigs by variant tags specified in ControlRig Blueprint class settings"));
	const URigVMProjectSettings* Settings = GetDefault<URigVMProjectSettings>(URigVMProjectSettings::StaticClass());
	TArray<FRigVMTag> AvailableTags = Settings->VariantTags;

	TArray<TSharedRef<FRigVMFilterTag>> Filters;
	for (const FRigVMTag& CurTag : AvailableTags)
	{
		if (CurTag.bShowInUserInterface)
		{
			Filters.Add(MakeShared<FRigVMFilterTag>(ControlRigFilterCategory, CurTag));
			AssetPickerConfig.ExtraFrontendFilters.Add(Filters.Last());
		}
	}

	const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	AssetBrowserBox->SetContent(ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig));

	for (TSharedRef<FRigVMFilterTag> Filter : Filters)
	{
		Filter->SetActive(Filter->ShouldBeMarkedAsInvalid());
	}
}

TSharedPtr<SWidget> SRigModuleAssetBrowser::OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const
{
	if (SelectedAssets.Num() <= 0)
	{
		return nullptr;
	}

	UObject* SelectedAsset = SelectedAssets[0].GetAsset();
	if (SelectedAsset == nullptr)
	{
		return nullptr;
	}
	
	FMenuBuilder MenuBuilder(true, MakeShared<FUICommandList>());

	MenuBuilder.BeginSection(TEXT("Asset"), LOCTEXT("AssetSectionLabel", "Asset"));
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Browse", "Browse to Asset"),
			LOCTEXT("BrowseTooltip", "Browses to the associated asset and selects it in the most recently used Content Browser (summoning one if necessary)"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "SystemWideCommands.FindInContentBrowser.Small"),
			FUIAction(
				FExecuteAction::CreateLambda([SelectedAsset] ()
				{
					if (SelectedAsset)
					{
						const TArray<FAssetData>& Assets = { SelectedAsset };
						const FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
						ContentBrowserModule.Get().SyncBrowserToAssets(Assets);
					}
				}),
				FCanExecuteAction::CreateLambda([] () { return true; })
			)
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

bool SRigModuleAssetBrowser::OnShouldFilterAsset(const struct FAssetData& AssetData)
{
	// is this an control rig blueprint asset?
	if (!AssetData.IsInstanceOf(UControlRigBlueprint::StaticClass()))
	{
		return true;
	}
	
	static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
	const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
	if (ControlRigTypeStr.IsEmpty())
	{
		return true;
	}

	const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
	if(ControlRigType != EControlRigType::RigModule)
	{
		return true;
	}

	return false;
}

void SRigModuleAssetBrowser::OnAssetDoubleClicked(const FAssetData& AssetData)
{
	if (UAssetEditorSubsystem* EditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
	{
		EditorSubsystem->OpenEditorForAsset(AssetData.ToSoftObjectPath());
	}
}

TSharedRef<SToolTip> SRigModuleAssetBrowser::CreateCustomAssetToolTip(FAssetData& AssetData)
{
	if (UE::Editor::ContentBrowser::IsNewStyleEnabled())
	{
		return CreateCustomAssetToolTipNewStyle(AssetData);
	}

	// Make a list of tags to show
	TArray<UObject::FAssetRegistryTag> Tags;
	UClass* AssetClass = FindObject<UClass>(AssetData.AssetClassPath);
	check(AssetClass);
	UObject* DefaultObject = AssetClass->GetDefaultObject();
	FAssetRegistryTagsContextData TagsContext(DefaultObject, EAssetRegistryTagsCaller::Uncategorized);
	DefaultObject->GetAssetRegistryTags(TagsContext);

	TArray<FName> TagsToShow;
	static const FName ModulePath(TEXT("Path"));
	static const FName ModuleSettings(TEXT("RigModuleSettings"));
	for (const TPair<FName, UObject::FAssetRegistryTag>& TagPair : TagsContext.Tags)
	{
		if(TagPair.Key == ModulePath ||
			TagPair.Key == ModuleSettings)
		{
			TagsToShow.Add(TagPair.Key);
		}
	}

	TMap<FName, FText> TagsAndValuesToShow;

	// Add asset registry tags to a text list; except skeleton as that is implied in Persona
	TSharedRef<SVerticalBox> DescriptionBox = SNew(SVerticalBox);

	static const FName AssetVariantPropertyName = GET_MEMBER_NAME_CHECKED(URigVMBlueprint, AssetVariant);
	const FProperty* AssetVariantProperty = CastField<FProperty>(URigVMBlueprint::StaticClass()->FindPropertyByName(AssetVariantPropertyName));
	const FString VariantStr = AssetData.GetTagValueRef<FString>(AssetVariantPropertyName);
	if(!VariantStr.IsEmpty())
	{
		FRigVMVariant AssetVariant;
		AssetVariantProperty->ImportText_Direct(*VariantStr, &AssetVariant, nullptr, EPropertyPortFlags::PPF_None);

		if(!AssetVariant.Tags.IsEmpty())
		{
			DescriptionBox->AddSlot()
			.AutoHeight()
			.Padding(0,0,5,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AssetBrowser_RigVMTagsLabel", "Tags :"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(SRigVMVariantTagWidget)
					.Visibility(EVisibility::Visible)
					.CanAddTags(false)
					.EnableContextMenu(false)
					.EnableTick(false)
					.Orientation(EOrientation::Orient_Horizontal)
					.OnGetTags_Lambda([AssetVariant]() { return AssetVariant.Tags; })
				]
			];
		}
	}
	
	for(TPair<FName, FAssetTagValueRef> TagPair : AssetData.TagsAndValues)
	{
		if(TagsToShow.Contains(TagPair.Key))
		{
			// Check for DisplayName metadata
			FName DisplayName;
			if (FProperty* Field = FindFProperty<FProperty>(AssetClass, TagPair.Key))
			{
				DisplayName = *Field->GetDisplayNameText().ToString();
			}
			else
			{
				DisplayName = TagPair.Key;
			}

			if (TagPair.Key == ModuleSettings)
			{
				FRigModuleSettings Settings;
				FRigVMPinDefaultValueImportErrorContext ErrorPipe;
				FRigModuleSettings::StaticStruct()->ImportText(*TagPair.Value.GetValue(), &Settings, nullptr, PPF_None, &ErrorPipe, FString());
				if (ErrorPipe.NumErrors == 0)
				{
					TagsAndValuesToShow.Add(TEXT("Default Name"), FText::FromString(Settings.Identifier.Name));
					TagsAndValuesToShow.Add(TEXT("Category"), FText::FromString(Settings.Category));
					TagsAndValuesToShow.Add(TEXT("Keywords"), FText::FromString(Settings.Keywords));
					TagsAndValuesToShow.Add(TEXT("Description"), FText::FromString(Settings.Description));
				}
			}
			else
			{
				TagsAndValuesToShow.Add(DisplayName, TagPair.Value.AsText());
			}
		}
	}

	for (const TPair<FName, FText>& TagPair : TagsAndValuesToShow)
	{
		DescriptionBox->AddSlot()
		.AutoHeight()
		.Padding(0,0,5,0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("AssetTagKey", "{0}: "), FText::FromName(TagPair.Key)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(TagPair.Value)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	DescriptionBox->AddSlot()
		.AutoHeight()
		.Padding(0,0,5,0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetBrowser_FolderPathLabel", "Folder :"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromName(AssetData.PackagePath))
				.ColorAndOpacity(FSlateColor::UseForeground())
				.WrapTextAt(300.f)
			]
		];

	TSharedPtr<SHorizontalBox> ContentBox = nullptr;
	TSharedRef<SToolTip> ToolTipWidget = SNew(SToolTip)
	.TextMargin(1.f)
	.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ToolTipBorder"))
	[
		SNew(SBorder)
		.Padding(6.f)
		.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,0,0,4)
			[
				SNew(SBorder)
				.Padding(6.f)
				.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
				[
					SNew(SBox)
					.HAlign(HAlign_Left)
					[
						SNew(STextBlock)
						.Text(FText::FromName(AssetData.AssetName))
						.Font(FAppStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
					]
				]
			]
		
			+ SVerticalBox::Slot()
			[
				SAssignNew(ContentBox, SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBorder)
					.Padding(6.f)
					.BorderImage(FAppStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
					[
						DescriptionBox
					]
				]
			]
		]
	];
	return ToolTipWidget;
}

TSharedRef<SToolTip> SRigModuleAssetBrowser::CreateCustomAssetToolTipNewStyle(FAssetData& AssetData)
{
	// Make a list of tags to show
	TArray<UObject::FAssetRegistryTag> Tags;
	UClass* AssetClass = FindObject<UClass>(AssetData.AssetClassPath);
	check(AssetClass);
	UObject* DefaultObject = AssetClass->GetDefaultObject();
	FAssetRegistryTagsContextData TagsContext(DefaultObject, EAssetRegistryTagsCaller::Uncategorized);
	DefaultObject->GetAssetRegistryTags(TagsContext);

	TArray<FName> TagsToShow;
	static const FName ModulePath(TEXT("Path"));
	static const FName ModuleSettings(TEXT("RigModuleSettings"));
	for (const TPair<FName, UObject::FAssetRegistryTag>& TagPair : TagsContext.Tags)
	{
		if(TagPair.Key == ModulePath ||
			TagPair.Key == ModuleSettings)
		{
			TagsToShow.Add(TagPair.Key);
		}
	}

	TSharedRef<SVerticalBox> OverallTooltipVBox = SNew(SVerticalBox);

	// Asset Name/Type Area
	{
		const FSlateBrush* ClassIcon = FAppStyle::GetDefaultBrush();
		TOptional<FLinearColor> Color;
		if (const UAssetDefinition* AssetDefinition = UAssetDefinitionRegistry::Get()->GetAssetDefinitionForClass(AssetData.GetClass()))
		{
			ClassIcon = AssetDefinition->GetIconBrush(AssetData, AssetData.AssetClassPath.GetAssetName());
			Color = AssetDefinition->GetAssetColor();
		}

		if (ClassIcon == nullptr || ClassIcon == FAppStyle::GetDefaultBrush())
		{
			ClassIcon = FSlateIconFinder::FindIconForClass(AssetData.GetClass()).GetIcon();
		}

		FText ClassNameText = LOCTEXT("ClassNameText", "Not Found");
		if (AssetClass != NULL)
		{
			ClassNameText = AssetClass->GetDisplayNameText();
		}
		else if (!AssetData.AssetClassPath.IsNull())
		{
			ClassNameText = FText::FromString(AssetData.AssetClassPath.ToString());
		}

		const FText NameText = FText::FromString(AssetData.AssetName.ToString());

		// Name/Type Slot
		OverallTooltipVBox->AddSlot()
		.AutoHeight()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.Padding(0.f, 0.f, 0.f, 6.f)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(NameText)
				.ColorAndOpacity(FStyleColors::White)
			]

			+ SVerticalBox::Slot()
			.Padding(0.f, 0.f, 0.f, 6.f)
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SBox)
					.WidthOverride(16.f)
					.HeightOverride(16.f)
					[
						SNew(SImage)
						.Image(ClassIcon)
						.ColorAndOpacity_Lambda([Color] () { return Color.IsSet() ? Color.GetValue() : FStyleColors::White;})
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(ClassNameText)
				]
			]
		];
	}

	// Separator
	OverallTooltipVBox->AddSlot()
	.Padding(0.f,0.f, 0.f, 6.f)
	.AutoHeight()
	[
		SNew(SSeparator)
		.Orientation(Orient_Horizontal)
		.Thickness(1.f)
		.ColorAndOpacity(COLOR("#484848FF"))
		.SeparatorImage(FAppStyle::Get().GetBrush("WhiteBrush"))
	];

	// Add asset registry tags to a text list; except skeleton as that is implied in Persona
	TMap<FName, FText> TagsAndValuesToShow;

	static const FName AssetVariantPropertyName = GET_MEMBER_NAME_CHECKED(URigVMBlueprint, AssetVariant);
	const FProperty* AssetVariantProperty = CastField<FProperty>(URigVMBlueprint::StaticClass()->FindPropertyByName(AssetVariantPropertyName));
	const FString VariantStr = AssetData.GetTagValueRef<FString>(AssetVariantPropertyName);
	if(!VariantStr.IsEmpty())
	{
		FRigVMVariant AssetVariant;
		AssetVariantProperty->ImportText_Direct(*VariantStr, &AssetVariant, nullptr, EPropertyPortFlags::PPF_None);

		if(!AssetVariant.Tags.IsEmpty())
		{
			OverallTooltipVBox->AddSlot()
			.AutoHeight()
			.Padding(0,0,5,0)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AssetBrowser_RigVMTagsLabel_NewStyle", "Tags :"))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(4, 0, 0, 0)
				[
					SNew(SRigVMVariantTagWidget)
					.CapsuleTagBorder(FRigVMEditorStyle::Get().GetBrush("RigVM.TagCapsuleDark"))
					.Visibility(EVisibility::Visible)
					.CanAddTags(false)
					.EnableContextMenu(false)
					.EnableTick(false)
					.Orientation(EOrientation::Orient_Horizontal)
					.OnGetTags_Lambda([AssetVariant]() { return AssetVariant.Tags; })
				]
			];
		}
	}
	
	for(TPair<FName, FAssetTagValueRef> TagPair : AssetData.TagsAndValues)
	{
		if(TagsToShow.Contains(TagPair.Key))
		{
			// Check for DisplayName metadata
			FName DisplayName;
			if (FProperty* Field = FindFProperty<FProperty>(AssetClass, TagPair.Key))
			{
				DisplayName = *Field->GetDisplayNameText().ToString();
			}
			else
			{
				DisplayName = TagPair.Key;
			}

			if (TagPair.Key == ModuleSettings)
			{
				FRigModuleSettings Settings;
				FRigVMPinDefaultValueImportErrorContext ErrorPipe;
				FRigModuleSettings::StaticStruct()->ImportText(*TagPair.Value.GetValue(), &Settings, nullptr, PPF_None, &ErrorPipe, FString());
				if (ErrorPipe.NumErrors == 0)
				{
					TagsAndValuesToShow.Add(TEXT("Default Name"), FText::FromString(Settings.Identifier.Name));
					TagsAndValuesToShow.Add(TEXT("Category"), FText::FromString(Settings.Category));
					TagsAndValuesToShow.Add(TEXT("Keywords"), FText::FromString(Settings.Keywords));
					TagsAndValuesToShow.Add(TEXT("Description"), FText::FromString(Settings.Description));
				}
			}
			else
			{
				TagsAndValuesToShow.Add(DisplayName, TagPair.Value.AsText());
			}
		}
	}

	for (const TPair<FName, FText>& TagPair : TagsAndValuesToShow)
	{
		OverallTooltipVBox->AddSlot()
		.AutoHeight()
		.Padding(0,0,5,0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::Format(LOCTEXT("AssetTagKey_NewStyle", "{0}: "), FText::FromName(TagPair.Key)))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(TagPair.Value)
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
	}

	OverallTooltipVBox->AddSlot()
		.AutoHeight()
		.Padding(0,0,5,0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssetBrowser_FolderPathLabel_NewStyle", "Folder :"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromName(AssetData.PackagePath))
				.ColorAndOpacity(FSlateColor::UseForeground())
				.WrapTextAt(300.f)
			]
		];

	TSharedRef<SToolTip> ToolTipWidget = SNew(SToolTip)
	.TextMargin(FMargin(12.f, 8.f, 12.f, 8.f))
	.BorderImage(FAppStyle::GetBrush("AssetThumbnail.Tooltip.Border"))
	[
		OverallTooltipVBox
	];

	return ToolTipWidget;
}

#undef LOCTEXT_NAMESPACE
