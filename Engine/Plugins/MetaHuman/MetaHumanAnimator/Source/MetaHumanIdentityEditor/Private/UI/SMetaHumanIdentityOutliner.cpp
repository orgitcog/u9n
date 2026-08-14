// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetaHumanIdentityOutliner.h"

#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanEditorViewportClient.h"
#include "MetaHumanCurveDataController.h"
#include "MetaHumanIdentityStyle.h"

#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Algo/Count.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "ScopedTransaction.h"
#include "Dialogs/Dialogs.h"
#include "Widgets/SToolTip.h"
#include "LandmarkConfigIdentityHelper.h"

#define LOCTEXT_NAMESPACE "MetaHumanIdentityOutliner"

static const FName OutlinerTree_ColumnName_Frame = TEXT("Frame");
static const FName OutlinerTree_ColumnName_Visible = TEXT("Visible");
static const FName OutlinerTree_ColumnName_Active = TEXT("Active");

static const TCHAR* IdentityOutlinerTransactionContext = TEXT("IdentityOutlinerTransaction");

/////////////////////////////////////////////////////
// FIdentityOutlinerTreeNode

bool FIdentityOutlinerTreeNode::IsGroupNode() const
{
	return !InternalGroupName.IsEmpty();
}

bool FIdentityOutlinerTreeNode::IsCurveNode() const
{
	return !OutlinerCurveName.IsEmpty();
}

bool FIdentityOutlinerTreeNode::IsFrameNode() const
{
	return PromotedFrame.IsValid() && FrameIndex != INDEX_NONE;
}

FText FIdentityOutlinerTreeNode::GetLabel() const
{
	if (IsGroupNode())
	{
		return OutlinerGroupName;
	}

	if (IsCurveNode())
	{
		return OutlinerCurveName;
	}

	if (PromotedFrame.IsValid() && !PromotedFrame->FrameName.IsEmptyOrWhitespace())
	{
		return PromotedFrame->FrameName;
	}
	else
	{
		return FText::Format(LOCTEXT("FrameLabel", "Frame {0}"), { FrameIndex });
	}

	return LOCTEXT("InvalidNodeName", "<Invalid Node>");
}

void FIdentityOutlinerTreeNode::GetCurveNamesRecursive(TArray<FString>& OutCurveNames)
{
	if (IsGroupNode() || IsFrameNode())
	{
		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : Children)
		{
			Child->GetCurveNamesRecursive(OutCurveNames);
		}
	}

	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		OutCurveNames.Add(InternalCurveName);
	}
}

const FSlateBrush* FIdentityOutlinerTreeNode::GetCurveOrGroupIcon()
{
	if (IsGroupNode())
	{
		return FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.MarkerGroup");
	}
	else if (IsCurveNode())
	{
		return FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.MarkerCurve");
	}
	else if (IsFrameNode())
	{
		return FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Frame");
	}
	else
	{
		return FAppStyle::GetNoBrush();
	}
}

const FText FIdentityOutlinerTreeNode::GetCurveOrGroupIconTooltip()
{
	if (IsGroupNode())
	{
		return LOCTEXT("IdentityOutlinerItemTypeGroupTooltip", "This is a Group of Marker Curves\nClick on icons on the right to turn on/off Visibility or Used for Solve for the entire group of curves");
	}
	else if (IsCurveNode())
	{
		return LOCTEXT("IdentityOutlinerItemTypeCurveTooltip", "This is a Marker Curve");
	}
	else if (IsFrameNode())
	{
		return LOCTEXT("IdentityOutlinerItemTypeFrameTooltip", "This is a Promoted Frame");
	}

	return LOCTEXT("IdentityOutlinerItemTypeUndefinedTooltip", "[Item type undefined]");
}

FText FIdentityOutlinerTreeNode::GetTooltipForVisibilityCheckBox() const
{
	ECheckBoxState VisibleCheckBoxState = IsVisibleCheckState();

	if (VisibleCheckBoxState == ECheckBoxState::Checked)
	{
		return FText::Format(LOCTEXT("IdentityOutlinerVisibilityCheckBoxTooltipVisible", "This {0} is currently visible"), GetNodeTypeName());
	}
	else if (VisibleCheckBoxState == ECheckBoxState::Unchecked)
	{
		return FText::Format(LOCTEXT("IdentityOutlinerVisibilityCheckBoxTooltipHidden", "This {0} is currently hidden"), GetNodeTypeName());
	}
	else
	{
		return LOCTEXT("IdentityOutlinerVisibilityCheckBoxTooltipMixed", "The sub-nodes contain mixed values");
	}
}

FText FIdentityOutlinerTreeNode::GetTooltipForUsedToSolveCheckBox() const
{
	ECheckBoxState ActiveCheckBoxState = IsActiveCheckState();
	if (ActiveCheckBoxState == ECheckBoxState::Checked)
	{
		return FText::Format(LOCTEXT("IdentityOutlinerUsedForSolveCheckBoxTooltipChecked", "This {0} is currently used for solve"), GetNodeTypeName());
	}
	else if (ActiveCheckBoxState == ECheckBoxState::Unchecked)
	{
		return FText::Format(LOCTEXT("IdentityOutlinerUsedForSolveCheckBoxTooltipUnchecked", "This {0} is currently not used for solve"), GetNodeTypeName());
	}
	else
	{
		return LOCTEXT("IdentityOutlinerUsedForSolveCheckBoxTooltipMixed", "The sub-nodes contain mixed values");
	}
}

