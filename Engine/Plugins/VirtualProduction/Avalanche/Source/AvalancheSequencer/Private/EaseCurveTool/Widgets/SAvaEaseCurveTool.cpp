// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaEaseCurveTool.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "EaseCurveTool/AvaEaseCurvePreset.h"
#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EaseCurveTool/AvaEaseCurveToolSettings.h"
#include "EaseCurveTool/AvaEaseCurveSubsystem.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurveEditor.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePreset.h"
#include "EaseCurveTool/Widgets/AvaEaseCurveToolContextMenu.h"
#include "Editor.h"
#include "EngineAnalytics.h"
#include "SAvaEaseCurveTangents.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Math/UnrealMathUtility.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "SAvaEaseCurveTool"

void SAvaEaseCurveTool::Construct(const FArguments& InArgs, const TSharedRef<FAvaEaseCurveTool>& InEaseCurveTool)
{
	ToolMode = InArgs._ToolMode;
	ToolOperation = InArgs._ToolOperation;

	EaseCurveToolWeak = InEaseCurveTool;

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 1.f, 0.f, 0.f)
			[
				SAssignNew(CurvePresetWidget, SAvaEaseCurvePreset)
				.OnPresetChanged(this, &SAvaEaseCurveTool::OnPresetChanged)
				.OnQuickPresetChanged(this, &SAvaEaseCurveTool::OnQuickPresetChanged)
				.OnGetNewPresetTangents(this, &SAvaEaseCurveTool::OnGetNewPresetTangents)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				ConstructCurveEditorPanel()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Fill)
			.Padding(0.f, 3.f, 0.f, 0.f)
			[
				SNew(SAvaEaseCurveTangents)
				.InitialTangents(GetTangents())
				.OnStartTangentChanged(this, &SAvaEaseCurveTool::OnStartTangentSpinBoxChanged)
				.OnStartWeightChanged(this, &SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged)
				.OnEndTangentChanged(this, &SAvaEaseCurveTool::OnEndTangentSpinBoxChanged)
				.OnEndWeightChanged(this, &SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged)
				.OnBeginSliderMovement(this, &SAvaEaseCurveTool::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &SAvaEaseCurveTool::OnEndSliderMovement)
			]
		];

	if (GEditor)
	{
		GEditor->RegisterForUndo(this);
	}

	CurvePresetWidget->SetSelectedItem(InArgs._InitialTangents);
}

TSharedRef<SWidget> SAvaEaseCurveTool::ConstructCurveEditorPanel()
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	check(EaseCurveTool.IsValid());

	CurrentGraphSize = GetDefault<UAvaEaseCurveToolSettings>()->GetGraphSize();
	
	ContextMenu = MakeShared<FAvaEaseCurveToolContextMenu>(EaseCurveTool->GetCommandList()
		, FAvaEaseCurveToolOnGraphSizeChanged::CreateLambda([this](const int32 InNewSize)
			{
				CurrentGraphSize = InNewSize;
			}));

	return SNew(SBorder)
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(CurveEaseEditorWidget, SAvaEaseCurveEditor, EaseCurveTool->GetToolCurve())
				.DisplayRate(this, &SAvaEaseCurveTool::GetDisplayRate)
				.Operation(this, &SAvaEaseCurveTool::GetToolOperation)
				.DesiredSize(this, &SAvaEaseCurveTool::GetEditorSize)
				.ShowEqualValueKeyError(this, &SAvaEaseCurveTool::ShowEqualValueKeyError)
				.IsEaseCurveSelection(this, &SAvaEaseCurveTool::IsEaseCurveSelection)
				.OnTangentsChanged(this, &SAvaEaseCurveTool::HandleEditorTangentsChanged)
				.GridSnap_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSnap)
				.GridSize_UObject(GetDefault<UAvaEaseCurveToolSettings>(), &UAvaEaseCurveToolSettings::GetGridSize)
				.GetContextMenuContent(ContextMenu.ToSharedRef(), &FAvaEaseCurveToolContextMenu::GenerateWidget)
				.StartText(this, &SAvaEaseCurveTool::GetStartText)
				.StartTooltipText(this, &SAvaEaseCurveTool::GetStartTooltipText)
				.EndText(this, &SAvaEaseCurveTool::GetEndText)
				.EndTooltipText(this, &SAvaEaseCurveTool::GetEndTooltipText)
				.OnKeyDown(this, &SAvaEaseCurveTool::OnKeyDown)
				.OnDragStart(this, &SAvaEaseCurveTool::OnEditorDragStart)
				.OnDragEnd(this, &SAvaEaseCurveTool::OnEditorDragEnd)
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>(TEXT("ScrollBox")).TopShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Bottom)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>(TEXT("ScrollBox")).BottomShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>(TEXT("ScrollBox")).LeftShadowBrush))
			]
			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Fill)
			[
				SNew(SImage)
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.5f))
				.Image(&(FCoreStyle::Get().GetWidgetStyle<FScrollBoxStyle>(TEXT("ScrollBox")).RightShadowBrush))
			]
		];
}

