// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/AvaEaseCurveTool.h"
#include "EditorUndoClient.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FAvaEaseCurveToolContextMenu;
class FText;
class SAvaEaseCurveEditor;
class SAvaEaseCurvePreset;
struct FKeyHandle;

class SAvaEaseCurveTool
	: public SCompoundWidget
	, public FEditorUndoClient
{
public:
	static constexpr int32 DefaultGraphSize = 200;
	
	SLATE_BEGIN_ARGS(SAvaEaseCurveTool)
		: _ToolMode(EAvaEaseCurveToolMode::DualKeyEdit)
		, _ToolOperation(EAvaEaseCurveToolOperation::InOut)
	{}
		SLATE_ATTRIBUTE(EAvaEaseCurveToolMode, ToolMode)
		SLATE_ATTRIBUTE(EAvaEaseCurveToolOperation, ToolOperation)
		SLATE_ARGUMENT(FAvaEaseCurveTangents, InitialTangents)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<FAvaEaseCurveTool>& InEaseCurveTool);

	void SetTangents(const FAvaEaseCurveTangents& InTangents, EAvaEaseCurveToolOperation InOperation,
		const bool bInSetEaseCurve, const bool bInBroadcastUpdate, const bool bInSetSequencerTangents) const;

	FAvaEaseCurveTangents GetTangents() const;

	FKeyHandle GetSelectedKeyHandle() const;

	void ZoomToFit() const;

protected:
	TSharedRef<SWidget> ConstructCurveEditorPanel();

	FFrameRate GetDisplayRate() const;
	EAvaEaseCurveToolOperation GetToolOperation() const;
	FVector2D GetEditorSize() const;
	bool ShowEqualValueKeyError() const;
	bool IsEaseCurveSelection() const;

	void HandleEditorTangentsChanged(const FAvaEaseCurveTangents& InTangents) const;

	void OnStartTangentSpinBoxChanged(const double InNewValue) const;
	void OnStartTangentWeightSpinBoxChanged(const double InNewValue) const;
	void OnEndTangentSpinBoxChanged(const double InNewValue) const;
	void OnEndTangentWeightSpinBoxChanged(const double InNewValue) const;
	void OnBeginSliderMovement();
	void OnEndSliderMovement(const double InNewValue);

	void OnPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const;
	void OnQuickPresetChanged(const TSharedPtr<FAvaEaseCurvePreset>& InPreset) const;
	bool OnGetNewPresetTangents(FAvaEaseCurveTangents& OutTangents) const;

	void UndoAction();
	void RedoAction();

	void OnEditorDragStart() const;
	void OnEditorDragEnd() const;

	FText GetStartText() const;
	FText GetStartTooltipText() const;
	FText GetEndText() const;
	FText GetEndTooltipText() const;

	void ResetTangentsAndNotify() const;

	//~ Begin SWidget
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget

	TWeakPtr<FAvaEaseCurveTool> EaseCurveToolWeak;

	TAttribute<EAvaEaseCurveToolMode> ToolMode;
	TAttribute<EAvaEaseCurveToolOperation> ToolOperation;

	TSharedPtr<SAvaEaseCurveEditor> CurveEaseEditorWidget;
	TSharedPtr<SAvaEaseCurvePreset> CurvePresetWidget;

	int32 CurrentGraphSize = DefaultGraphSize;

	TSharedPtr<FAvaEaseCurveToolContextMenu> ContextMenu;
};
