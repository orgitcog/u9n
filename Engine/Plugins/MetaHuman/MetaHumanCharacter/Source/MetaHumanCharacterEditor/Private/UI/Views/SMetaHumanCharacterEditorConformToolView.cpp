// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorConformToolView.h"

#include "Algo/Find.h"
#include "IDetailsView.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tools/MetaHumanCharacterEditorConformTool.h"
#include "Tools/MetaHumanCharacterEditorBodyConformTool.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "InteractiveToolManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorConformToolView"

void SMetaHumanCharacterEditorConformToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorToolWithSubTools* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorConformToolView::GetToolProperties() const
{
	TArray<UObject*> ToolProperties;
	const UMetaHumanCharacterEditorConformTool* ConformTool = Cast<UMetaHumanCharacterEditorConformTool>(Tool);
	const UMetaHumanCharacterEditorBodyConformTool* BodyConformTool = Cast<UMetaHumanCharacterEditorBodyConformTool>(Tool);

	constexpr bool bOnlyEnabled = true;
	if (IsValid(ConformTool))
	{
		ToolProperties = ConformTool->GetToolProperties(bOnlyEnabled);
	}
	else if (IsValid(BodyConformTool))
	{
		ToolProperties = BodyConformTool->GetToolProperties(bOnlyEnabled);
	}

	UObject* const* SubToolProperties = Algo::FindByPredicate(ToolProperties,
		[](UObject* ToolProperty)
		{
			return IsValid(Cast<UMetaHumanCharacterImportSubToolBase>(ToolProperty));
		});

	return SubToolProperties ? Cast<UInteractiveToolPropertySet>(*SubToolProperties) : nullptr;
}

void SMetaHumanCharacterEditorConformToolView::MakeToolView()
{
	if(ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f)
				.AutoHeight()
				[
					CreateConformToolViewWarningSection()
				]
			];
	}

	if (ToolViewMainBox.IsValid())
	{
		ToolViewMainBox->AddSlot()
			.Padding(0.f, 4.f, 0.f, 0.f)
			.AutoHeight()
			[
				CreateConformToolViewImportSection()
			];
	}
}

void SMetaHumanCharacterEditorConformToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorConformToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

TSharedRef<SWidget> SMetaHumanCharacterEditorConformToolView::CreateConformToolViewWarningSection()
{
	const TSharedRef<SWidget> WarningSectionWidget =
		SNew(SBox)
		.Padding(4.f)
		[
			SNew(SWarningOrErrorBox)
			.AutoWrapText(false)
			.MessageStyle(EMessageStyle::Warning)
			.Visibility(this, &SMetaHumanCharacterEditorConformToolView::GetWarningVisibility)
			.Message_Lambda([this] { return GetWarning(); })		
		];

	return WarningSectionWidget;
}

EVisibility SMetaHumanCharacterEditorConformToolView::GetWarningVisibility() const
{
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	EVisibility Visibility = EVisibility::Collapsed;
	if (IsValid(Cast<UMetaHumanCharacterImportIdentityProperties>(SubToolProperties)))
	{
		Visibility = EVisibility::Visible; // always visible for identity
	}
	else if (IsValid(Cast<UMetaHumanCharacterImportTemplateProperties>(SubToolProperties)))
	{
		UMetaHumanCharacterImportTemplateProperties* Props = Cast<UMetaHumanCharacterImportTemplateProperties>(SubToolProperties);
		if (Props->ImportOptions.bAdaptNeck)
		{
			Visibility = EVisibility::Visible;
		}
	}
	else if (IsValid(Cast<UMetaHumanCharacterImportDNAProperties>(SubToolProperties)))
	{
		UMetaHumanCharacterImportDNAProperties* Props = Cast<UMetaHumanCharacterImportDNAProperties>(SubToolProperties);
		if (Props->ImportOptions.bAdaptNeck || Props->ImportOptions.bImportWholeRig)
		{
			Visibility = EVisibility::Visible;
		}
	}

	return Visibility;
}