FText FIdentityOutlinerTreeNode::GetNodeTypeName() const
{
	if (IsGroupNode())
	{
		return LOCTEXT("IdentityOutlinerNodeTypeNameCurveGroup", "curve group");
	}
	else if (IsCurveNode())
	{
		return LOCTEXT("IdentityOutlinerNodeTypeNameCurve", "curve");
	}
	else if (IsFrameNode())
	{
		return LOCTEXT("IdentityOutlinerNodeTypeNameFrame", "frame");
	}

	return LOCTEXT("IdentityOutlinerNodeTypeNameUnknown", "<Unknown type>");	
}

bool FIdentityOutlinerTreeNode::IsEnabled() const
{
	return PromotedFrame->bUseToSolve;
}

void FIdentityOutlinerTreeNode::OnVisibleStateChanged(ECheckBoxState InNewState)
{
	if (EnableFaceRefinementWorkflowDelegate.IsBound() && EnableFaceRefinementWorkflowDelegate.Execute())
	{
		VisibleStateChangedRecursive(InNewState);

		TArray<FString> ChangedCurves;
		GetCurveNamesRecursive(ChangedCurves);
		PromotedFrame->CurveDataController->ResolvePointSelectionOnCurveVisibilityChanged(ChangedCurves, IsCurveNode(), bIsNodeVisible);
	}
}

void FIdentityOutlinerTreeNode::VisibleStateChangedRecursive(ECheckBoxState InNewState)
{
	if (IsGroupNode())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("GroupVisibilityChangedTransaction", "Edit Group Is Visible"), PromotedFrame.Get());
		PromotedFrame->Modify();

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : Children)
		{
			Child->VisibleStateChangedRecursive(InNewState);
		}
	}

	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("CurveVisibilityChangedTransaction", "Edit Curve Is Visible"), PromotedFrame.Get());
		PromotedFrame->Modify();

		const bool bIsVisible = (InNewState == ECheckBoxState::Checked);

		TMap<FString, FTrackingContour>& Contours = PromotedFrame->ContourData->FrameTrackingContourData.TrackingContours;

		const FString& CurveStartPoint = Contours[InternalCurveName].StartPointName;
		const FString& CurveEndPointName = Contours[InternalCurveName].EndPointName;

		Contours[InternalCurveName].State.bVisible = bIsVisible;

		if (!CurveStartPoint.IsEmpty())
		{
			Contours[CurveStartPoint].State.bVisible = IsKeypointVisibleForAnyCurve(CurveStartPoint);
		}

		if (!CurveEndPointName.IsEmpty())
		{
			Contours[CurveEndPointName].State.bVisible = IsKeypointVisibleForAnyCurve(CurveEndPointName);
		}
	}

	if (IsFrameNode())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("FrameVisibilityChangedTransaction", "Edit PromotedFrame Is Visible"), PromotedFrame.Get());
		PromotedFrame->Modify();

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : Children)
		{
			Child->VisibleStateChangedRecursive(InNewState);
		}
	}
}

bool FIdentityOutlinerTreeNode::IsKeypointVisibleForAnyCurve(const FString& InKeypointName)
{
	const TMap<FString, FTrackingContour>& Contours = PromotedFrame->GetFrameTrackingContourData()->TrackingContours;
	bool bVisible = false;

	for(const TPair<FString, FTrackingContour>& Contour : Contours)
	{
		if(Contour.Value.StartPointName == InKeypointName || Contour.Value.EndPointName == InKeypointName)
		{
			bVisible |= Contour.Value.State.bVisible;
		}
	}

	return bVisible;
}

ECheckBoxState FIdentityOutlinerTreeNode::IsVisibleCheckState() const
{
	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		if (PromotedFrame->GetFrameTrackingContourData()->TrackingContours[InternalCurveName].State.bVisible)
		{
			return ECheckBoxState::Checked;
		}
		else
		{
			return ECheckBoxState::Unchecked;
		}
	}

	if (IsGroupNode() || IsFrameNode())
	{
		const int32 NumVisibleChildren = Algo::CountIf(Children, [](const TSharedRef<FIdentityOutlinerTreeNode>& Child)
		{
			return Child->IsVisibleCheckState() == ECheckBoxState::Checked;
		});

		if (NumVisibleChildren == Children.Num())
		{
			return ECheckBoxState::Checked;
		}

		if (NumVisibleChildren == 0)
		{
			return ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

void FIdentityOutlinerTreeNode::OnActiveStateChanged(ECheckBoxState InNewState)
{
	if (EnableFaceRefinementWorkflowDelegate.IsBound() && EnableFaceRefinementWorkflowDelegate.Execute())
	{
		ActiveStateChangedRecursive(InNewState);

		if (PromotedFrame->CurveDataController->TriggerContourUpdate().IsBound())
		{
			PromotedFrame->CurveDataController->TriggerContourUpdate().Broadcast();
		}
	}
}

void FIdentityOutlinerTreeNode::ActiveStateChangedRecursive(ECheckBoxState InNewState)
{
	if (IsGroupNode())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("GroupActiveChangedTransaction", "Edit Group Is Active"), PromotedFrame.Get());
		PromotedFrame->Modify();

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : Children)
		{
			Child->ActiveStateChangedRecursive(InNewState);
		}
	}

	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("CurveActiveChangedTransaction", "Edit Curve Is Active"), PromotedFrame.Get());
		PromotedFrame->Modify();

		TMap<FString, FTrackingContour>& Contours = PromotedFrame->ContourData->FrameTrackingContourData.TrackingContours;
		const bool bIsActive = (InNewState == ECheckBoxState::Checked);

		const FString& CurveStartPoint = Contours[InternalCurveName].StartPointName;
		const FString& CurveEndPointName = Contours[InternalCurveName].EndPointName;

		Contours[InternalCurveName].State.bActive = bIsActive;

		if (!CurveStartPoint.IsEmpty())
		{
			Contours[CurveStartPoint].State.bActive = IsKeypointActiveForAnyCurve(CurveStartPoint);
		}

		if (!CurveEndPointName.IsEmpty())
		{
			Contours[CurveEndPointName].State.bActive = IsKeypointActiveForAnyCurve(CurveEndPointName);
		}
	}

	if (IsFrameNode())
	{
		const FScopedTransaction Transaction(IdentityOutlinerTransactionContext, LOCTEXT("FrameActiveChangedTransaction", "Edit PromotedFrame Is Active"), PromotedFrame.Get());
		PromotedFrame->Modify();

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : Children)
		{
			Child->ActiveStateChangedRecursive(InNewState);
		}
	}
}