FFrameRate SAvaEaseCurveTool::GetDisplayRate() const
{
	return EaseCurveToolWeak.IsValid() ? EaseCurveToolWeak.Pin()->GetDisplayRate() : FFrameRate();
}

EAvaEaseCurveToolOperation SAvaEaseCurveTool::GetToolOperation() const
{
	return EaseCurveToolWeak.IsValid() ? EaseCurveToolWeak.Pin()->GetToolOperation() : EAvaEaseCurveToolOperation::InOut;
}

FVector2D SAvaEaseCurveTool::GetEditorSize() const
{
	return FVector2D(CurrentGraphSize);
}

bool SAvaEaseCurveTool::ShowEqualValueKeyError() const
{
	return !EaseCurveToolWeak.Pin()->HasCachedKeysToEase();
}

bool SAvaEaseCurveTool::IsEaseCurveSelection() const
{
	return EaseCurveToolWeak.Pin()->AreAllEaseCurves();
}

void SAvaEaseCurveTool::HandleEditorTangentsChanged(const FAvaEaseCurveTangents& InTangents) const
{
	SetTangents(InTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEditorDragStart() const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	EaseCurveTool->BeginTransaction(LOCTEXT("EditorDragStartLabel", "Ease Curve Graph Drag"));
}

void SAvaEaseCurveTool::OnEditorDragEnd() const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	EaseCurveTool->EndTransaction();

	if (!EaseCurveTool->HasCachedKeysToEase())
	{
		ResetTangentsAndNotify();
	}
}

void SAvaEaseCurveTool::SetTangents(const FAvaEaseCurveTangents& InTangents, EAvaEaseCurveToolOperation InOperation
	, const bool bInSetEaseCurve, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents) const
{
	if (CurvePresetWidget.IsValid() && !CurvePresetWidget->SetSelectedItem(InTangents))
	{
		CurvePresetWidget->ClearSelection();
	}

	// To change the graph UI tangents, we need to change the ease curve object tangents and the graph will reflect.
	if (bInSetEaseCurve)
	{
        if (const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin())
        {
        	EaseCurveTool->SetEaseCurveTangents(InTangents, InOperation, bInBroadcastUpdate, bInSetSequencerTangents);
        }
	}

	if (GetDefault<UAvaEaseCurveToolSettings>()->GetAutoZoomToFit())
	{
		ZoomToFit();
	}
}

FAvaEaseCurveTangents SAvaEaseCurveTool::GetTangents() const
{
	if (const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin())
	{
		return EaseCurveTool->GetEaseCurveTangents();
	}
	return FAvaEaseCurveTangents();
}

void SAvaEaseCurveTool::OnStartTangentSpinBoxChanged(const double InNewValue) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.Start = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnStartTangentWeightSpinBoxChanged(const double InNewValue) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.StartWeight = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentSpinBoxChanged(const double InNewValue) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.End = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnEndTangentWeightSpinBoxChanged(const double InNewValue) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	FAvaEaseCurveTangents NewTangents = EaseCurveTool->GetEaseCurveTangents();
	NewTangents.EndWeight = InNewValue;
	SetTangents(NewTangents, ToolOperation.Get(), true, true, true);
}

void SAvaEaseCurveTool::OnBeginSliderMovement()
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
    if (!EaseCurveTool.IsValid())
    {
    	return;
    }

	EaseCurveTool->BeginTransaction(LOCTEXT("SliderDragStartLabel", "Ease Curve Slider Drag"));
}

void SAvaEaseCurveTool::OnEndSliderMovement(const double InNewValue)
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	EaseCurveTool->EndTransaction();
}

