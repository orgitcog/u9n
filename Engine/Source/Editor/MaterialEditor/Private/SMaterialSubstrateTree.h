// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DetailColumnSizeData.h"
#include "Engine/EngineTypes.h"
#include "IDetailPropertyRow.h"
#include "IDetailTreeNode.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "MaterialPropertyHelpers.h"
#include "Materials/Material.h"
#include "PropertyCustomizationHelpers.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "SMaterialLayersFunctionsTree.h"

class IPropertyHandle;
class SMaterialSubstrateTree;
class SMaterialLayersFunctionsInstanceWrapper;
class UDEditorParameterValue;
class UMaterialEditorInstanceConstant;
struct FRecursiveCreateWidgetsContext;

typedef TSharedPtr<FSortedParamData> FSortedParamDataPtr;
class SMaterialSubstrateTree : public SMaterialLayersTree
{
	friend class SMaterialSubstrateTreeItem;
public:
	SLATE_BEGIN_ARGS(SMaterialSubstrateTree)
		: _InMaterialEditorInstance(nullptr)
		, _InGenerator(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorParameters*, InMaterialEditorInstance)
	SLATE_ARGUMENT(SMaterialLayersFunctionsInstanceWrapper*, InWrapper)
	SLATE_ARGUMENT(TSharedPtr<class IPropertyRowGenerator>, InGenerator)
	SLATE_ARGUMENT(FGetShowHiddenParameters, InShowHiddenDelegate)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> InParent, TArray< TSharedPtr<FSortedParamData> >& OutChildren);
	void OnExpansionChanged(TSharedPtr<FSortedParamData> Item, bool bIsExpanded);
	void OnSelectionChangedMaterialSubstrateView(TSharedPtr<FSortedParamData> InSelectedItem, ESelectInfo::Type SelectInfo);
	void SetParentsExpansionState();

	int32 GetLayerFunctionIndex(int32 NodeIndex) const;
	int32 GetBlendFunctionIndex(int32 NodeIndex) const;
	TSharedPtr<SWidget> CreateContextMenu();
	void CreateCommandList();
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	void ShowHiddenValues(bool& bShowHiddenParameters) { bShowHiddenParameters = true; }
	TWeakObjectPtr<class UDEditorParameterValue> FunctionParameter;
	struct FMaterialLayersFunctions* FunctionInstance;
	TSharedPtr<IPropertyHandle> FunctionInstanceHandle;
	void RefreshOnAssetChange(const struct FAssetData& InAssetData, int32 InNodeId, EMaterialParameterAssociation MaterialType);
	void ResetAssetToDefault(TSharedPtr<FSortedParamData> InData);

	virtual void AddRootNodeLayer() override { AddNodeLayer(); }
	void AddNodeLayer(int32 InParent = -1);
	void RemoveNodeLayer(int32 InNodeId);

	void UnlinkLayer(int32 Index);
	virtual FReply RelinkLayersToParent() override;
	EVisibility GetUnlinkLayerVisibility(int32 Index) const;
	virtual EVisibility GetRelinkLayersToParentVisibility() const override;
	virtual void SetMaterialEditorInstance(UMaterialEditorParameters* InMaterialEditorInstance) override { MaterialEditorInstance = InMaterialEditorInstance; } 
	
	FReply ToggleLayerVisibility(int32 Index);
	bool IsLayerVisible(int32 Index) const;

	TSharedPtr<class FAssetThumbnailPool> GetTreeThumbnailPool();

	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorParameters* MaterialEditorInstance;

	/** Builds the custom parameter groups category */
	virtual void CreateGroupsWidget() override;
	virtual TWeakObjectPtr<class UDEditorParameterValue> GetFunctionParameter() override{ return FunctionParameter; }

	SMaterialLayersFunctionsInstanceWrapper* GetWrapper() { return Wrapper; }

	TSharedPtr<IDetailTreeNode> FindParameterGroupsNode(TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator);

	TSharedRef<SWidget> CreateThumbnailWidget(EMaterialParameterAssociation InAssociation, int32 InIndex, float InThumbnailSize);
	virtual void UpdateThumbnailMaterial(TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 InIndex, bool bAlterBlendIndex = false) override;
	FReply OnThumbnailDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent, EMaterialParameterAssociation InAssociation, int32 InIndex);
	bool IsOverriddenExpression(class UDEditorParameterValue* Parameter, int32 InIndex);
	bool IsOverriddenExpression(TObjectPtr<UDEditorParameterValue> Parameter, int32 InIndex);
	
	FGetShowHiddenParameters GetShowHiddenDelegate() const;


	void CollectStackItemsRecursively(TSharedPtr<FSortedParamData> Item, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer);

	virtual void CollectAssetStackItemsRecursively(TSharedPtr<FSortedParamData> Item, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer, TArray<uint32>& OutNodeIdsContainer) override;

	void CollectStackItemsForMaterialFunctionAsset(EMaterialParameterAssociation InAssociation, int32 InAssetIndex, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer);