bool FIdentityOutlinerTreeNode::IsKeypointActiveForAnyCurve(const FString& InKeypointName)
{
	const TMap<FString, FTrackingContour>& Contours = PromotedFrame->ContourData->FrameTrackingContourData.TrackingContours;
	bool bActive = false;

	for(const TPair<FString, FTrackingContour>& Contour : Contours)
	{
		if(Contour.Value.StartPointName == InKeypointName || Contour.Value.EndPointName == InKeypointName)
		{
			bActive |= Contour.Value.State.bActive;
		}
	}

	return bActive;
}

ECheckBoxState FIdentityOutlinerTreeNode::IsActiveCheckState() const
{
	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		if (PromotedFrame->GetFrameTrackingContourData()->TrackingContours[InternalCurveName].State.bActive)
		{
			return ECheckBoxState::Checked;
		}
		else
		{
			return ECheckBoxState::Unchecked;
		}
	}

	if (IsGroupNode() || IsFrameNode())
	{
		const int32 NumActiveChildren = Algo::CountIf(Children, [](const TSharedRef<FIdentityOutlinerTreeNode>& Child)
		{
			return Child->IsActiveCheckState() == ECheckBoxState::Checked;
		});

		if (NumActiveChildren == Children.Num())
		{
			return ECheckBoxState::Checked;
		}

		if (NumActiveChildren == 0)
		{
			return ECheckBoxState::Unchecked;
		}
	}

	return ECheckBoxState::Undetermined;
}

bool FIdentityOutlinerTreeNode::IsSelected(bool bInRecursive) const
{
	if (IsCurveNode() && PromotedFrame.IsValid())
	{
		return PromotedFrame->GetFrameTrackingContourData()->TrackingContours[InternalCurveName].State.bSelected;
	}

	if (bInRecursive && (IsGroupNode() || IsFrameNode()))
	{
		const int32 NumChildrenSelected = Algo::CountIf(Children, [](const TSharedRef<FIdentityOutlinerTreeNode>& Child)
		{
			return Child->IsSelected();
		});

		return NumChildrenSelected == Children.Num();
	}

	return false;
}

/////////////////////////////////////////////////////
// SOutlinerTreeRow