FText SMetaHumanCharacterEditorConformToolView::GetWarning() const
{
	FText Warning;
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	if (IsValid(Cast<UMetaHumanCharacterImportIdentityProperties>(SubToolProperties)))
	{
		Warning = LOCTEXT("IdentityConformWarning", "Conforming to a MetaHuman Identity Asset will reposition joints and\nvertices to best align Head and Body.\n\nThe originating File or Asset won’t be modified.");
	}
	else if (IsValid(Cast<UMetaHumanCharacterImportTemplateProperties>(SubToolProperties)))
	{
		Warning = LOCTEXT("TemplateConformWarning", "The Adapt Neck option will reposition joints and vertices to best align \nHead and Body.\n\nThe originating File or Asset won’t be modified.");
	}
	else if (IsValid(Cast<UMetaHumanCharacterImportDNAProperties>(SubToolProperties)))
	{
		UMetaHumanCharacterImportDNAProperties* Props = Cast<UMetaHumanCharacterImportDNAProperties>(SubToolProperties);
		if (Props->ImportOptions.bImportWholeRig)
		{
			Warning = LOCTEXT("DNAImportRigWarning", "The Import Whole Rig option imports Neutral Pose and all Expressions\nfrom the MetaHuman DNA file and sets the Asset to a Rigged State.\n\nBody and Head alignment will depend entirely on the data, and the\nBody needs to have been set correctly prior to Conforming the Head.");
		}
		else if (Props->ImportOptions.bAdaptNeck)
		{
			Warning = LOCTEXT("DNAConformWarning", "The Adapt Neck option will reposition joints and vertices to best align \nHead and Body.\n\nThe originating File or Asset won’t be modified.");
		}
	}

	return Warning;
}

TSharedRef<SWidget> SMetaHumanCharacterEditorConformToolView::CreateConformToolViewImportSection()
{
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	if (!Tool.IsValid() || !SubToolProperties)
	{
		return SNullWidget::NullWidget;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(SubToolProperties);

	Tool.Pin()->OnPropertySetsModified.AddSP(this, &SMetaHumanCharacterEditorConformToolView::OnPropertySetsModified);

	const TSharedRef<SWidget> ImportSectionWidget =
		SNew(SBorder)
		.Padding(-4.f)
		.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ActiveToolLabel"))
		[
			SNew(SVerticalBox)

			// SubTool properties details view section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				DetailsView.ToSharedRef()
			]

			// Import button section
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(50.f)
				.HAlign(HAlign_Fill)
				.Padding(10.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
					.ForegroundColor(FLinearColor::White)
					.IsEnabled(this, &SMetaHumanCharacterEditorConformToolView::IsImportButtonEnabled)
					.OnClicked(this, &SMetaHumanCharacterEditorConformToolView::OnImportButtonClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SMetaHumanCharacterEditorConformToolView::GetImportButtonText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
			]
		];

	return ImportSectionWidget;
}

void SMetaHumanCharacterEditorConformToolView::OnPropertySetsModified()
{
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	if (DetailsView.IsValid() && IsValid(SubToolProperties))
	{
		constexpr bool bForceRefresh = true;
		DetailsView->SetObject(SubToolProperties, bForceRefresh);
		// clear any warning
		UMetaHumanCharacterEditorToolWithSubTools* OwnerTool = SubToolProperties->GetTypedOuter<UMetaHumanCharacterEditorToolWithSubTools>();
		if (OwnerTool)
		{
			OwnerTool->GetToolManager()->DisplayMessage(FText(), EToolMessageLevel::UserError);
		}
	}
}

bool SMetaHumanCharacterEditorConformToolView::IsImportButtonEnabled() const
{
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	return IsValid(SubToolProperties) && SubToolProperties->CanImport();
}

FReply SMetaHumanCharacterEditorConformToolView::OnImportButtonClicked()
{
	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	if (IsValid(SubToolProperties))
	{
		SubToolProperties->Import();
	}

	return FReply::Handled();
}

FText SMetaHumanCharacterEditorConformToolView::GetImportButtonText() const
{
	FText ImportButtonText = LOCTEXT("ImportButtonText", "Import");

	UMetaHumanCharacterImportSubToolBase* SubToolProperties = Cast<UMetaHumanCharacterImportSubToolBase>(GetToolProperties());
	const bool bIsIdentityOrTemplate =
		IsValid(Cast<UMetaHumanCharacterImportIdentityProperties>(SubToolProperties)) ||
		IsValid(Cast<UMetaHumanCharacterImportTemplateProperties>(SubToolProperties));
	if (bIsIdentityOrTemplate)
	{
		ImportButtonText = LOCTEXT("ConformButtonText", "Conform");
	}

	return ImportButtonText;
}

#undef LOCTEXT_NAMESPACE