protected:
	using FNodeId = int32;
	void RecursiveCreateWidgets(struct FRecursiveCreateWidgetsContext* Context, FNodeId InNodeId, TArray<TSharedPtr<FSortedParamData>>& InParentContainer, bool GenerateChildren);
	void ShowSubParameters(FRecursiveCreateWidgetsContext* InContext, TSharedPtr<FSortedParamData> ParentParameter);
	void OnDeleteSelectedTreeViewItems();
	bool CanDeleteSelectedTreeViewItems() const;
	void OnRenameSelectedTreeViewItems();
	bool CanRenameSelectedTreeViewItem() const;
private:
	TSharedPtr<FUICommandList> CommandList;
	TArray<TSharedPtr<FSortedParamData>> LayerProperties;
	
	FDetailColumnSizeData ColumnSizeData;
	
	SMaterialLayersFunctionsInstanceWrapper* Wrapper;
	
	TSharedPtr<class IPropertyRowGenerator> Generator;
	
	bool bLayerIsolated;
	
	/** Delegate to call to determine if hidden parameters should be shown */
	FGetShowHiddenParameters ShowHiddenDelegate;
};

class SMaterialSubstrateTreeItem : public STableRow< FSortedParamDataPtr >, public IDraggableItem
{
public:

	SLATE_BEGIN_ARGS(SMaterialSubstrateTreeItem)
		: _StackParameterData(nullptr),
		_MaterialEditorInstance(nullptr),
		_InTree(nullptr),
		_Padding( FMargin(0) )
	{}

	/** The item content. */
	SLATE_ARGUMENT(FSortedParamDataPtr, StackParameterData)
	SLATE_ARGUMENT(UMaterialEditorParameters*, MaterialEditorInstance)
	SLATE_ARGUMENT(SMaterialSubstrateTree*, InTree)
	SLATE_ATTRIBUTE( FMargin, Padding )
	SLATE_END_ARGS()

	bool bIsBeingDragged = false;


private:
	bool bIsHoveredDragTarget = false;
	
	/** Widget to display the name of the asset item and allows for renaming */
	TSharedPtr<class SInlineEditableTextBlock> InlineRenameWidget;
	
	/** Handles verifying name changes */
	bool VerifyNameChanged(const FText& InName, FText& OutError) const;

	/** Returns false if this folder is in the process of being created */
	bool IsReadOnly() const;
	FString GetCurvePath(class UDEditorScalarParameterValue* Parameter) const;
	const FSlateBrush* GetBorderImage() const;

	FSlateColor GetOuterBackgroundColor(TSharedPtr<FSortedParamData> InParamData) const;
public:

	void RefreshMaterialViews();
	bool GetFilterState(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const;
	void FilterClicked(const ECheckBoxState NewCheckedState, SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData);
	ECheckBoxState GetFilterChecked(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const;
	int32 GetLayerFunctionIndex() const;
	int32 GetBlendFunctionIndex() const;
	FText GetLayerName() const;
	FText GetLayerDesc() const;
	void OnNameChanged(const FText& InText, ETextCommit::Type CommitInfo);
	
	FReply ToggleLayerVisibility();
	bool IsLayerVisible() const;

	FReply UnlinkLayer();
	EVisibility GetUnlinkLayerVisibility() const;

	TOptional<EItemDropZone> CanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSortedParamDataPtr Item);
	FReply OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSortedParamDataPtr TargetItem);
	void OnLayerDragEnter(const FDragDropEvent& DragDropEvent) override
	{
		//if (StackParameterData->ParameterInfo.Index != 0)
		{
			bIsHoveredDragTarget = true;
		}
	}

	void OnLayerDragLeave(const FDragDropEvent& DragDropEvent) override
	{
		bIsHoveredDragTarget = false;
	}

	void OnLayerDragDetected() override
	{
		bIsBeingDragged = true;
	}

	FReply OnLayerDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSortedParamDataPtr TargetItem);
	void OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter);
	void OnOverrideParameter(bool NewValue, TObjectPtr<UDEditorParameterValue> Parameter);

	FReply OnAddMaterialAttributeClicked(int32 ParentIndex);
	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);
	FText GetDisplayName() const;
	int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;
	void Rename();
	/** The node info to build the tree view row from. */
	
	FSortedParamDataPtr StackParameterData;

	SMaterialSubstrateTree* Tree;

	UMaterialEditorParameters* MaterialEditorInstance;
	FSlateBrush* HalfRoundBrush = new FSlateBrush();
	FString GetInstancePath(SMaterialSubstrateTree* InTree) const;
};