class SOutlinerTreeRow
	: public SMultiColumnTableRow<TSharedRef<FIdentityOutlinerTreeNode>>
{
public:
	SLATE_BEGIN_ARGS(SOutlinerTreeRow) {}
		SLATE_ARGUMENT(TSharedPtr<FIdentityOutlinerTreeNode>, Item)
		SLATE_ARGUMENT(FEnableFaceRefinementWorkflowDelegate, EnableFaceRefinementWorkflow)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableBase)
	{
		Item = InArgs._Item;
		Item->EnableFaceRefinementWorkflowDelegate = InArgs._EnableFaceRefinementWorkflow;

		SMultiColumnTableRow<TSharedRef<FIdentityOutlinerTreeNode>>::Construct(FSuperRowType::FArguments(), InOwnerTableBase);
	}

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& InColumnName) override
	{
		if (InColumnName == OutlinerTree_ColumnName_Frame)
		{
			const FString TooltipStyleName = FString::Printf(TEXT("Identity.Outliner.%s"), *Item->InternalCurveName);
			const FSlateBrush* ThumbnailBrush = Item->InternalCurveName.IsEmpty() ? FStyleDefaults::GetNoBrush() : FMetaHumanIdentityStyle::Get().GetBrush(*TooltipStyleName);

			const float ThumbnailSize = 256.0f;

			return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SExpanderArrow, SharedThis(this))
				.IndentAmount(16)
				.ShouldDrawWires(true)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(Item->GetCurveOrGroupIcon()) //these are never changing so we don't need a delegate
				.ToolTipText(Item->GetCurveOrGroupIconTooltip())
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Margin(4)
				.Text(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::GetLabel)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			.AutoWidth()
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Help"))
				.Visibility_Lambda([this]
				{
					if (Item && !Item->InternalCurveName.IsEmpty() && IsHovered())
					{
						return EVisibility::Visible;
					}
					else
					{
						return EVisibility::Collapsed;
					}
				})
				.ToolTip(
					SNew(SToolTip)
					[
						SNew(SBox)
						.HeightOverride(ThumbnailSize)
						.WidthOverride(ThumbnailSize)
						[
							SNew(SImage)
							.ColorAndOpacity(FSlateColor::UseForeground())
							.Image(ThumbnailBrush)
						]
					]
				)
			];
		}

		if (InColumnName == OutlinerTree_ColumnName_Visible)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::IsVisibleCheckState)
					.OnCheckStateChanged(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::OnVisibleStateChanged)
					.IsEnabled(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::IsEnabled)
					.BackgroundImage(FStyleDefaults::GetNoBrush())
					.BackgroundHoveredImage(FStyleDefaults::GetNoBrush())
					.BackgroundPressedImage(FStyleDefaults::GetNoBrush())
					.CheckedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Visible"))
					.CheckedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Visible"))
					.CheckedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Visible"))
					.UncheckedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Hidden"))
					.UncheckedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Hidden"))
					.UncheckedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Hidden"))
					.UndeterminedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.UndeterminedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.UndeterminedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.ToolTipText(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::GetTooltipForVisibilityCheckBox)
				];
		}

		if (InColumnName == OutlinerTree_ColumnName_Active)
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::IsActiveCheckState)
					.OnCheckStateChanged(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::OnActiveStateChanged)
					.IsEnabled(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::IsEnabled)
					.BackgroundImage(FStyleDefaults::GetNoBrush())
					.BackgroundHoveredImage(FStyleDefaults::GetNoBrush())
					.BackgroundPressedImage(FStyleDefaults::GetNoBrush())
					.CheckedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveFull"))
					.CheckedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveFull"))
					.CheckedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveFull"))
					.UncheckedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveEmpty"))
					.UncheckedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveEmpty"))
					.UncheckedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.UsedToSolveEmpty"))
					.UndeterminedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.UndeterminedHoveredImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.UndeterminedPressedImage(FMetaHumanIdentityStyle::Get().GetBrush("Identity.Outliner.Mixed"))
					.ToolTipText(Item.ToSharedRef(), &FIdentityOutlinerTreeNode::GetTooltipForUsedToSolveCheckBox)
				];
		}

		return SNullWidget::NullWidget;
	}

private:
	TSharedPtr<FIdentityOutlinerTreeNode> Item;
};

/////////////////////////////////////////////////////
// SMetaHumanIdentityOutliner

void SMetaHumanIdentityOutliner::Construct(const FArguments& InArgs)
{
	LandmarkConfigHelper = InArgs._LandmarkConfigHelper;
	bFaceIsConformed = InArgs._FaceIsConformed;

	ViewportClient = InArgs._ViewportClient;
	EnableFaceRefinementWorkflowDelegate.BindSP(this, &SMetaHumanIdentityOutliner::IsFaceRefinementWorkflowEnabled);

	check(LandmarkConfigHelper.IsValid());

	CreateCurveNameMappingFromFile();

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SAssignNew(OutlinerTreeWidget, STreeView<TSharedRef<FIdentityOutlinerTreeNode>>)
			.SelectionMode(ESelectionMode::Multi)
			.TreeItemsSource(&RootNodes)
			.AllowInvisibleItemSelection(true)
			.HighlightParentNodesForSelection(true)
			.HeaderRow(MakeHeaderRow())
			.OnGenerateRow(this, &SMetaHumanIdentityOutliner::HandleGenerateOutlinerTreeRow)
			.OnGetChildren(this, &SMetaHumanIdentityOutliner::HandleOutlinerTreeGetChildren)
			.OnSetExpansionRecursive(this, &SMetaHumanIdentityOutliner::HandleOutlinerTreeSetExpansionRecursive)
			.OnSelectionChanged(this, &SMetaHumanIdentityOutliner::HandleOutlinerTreeSelectionChanged)
		]
	];
}

void SMetaHumanIdentityOutliner::SetPromotedFrame(UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex, const EIdentityPoseType& InSelectedPose)
{
	if (PromotedFrame.IsValid())
	{
		PromotedFrame->GetCurveDataController()->GetCurvesSelectedDelegate().RemoveAll(this);
	}

	PromotedFrame = InPromotedFrame;
	if (PromotedFrame.IsValid())
	{
		RootNodes = { MakeOutlinerTreeNodeForPromotedFrame(PromotedFrame.Get(), InFrameIndex, InSelectedPose) };
		OutlinerTreeWidget->SetItemExpansion(RootNodes[0], true);
		InPromotedFrame->GetCurveDataController()->GetCurvesSelectedDelegate().AddSP(this, &SMetaHumanIdentityOutliner::RefreshTreeSelectionFromContourData);
	}
	else
	{
		RootNodes.Empty();
	}

	RefreshTreeSelectionFromContourData();
	OutlinerTreeWidget->RequestTreeRefresh();
}

TSharedRef<SHeaderRow> SMetaHumanIdentityOutliner::MakeHeaderRow() const
{
	return SNew(SHeaderRow)
		.Visibility(EVisibility::All)

		+ SHeaderRow::Column(OutlinerTree_ColumnName_Frame)
		.DefaultLabel(LOCTEXT("OutlineColumnLabel_Frame", "Frame"))
		.FillWidth(0.6f)

		+SHeaderRow::Column(OutlinerTree_ColumnName_Visible)
		.DefaultLabel(FText()) //we don't need column label as it takes space, and all the info is in the icons and the tooltips
		.FillWidth(0.05f)

		+ SHeaderRow::Column(OutlinerTree_ColumnName_Active)
		.DefaultLabel(FText()) //same as above
		.FillWidth(0.05f);
}

