// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorPipelineToolView.h"

#include "IDetailsView.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tools/MetaHumanCharacterEditorPipelineTools.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorPipelineToolView"

void SMetaHumanCharacterEditorPipelineToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorPipelineTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

UInteractiveToolPropertySet* SMetaHumanCharacterEditorPipelineToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorPipelineTool* PipelineTool = Cast<UMetaHumanCharacterEditorPipelineTool>(Tool);
	return IsValid(PipelineTool) ? PipelineTool->GetPipelineProperty() : nullptr;
}

void SMetaHumanCharacterEditorPipelineToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.Padding(4.f, 10.f)
				.AutoHeight()
				[
					CreatePipelineToolViewDetailsViewSection()
				]
			];
	}

	if (ToolViewMainBox.IsValid())
	{
		ToolViewMainBox->AddSlot()
			.Padding(0.f, 4.f, 0.f, 0.f)
			.AutoHeight()
			[
				CreatePipelineToolViewAssembleSection()
			];
	}
}

void SMetaHumanCharacterEditorPipelineToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorPipelineToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPipelineToolView::CreatePipelineToolViewDetailsViewSection()
{
	UMetaHumanCharacterEditorPipelineToolProperties* Properties = Cast<UMetaHumanCharacterEditorPipelineToolProperties>(GetToolProperties());
	if (!Properties)
	{
		return SNullWidget::NullWidget;
	}

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(FName("PropertyEditor"));
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = this;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(Properties);
	return DetailsView.ToSharedRef();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorPipelineToolView::CreatePipelineToolViewAssembleSection()
{
	return 
		SNew(SBorder)
		.Padding(-4.f)
		.BorderImage(FMetaHumanCharacterEditorStyle::Get().GetBrush("MetaHumanCharacterEditorTools.ActiveToolLabel"))
		[
			SNew(SVerticalBox)

			// Build warning label
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.Padding(4.f)
				[
					SNew(SWarningOrErrorBox)
					.AutoWrapText(true)
					.MessageStyle(EMessageStyle::Warning)
					.Visibility(this, &SMetaHumanCharacterEditorPipelineToolView::GetWarningVisibility)
					.Message_Lambda([this](){ return BuildErrorMsg; })
				]
			]

			// Build button
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(50.f)
				.HAlign(HAlign_Fill)
				.Padding(10.f)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), FName("FlatButton.Success"))
					.ForegroundColor(FLinearColor::White)
					.IsEnabled(this, &SMetaHumanCharacterEditorPipelineToolView::IsAssembleButtonEnabled)
					.OnClicked(this, &SMetaHumanCharacterEditorPipelineToolView::OnAssembleButtonClicked)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SMetaHumanCharacterEditorPipelineToolView::GetAssembleButtonText)
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
					]
				]
			]
		];
}

bool SMetaHumanCharacterEditorPipelineToolView::IsAssembleButtonEnabled() const
{
	if (const UMetaHumanCharacterEditorPipelineTool* PipelineTool = Cast<UMetaHumanCharacterEditorPipelineTool>(Tool))
	{
		FText ErrorMsg;
		return PipelineTool->CanBuild(ErrorMsg);
	}

	return false;
}

FReply SMetaHumanCharacterEditorPipelineToolView::OnAssembleButtonClicked()
{
	if (const UMetaHumanCharacterEditorPipelineTool* PipelineTool = Cast<UMetaHumanCharacterEditorPipelineTool>(Tool))
	{
		PipelineTool->Build();
	}

	return FReply::Handled();
}

FText SMetaHumanCharacterEditorPipelineToolView::GetAssembleButtonText() const
{
	// Different pipelines might want to use different texts
	return LOCTEXT("AssembleButtonText", "Assemble");
}

EVisibility SMetaHumanCharacterEditorPipelineToolView::GetWarningVisibility() const
{
	if (const UMetaHumanCharacterEditorPipelineTool* PipelineTool = Cast<UMetaHumanCharacterEditorPipelineTool>(Tool))
	{
		SMetaHumanCharacterEditorPipelineToolView* MutableThis = const_cast<SMetaHumanCharacterEditorPipelineToolView*>(this);

		if (PipelineTool->CanBuild(MutableThis->BuildErrorMsg))
		{
			return EVisibility::Collapsed;
		}
	}

	return EVisibility::HitTestInvisible;
}

#undef LOCTEXT_NAMESPACE