void SAvaEaseCurveTool::OnPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return;
	}

	if (!EaseCurveTool->HasCachedKeysToEase())
	{
		ResetTangentsAndNotify();
		return;
	}

	if (InPreset.IsValid())
	{
		SetTangents(InPreset->Tangents, ToolOperation.Get(), true, true, true);
	}

	FSlateApplication::Get().SetAllUserFocus(CurveEaseEditorWidget);

	if (FEngineAnalytics::IsAvailable())
	{
		// Only send analytics for default presets
		const TMap<FString, TArray<FString>>& DefaultPresetNames = UAvaEaseCurveSubsystem::GetDefaultCategoryPresetNames();
		if (DefaultPresetNames.Contains(InPreset->Category)
			&& DefaultPresetNames[InPreset->Category].Contains(InPreset->Name))
		{
			TArray<FAnalyticsEventAttribute> Attributes;
			Attributes.Emplace(TEXT("Category"), InPreset->Category);
			Attributes.Emplace(TEXT("Name"), InPreset->Name);

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.MotionDesign.EaseCurveTool.SetTangentsPreset"), Attributes);
		}
	}
}

void SAvaEaseCurveTool::OnQuickPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const
{
	FSlateApplication::Get().SetAllUserFocus(CurveEaseEditorWidget);
}

bool SAvaEaseCurveTool::OnGetNewPresetTangents(FAvaEaseCurveTangents& OutTangents) const
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return false;
	}

	OutTangents = EaseCurveTool->GetEaseCurveTangents();
	return true;
}

void SAvaEaseCurveTool::UndoAction()
{
	if (GEditor)
	{
		GEditor->UndoTransaction();
	}
}

void SAvaEaseCurveTool::RedoAction()
{
	if (GEditor)
	{
		GEditor->RedoTransaction();
	}
}

void SAvaEaseCurveTool::ZoomToFit() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		CurveEaseEditorWidget->ZoomToFit();
	}
}

FKeyHandle SAvaEaseCurveTool::GetSelectedKeyHandle() const
{
	if (CurveEaseEditorWidget.IsValid())
	{
		return CurveEaseEditorWidget->GetSelectedKeyHandle();
	}
	return FKeyHandle::Invalid();
}

FReply SAvaEaseCurveTool::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	const TSharedPtr<FAvaEaseCurveTool> EaseCurveTool = EaseCurveToolWeak.Pin();
	if (!EaseCurveTool.IsValid())
	{
		return FReply::Unhandled();
	}

	const TSharedPtr<FUICommandList> CommandList = EaseCurveTool->GetCommandList();
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FText SAvaEaseCurveTool::GetStartText() const
{
	return (ToolMode.Get(EAvaEaseCurveToolMode::DualKeyEdit) == EAvaEaseCurveToolMode::DualKeyEdit)
		? LOCTEXT("StartText", "Leave")
		: LOCTEXT("ArriveText", "Arrive");
}

FText SAvaEaseCurveTool::GetStartTooltipText() const
{
	return (ToolMode.Get(EAvaEaseCurveToolMode::DualKeyEdit) == EAvaEaseCurveToolMode::DualKeyEdit)
		? LOCTEXT("StartTooltipText", "Start: The selected key's leave tangent")
		: LOCTEXT("ArriveTooltipText", "Arrive");
}

FText SAvaEaseCurveTool::GetEndText() const
{
	return (ToolMode.Get(EAvaEaseCurveToolMode::DualKeyEdit) == EAvaEaseCurveToolMode::DualKeyEdit)
		? LOCTEXT("EndText", "Arrive")
		: LOCTEXT("LeaveText", "Leave");
}

FText SAvaEaseCurveTool::GetEndTooltipText() const
{
	return (ToolMode.Get(EAvaEaseCurveToolMode::DualKeyEdit) == EAvaEaseCurveToolMode::DualKeyEdit)
		? LOCTEXT("EndTooltipText", "End: The next key's arrive tangent")
		: LOCTEXT("LeaveTooltipText", "Leave");
}

void SAvaEaseCurveTool::ResetTangentsAndNotify() const
{
	CurvePresetWidget->ClearSelection();

	SetTangents(FAvaEaseCurveTangents(), EAvaEaseCurveToolOperation::InOut, true, true, false);

	FAvaEaseCurveTool::ShowNotificationMessage(LOCTEXT("EqualValueKeys", "No different key values to create ease curve!"));
}

#undef LOCTEXT_NAMESPACE