TSharedRef<ITableRow> SMetaHumanIdentityOutliner::HandleGenerateOutlinerTreeRow(TSharedRef<FIdentityOutlinerTreeNode> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SOutlinerTreeRow, InOwnerTable)
		.Item(InItem)
		.EnableFaceRefinementWorkflow(EnableFaceRefinementWorkflowDelegate);
}

void SMetaHumanIdentityOutliner::HandleOutlinerTreeGetChildren(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutChildren)
{
	for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
	{
		if (Child->bIsNodeVisible)
		{
			OutChildren.Add(Child);
		}
	}
}

void SMetaHumanIdentityOutliner::HandleOutlinerTreeSelectionChanged(TSharedPtr<FIdentityOutlinerTreeNode> InItem, ESelectInfo::Type InSelectInfo)
{
	if (PromotedFrame.IsValid() && PromotedFrame->GetCurveDataController().IsValid())
	{
		TSet<FString> SelectedCurveNames = FindSelectedCurveNamesFromNodeSelection();
		PromotedFrame->GetCurveDataController()->SetCurveSelection(SelectedCurveNames, true);
	}
}

void SMetaHumanIdentityOutliner::HandleOutlinerTreeSetExpansionRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, bool bInShouldExpand)
{
	if (OutlinerTreeWidget != nullptr)
	{
		OutlinerTreeWidget->SetItemExpansion(InItem, bInShouldExpand);

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
		{
			HandleOutlinerTreeSetExpansionRecursive(Child, bInShouldExpand);
		}
	}
}

TSharedRef<FIdentityOutlinerTreeNode> SMetaHumanIdentityOutliner::MakeOutlinerTreeNodeForPromotedFrame(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex, const EIdentityPoseType& InSelectedPose)
{
	TSharedRef<FIdentityOutlinerTreeNode> FrameNode = MakeShared<FIdentityOutlinerTreeNode>();
	FrameNode->PromotedFrame = InPromotedFrame;
	FrameNode->FrameIndex = InFrameIndex;

	TSet<FString> CurveSet;
	TSharedPtr<FMarkerDefs> AllConfigData = LandmarkConfigHelper->GetMarkerDefs();
	for (const FMarkerCurveDef& CurveDef : AllConfigData->CurveDefs)
	{
		if (InPromotedFrame->GetFrameTrackingContourData()->TrackingContours.Contains(CurveDef.Name))
		{
			CurveSet.Add(CurveDef.Name);
		}
	}

	ECurvePresetType CurvePreset = LandmarkConfigHelper->GetCurvePresetFromIdentityPose(InSelectedPose);
	TArray<FString> GroupNames = LandmarkConfigHelper->GetGroupListForSelectedPreset(CurvePreset);

	for (const FString& GroupName : GroupNames)
	{
		TSharedRef<FIdentityOutlinerTreeNode> GroupNode = MakeShared<FIdentityOutlinerTreeNode>();
		GroupNode->PromotedFrame = InPromotedFrame;
		GroupNode->Parent = FrameNode;
		GroupNode->InternalGroupName = GroupName;
		GroupNode->OutlinerGroupName = InternalToOutlinerNamingMap.Contains(GroupName) ? InternalToOutlinerNamingMap[GroupName] : FText::FromString(GroupName);

		for (const FMarkerCurveDef& CurveDef : AllConfigData->CurveDefs)
		{
			if (CurveDef.GroupTagIDs.Contains(GroupName) && CurveSet.Contains(CurveDef.Name))
			{
				TSharedRef<FIdentityOutlinerTreeNode> CurveNode = MakeShared<FIdentityOutlinerTreeNode>();
				CurveNode->PromotedFrame = InPromotedFrame;
				CurveNode->Parent = GroupNode;

				FText OutlinerCurveName = InternalToOutlinerNamingMap.Contains(CurveDef.Name) ? InternalToOutlinerNamingMap[CurveDef.Name] : FText::FromString(CurveDef.Name);
				CurveNode->OutlinerCurveName = OutlinerCurveName;
				CurveNode->InternalCurveName = CurveDef.Name;

				GroupNode->Children.Add(CurveNode);
				CurveSet.Remove(CurveDef.Name);
			}
		}

		FrameNode->Children.Add(GroupNode);
	}

	if (!CurveSet.IsEmpty())
	{
		TSharedRef<FIdentityOutlinerTreeNode> OtherGroupNode = MakeShared<FIdentityOutlinerTreeNode>();
		OtherGroupNode->PromotedFrame = InPromotedFrame;
		OtherGroupNode->Parent = FrameNode;
		OtherGroupNode->InternalGroupName = TEXT("Other");
		OtherGroupNode->OutlinerGroupName = LOCTEXT("GrpOther", "Other");

		// Add the remaining curves into an a virtual "Other" group
		for (const FString& OtherCurve : CurveSet)
		{
			TSharedRef<FIdentityOutlinerTreeNode> OtherCurveNode = MakeShared<FIdentityOutlinerTreeNode>();
			OtherCurveNode->PromotedFrame = InPromotedFrame;
			OtherCurveNode->Parent = OtherGroupNode;
			FText OutlinerCurveName = InternalToOutlinerNamingMap.Contains(OtherCurve) ? InternalToOutlinerNamingMap[OtherCurve] : FText::FromString(OtherCurve);
			OtherCurveNode->OutlinerCurveName = OutlinerCurveName;
			OtherCurveNode->InternalCurveName = OtherCurve;
			OtherGroupNode->Children.Add(OtherCurveNode);
		}

		FrameNode->Children.Add(OtherGroupNode);
	}

	return FrameNode;
}

