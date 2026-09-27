// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "EditorUndoClient.h"

DECLARE_DELEGATE_RetVal(bool, FEnableFaceRefinementWorkflowDelegate)

/////////////////////////////////////////////////////
// FIdentityOutlinerTreeNode

struct FIdentityOutlinerTreeNode
{
	/** The promoted frame associated with this node */
	TWeakObjectPtr<class UMetaHumanIdentityPromotedFrame> PromotedFrame;

	/** The index of the promoted frame associated with this node */
	int32 FrameIndex = INDEX_NONE;

	/** Group name as visualized by the outliner */
	FText OutlinerGroupName;

	/** Curve name as visualized by the outliner */
	FText OutlinerCurveName;

	/** Internal group name as specified in the group config. Internal names are used by everything outside the outliner */
	FString InternalGroupName;

	/** Internal curve name as specified in the curves config. Internal names are used by everything outside the outliner */
	FString InternalCurveName;

	/** The parent of this node, nullptr if this is the root */
	TWeakPtr<FIdentityOutlinerTreeNode> Parent;

	/** The list of child nodes */
	TArray<TSharedRef<FIdentityOutlinerTreeNode>> Children;

	/** If the node is visible in the tree view */
	bool bIsNodeVisible  = true;

	bool IsFrameNode() const;

	bool IsGroupNode() const;

	bool IsCurveNode() const;

	const FSlateBrush* GetCurveOrGroupIcon();

	const FText GetCurveOrGroupIconTooltip();

	FText GetLabel() const;

	void GetCurveNamesRecursive(TArray<FString>& OutCurveNames);

	void OnVisibleStateChanged(ECheckBoxState InNewState);

	void VisibleStateChangedRecursive(ECheckBoxState InNewState);

	bool IsKeypointVisibleForAnyCurve(const FString& InKeypointName);

	ECheckBoxState IsVisibleCheckState() const;

	void OnActiveStateChanged(ECheckBoxState InNewState);

	void ActiveStateChangedRecursive(ECheckBoxState InNewState);

	bool IsKeypointActiveForAnyCurve(const FString& InKeypointName);

	ECheckBoxState IsActiveCheckState() const;

	bool IsSelected(bool bInRecursive = true) const;

	FEnableFaceRefinementWorkflowDelegate EnableFaceRefinementWorkflowDelegate;

	FText GetTooltipForVisibilityCheckBox() const;
	FText GetTooltipForUsedToSolveCheckBox() const;
	FText GetNodeTypeName() const;

	bool IsEnabled() const;
};

/////////////////////////////////////////////////////
// SMetaHumanIdentityOutliner

enum class EIdentityPoseType : uint8;

DECLARE_DELEGATE_OneParam(FOnOutlinerSelectionChanged, const TArray<FString>& SelectedCurves)

class SMetaHumanIdentityOutliner
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaHumanIdentityOutliner) {}

		SLATE_ARGUMENT(TSharedPtr<class FLandmarkConfigIdentityHelper>, LandmarkConfigHelper)

		SLATE_ARGUMENT(TSharedPtr<class FMetaHumanEditorViewportClient>, ViewportClient)

		SLATE_EVENT(FOnOutlinerSelectionChanged, OnSelectionChanged)

		SLATE_EVENT(FSimpleDelegate, OnResetImageViewerPoints)

		SLATE_ATTRIBUTE(bool, FaceIsConformed)


	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetPromotedFrame(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex, const EIdentityPoseType& InSelectedPose);

private:

	/** Create the Header Row for the outliner tree view */
	TSharedRef<SHeaderRow> MakeHeaderRow() const;

	/** */
	TSharedRef<ITableRow> HandleGenerateOutlinerTreeRow(TSharedRef<FIdentityOutlinerTreeNode> InItem, const TSharedRef<STableViewBase>& InOwnerTable);

	/** */
	void HandleOutlinerTreeGetChildren(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutChildren);

	/** */
	void HandleOutlinerTreeSelectionChanged(TSharedPtr<FIdentityOutlinerTreeNode> InItem, ESelectInfo::Type InSelectInfo);

	/** Enable recursive expansion using Shift + click to expand a node */
	void HandleOutlinerTreeSetExpansionRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, bool bInShouldExpand);

	/** Populate the name mapping from specified config path. Returns true if parsing was successful */
	void CreateCurveNameMappingFromFile();

	/** Update the node expansion for selected nodes */
	void RefreshSelectedNodeExpansion(const TArray<TSharedRef<FIdentityOutlinerTreeNode>>& InSelectedNodes);

	/** Checks selection from contour data and updates selection for tree */
	void RefreshTreeSelectionFromContourData(bool bClearPointSelection = false);

	/** */
	TSharedRef<FIdentityOutlinerTreeNode> MakeOutlinerTreeNodeForPromotedFrame(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex, const EIdentityPoseType& InSelectedPose);

	/** */
	void FindSelectedItemsRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutSelectedItems) const;

	/** Looks up Contour Data to determine which curves are selected */
	void FindSelectionFromContourDataRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutSelectedItems) const;

	/** populates a list of tree nodes that have a matching curve name */
	void FindItemsWithCurveNamesRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, const TSet<FString>& InNames, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutItems) const;

	/** populates the list of curve nodes from the input node node recursively */
	void FindAllCurveNodesRecursive(TSharedRef<FIdentityOutlinerTreeNode> InItem, TArray<TSharedRef<FIdentityOutlinerTreeNode>>& OutItems) const;

	/** Returns a list of curves that have been selected from contour data state */
	TSet<FString> FindSelectedCurveNamesFromContourData() const;

	/** Returns a list of curves that have been selected from contour data state */
	TSet<FString> FindSelectedCurveNamesFromNodeSelection() const;

	/** Returns true if face has been conformed or user manually sets it */
	bool IsFaceRefinementWorkflowEnabled();

	/** Creates suppressible dialog checking if the user wants to manually place curves */
	bool EnableCurveEditingForUnconformedFaceDialog() const;

private:

	/** Reference to the Promoted Frame we are editing */
	TWeakObjectPtr<class UMetaHumanIdentityPromotedFrame> PromotedFrame;

	/** */
	TSharedPtr<class FLandmarkConfigIdentityHelper> LandmarkConfigHelper;

	/** Command list for handling actions in the tree view */
	TSharedPtr<class FUICommandList> CommandList;

	/** Hold a shared pointer to viewport client */
	TSharedPtr<FMetaHumanEditorViewportClient> ViewportClient;

	/** A pointer to the Identity tree view */
	TSharedPtr<STreeView<TSharedRef<FIdentityOutlinerTreeNode>>> OutlinerTreeWidget;

	/** List of root nodes of the Outliner tree */
	TArray<TSharedRef<FIdentityOutlinerTreeNode>> RootNodes;

	/** Mapping for Outliner curve names */
	TMap<FString, FText> InternalToOutlinerNamingMap;

	/**  A delegate that returns the state for improved face conformation flow */
	FEnableFaceRefinementWorkflowDelegate EnableFaceRefinementWorkflowDelegate;

	/** An attribute to check if the face has been conformed */
	TAttribute<bool> bFaceIsConformed;

	/** True if the user selected manual curve interaction before solve took place */
	bool bManualCurveInteraction = false;
};