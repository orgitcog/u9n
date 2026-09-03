// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanCharacterEditorFaceToolView.h"

#include "Algo/Find.h"
#include "IDetailsView.h"
#include "MetaHumanCharacterEditorStyle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tools/MetaHumanCharacterEditorFaceEditingTools.h"
#include "UI/Widgets/SMetaHumanCharacterEditorToolPanel.h"
#include "InteractiveToolManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "SWarningOrErrorBox.h"

#define LOCTEXT_NAMESPACE "SMetaHumanCharacterEditorFaceSculptingToolView"


UInteractiveToolPropertySet* SMetaHumanCharacterEditorFaceToolView::GetToolProperties() const
{
	const UMetaHumanCharacterEditorFaceTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceTool>(Tool);
	return IsValid(FaceTool) ? FaceTool->GetFaceToolHeadParameterProperties() : nullptr;
}

void SMetaHumanCharacterEditorFaceToolView::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	OnPreEditChangeProperty(PropertyAboutToChange, PropertyAboutToChange->GetName());
}

void SMetaHumanCharacterEditorFaceToolView::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	const bool bIsInteractive = PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive;
	OnPostEditChangeProperty(PropertyThatChanged, bIsInteractive);
}

FReply SMetaHumanCharacterEditorFaceToolView::OnResetButtonClicked() const
{
	Cast<UMetaHumanCharacterEditorFaceTool>(Tool)->ResetFace();
	return FReply::Handled();
}

FReply SMetaHumanCharacterEditorFaceToolView::OnResetNeckButtonClicked() const
{
	Cast<UMetaHumanCharacterEditorFaceTool>(Tool)->ResetFaceNeck();
	return FReply::Handled();
}

TSharedRef<SWidget> SMetaHumanCharacterEditorFaceToolView::CreateManipulatorsViewSection()
{
	const UMetaHumanCharacterEditorFaceTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceTool>(Tool);
	if (!IsValid(FaceTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorMeshEditingToolProperties* ManipulatorProperties = Cast<UMetaHumanCharacterEditorMeshEditingToolProperties>(FaceTool->GetMeshEditingToolProperties());
	if (!ManipulatorProperties)
	{
		return SNullWidget::NullWidget;
	}


	FProperty* SizeProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, Size));
	FProperty* SpeedProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, Speed));
	FProperty* HideProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, bHideWhileDragging));
	FProperty* SymmetricProperty = UMetaHumanCharacterEditorMeshEditingToolProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorMeshEditingToolProperties, bSymmetricModeling));

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(LOCTEXT("Size", "Size").ToString(), SizeProperty, ManipulatorProperties)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(LOCTEXT("InteractionSpeed", "Speed").ToString(), SpeedProperty, ManipulatorProperties)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertyCheckBoxWidget(LOCTEXT("SymmetricManipulation", "Symmetric Manipulation").ToString(), SymmetricProperty, ManipulatorProperties)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertyCheckBoxWidget(LOCTEXT("HideWhileDragging", "Hide While Dragging").ToString(), HideProperty, ManipulatorProperties)
		];
}

/** Creates the section widget for showing the head parameter properties. */
TSharedRef<SWidget> SMetaHumanCharacterEditorFaceToolView::CreateHeadParametersViewSection()
{
	const UMetaHumanCharacterEditorFaceTool* FaceTool = Cast<UMetaHumanCharacterEditorFaceTool>(Tool);
	if (!IsValid(FaceTool))
	{
		return SNullWidget::NullWidget;
	}

	UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties* HeadParameterProperties = Cast<UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties>(FaceTool->GetFaceToolHeadParameterProperties());
	if (!HeadParameterProperties)
	{
		return SNullWidget::NullWidget;
	}

	FProperty* GlobalDeltaProperty = UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties, GlobalDelta));
	FProperty* HeadScaleProperty = UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorFaceEditingToolHeadParameterProperties, HeadScale));

	return SNew(SVerticalBox)
		// Global delta
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(TEXT("Global Delta"), GlobalDeltaProperty, HeadParameterProperties)
		]

		// Head size
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreatePropertySpinBoxWidget(TEXT("Head Scale"), HeadScaleProperty, HeadParameterProperties)
		]
		+ SVerticalBox::Slot()
		.Padding(4.f)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
				.ForegroundColor(FLinearColor::White)
				.OnClicked(this, &SMetaHumanCharacterEditorFaceSculptToolView::OnResetButtonClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("ResetFaceToolTip", "Reverts the face back to default."))
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ResetFace", "Reset Head Parameters"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		]
		+ SVerticalBox::Slot()
		.Padding(4.f)
		[
			SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
				.ForegroundColor(FLinearColor::White)
				.OnClicked(this, &SMetaHumanCharacterEditorFaceSculptToolView::OnResetNeckButtonClicked)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT("ResetFaceNeckToolTip", "Reverts the neck region and aligns it to the body."))
				[
					SNew(STextBlock)
						.Text(LOCTEXT("ResetFaceNeck", "Align Neck to Body"))
						.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
		];
}

void SMetaHumanCharacterEditorFaceSculptToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorFaceSculptTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

void SMetaHumanCharacterEditorFaceSculptToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						SNew(SMetaHumanCharacterEditorToolPanel)
							.Label(LOCTEXT("FaceSculptToolManipulator", "Manipulator"))
							.Content()
							[
								CreateManipulatorsViewSection()
							]
					]
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						SNew(SMetaHumanCharacterEditorToolPanel)
							.Label(LOCTEXT("FaceSculptToolOptions", "Head Parameters"))
							.Content()
							[
								CreateHeadParametersViewSection()
							]
					]
			];
	}
}

void SMetaHumanCharacterEditorFaceMoveToolView::Construct(const FArguments& InArgs, UMetaHumanCharacterEditorFaceMoveTool* InTool)
{
	SMetaHumanCharacterEditorToolView::Construct(SMetaHumanCharacterEditorToolView::FArguments(), InTool);
}

void SMetaHumanCharacterEditorFaceMoveToolView::MakeToolView()
{
	if (ToolViewScrollBox.IsValid())
	{
		ToolViewScrollBox->AddSlot()
			.VAlign(VAlign_Top)
			[
				SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Padding(4.f)
					.AutoHeight()
					[
						CreateGizmoSelectionSection()
					]

					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						SNew(SMetaHumanCharacterEditorToolPanel)
							.Label(LOCTEXT("FaceSculptToolManipulator", "Manipulator"))
							.Content()
							[
								CreateManipulatorsViewSection()
							]
					]
					+ SVerticalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Bottom)
					.Padding(4.f)
					.AutoHeight()
					[
						SNew(SMetaHumanCharacterEditorToolPanel)
							.Label(LOCTEXT("FaceSculptToolOptions", "Head Parameters"))
							.Content()
							[
								CreateHeadParametersViewSection()
							]
					]
			];
	}
}

TSharedRef<SWidget> SMetaHumanCharacterEditorFaceMoveToolView::CreateGizmoSelectionSection()
{
	const UMetaHumanCharacterEditorFaceMoveTool* FaceMoveTool = Cast<UMetaHumanCharacterEditorFaceMoveTool>(Tool);
	if (!IsValid(FaceMoveTool))
	{
		return SNullWidget::NullWidget;;
	}
	UMetaHumanCharacterEditorFaceMoveToolProperties* MoveToolProperties = FaceMoveTool->GetFaceMoveToolProperties();
	if (!IsValid(MoveToolProperties))
	{
		return SNullWidget::NullWidget;;
	}

	TSharedRef<SSegmentedControl<EMetaHumanCharacterMoveToolManipulationGizmos>> GizmoSelectionWidget =
		SNew(SSegmentedControl<EMetaHumanCharacterMoveToolManipulationGizmos>)
		.Value_Lambda([MoveToolProperties]
			{
				return MoveToolProperties->GizmoType;
			})
		.OnValueChanged_Lambda([this, MoveToolProperties](EMetaHumanCharacterMoveToolManipulationGizmos Selection)
			{
				CastChecked<UMetaHumanCharacterEditorFaceMoveTool>(GetTool())->SetGizmoType(Selection);
			});

	for (EMetaHumanCharacterMoveToolManipulationGizmos GizmoSelection : TEnumRange<EMetaHumanCharacterMoveToolManipulationGizmos>())
	{
		const FSlateBrush* Brush = nullptr;
		switch (GizmoSelection)
		{
		default:
		case EMetaHumanCharacterMoveToolManipulationGizmos::ScreenSpace:
			Brush = FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Face.ScreenSpaceMoveTool"));
			break;
		case EMetaHumanCharacterMoveToolManipulationGizmos::Translate:
			Brush = FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Face.TranslateMoveTool"));
				break;
		case EMetaHumanCharacterMoveToolManipulationGizmos::Rotate:
			Brush = FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Face.RotateMoveTool"));
				break;
		case EMetaHumanCharacterMoveToolManipulationGizmos::UniformScale:
			Brush = FMetaHumanCharacterEditorStyle::Get().GetBrush(TEXT("MetaHumanCharacterEditorTools.Face.ScaleMoveTool"));
				break;
		}
		GizmoSelectionWidget->AddSlot(GizmoSelection)
			.Icon(Brush)
			.ToolTip(UEnum::GetDisplayValueAsText(GizmoSelection));
	}

	return GizmoSelectionWidget;
}

#undef LOCTEXT_NAMESPACE