void SMetaHumanIdentityOutliner::RefreshSelectedNodeExpansion(const TArray<TSharedRef<FIdentityOutlinerTreeNode>>& InSelectedNodes)
{
	for (const TSharedRef<FIdentityOutlinerTreeNode>& SelectedNode : InSelectedNodes)
	{
		OutlinerTreeWidget->SetItemExpansion(SelectedNode, true);

		if (SelectedNode->IsCurveNode())
		{
			TWeakPtr<FIdentityOutlinerTreeNode> CurrentNode = SelectedNode->Parent;
			while (CurrentNode.IsValid())
			{
				OutlinerTreeWidget->SetItemExpansion(CurrentNode.Pin().ToSharedRef(), true);

				CurrentNode = CurrentNode.Pin()->Parent;
			}
		}
	}
}

void SMetaHumanIdentityOutliner::RefreshTreeSelectionFromContourData(bool bClearPoints)
{
	TArray<TSharedRef<FIdentityOutlinerTreeNode>> SelectedNodes;
	OutlinerTreeWidget->Private_ClearSelection();

	for (const TSharedRef<FIdentityOutlinerTreeNode>& FrameNode : RootNodes)
	{
		FindSelectionFromContourDataRecursive(FrameNode, SelectedNodes);
	}

	for (const TSharedRef<FIdentityOutlinerTreeNode>& Item : SelectedNodes)
	{
		OutlinerTreeWidget->Private_SetItemSelection(Item, true);
	}

	RefreshSelectedNodeExpansion(SelectedNodes);
	OutlinerTreeWidget->RequestTreeRefresh();
}

void SMetaHumanIdentityOutliner::FindSelectedItemsRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutSelectedItems) const
{
	const bool bSearchRecursive = false;
	if (InItem->IsSelected(bSearchRecursive))
	{
		OutSelectedItems.Add(InItem);
	}

	for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
	{
		FindSelectedItemsRecursive(Child, OutSelectedItems);
	}
}

void SMetaHumanIdentityOutliner::FindSelectionFromContourDataRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutSelectedItems) const
{
	if (PromotedFrame.IsValid())
	{
		if (InItem->IsCurveNode())
		{
			const TMap<FString, FTrackingContour>& ContourData = PromotedFrame->GetFrameTrackingContourData()->TrackingContours;
			FString ContourName = InItem->InternalCurveName;
			
			if (ContourData[ContourName].State.bSelected)
			{
				OutSelectedItems.Add(InItem);
			}
		}

		for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
		{
			FindSelectionFromContourDataRecursive(Child, OutSelectedItems);
		}
	}
}

void SMetaHumanIdentityOutliner::FindItemsWithCurveNamesRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, const TSet<FString>& InNames, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutItems) const
{
	if (InNames.Contains(InItem->InternalCurveName))
	{
		OutItems.Add(InItem);
	}

	for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
	{
		FindItemsWithCurveNamesRecursive(Child, InNames, OutItems);
	}
}

void SMetaHumanIdentityOutliner::FindAllCurveNodesRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutItems) const
{
	if (InItem->IsCurveNode())
	{
		OutItems.Add(InItem);
	}

	for (const TSharedRef<FIdentityOutlinerTreeNode>& Child : InItem->Children)
	{
		FindAllCurveNodesRecursive(Child, OutItems);
	}
}

TSet<FString> SMetaHumanIdentityOutliner::FindSelectedCurveNamesFromContourData() const
{
	TArray<TSharedRef<FIdentityOutlinerTreeNode>> SelectedNodes;
	for (const TSharedRef<FIdentityOutlinerTreeNode>& FrameNode : RootNodes)
	{
		FindSelectedItemsRecursive(FrameNode, SelectedNodes);
	}

	TSet<FString> CurveNodes;
	for (const TSharedRef<FIdentityOutlinerTreeNode>& SelectedNode : SelectedNodes)
	{
		if (SelectedNode->IsCurveNode())
		{
			CurveNodes.Add(SelectedNode->InternalCurveName);
		}
	}

	return CurveNodes;
}

TSet<FString> SMetaHumanIdentityOutliner::FindSelectedCurveNamesFromNodeSelection() const
{
	TSet<FString> SelectedCurves;
	TArray<TSharedRef<FIdentityOutlinerTreeNode>> NodeItems;
	const TArray<TSharedRef<FIdentityOutlinerTreeNode>> SelectedItems = OutlinerTreeWidget->GetSelectedItems();

	// Selection could be a group or root node selected in the outliner
	for (const TSharedRef<FIdentityOutlinerTreeNode>& Item : SelectedItems)
	{
		FindAllCurveNodesRecursive(Item, NodeItems);
	}

	for (const TSharedRef<FIdentityOutlinerTreeNode>& CurveNode : NodeItems)
	{
		SelectedCurves.Add(CurveNode->InternalCurveName);
	}
	
	return SelectedCurves;
}

bool SMetaHumanIdentityOutliner::IsFaceRefinementWorkflowEnabled()
{
	bool bConformedCameraFrame = PromotedFrame->IsA(UMetaHumanIdentityCameraFrame::StaticClass()) && bFaceIsConformed.Get();
	bool bHasCorrectAlignment = bConformedCameraFrame || PromotedFrame->bIsHeadAlignmentSet;

	// Remember the manual override response for current identity editor
	if (!bHasCorrectAlignment && !bManualCurveInteraction)
	{
		bManualCurveInteraction = EnableCurveEditingForUnconformedFaceDialog();
	}

	return bHasCorrectAlignment || bManualCurveInteraction;
}

bool SMetaHumanIdentityOutliner::EnableCurveEditingForUnconformedFaceDialog() const
{
	FSuppressableWarningDialog::FSetupInfo Info(
		LOCTEXT("ShouldEnableCurves", "The Template Mesh associated with this Promoted Frame has not been aligned. \n"
			"It is advised to complete the solve workflow using the default curve set before adding additional curves. Would you still like to continue ?"),
		LOCTEXT("ShouldEnableCurvesTitle", "Use refinement workflow curves"),
		TEXT("UseRefinementCurves"));
	Info.ConfirmText = LOCTEXT("ShouldEnableCurves_ConfirmText", "Yes");
	Info.CancelText = LOCTEXT("ShouldEnableCurves_CancelText", "Cancel");

	FSuppressableWarningDialog ShouldRecordDialog(Info);
	FSuppressableWarningDialog::EResult UserInput = ShouldRecordDialog.ShowModal();

	return UserInput == FSuppressableWarningDialog::EResult::Cancel ? false : true;
}

void SMetaHumanIdentityOutliner::CreateCurveNameMappingFromFile()
{
	InternalToOutlinerNamingMap =
	{
		// Curves
		{ TEXT("crv_lip_upper_outer_l"), LOCTEXT("CrvLipOuterUpperL", "Lip Outer Upper (L)") },
		{ TEXT("crv_lip_upper_outer_r"), LOCTEXT("CrvLipOuterUpperR", "Lip Outer Upper (R)") },
		{ TEXT("crv_lip_lower_outer_l"), LOCTEXT("CrvLipOuterLowerL", "Lip Outer Lower (L)") },
		{ TEXT("crv_lip_lower_outer_r"), LOCTEXT("CrvLipOuterLowerR", "Lip Outer Lower (R)") },
		{ TEXT("crv_lip_lower_inner_l"), LOCTEXT("CrvLipInnerLowerL", "Lip Inner Lower (L)") },
		{ TEXT("crv_lip_lower_inner_r"), LOCTEXT("CrvLipInnerLowerR", "Lip Inner Lower (R)") },
		{ TEXT("crv_lip_upper_inner_l"), LOCTEXT("CrvLipInnerUpperL", "Lip Inner Upper (L)") },
		{ TEXT("crv_lip_upper_inner_r"), LOCTEXT("CrvLipInnerUpperR", "Lip Inner Upper (R)") },
		{ TEXT("crv_lip_philtrum_l"), LOCTEXT("CrvLipPhiltrumL", "Lip Philtrum (L)") },
		{ TEXT("crv_lip_philtrum_r"), LOCTEXT("CrvLipPhiltrumR", "Lip Philtrum (R)") },
		{ TEXT("crv_nasolabial_l"), LOCTEXT("CrvNasolabialL", "Nasolabial (L)") },
		{ TEXT("crv_nasolabial_r"), LOCTEXT("CrvNasolabialR", "Nasolabial (R)") },
		{ TEXT("crv_nostril_l"), LOCTEXT("CrvNostrilL", "Nostril (L)") },
		{ TEXT("crv_nostril_r"), LOCTEXT("CrvNostrilR", "Nostril (R)") },
		{ TEXT("crv_ear_outer_helix_l"), LOCTEXT("CrvEarHelixOuterL", "Ear Helix Outer (L)") },
		{ TEXT("ear_outer_helix_r"), LOCTEXT("CrvEarHelixOuterR", "Ear Helix Outer (R)") },
		{ TEXT("crv_ear_inner_helix_l"), LOCTEXT("CrvEarHelixInnerL", "Ear Helix Inner (L)") },
		{ TEXT("crv_ear_inner_helix_r"), LOCTEXT("CrvEarHelixInnerR", "Ear Helix Inner (R)") },
		{ TEXT("crv_ear_central_lower_l"), LOCTEXT("EarCentralLowerL", "Ear Central Lower (L)") },
		{ TEXT("crv_ear_central_lower_r"), LOCTEXT("EarCentralLowerR", "Ear Central Lower (R)") },
		{ TEXT("crv_ear_central_upper_l"), LOCTEXT("EarCentralUpperL", "Ear Central Upper (L)") },
		{ TEXT("crv_ear_central_upper_r"), LOCTEXT("EarCentralUpperR", "Ear Central Upper (R)") },
		{ TEXT("crv_brow_upper_l"), LOCTEXT("CrvBrowUpperL", "Brow Upper (L)") },
		{ TEXT("brow_middle_line_l"), LOCTEXT("CrvBrowMiddleL", "Brow Middle (L)") },
		{ TEXT("crv_brow_lower_l"), LOCTEXT("CrvBrowLowerL", "Brow Lower (L)") },
		{ TEXT("crv_brow_intermediate_l"), LOCTEXT("CrvBrowIntermediateL", "Brow Intermediate (L)") },
		{ TEXT("crv_brow_upper_r"), LOCTEXT("CrvBrowUpperR", "Brow Upper (R)") },
		{ TEXT("brow_middle_line_r"), LOCTEXT("CrvBrowMiddleR", "Brow Middle (R)") },
		{ TEXT("crv_brow_lower_r"), LOCTEXT("CrvBrowLowerR", "Brow Lower (R)") },
		{ TEXT("crv_brow_intermediate_r"), LOCTEXT("CrvBrowIntermediateR", "Brow Intermediate (R)") },
		{ TEXT("crv_mentolabial_fold"), LOCTEXT("CrvMentolabialFoldC", "Mentolabial Fold (C)") },
		{ TEXT("crv_eyecrease_l"), LOCTEXT("CrvEyeCreaseL", "Eye Crease (L)") },
		{ TEXT("crv_eyecrease_r"), LOCTEXT("CrvEyeCreaseR", "Eye Crease (R)") },
		{ TEXT("crv_eyelid_lower_l"), LOCTEXT("CrvEyelidLowerL", "Eyelid Lower (L)") },
		{ TEXT("crv_eyelid_lower_r"), LOCTEXT("CrvEyelidLowerR", "Eyelid Lower (R)") },
		{ TEXT("crv_eyelid_upper_l"), LOCTEXT("CrvEyelidUpperL", "Eyelid Upper (L)") },
		{ TEXT("crv_eyelid_upper_r"), LOCTEXT("CrvEyelidUpperR", "Eyelid Upper (R)") },
		{ TEXT("eye_plica_semilunaris_l"), LOCTEXT("CrvPlicaSmilunarisL", "Plica Semilunaris (L)") },
		{ TEXT("eye_plica_semilunaris_r"), LOCTEXT("CrvPlicaSmilunarisR", "Plica Semilunaris (R)") },
		{ TEXT("crv_outer_eyelid_edge_left_lower"), LOCTEXT("CrvEyelidOuterLowerL", "Eyelid Outer Lower (L)") },
		{ TEXT("crv_outer_eyelid_edge_right_lower"), LOCTEXT("CrvEyelidOuterLowerR", "Eyelid Outer Lower (R)") },
		{ TEXT("crv_outer_eyelid_edge_left_upper"), LOCTEXT("CrvEyelidOuterUpperL", "Eyelid Outer Upper (L)") },
		{ TEXT("crv_outer_eyelid_edge_right_upper"), LOCTEXT("CrvEyelidOuterUpperR", "Eyelid Outer Upper (R)") },
		{ TEXT("pt_frankfurt_fl"), LOCTEXT("CrvFrankfurtFrontL", "Frankfurt Front (L)") },
		{ TEXT("pt_frankfurt_fr"), LOCTEXT("CrvFrankfurtFrontR", "Frankfurt Front (R)") },
		{ TEXT("pt_frankfurt_rl"), LOCTEXT("CrvFrankfurtRearL", "Frankfurt Rear (L)") },
		{ TEXT("pt_frankfurt_rr"), LOCTEXT("CrvFrankfurtRearR", "Frankfurt Rear (R)") },
		{ TEXT("crv_iris_r"), LOCTEXT("CrvIrisTopR", "Iris Top (R)") },
		{ TEXT("crv_iris_l"), LOCTEXT("CrvIrisTopL", "Iris Top (L)") },
		{ TEXT("pt_tooth_upper"), LOCTEXT("CrvToothUpper", "Tooth Upper") },
		{ TEXT("pt_tooth_lower"), LOCTEXT("CrvToothLower", "Tooth Lower") },
		{ TEXT("pt_tooth_upper_2"), LOCTEXT("CrvToothUpper2", "Tooth Upper 2") },
		{ TEXT("pt_tooth_lower_2"), LOCTEXT("CrvToothLower2", "Tooth Lower 2") },
		{ TEXT("pt_left_contact"), LOCTEXT("CrvLeftContact", "Lip Contact Point (L)") },
		{ TEXT("pt_right_contact"), LOCTEXT("CrvRightContact", "Lip Contact Point (R)") },

		// Groups
		{ TEXT("brow_l"), LOCTEXT("GrpBrowL", "Brow (L)") },
		{ TEXT("brow_r"), LOCTEXT("GrpBrowR", "Brow (R)") },
		{ TEXT("eye_l"), LOCTEXT("GrpEyeL", "Eye (L)") },
		{ TEXT("eye_r"), LOCTEXT("GrpEyeR", "Eye (R)") },
		{ TEXT("lip_upper"), LOCTEXT("GrpLipUpper", "Lip Upper") },
		{ TEXT("lip_lower"), LOCTEXT("GrpLipLower", "Lip Lower") },
		{ TEXT("nose_l"), LOCTEXT("GrpNoseL", "Nose (L)") },
		{ TEXT("nose_r"), LOCTEXT("GrpNoseR", "Nose (R)") },
		{ TEXT("cheeks_l"), LOCTEXT("GrpCheeksL", "Cheeks (L)") },
		{ TEXT("cheeks_r"), LOCTEXT("GrpCheeksR", "Cheeks (R)") },
		{ TEXT("ear_l"), LOCTEXT("GrpEarL", "Ear (L)") },
		{ TEXT("ear_r"), LOCTEXT("GrpEarR", "Ear (R)") },
		{ TEXT("teeth"), LOCTEXT("GrpTeeth", "Teeth") },
		{ TEXT("lip_contacts"), LOCTEXT("GrpLipContacts", "Lip Contacts") }
	};
}

#undef LOCTEXT_NAMESPACE