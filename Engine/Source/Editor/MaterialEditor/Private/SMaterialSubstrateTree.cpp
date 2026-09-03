// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMaterialSubstrateTree.h"
#include "SMaterialLayersFunctionsTree.h"

#include "SMaterialLayersFunctionsTree.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorMaterialLayersParameterValue.h"
#include "MaterialEditor/DEditorRuntimeVirtualTextureParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/Material.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Styling/AppStyle.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "IPropertyRowGenerator.h"
#include "Widgets/Views/STreeView.h"
#include "IDetailTreeNode.h"
#include "AssetThumbnail.h"
#include "ContentBrowserModule.h"
#include "MaterialEditorInstanceDetailCustomization.h"
#include "MaterialPropertyHelpers.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorSupportDelegates.h"
#include "Widgets/Images/SImage.h"
#include "MaterialEditor/MaterialEditorPreviewParameters.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"
#include "EditorFontGlyphs.h"
#include "IContentBrowserSingleton.h"
#include "SResetToDefaultPropertyEditor.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "SubstrateMaterialEditorStyle.h"
#include "DetailTreeNode.h"
#include "Framework/Commands/GenericCommands.h"
#include "Materials/MaterialFunctionMaterialLayer.h"
#include "Materials/MaterialFunctionMaterialLayerBlend.h"
#include "MaterialShared.h"
#include "RenderUtils.h"

#define LOCTEXT_NAMESPACE "MaterialSubstrateTree"

// Check if the assetdata is avalid MaterialLayer Function or FunctionInstance
bool IsAssetDataAMaterialLayerFunction(const FAssetData& AssetData)
{
	return AssetData.IsInstanceOf(UMaterialFunctionMaterialLayer::StaticClass()) || AssetData.IsInstanceOf(UMaterialFunctionMaterialLayerInstance::StaticClass());
}

// Check if the assetdata is avalid MaterialLayerBlend Function or FunctionInstance
bool IsAssetDataAMaterialLayerBlendFunction(const FAssetData& AssetData)
{
	return AssetData.IsInstanceOf(UMaterialFunctionMaterialLayerBlend::StaticClass()) || AssetData.IsInstanceOf(UMaterialFunctionMaterialLayerBlendInstance::StaticClass());
}

// Check if the assetdata is avalid MaterialLayerBlend Function or FunctionInstance
bool FilterAssetDataAMaterialLayerBlendFunction(const FAssetData& AssetData)
{
	return !AssetData.IsInstanceOf(UMaterialFunctionMaterialLayerBlend::StaticClass()) && !AssetData.IsInstanceOf(UMaterialFunctionMaterialLayerBlendInstance::StaticClass());
}

/// ===========================================================================================================
/// SMaterialSubstrateItem Methods
/// ===========================================================================================================
FString SMaterialSubstrateTreeItem::GetCurvePath(UDEditorScalarParameterValue* Parameter) const
{
	FString Path = Parameter->AtlasData.Curve->GetPathName();
	return Path;
}

const FSlateBrush* SMaterialSubstrateTreeItem::GetBorderImage() const
{
	return FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle");
}

FSlateColor SMaterialSubstrateTreeItem::GetOuterBackgroundColor(TSharedPtr<FSortedParamData> InParamData) const
{
	if (InParamData->StackDataType == EStackDataType::Stack)
	{
		if (bIsBeingDragged)
		{
			return FAppStyle::Get().GetSlateColor("Colors.Recessed");
		}
		else if (bIsHoveredDragTarget)
		{
			return FAppStyle::Get().GetSlateColor("Colors.Highlight");
		}
		else
		{
			return FAppStyle::Get().GetSlateColor("Colors.Header");
		}
	}
	else if (IsHovered() || InParamData->StackDataType == EStackDataType::Group)
	{
		return FAppStyle::Get().GetSlateColor("Colors.Header");
	}

	return FAppStyle::Get().GetSlateColor("Colors.Panel");
}

void SMaterialSubstrateTreeItem::RefreshMaterialViews()
{
	if (SMaterialLayersFunctionsInstanceWrapper* Wrapper = Tree->GetWrapper())
	{
		Tree->CreateGroupsWidget();
		Tree->RequestTreeRefresh();
		
		if (Wrapper->OnLayerPropertyChanged.IsBound())
		{
			Wrapper->OnLayerPropertyChanged.Execute();
		}
	}
}

bool SMaterialSubstrateTreeItem::GetFilterState(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const
{
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
	{
		return InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index];
	}
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
	{
		return InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index];
	}
	return false;
}

void SMaterialSubstrateTreeItem::FilterClicked(const ECheckBoxState NewCheckedState, SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData)
{
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter)
	{
		InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index] = !InTree->FunctionInstance->EditorOnly.RestrictToLayerRelatives[InStackData->ParameterInfo.Index];
	}
	if (InStackData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter)
	{
		InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index] = !InTree->FunctionInstance->EditorOnly.RestrictToBlendRelatives[InStackData->ParameterInfo.Index];
	}
}

ECheckBoxState SMaterialSubstrateTreeItem::GetFilterChecked(SMaterialSubstrateTree* InTree, TSharedPtr<FSortedParamData> InStackData) const
{
	return GetFilterState(InTree, InStackData) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

int32 SMaterialSubstrateTreeItem::GetLayerFunctionIndex() const
{
	return Tree->GetLayerFunctionIndex(StackParameterData->ParameterInfo.Index);
}

int32 SMaterialSubstrateTreeItem::GetBlendFunctionIndex() const
{
	return Tree->GetBlendFunctionIndex(StackParameterData->ParameterInfo.Index);
}

FReply SMaterialSubstrateTreeItem::ToggleLayerVisibility()
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	return Tree->ToggleLayerVisibility(LayerFuncIndex);
}

bool SMaterialSubstrateTreeItem::IsLayerVisible() const
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	return Tree->IsLayerVisible(LayerFuncIndex);
}

EVisibility SMaterialSubstrateTreeItem::GetUnlinkLayerVisibility() const
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	return Tree->GetUnlinkLayerVisibility(LayerFuncIndex);
}

FText SMaterialSubstrateTreeItem::GetLayerName() const
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	return Tree->FunctionInstance->GetLayerName(LayerFuncIndex);
}

FText SMaterialSubstrateTreeItem::GetLayerDesc() const
{
	const FText LayerDescText[] = { LOCTEXT("Slab", "Slab"), LOCTEXT("Attributes", "Attributes") };
	
	return LayerDescText[GetIndentLevel()];
}

void SMaterialSubstrateTreeItem::OnNameChanged(const FText& InText, ETextCommit::Type CommitInfo)
{
	const FScopedTransaction Transaction(LOCTEXT("RenamedSection", "Renamed layer and blend section"));
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	Tree->FunctionInstanceHandle->NotifyPreChange();
	Tree->FunctionInstance->EditorOnly.LayerNames[LayerFuncIndex] = InText;
	Tree->FunctionInstance->UnlinkLayerFromParent(LayerFuncIndex);
	Tree->MaterialEditorInstance->CopyToSourceInstance(true);
	Tree->FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
}

TOptional<EItemDropZone> SMaterialSubstrateTreeItem::CanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FSortedParamDataPtr Item)
{
	TSharedPtr<FLayerDragDropOp> LayerDragDropOperation = DragDropEvent.GetOperationAs<FLayerDragDropOp>();
	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

	
	// Drop above or below could CREATE a new layer node:
	int32 TargetNodeId = StackParameterData->ParameterInfo.Index;
	int32 ParentNodeId = Tree->FunctionInstance->GetNodeParent(TargetNodeId);
	auto ChildrenNodeId = Tree->FunctionInstance->GetNodeChildren(ParentNodeId);
	int32 SiblingIdx = -1;
	bool FoundTarget = ChildrenNodeId.Find(TargetNodeId, SiblingIdx);
	const int LastSiblingIdx = ChildrenNodeId.Num() - 1;
	
	if (LayerDragDropOperation.IsValid() && LayerDragDropOperation->OwningStack.IsValid())
	{
		TSharedPtr<SMaterialSubstrateTreeItem> LayerBeingDraggedPtr = StaticCastWeakPtr<SMaterialSubstrateTreeItem>(LayerDragDropOperation->OwningStack).Pin();
		int32 SourceNodeId = LayerBeingDraggedPtr->StackParameterData->ParameterInfo.Index;
		
		int32 SourceDepth = Tree->FunctionInstance->GetNodeDepth(SourceNodeId);
		int32 DestDepth = Tree->FunctionInstance->GetNodeDepth(TargetNodeId);
		
		// Allow layers at same depth to be moved
		if (SourceDepth == DestDepth)
		{
			// Allow dropping above items or below last item (sibling index 0)
			if (DropZone == EItemDropZone::AboveItem || ((SiblingIdx == 0) && DropZone == EItemDropZone::BelowItem))
			{
				return DropZone;
			}
		}
		// else if a Mat Attribute layer is dragged and dropped Onto a Mat Eval layer, we allow it
		else if (SourceDepth > DestDepth && DropZone == EItemDropZone::OntoItem)
		{
			return DropZone;
		}
	}
	else if (AssetDragDropOperation.IsValid())
	{

		// Identify the type of asset
		bool HasLayerFuncAsset = false;
		bool HasBlendFuncAsset = false;
		for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
		{
			HasLayerFuncAsset |= IsAssetDataAMaterialLayerFunction(AssetData);
			HasBlendFuncAsset |= IsAssetDataAMaterialLayerBlendFunction(AssetData);
		}

		switch (DropZone)
		{
		case EItemDropZone::AboveItem:
			if (!HasLayerFuncAsset) // Can only add above if drop a new layer function
			{
				return TOptional<EItemDropZone>();
			}
			break;

		case EItemDropZone::BelowItem:
			if (!HasLayerFuncAsset  // Can only add under if drop a new layer function
				|| (SiblingIdx > 0)) // Disable adding below layers for all except the first child
			{
				return TOptional<EItemDropZone>();
			}
			break;
		case EItemDropZone::OntoItem:
			if (!(HasLayerFuncAsset || HasBlendFuncAsset) // Can only drop valid assets
				|| (!HasLayerFuncAsset && HasBlendFuncAsset)) 
			{
				return TOptional<EItemDropZone>();
			}
			break;
		default:
			break;
		}

		return DropZone;
	}
	return TOptional<EItemDropZone>();
}

FReply SMaterialSubstrateTreeItem::OnLayerDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone,	FSortedParamDataPtr TargetItem)
{
	if (!bIsHoveredDragTarget)
	{
		return FReply::Unhandled();
	}
	FScopedTransaction Transaction(NSLOCTEXT("UnrealEd", "MoveLayer", "Move Layer"));
	Tree->FunctionInstanceHandle->NotifyPreChange();
	bIsHoveredDragTarget = false;
	TSharedPtr<FLayerDragDropOp> LayerDragDropOp = DragDropEvent.GetOperationAs< FLayerDragDropOp >();
	TSharedPtr<SMaterialSubstrateTreeItem> LayerPtr = nullptr;
	if (LayerDragDropOp.IsValid() && LayerDragDropOp->OwningStack.IsValid())
	{
		LayerPtr = StaticCastWeakPtr<SMaterialSubstrateTreeItem>(LayerDragDropOp->OwningStack).Pin();
		if (LayerPtr.IsValid())
		{
			LayerPtr->bIsBeingDragged = false;
			TSharedPtr<FSortedParamData> SourcePropertyData = LayerPtr->StackParameterData;
			TSharedPtr<FSortedParamData> DestPropertyData = StackParameterData;
			if (SourcePropertyData.IsValid() && DestPropertyData.IsValid())
			{
				int32 SourceNodeId = SourcePropertyData->ParameterInfo.Index;
				int32 DestNodeId = DestPropertyData->ParameterInfo.Index;
				
				bool bShouldDuplicate = (DragDropEvent.GetModifierKeys().IsShiftDown() || DragDropEvent.GetModifierKeys().IsControlDown());
				
				if (SourceNodeId != DestNodeId || bShouldDuplicate)
				{
					int32 SourceParent = Tree->FunctionInstance->GetNodeParent(SourceNodeId);
					int32 DestParent = Tree->FunctionInstance->GetNodeParent(DestNodeId);

					int32 DestSiblingIdx = -1;
					int32 SrcSiblingIdx = -1;
					auto ChildrenNodeId = Tree->FunctionInstance->GetNodeChildren(DestParent);

					ChildrenNodeId.Find(DestNodeId, DestSiblingIdx);
					ChildrenNodeId.Find(SourceNodeId, SrcSiblingIdx);

					//  Adjust the sibling index if the node is being moved within the same parent
					if (DropZone == EItemDropZone::BelowItem)
					{
						// only allowed to go below first index (or lowest in UI view)
						DestSiblingIdx = 0;
					}
					else if (DropZone == EItemDropZone::AboveItem)
					{
						if (SourceParent == DestParent)
						{
							// If the original index was before the target index, adjust the target index
							if (DestSiblingIdx < SrcSiblingIdx || bShouldDuplicate)
							{
								++DestSiblingIdx;
							}
						}
						else
						// if coming from a different parent (moving attribute from one Mat Eval to another)
						{
							++DestSiblingIdx;
						}
					}
					else if (DropZone == EItemDropZone::OntoItem)
					{
						int32 SourceDepth = Tree->FunctionInstance->GetNodeDepth(SourceNodeId);
						int32 DestDepth = Tree->FunctionInstance->GetNodeDepth(DestNodeId);
						check (SourceDepth > DestDepth)

						// if a material attr layer is dropped on to a mat eval layer, we add it to the end (top) of it's children
						DestSiblingIdx = ChildrenNodeId.Num() - 1;
						DestParent = DestNodeId;
					}
					
					Tree->FunctionInstance->MoveLayerNode(SourceNodeId, DestParent, DestSiblingIdx, bShouldDuplicate);
					Tree->FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
					RefreshMaterialViews();
				}
			}
			return FReply::Handled();
		}
	}
	else
	{
		// see if it is an accepted asset drop
		TSharedPtr<FAssetDragDropOp> AssetDropOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();

		if (AssetDropOp.IsValid())
		{
			// Identify the type of asset
			bool HasLayerFuncAsset = false;
			bool HasBlendFuncAsset = false;
			for (const FAssetData& AssetData : AssetDropOp->GetAssets())
			{
				HasLayerFuncAsset |= IsAssetDataAMaterialLayerFunction(AssetData);
				HasBlendFuncAsset |= IsAssetDataAMaterialLayerBlendFunction(AssetData);
			}

			// Drop above or below could CREATE a new layer node:
			int32 TargetNodeId = StackParameterData->ParameterInfo.Index;
			int32 TargetNodeDepth = Tree->FunctionInstance->GetNodeDepth(TargetNodeId);
			int32 ParentNodeId = Tree->FunctionInstance->GetNodeParent(TargetNodeId);
			auto ChildrenNodeId = Tree->FunctionInstance->GetNodeChildren(ParentNodeId);
			int32 SiblingIdx = -1;
			bool FoundTarget = ChildrenNodeId.Find(TargetNodeId, SiblingIdx);

			bool DidModifyTree = false;

			switch (DropZone)
			{
			// NOTE: The drop cases Above and Below take into account the fact that the list is displayed bottom up!
			case EItemDropZone::AboveItem:
				check(HasLayerFuncAsset); // Only add with a valid new LayerFunc asset
				// if new layer node's parent is root then add a L1 group layer and THEN the L2 first node
				if (ParentNodeId == FMaterialLayersFunctionsTree::InvalidId)
				{
					ParentNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx + 1);
					SiblingIdx = -1; // the attributes will be aded first
				}
				TargetNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx + 1); // Above means insert after
				DidModifyTree = true;
				break;

			case EItemDropZone::BelowItem:
				check(HasLayerFuncAsset); // Only add with a valid new LayerFunc asset
				// if new layer node's parent is root then add a L1 group layer and THEN the L2 first node
				if (ParentNodeId == FMaterialLayersFunctionsTree::InvalidId)
				{
					ParentNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx);
					SiblingIdx = -1; // the attributes will be aded first
				}
				TargetNodeId = Tree->FunctionInstance->AppendLayerNode(ParentNodeId, SiblingIdx); // Under means insert at
				DidModifyTree = true;
				break;

			case EItemDropZone::OntoItem:
				// Dropping LayerFunc asset (with blendfunc too maybe) on a top level layer means a NEW sub layer is created
				if (HasLayerFuncAsset && (TargetNodeDepth <= 1)) // Top level Layers
				{
					// add a new layer in this target node last on the stack
					TargetNodeId = Tree->FunctionInstance->AppendLayerNode(TargetNodeId, -1);
					DidModifyTree = true;
				}
				else
				{
					// Assign the new asset(s) to this particular target node
				}
				break;
			default:
				break;
			}

			// Then drop
			for (const FAssetData& AssetData : AssetDropOp->GetAssets())
			{
				EMaterialParameterAssociation InAssociation = EMaterialParameterAssociation::GlobalParameter;
			
				if (IsAssetDataAMaterialLayerFunction(AssetData))
				{
					InAssociation = EMaterialParameterAssociation::LayerParameter;

					Tree->RefreshOnAssetChange(AssetData, TargetNodeId, InAssociation);
					DidModifyTree = true;
				}
				else if (IsAssetDataAMaterialLayerBlendFunction(AssetData))
				{
					InAssociation = EMaterialParameterAssociation::BlendParameter;
					Tree->RefreshOnAssetChange(AssetData, TargetNodeId, InAssociation);
					DidModifyTree = true;
				}
			}

			if (DidModifyTree)
			{
				RefreshMaterialViews();
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();
}


bool SMaterialSubstrateTree::IsOverriddenExpression(class UDEditorParameterValue* Parameter, int32 InIndex)
{
	return FMaterialPropertyHelpers::IsOverriddenExpression(Parameter) && FunctionInstance->EditorOnly.LayerStates[InIndex];
}

bool SMaterialSubstrateTree::IsOverriddenExpression(TObjectPtr<UDEditorParameterValue> Parameter, int32 InIndex)
{
	return IsOverriddenExpression(Parameter.Get(), InIndex);
}

FGetShowHiddenParameters SMaterialSubstrateTree::GetShowHiddenDelegate() const
{
	return ShowHiddenDelegate;
}

void  SMaterialSubstrateTreeItem::OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter)
{
	FMaterialPropertyHelpers::OnOverrideParameter(NewValue, Parameter, Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance));
}

void  SMaterialSubstrateTreeItem::OnOverrideParameter(bool NewValue, TObjectPtr<UDEditorParameterValue> Parameter)
{
	OnOverrideParameter(NewValue, Parameter.Get());
}

FReply SMaterialSubstrateTreeItem::OnAddMaterialAttributeClicked(int32 index)
{
	Tree->AddNodeLayer(index);
	return FReply::Handled();
}

void SMaterialSubstrateTreeItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	HalfRoundBrush = new FSlateRoundedBoxBrush(FStyleColors::Recessed);
	FOnTableRowDragEnter LayerDragDelegate = FOnTableRowDragEnter::CreateSP(this, &SMaterialSubstrateTreeItem::OnLayerDragEnter);
	FOnTableRowDragLeave LayerDragLeaveDelegate = FOnTableRowDragLeave::CreateSP(this, &SMaterialSubstrateTreeItem::OnLayerDragLeave);

	StackParameterData = InArgs._StackParameterData;
	MaterialEditorInstance = InArgs._MaterialEditorInstance;
	Tree = InArgs._InTree;
	int32 NodeId = StackParameterData->ParameterInfo.Index;
	int32 Depth = Tree->FunctionInstance->GetNodeDepth(NodeId);
	FName StyleName = FName(FString::Format(TEXT("LayerView.Row{0}"), {Depth}));
	STableRow< TSharedPtr<FSortedParamData> >::ConstructInternal(
		STableRow< TSharedPtr<FSortedParamData> >::FArguments()
		.Style(FSubstrateMaterialEditorStyle::Get(), StyleName)
		.OnCanAcceptDrop(this, &SMaterialSubstrateTreeItem::CanAcceptDrop)
		.OnAcceptDrop(this, &SMaterialSubstrateTreeItem::OnLayerDrop)
		.OnDragEnter(LayerDragDelegate)
		.OnDragLeave(LayerDragLeaveDelegate),
		InOwnerTableView
	);

	TSharedRef<SWidget> LeftSideWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> RightSideWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> ResetWidget = SNullWidget::NullWidget;
	FText NameOverride;
	
	TSharedPtr<SHorizontalBox> MainStack;
	TSharedRef<SVerticalBox> WrapperWidget = SNew(SVerticalBox);

	if (StackParameterData->StackDataType == EStackDataType::Stack)
	{
		WrapperWidget->AddSlot()
		.Padding(2.0f)
		[
			SAssignNew(MainStack, SHorizontalBox)
		];
	}
		
	EHorizontalAlignment ValueAlignment = HAlign_Left;

	bool bCanAppendSubLayer = Tree->FunctionInstance->CanAppendLayerNode(StackParameterData->ParameterInfo.Index);
	bool bIsSlabWithNoAttributes = bCanAppendSubLayer && Tree->FunctionInstance->GetNodeChildren(StackParameterData->ParameterInfo.Index).IsEmpty();

// STACK --------------------------------------------------
	if (StackParameterData->StackDataType == EStackDataType::Stack)
	{
#if WITH_EDITOR
		int32 LayerFuncIndex = GetLayerFunctionIndex();
		NameOverride = Tree->FunctionInstance->GetLayerName(LayerFuncIndex);
#endif
		TSharedRef<SHorizontalBox> HeaderRowWidget = SNew(SHorizontalBox);

		if (StackParameterData->ParameterInfo.Index != 0)
		{
			TAttribute<bool>::FGetter IsEnabledGetter = TAttribute<bool>::FGetter::CreateSP(this, &SMaterialSubstrateTreeItem::IsLayerVisible);
			TAttribute<bool> IsEnabledAttribute = TAttribute<bool>::Create(IsEnabledGetter);

			FOnClicked VisibilityClickedDelegate = FOnClicked::CreateSP(this, &SMaterialSubstrateTreeItem::ToggleLayerVisibility);

			HeaderRowWidget->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeVisibilityButton(VisibilityClickedDelegate, FText(), IsEnabledAttribute)
				];
		}
		const float ThumbnailSize = 40.0f;
		TArray<TSharedPtr<FSortedParamData>> AssetChildren = StackParameterData->Children;
		// Extract the asset elements to represent them as thumbnail boxes
		for (TSharedPtr<FSortedParamData> AssetChild : AssetChildren)
		{
			if (AssetChild->StackDataType == EStackDataType::Asset)
			{
				TSharedPtr<SBox> ThumbnailBox;
				UObject* AssetObject = nullptr;
				AssetChild->ParameterHandle->GetValue(AssetObject);
				int32 PreviewIndex = INDEX_NONE;
				int32 ThumbnailIndex = INDEX_NONE;
				EMaterialParameterAssociation PreviewAssociation = EMaterialParameterAssociation::GlobalParameter;
				
				if (AssetChild->ParameterInfo.Association == LayerParameter)
				{
					PreviewIndex = LayerFuncIndex;
					PreviewAssociation = EMaterialParameterAssociation::LayerParameter;
					Tree->UpdateThumbnailMaterial(PreviewAssociation, PreviewIndex);
					ThumbnailIndex = PreviewIndex;
				
					HeaderRowWidget->AddSlot()
						.AutoWidth()
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Padding(4.0f)
						.MaxWidth(ThumbnailSize)
						[
							SAssignNew(ThumbnailBox, SBox)
							.MaxDesiredWidth(ThumbnailSize)
							.MinDesiredWidth(ThumbnailSize)
							.MaxDesiredHeight(ThumbnailSize)
							.MinDesiredHeight(ThumbnailSize)
								[
									Tree->CreateThumbnailWidget(PreviewAssociation, ThumbnailIndex, ThumbnailSize)
								]
						];
				}
				// if blend asset, we set it up in the Wrapper Widget at the bottom of the VerticalBox
                else if (AssetChild->ParameterInfo.Association == BlendParameter)
                {
                	// only show separator for attributes
                	EVisibility SeparatorVisibility = (Depth > 1) ? EVisibility::Visible : EVisibility::Hidden;
                	WrapperWidget->AddSlot()
					.AutoHeight()
					[
					   SNew(SSeparator)
						.Visibility(SeparatorVisibility)
					   .Thickness(2.0f) // Set the thickness of the separator
					];
                	IDetailTreeNode& Node = *AssetChild->ParameterNode;
                	TSharedPtr<IDetailPropertyRow> GeneratedRow = StaticCastSharedPtr<IDetailPropertyRow>(Node.GetRow());
                	IDetailPropertyRow& Row = *GeneratedRow.Get();
                	
                	TSharedRef<SWidget> AssetPickerWidget = SNew(SObjectPropertyEntryBox)
                		.ObjectPath_Lambda([=, this]()
                		{
                			UObject* AssetObject = nullptr;
                			AssetChild->ParameterHandle->GetValue(AssetObject);
                			return AssetObject->GetPathName();
                		})
                		.OnObjectChanged_Lambda([=, this](const FAssetData& InAssetData)
                		{
							FSoftObjectPath ObjPath = InAssetData.GetSoftObjectPath();
							AssetChild->ParameterHandle->SetValue(ObjPath.TryLoad());

							Tree->FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
                			RefreshMaterialViews();
                		})
						.OnShouldFilterAsset_Static(&FilterAssetDataAMaterialLayerBlendFunction)
                		.AllowClear(true)
                		.DisplayUseSelected(false)
                		.DisplayBrowse(false);
                	
                	 WrapperWidget->AddSlot()
                		.Padding(8.0f)
                		.AutoHeight()
                	 [
                		SNew(SHorizontalBox)
                		// + SHorizontalBox::Slot()
                		// .AutoWidth()
                		// .VAlign(VAlign_Center)
                		// [
                		// 	PropertyCustomizationHelpers::MakeVisibilityButton(VisibilityClickedDelegate, FText(), IsEnabledAttribute)
                		// ]
                		
                		+ SHorizontalBox::Slot()
                		.Padding(8.0f, 0.f, 0.0f, 0.0f)
	                	.AutoWidth()
                		.VAlign(VAlign_Center)
                		[
                			SNew(STextBlock)
                			.Text(LOCTEXT("BlendLabel", "Blend"))
                		]
                		+ SHorizontalBox::Slot()
                		.Padding(16.0f, 0.f, 0.0f, 0.0f)
	                	.FillWidth(1.0f)
                		[
                			AssetPickerWidget
                		]
                	 ];
                }
			}
		}

		{
			HeaderRowWidget->AddSlot()
				.AutoWidth()
				.Padding(8.0f, 2.0f, 0.0f, 0.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
							.Text(this, &SMaterialSubstrateTreeItem::GetDisplayName)
							.OnTextCommitted(this, &SMaterialSubstrateTreeItem::OnNameChanged)
					]

					+ SVerticalBox::Slot()
					.Padding(0, 4, 0, 0)
					[
						SNew(STextBlock)
						.Text(TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &SMaterialSubstrateTreeItem::GetLayerDesc)))
						.TextStyle(FSubstrateMaterialEditorStyle::Get(), "LayerView.Row.HeaderText.Small")
					]
				
				];
		}
		LeftSideWidget = HeaderRowWidget;
	}
// END STACK


// FINAL WRAPPER
	if (StackParameterData->StackDataType == EStackDataType::Stack)
	{
		MainStack->AddSlot()
			.Padding(FMargin(2.0f))
			.VAlign(VAlign_Center)
			[
				LeftSideWidget
			];

		// Add button
		if (bCanAppendSubLayer)
		{
			MainStack->AddSlot()
				.VAlign(VAlign_Center)
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				[
					SNew(SButton)
						.ContentPadding(FMargin(1, 0))
						.ButtonStyle(FAppStyle::Get(), "SimpleButton")
						.ToolTipText(LOCTEXT("AddMaterialAttribute", "Add Material Attribute"))
						.OnClicked(this, &SMaterialSubstrateTreeItem::OnAddMaterialAttributeClicked, StackParameterData->ParameterInfo.Index)
						[
							SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Plus"))
								.ColorAndOpacity(FStyleColors::AccentGreen)
						]

				];
		}

		MainStack->AddSlot()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(2.5f, 0)
			.AutoWidth()
			[
				FMaterialPropertyHelpers::MakeStackReorderHandle(SharedThis(this))
			];


		if (bCanAppendSubLayer && !bIsSlabWithNoAttributes)
		{
			MainStack->AddSlot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FMargin(2.0f))
				[
					SNew(SExpanderArrow, SharedThis(this))
				];
		}

		if (bIsSlabWithNoAttributes)
		{
			WrapperWidget->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				.Padding(8)
				[
					SNew(SBorder)
						.Padding(8)
						[
							SNew(STextBlock)
								.Text(LOCTEXT("AddAttributes", "Add Attributes"))
								.TextStyle(FSubstrateMaterialEditorStyle::Get(), "LayerView.Row.SlabWithoutAttributes")
						]
				];
		}
	}

	this->ChildSlot
		[
			WrapperWidget
		];
	
	this->SetDesiredSizeScale(FVector2d(1.0f, 1.2f));
}
FText SMaterialSubstrateTreeItem::GetDisplayName() const
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	return Tree->FunctionInstance->GetLayerName(LayerFuncIndex);
}
int32 SMaterialSubstrateTreeItem::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	int32 TargetNodeId = StackParameterData->ParameterInfo.Index;
	int32 ParentNodeId = Tree->FunctionInstance->GetNodeParent(TargetNodeId);
	auto ChildrenNodeIds = Tree->FunctionInstance->GetNodeChildren(ParentNodeId);
	int32 SiblingIdx = -1;
	ChildrenNodeIds.Find(TargetNodeId, SiblingIdx);
	bool bFirstChild = SiblingIdx == 0; 

	const FSlateBrush* BackgroundBrushResource = Tree->BackgroundBrush.Get();
	const int32 IndentLevel = GetIndentLevel();
	
	FVector2d OuterBorderSize = AllottedGeometry.GetLocalSize();
	float OffsetX = 20.0f;
	FVector2D Offset = FVector2D(OffsetX * (IndentLevel+1), bFirstChild ? 5.0f : 0.0f);

	const FSlateBrush* RoundedBoxBrush = FSubstrateMaterialEditorStyle::GetBrush("LayerView.Row.OuterRoundBrush");
	if (IndentLevel > 0)
	{
		FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(OuterBorderSize - Offset, FSlateLayoutTransform(Offset/2)),
				RoundedBoxBrush,
				ESlateDrawEffect::None,
				FStyleColors::Recessed.GetSpecifiedColor() * InWidgetStyle.GetColorAndOpacityTint() 
		);
		// we also need to draw to fill in the rounded edges of the items in between (other than last or first) 
		// unless they're the first or last items in the list which would have a rounded top and a rounded bottom respectively
		if (!bFirstChild)
		{
			FVector2D NonRoundBorderOffset = (OuterBorderSize - Offset) * FVector2D(1, 0.25f);
			FVector2D TranslationOffset = Offset/2 + FVector2D(0, OuterBorderSize.Y - 15.0f);
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId+1,
				AllottedGeometry.ToPaintGeometry(NonRoundBorderOffset, FSlateLayoutTransform(TranslationOffset)),
				BackgroundBrushResource,
				ESlateDrawEffect::None,
				FStyleColors::Recessed.GetSpecifiedColor() * InWidgetStyle.GetColorAndOpacityTint() 
			);
		}
	}

	const FSlateBrush* BrushToUse = BackgroundBrushResource;
	FVector2D ExpandedOffset = FVector2d(0.0f, 0.0f);
	FLinearColor ColorToUse = FStyleColors::Dropdown.GetSpecifiedColor();
	Offset = FVector2D(16.0f, 0.0f);
	if (IndentLevel == 0)
	{
		Offset = FVector2D(16.0f, 20.0f);
		ExpandedOffset = FVector2d(0.0f, 10.0f);
		BrushToUse = BackgroundBrushResource;
	}
	else if (bFirstChild)
	{
		Offset = FVector2D(16.0f, 0.0f);
		ExpandedOffset = FVector2d(0.0f, 0.0f);
		HalfRoundBrush->OutlineSettings.CornerRadii = FVector4(0.0f, 0.0f, 15.0f, 15.0f);
		HalfRoundBrush->OutlineSettings.RoundingType = ESlateBrushRoundingType::Type::FixedRadius;
		HalfRoundBrush->DrawAs = ESlateBrushDrawType::RoundedBox;
		BrushToUse = HalfRoundBrush;
	}
	// only need to draw gray part for children or if the parent (Evaluation layer) is expanded
	if (IndentLevel > 0 || IsItemExpanded())
	{
		FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId,
				AllottedGeometry.ToPaintGeometry(OuterBorderSize - Offset, FSlateLayoutTransform(Offset/2 + ExpandedOffset)),
				BrushToUse,
				ESlateDrawEffect::None,
				ColorToUse * InWidgetStyle.GetColorAndOpacityTint() 
		);
	}
		
	const float AddAttributeBoxHeight = 36;
	const float AddAttributeBoxPadding = 8;
	float NoChildrenBoxHeight = 0.0f;
	if (IndentLevel == 0 && ChildrenNodeIds.IsEmpty())
	{
		NoChildrenBoxHeight = AddAttributeBoxHeight + AddAttributeBoxPadding * 2;
	}
	OffsetX = 40.0f;
	const float ReductionFactorX = 15.0f;
	const float ReductionFactorY = 10.0f;
	Offset = FVector2d((OffsetX * IndentLevel) + ReductionFactorX, ReductionFactorY * (2 - IndentLevel) - NoChildrenBoxHeight);
 
	FGeometry BorderGeom = AllottedGeometry.MakeChild(AllottedGeometry.GetLocalSize() - Offset, FSlateLayoutTransform(Offset/2));
	return STableRow<TSharedPtr<FSortedParamData>>::OnPaint(Args, BorderGeom, MyCullingRect, OutDrawElements, LayerId+2, InWidgetStyle, bParentEnabled);
}

void SMaterialSubstrateTreeItem::Rename()
{
	if (InlineRenameWidget)
	{
		InlineRenameWidget->EnterEditingMode();
	}
}


FString SMaterialSubstrateTreeItem::GetInstancePath(SMaterialSubstrateTree* InTree) const
{
	int32 LayerFuncIndex = GetLayerFunctionIndex();
	int32 BlendFuncIndex = GetBlendFunctionIndex();

	FString InstancePath;
	if (StackParameterData->ParameterInfo.Association == EMaterialParameterAssociation::BlendParameter && InTree->FunctionInstance->Blends.IsValidIndex(BlendFuncIndex))
	{
		InstancePath = InTree->FunctionInstance->Blends[BlendFuncIndex]->GetPathName();
	}
	else if (StackParameterData->ParameterInfo.Association == EMaterialParameterAssociation::LayerParameter && InTree->FunctionInstance->Layers.IsValidIndex(LayerFuncIndex))
	{
		InstancePath = InTree->FunctionInstance->Layers[LayerFuncIndex]->GetPathName();
	}
	return InstancePath;
}

TSharedPtr<SWidget> SMaterialSubstrateTree::CreateContextMenu()
{
	FMenuBuilder MenuBuilder(true, CommandList);

	TArray<FSortedParamDataPtr> SelectedItemsArray = GetSelectedItems();
	if (SelectedItemsArray.Num() > 0)
	{
		FSortedParamDataPtr StackParameterData = SelectedItemsArray[0];
		bool bCanUnlinkLayer = GetUnlinkLayerVisibility(StackParameterData->ParameterInfo.Index) == EVisibility::Visible;
		bool bCanAppendSubLayer = FunctionInstance->CanAppendLayerNode(StackParameterData->ParameterInfo.Index);
		if (bCanAppendSubLayer)
		{
			const FSlateIcon PlusIcon(FAppStyle::GetAppStyleSetName(), "Plus");
			MenuBuilder.AddMenuEntry(LOCTEXT("AddNewLayer", "Add New Layer"), FText(), PlusIcon, FUIAction(FExecuteAction::CreateSP(this, &SMaterialSubstrateTree::AddNodeLayer, StackParameterData->ParameterInfo.Index)));
		}
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete);
		MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename);
		
		if (bCanUnlinkLayer)
		{
			int32 LayerFuncIndex;
			LayerFuncIndex = GetLayerFunctionIndex(StackParameterData->ParameterInfo.Index);
			MenuBuilder.AddMenuEntry(LOCTEXT("UnlinkLayer", "Unlink Layer"),
			                         LOCTEXT("UnlinkLayerTooltip", "Whether or not to unlink this layer/blend combination from the parent."),
			                         FSlateIcon(),
			                         FUIAction(FExecuteAction::CreateSP(this, &SMaterialSubstrateTree::UnlinkLayer, LayerFuncIndex)));
		}
	}
	return MenuBuilder.MakeWidget();
}

void SMaterialSubstrateTree::CreateCommandList()
{
	CommandList = MakeShareable(new FUICommandList);
	CommandList->MapAction(FGenericCommands::Get().Delete,
		FUIAction(
			FExecuteAction::CreateSP(this, &SMaterialSubstrateTree::OnDeleteSelectedTreeViewItems),
			FCanExecuteAction::CreateSP(this, &SMaterialSubstrateTree::CanDeleteSelectedTreeViewItems)
		)
	);
	CommandList->MapAction(FGenericCommands::Get().Rename,
		FUIAction(
			FExecuteAction::CreateSP(this, &SMaterialSubstrateTree::OnRenameSelectedTreeViewItems),
			FCanExecuteAction::CreateSP(this, &SMaterialSubstrateTree::CanRenameSelectedTreeViewItem)
		)
	);
}


FReply SMaterialSubstrateTree::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMaterialSubstrateTree::Construct(const FArguments& InArgs)
{
	ColumnSizeData.SetValueColumnWidth(0.5f);

	MaterialEditorInstance = InArgs._InMaterialEditorInstance;
	Generator = InArgs._InGenerator;
	Wrapper = InArgs._InWrapper;
	ShowHiddenDelegate = InArgs._InShowHiddenDelegate;
	CreateGroupsWidget();
	CreateCommandList();
#if WITH_EDITOR
	//Fixup for adding new bool arrays to the class
	if (FunctionInstance)
	{
		if (FunctionInstance->Layers.Num() != FunctionInstance->EditorOnly.RestrictToLayerRelatives.Num())
		{
			int32 OriginalSize = FunctionInstance->EditorOnly.RestrictToLayerRelatives.Num();
			for (int32 LayerIt = 0; LayerIt < FunctionInstance->Layers.Num() - OriginalSize; LayerIt++)
			{
				FunctionInstance->EditorOnly.RestrictToLayerRelatives.Add(false);
			}
		}
		if (FunctionInstance->Blends.Num() != FunctionInstance->EditorOnly.RestrictToBlendRelatives.Num())
		{
			int32 OriginalSize = FunctionInstance->EditorOnly.RestrictToBlendRelatives.Num();
			for (int32 BlendIt = 0; BlendIt < FunctionInstance->Blends.Num() - OriginalSize; BlendIt++)
			{
				FunctionInstance->EditorOnly.RestrictToBlendRelatives.Add(false);
			}
		}
	}
#endif

	STreeView<TSharedPtr<FSortedParamData>>::Construct(
		STreeView::FArguments()
		.TreeItemsSource(&LayerProperties)
		.OnContextMenuOpening(this, &SMaterialSubstrateTree::CreateContextMenu)
		.SelectionMode(ESelectionMode::Single)
		.OnSelectionChanged(this, &SMaterialSubstrateTree::OnSelectionChangedMaterialSubstrateView)
		.OnGenerateRow(this, &SMaterialSubstrateTree::OnGenerateRowMaterialLayersFunctionsTreeView)
		.OnGetChildren(this, &SMaterialSubstrateTree::OnGetChildrenMaterialLayersFunctionsTreeView)
		.OnExpansionChanged(this, &SMaterialSubstrateTree::OnExpansionChanged)
	);

	SetParentsExpansionState();
}
void SMaterialSubstrateTree::OnSelectionChangedMaterialSubstrateView(TSharedPtr<FSortedParamData> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	if (MaterialEditorInstance->IsA<UMaterialEditorInstanceConstant>())
	{
		UMaterialEditorInstanceConstant* MaterialEditorInstanceConstant = Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance);
		if (TSharedPtr<IDetailsView> DetailsViewPinned = MaterialEditorInstanceConstant->DetailsView.Pin())
		{
			DetailsViewPinned->ForceRefresh();
		}
	}
}
TSharedRef< ITableRow > SMaterialSubstrateTree::OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SMaterialSubstrateTreeItem > ReturnRow = SNew(SMaterialSubstrateTreeItem, OwnerTable)
		.StackParameterData(Item)
		.MaterialEditorInstance(MaterialEditorInstance)
		.InTree(this);
	return ReturnRow;
}

void SMaterialSubstrateTree::OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> InParent, TArray< TSharedPtr<FSortedParamData> >& OutChildren)
{
	OutChildren = InParent->Children;
}


void SMaterialSubstrateTree::OnExpansionChanged(TSharedPtr<FSortedParamData> Item, bool bIsExpanded)
{
	UMaterialInterface* MaterialInterface = MaterialEditorInstance->GetMaterialInterface();
	bool* ExpansionValue = MaterialInterface->LayerParameterExpansion.Find(Item->NodeKey);
	if (ExpansionValue == nullptr)
	{
		MaterialInterface->LayerParameterExpansion.Add(Item->NodeKey, bIsExpanded);
	}
	else if (*ExpansionValue != bIsExpanded)
	{
		MaterialInterface->LayerParameterExpansion.Emplace(Item->NodeKey, bIsExpanded);
	}
	// Expand any children that are also expanded
	for (auto Child : Item->Children)
	{
		bool* ChildExpansionValue = MaterialInterface->LayerParameterExpansion.Find(Child->NodeKey);
		if (ChildExpansionValue != nullptr && *ChildExpansionValue == true)
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SMaterialSubstrateTree::SetParentsExpansionState()
{
	UMaterialInterface* MaterialInterface = MaterialEditorInstance->GetMaterialInterface();
	
	for (const auto& Pair : LayerProperties)
	{
		if (Pair->Children.Num())
		{
			bool* bIsExpanded = MaterialInterface->LayerParameterExpansion.Find(Pair->NodeKey);
			if (bIsExpanded)
			{
				SetItemExpansion(Pair, *bIsExpanded);
			}
		}
	}
}

int32 SMaterialSubstrateTree::GetLayerFunctionIndex(int32 NodeIndex) const
{
	return FunctionInstance ? FunctionInstance->GetLayerFuncIndex(NodeIndex) : -1;
}

int32 SMaterialSubstrateTree::GetBlendFunctionIndex(int32 NodeIndex) const
{
	return FunctionInstance ? FunctionInstance->GetBlendFuncIndex(NodeIndex) : -1;
}

void SMaterialSubstrateTree::RefreshOnAssetChange(const struct FAssetData& InAssetData, int32 InNodeId, EMaterialParameterAssociation MaterialType)
{
	auto NodePayload = FunctionInstance->GetNodePayload(InNodeId);

	int32 Index = (MaterialType == EMaterialParameterAssociation::BlendParameter ? NodePayload.Blend : NodePayload.Layer);
	// Early exit no op if the index for the asset modified is not valid
	if (Index < 0)
		return;

	FMaterialPropertyHelpers::OnMaterialLayerAssetChanged(InAssetData, Index, MaterialType, FunctionInstanceHandle, FunctionInstance);
	
	//set their overrides back to 0
	MaterialEditorInstance->CleanParameterStack(Index, MaterialType);
	MaterialEditorInstance->ResetOverrides(Index, MaterialType);
}

void SMaterialSubstrateTree::ResetAssetToDefault(TSharedPtr<FSortedParamData> InData)
{
	if (MaterialEditorInstance->IsA<UMaterialEditorInstanceConstant>())
	{
		FMaterialPropertyHelpers::ResetLayerAssetToDefault(InData->Parameter, InData->ParameterInfo.Association, InData->ParameterInfo.Index, Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance));
		UpdateThumbnailMaterial(InData->ParameterInfo.Association, InData->ParameterInfo.Index, false);
		CreateGroupsWidget();
		RequestTreeRefresh();
	}
}

void SMaterialSubstrateTree::AddNodeLayer(int32 InParent)
{
	/// Only if can really add a sub layer!
	if (!FunctionInstance->CanAppendLayerNode(InParent))
		return;

	const FScopedTransaction Transaction(LOCTEXT("AddLayer", "Add a new Layer in the tree"));
	FunctionInstanceHandle->NotifyPreChange();

	// Create a new node
	FunctionInstance->AppendLayerNode(InParent);
	
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	CreateGroupsWidget();
	RequestTreeRefresh();
}

void SMaterialSubstrateTree::RemoveNodeLayer(int32 InNodeId)
{
	/// Only if can really remove a sub layer!
	if (!FunctionInstance->CanRemoveLayerNode(InNodeId))
		return;

	const FScopedTransaction Transaction(LOCTEXT("RemoveLayerAndBlend", "Remove a Layer and the attached Blend"));
	FunctionInstanceHandle->NotifyPreChange();
	
	// Remove the node
	auto NodePayload = FunctionInstance->GetNodePayload(InNodeId);

	FunctionInstance->RemoveLayerNodeAt(InNodeId);
	if (MaterialEditorInstance->IsA<UMaterialEditorInstanceConstant>())
	{
		UMaterialEditorInstanceConstant* MaterialEditorInstanceConstant = Cast<UMaterialEditorInstanceConstant>(MaterialEditorInstance);
		if (MaterialEditorInstanceConstant && MaterialEditorInstanceConstant->SourceInstance)
		{
			MaterialEditorInstanceConstant->SourceInstance->RemoveLayerParameterIndex(NodePayload.Layer);
		}
	}
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ArrayRemove);
	CreateGroupsWidget();
	RequestTreeRefresh();
}

void SMaterialSubstrateTree::UnlinkLayer(int32 Index)
{
	const FScopedTransaction Transaction(LOCTEXT("UnlinkLayerFromParent", "Unlink a layer from the parent"));
	FunctionInstanceHandle->NotifyPreChange();
	FunctionInstance->UnlinkLayerFromParent(Index);
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	CreateGroupsWidget();
	Wrapper->Refresh();
}

FReply SMaterialSubstrateTree::RelinkLayersToParent()
{
	const FScopedTransaction Transaction(LOCTEXT("RelinkLayersToParent", "Relink layers to parent"));
	FunctionInstanceHandle->NotifyPreChange();
	FunctionInstance->RelinkLayersToParent();
	FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	MaterialEditorInstance->RegenerateArrays();
	CreateGroupsWidget();
	Wrapper->Refresh();
	return FReply::Handled();
}

EVisibility SMaterialSubstrateTree::GetUnlinkLayerVisibility(int32 Index) const
{
	if (FunctionInstance->IsLayerLinkedToParent(Index) && MaterialEditorInstance->IsA<UMaterialEditorInstanceConstant>())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

EVisibility SMaterialSubstrateTree::GetRelinkLayersToParentVisibility() const
{
	if (FunctionInstance->HasAnyUnlinkedLayers())
	{
		return EVisibility::Visible;
	}
	return EVisibility::Collapsed;
}

FReply SMaterialSubstrateTree::ToggleLayerVisibility(int32 Index)
{
	if (!FSlateApplication::Get().GetModifierKeys().AreModifersDown(EModifierKey::Alt))
	{
		bLayerIsolated = false;
		const FScopedTransaction Transaction(LOCTEXT("ToggleLayerAndBlendVisibility", "Toggles visibility for a blended layer"));
		FunctionInstanceHandle->NotifyPreChange();
		FunctionInstance->ToggleBlendedLayerVisibility(Index);
		FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		CreateGroupsWidget();
		return FReply::Handled();
	}
	else
	{
		const FScopedTransaction Transaction(LOCTEXT("ToggleLayerAndBlendVisibility", "Toggles visibility for a blended layer"));
		FunctionInstanceHandle->NotifyPreChange();
		if (FunctionInstance->GetLayerVisibility(Index) == false)
		{
			// Reset if clicking on a disabled layer
			FunctionInstance->SetBlendedLayerVisibility(Index, true);
			bLayerIsolated = false;
		}
		for (int32 LayerIt = 1; LayerIt < FunctionInstance->EditorOnly.LayerStates.Num(); LayerIt++)
		{
			if (LayerIt != Index)
			{
				FunctionInstance->SetBlendedLayerVisibility(LayerIt, bLayerIsolated);
			}
		}

		bLayerIsolated = !bLayerIsolated;
		FunctionInstanceHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		CreateGroupsWidget();
		return FReply::Handled();
	}

}

TSharedPtr<class FAssetThumbnailPool> SMaterialSubstrateTree::GetTreeThumbnailPool()
{
	return UThumbnailManager::Get().GetSharedThumbnailPool();
}

TSharedPtr<IDetailTreeNode> SMaterialSubstrateTree::FindParameterGroupsNode(TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator)
{
	const TArray<TSharedRef<IDetailTreeNode>> RootNodes = PropertyRowGenerator->GetRootTreeNodes();
	if (RootNodes.Num() > 0)
	{
		TSharedPtr<IDetailTreeNode> Category = RootNodes[0];
		TArray<TSharedRef<IDetailTreeNode>> Children;
		Category->GetChildren(Children);

		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ChildIdx++)
		{
			TSharedPtr<IPropertyHandle> PropertyHandle = Children[ChildIdx]->CreatePropertyHandle();
			if (PropertyHandle.IsValid() && PropertyHandle->GetProperty() && PropertyHandle->GetProperty()->GetName() == "ParameterGroups")
			{
				return Children[ChildIdx];
			}
		}
	}
	return nullptr;
}

struct FRecursiveCreateWidgetsContext
{
	UDEditorParameterValue* Parameter;
	TSharedPtr<IPropertyHandle>	LayerHandle;
	TSharedPtr<IPropertyHandle>	BlendHandle;
	TArray<FUnsortedParamData> NonLayerProperties;
};

void SMaterialSubstrateTree::RecursiveCreateWidgets(FRecursiveCreateWidgetsContext* InContext, FNodeId InNodeId, TArray<TSharedPtr<FSortedParamData>>& InParentContainer, bool GenerateChildren)
{
	auto Payload = FunctionInstance->Tree.Payloads[InNodeId];

	TSharedRef<FSortedParamData> StackProperty = MakeShared<FSortedParamData>();
	StackProperty->StackDataType = EStackDataType::Stack;
	StackProperty->Parameter = InContext->Parameter;
	StackProperty->ParameterInfo.Index = InNodeId;
	StackProperty->NodeKey = FString::FromInt(StackProperty->ParameterInfo.Index);

	if (GenerateChildren)
	{
		// Sub layers
		auto RootChildren = FunctionInstance->GetNodeChildren(InNodeId);
		for (int i = 0; i < RootChildren.Num(); ++i)
		{
			int Index = i;
			Index = RootChildren.Num() - 1 - i; // Reverse the order to display the layers bottom up
			RecursiveCreateWidgets(InContext, RootChildren[Index], StackProperty->Children, false);
		}
	}

	if (Payload.Layer != -1)
	{
		TSharedRef<FSortedParamData> ChildProperty = MakeShared<FSortedParamData>();
		ChildProperty->StackDataType = EStackDataType::Asset;
		ChildProperty->Parameter = InContext->Parameter;
		ChildProperty->ParameterHandle = InContext->LayerHandle->AsArray()->GetElement(Payload.Layer);
		ChildProperty->ParameterNode = Generator->FindTreeNode(ChildProperty->ParameterHandle);
		ChildProperty->ParameterInfo.Index = Payload.Layer;
		ChildProperty->ParameterInfo.Association = EMaterialParameterAssociation::LayerParameter;
		ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association);

		{
			UObject* AssetObject = nullptr;
			ChildProperty->ParameterHandle->GetValue(AssetObject);
			if(Substrate::IsMaterialLayeringSupportEnabled())
			{
				if (AssetObject)
				{
					UMaterialInterface* MaterialInterface = MaterialEditorInstance->GetMaterialInterface();
					FMaterialLayersFunctions LayersFunctions;
					if (MaterialInterface && MaterialInterface->GetMaterialLayers(LayersFunctions))
					{
						MaterialInterface->SyncLayersRuntimeGraphCache(&LayersFunctions);
						if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] == nullptr)
						{
							MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] = NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, FName(*FString::Printf(TEXT("Layer_%d_%s"), Payload.Layer, *AssetObject->GetName())));
						}
						UMaterialInterface* EditedMaterial = Cast<UMaterialFunctionInterface>(AssetObject)->GetPreviewMaterial();
						if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] && MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->Parent != EditedMaterial)
						{
							MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->SetParentEditorOnly(EditedMaterial);
						}

						TSharedPtr<FMaterialLayersFunctionsRuntimeGraphCache> LayerTreeCache = LayersFunctions.RuntimeGraphCache;
						if (LayerTreeCache)
						{
							if(FMaterialResource* MaterialResource = MaterialInterface->GetMaterialResource(ERHIFeatureLevel::SM6))
							{
								const FMaterialLayersFunctions* LayersFunctions_Original = MaterialResource->GetMaterialLayers();
								TSharedPtr<FMaterialLayersFunctionsRuntimeGraphCache> OriginalTreeCache = LayersFunctions_Original ? LayersFunctions_Original->RuntimeGraphCache : nullptr;
								if (OriginalTreeCache)
								{
									LayerTreeCache->NodePreviewMaterials = OriginalTreeCache->NodePreviewMaterials;									
								}
								MaterialResource->FeedbackMaterialLayersInstancedGraphFromCompilation(&LayersFunctions);
							}
							LayerTreeCache->NodePreviewMaterials[InNodeId] = (UMaterial*)EditedMaterial;
						}						
						MaterialInterface->SyncLayersRuntimeGraphCache(nullptr);
					}
				}
				else if(UMaterialInterface* MaterialInterface = MaterialEditorInstance->GetMaterialInterface())
				{
					FMaterialLayersFunctions LayersFunctions;
					if(MaterialInterface->GetMaterialLayers(LayersFunctions))
					{
						MaterialInterface->SyncLayersRuntimeGraphCache(&LayersFunctions);
						if (TSharedPtr<FMaterialLayersFunctionsRuntimeGraphCache> LayerTreeCache = LayersFunctions.RuntimeGraphCache)
						{
							UMaterialExpressionMaterialFunctionCall* Call = LayerTreeCache->NodeMaterialGraphExpressions[InNodeId];
							if (Call && Call->MaterialFunction)
							{
								if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] == nullptr)
								{
									MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] = NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, FName(*FString::Printf(TEXT("Layer_%d_%s"), Payload.Layer, *Call->MaterialFunction->GetName())));
								}

								UMaterial* PreviewMaterial = LayersFunctions.GetRuntimeNodePreviewMaterial(InNodeId);

								if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] && MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->Parent != PreviewMaterial)
								{
									MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->SetParentEditorOnly(PreviewMaterial);
								}
							}
						}
						MaterialInterface->SyncLayersRuntimeGraphCache(nullptr);
					}
				}
			}
			else if (AssetObject)
			{
				if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] == nullptr)
				{
					MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] = NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, FName(*FString::Printf(TEXT("Layer_%d_%s"), Payload.Layer, *AssetObject->GetName())));
				}
				UMaterialInterface* EditedMaterial = Cast<UMaterialFunctionInterface>(AssetObject)->GetPreviewMaterial();
				if (MaterialEditorInstance->StoredLayerPreviews[Payload.Layer] && MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->Parent != EditedMaterial)
				{
					MaterialEditorInstance->StoredLayerPreviews[Payload.Layer]->SetParentEditorOnly(EditedMaterial);
				}
			}
		}

		StackProperty->Children.Add(ChildProperty);
		ShowSubParameters(InContext, ChildProperty);

	}

	if (Payload.Blend != -1)
	{
		TSharedRef<FSortedParamData> ChildProperty = MakeShared<FSortedParamData>();
		ChildProperty->StackDataType = EStackDataType::Asset;
		ChildProperty->Parameter = InContext->Parameter;
		ChildProperty->ParameterHandle = InContext->BlendHandle->AsArray()->GetElement(Payload.Blend);
		ChildProperty->ParameterNode = Generator->FindTreeNode(ChildProperty->ParameterHandle);
		ChildProperty->ParameterInfo.Index = Payload.Blend;
		ChildProperty->ParameterInfo.Association = EMaterialParameterAssociation::BlendParameter;
		ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association);
		{
			UObject* AssetObject = nullptr;
			ChildProperty->ParameterHandle->GetValue(AssetObject);
			if (AssetObject)
			{
				if (MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] == nullptr)
				{
					MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] = NewObject<UMaterialInstanceConstant>(MaterialEditorInstance, FName(*FString::Printf(TEXT("Blend_%d_%s"), Payload.Blend, *AssetObject->GetName())));
				}
				UMaterialInterface* EditedMaterial = Cast<UMaterialFunctionInterface>(AssetObject)->GetPreviewMaterial();
				if (MaterialEditorInstance->StoredBlendPreviews[Payload.Blend] && MaterialEditorInstance->StoredBlendPreviews[Payload.Blend]->Parent != EditedMaterial)
				{
					MaterialEditorInstance->StoredBlendPreviews[Payload.Blend]->SetParentEditorOnly(EditedMaterial);
				}
			}
		}

		StackProperty->Children.Add(ChildProperty);
		ShowSubParameters(InContext, ChildProperty);
	}

	InParentContainer.Add(StackProperty);
}

void SMaterialSubstrateTree::CreateGroupsWidget()
{
	check(MaterialEditorInstance);
	if (MaterialEditorInstance->IsA<UMaterialEditorInstanceConstant>())
	{
		FMaterialLayersFunctions LayersFunctions;
		UMaterialInterface* MaterialInterface = MaterialEditorInstance->GetMaterialInterface();
		if (MaterialInterface && MaterialInterface->GetMaterialLayers(LayersFunctions))
		{
			MaterialInterface->SyncLayersRuntimeGraphCache(&LayersFunctions);
		}
		MaterialEditorInstance->RegenerateArrays();		
		if (MaterialInterface)
		{
			MaterialInterface->SyncLayersRuntimeGraphCache(nullptr);
		}
	}
	
	TArray<FUnsortedParamData> NonLayerProperties; // NonLayerProperties.Empty();
	LayerProperties.Empty();
	FunctionParameter = nullptr;
	TSharedPtr<IPropertyHandle> FunctionParameterHandle;

	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	if (!Generator.IsValid())
	{
		FPropertyRowGeneratorArgs Args;
		Generator = Module.CreatePropertyRowGenerator(Args);
		// the sizes of the parameter lists are only based on the parent material and not changed out from under the details panel 
		// When a parameter is added open MI editors are refreshed
		// the tree should also refresh if one of the layer or blend assets is swapped

		auto ValidationLambda = ([](const FRootPropertyNodeList& PropertyNodeList) { return true; });
		Generator->SetCustomValidatePropertyNodesFunction(FOnValidatePropertyRowGeneratorNodes::CreateLambda(MoveTemp(ValidationLambda)));

		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}
	else
	{
		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}
	

	TSharedPtr<IDetailTreeNode> ParameterGroups = FindParameterGroupsNode(Generator);
	if (ParameterGroups.IsValid())
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		ParameterGroups->GetChildren(Children);
		// the order of DeferredSearches should correspond to NonLayerProperties exactly
		TArray<TSharedPtr<IPropertyHandle>> DeferredSearches;
		for (int32 GroupIdx = 0; GroupIdx < Children.Num(); ++GroupIdx)
		{
			TArray<void*> GroupPtrs;
			TSharedPtr<IPropertyHandle> ChildHandle = Children[GroupIdx]->CreatePropertyHandle();
			ChildHandle->AccessRawData(GroupPtrs);
			auto GroupIt = GroupPtrs.CreateConstIterator();
			const FEditorParameterGroup* ParameterGroupPtr = reinterpret_cast<FEditorParameterGroup*>(*GroupIt);
			const FEditorParameterGroup& ParameterGroup = *ParameterGroupPtr;

			for (int32 ParamIdx = 0; ParamIdx < ParameterGroup.Parameters.Num(); ParamIdx++)
			{
				UDEditorParameterValue* Parameter = ParameterGroup.Parameters[ParamIdx];

				TSharedPtr<IPropertyHandle> ParametersArrayProperty = ChildHandle->GetChildHandle("Parameters");
				TSharedPtr<IPropertyHandle> ParameterProperty = ParametersArrayProperty->GetChildHandle(ParamIdx);
				TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");

				if (Cast<UDEditorMaterialLayersParameterValue>(Parameter))
				{
					FunctionParameterHandle = ChildHandle;
					if (FunctionParameter == nullptr)
					{
						FunctionParameter = Parameter;
					}
					TArray<void*> StructPtrs;
					ParameterValueProperty->AccessRawData(StructPtrs);
					auto It = StructPtrs.CreateConstIterator();
					FunctionInstance = reinterpret_cast<FMaterialLayersFunctions*>(*It);
					FunctionInstanceHandle = ParameterValueProperty;
				}
				else
				{
					FUnsortedParamData NonLayerProperty;
					UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);

					if (ScalarParam && ScalarParam->SliderMax > ScalarParam->SliderMin)
					{
						ParameterValueProperty->SetInstanceMetaData("UIMin", FString::Printf(TEXT("%f"), ScalarParam->SliderMin));
						ParameterValueProperty->SetInstanceMetaData("UIMax", FString::Printf(TEXT("%f"), ScalarParam->SliderMax));
					}

					NonLayerProperty.Parameter = Parameter;
					NonLayerProperty.ParameterGroup = ParameterGroup;

					DeferredSearches.Add(ParameterValueProperty);
					NonLayerProperty.UnsortedName = Parameter->ParameterInfo.Name;

					NonLayerProperties.Add(NonLayerProperty);
				}
			}
		}

		checkf(NonLayerProperties.Num() == DeferredSearches.Num(), TEXT("Internal inconsistency: number of node searches does not match the number of properties"));
		TArray<TSharedPtr<IDetailTreeNode>> DeferredResults = Generator->FindTreeNodes(DeferredSearches);
		checkf(NonLayerProperties.Num() == DeferredResults.Num(), TEXT("Internal inconsistency: number of node search results does not match the number of properties"));

		for (int Idx = 0, NumUnsorted = NonLayerProperties.Num(); Idx < NumUnsorted; ++Idx)
		{
			FUnsortedParamData& NonLayerProperty = NonLayerProperties[Idx];
			NonLayerProperty.ParameterNode = DeferredResults[Idx];
			NonLayerProperty.ParameterHandle = NonLayerProperty.ParameterNode->CreatePropertyHandle();
		}

		DeferredResults.Empty();
		DeferredSearches.Empty();

		// Create the hierarchy of Sorted items recursivelly following the LayerFunctions Tree
		if (Substrate::IsMaterialLayeringSupportEnabled() && FunctionParameterHandle)
		{
			TSharedPtr<IPropertyHandle>	LayerHandle = FunctionParameterHandle->GetChildHandle("Layers").ToSharedRef();
			TSharedPtr<IPropertyHandle> BlendHandle = FunctionParameterHandle->GetChildHandle("Blends").ToSharedRef();
			uint32 NumLayerChildren;
			LayerHandle->GetNumChildren(NumLayerChildren);
			uint32 NumBlendChildren;
			BlendHandle->GetNumChildren(NumBlendChildren);
			if (MaterialEditorInstance->StoredLayerPreviews.Num() != NumLayerChildren)
			{
				MaterialEditorInstance->StoredLayerPreviews.Empty();
				MaterialEditorInstance->StoredLayerPreviews.AddDefaulted(NumLayerChildren);
			}
			if (MaterialEditorInstance->StoredBlendPreviews.Num() != NumBlendChildren)
			{
				MaterialEditorInstance->StoredBlendPreviews.Empty();
				MaterialEditorInstance->StoredBlendPreviews.AddDefaulted(NumBlendChildren);
			}

			// root 
			auto StrongFunctionParameter = FunctionParameter.Pin();

			FRecursiveCreateWidgetsContext Context{
				.Parameter = StrongFunctionParameter.Get(),
				.LayerHandle = LayerHandle,
				.BlendHandle = BlendHandle,
				.NonLayerProperties = NonLayerProperties,
			};

			auto RootChildren = FunctionInstance->GetNodeChildren(-1);
			for (int i = 0; i < RootChildren.Num(); ++i)
			{
				int Index = i;
				Index = RootChildren.Num() - 1 - i; // Reverse the order to display the layers bottom up
				RecursiveCreateWidgets(&Context, RootChildren[Index], LayerProperties, true);
			}
		}
	}
	
	SetParentsExpansionState();
}

bool SMaterialSubstrateTree::IsLayerVisible(int32 Index) const
{
	if (FunctionParameter.IsValid())
	{
		return FunctionInstance->GetLayerVisibility(Index);
	}
	return false;
}

TSharedRef<SWidget> SMaterialSubstrateTree::CreateThumbnailWidget(EMaterialParameterAssociation InAssociation, int32 InIndex, float InThumbnailSize)
{
	UObject* ThumbnailObject = nullptr;
	if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		ThumbnailObject = MaterialEditorInstance->StoredLayerPreviews[InIndex];
	}
	else if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		ThumbnailObject = MaterialEditorInstance->StoredBlendPreviews[InIndex];
	}
	const TSharedPtr<FAssetThumbnail> AssetThumbnail = MakeShareable(new FAssetThumbnail(ThumbnailObject, InThumbnailSize, InThumbnailSize, GetTreeThumbnailPool()));

	FAssetThumbnailConfig ThumbnailConfig;
	// disable realtime on hovered since these will always be realtime.  MouseLeave events turn _off_ realtime updates which isn't the behavior we want.
	ThumbnailConfig.bAllowRealTimeOnHovered = false; 

	TSharedRef<SWidget> ThumbnailWidget = AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig);
	AssetThumbnail->SetRealTime(true);
	ThumbnailWidget->SetOnMouseDoubleClick(FPointerEventHandler::CreateSP(this, &SMaterialSubstrateTree::OnThumbnailDoubleClick, InAssociation, InIndex));
	return ThumbnailWidget;
}

void SMaterialSubstrateTree::UpdateThumbnailMaterial(TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 InIndex, bool bAlterBlendIndex)
{
	int32 AssetIndex = FunctionInstance->GetLayerFuncIndex(InIndex);

	UMaterialInstanceConstant* MaterialToUpdate = nullptr;
	if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		MaterialToUpdate = MaterialEditorInstance->StoredLayerPreviews[AssetIndex];
	}
	if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		MaterialToUpdate = MaterialEditorInstance->StoredBlendPreviews[AssetIndex];
	}

	if (MaterialToUpdate != nullptr)
	{
		// From the notification, we get the NodeId triggering a parameter change, or just a refresh and we require a render for that preview material
		TArray<int32> NodeParentsIds = FunctionInstance->GetNodeParents(InIndex);

		// if parents grab the parent preview mat:
		UMaterialInstanceConstant* ParentMaterialToUpdate = nullptr; 
		if (NodeParentsIds.Num() > 1)
		{
			int32 ParentAssetIndex = FunctionInstance->GetLayerFuncIndex(NodeParentsIds[0]);
			ParentMaterialToUpdate = MaterialEditorInstance->StoredLayerPreviews[ParentAssetIndex];
		}

		TArray<TSharedPtr<FSortedParamData>> AssetItemsContainer;
		CollectStackItemsForMaterialFunctionAsset(InAssociation, AssetIndex, AssetItemsContainer);

		TArray<FEditorParameterGroup> ParameterGroups;
		for (TSharedPtr<FSortedParamData> AssetItem : AssetItemsContainer)
		{
			if (AssetItem->StackDataType == EStackDataType::Asset)
			{
				for (TSharedPtr<FSortedParamData> Group : AssetItem->Children)
				{
					if (Group->ParameterInfo.Association == InAssociation)
					{
						FEditorParameterGroup DuplicatedGroup = FEditorParameterGroup();
						DuplicatedGroup.GroupAssociation = Group->Group.GroupAssociation;
						DuplicatedGroup.GroupName = Group->Group.GroupName;
						DuplicatedGroup.GroupSortPriority = Group->Group.GroupSortPriority;
						for (UDEditorParameterValue* Parameter : Group->Group.Parameters)
						{
							if (Parameter->ParameterInfo.Index == AssetIndex)
							{
								DuplicatedGroup.Parameters.Add(Parameter);
							}
						}
						ParameterGroups.Add(DuplicatedGroup);
					}
				}
			}	
		}

		FMaterialPropertyHelpers::TransitionAndCopyParameters(MaterialToUpdate, ParameterGroups, true);

		if (ParentMaterialToUpdate)
			FMaterialPropertyHelpers::CopyMaterialToInstance(ParentMaterialToUpdate, ParameterGroups);
	}
}

FReply SMaterialSubstrateTree::OnThumbnailDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent, EMaterialParameterAssociation InAssociation, int32 InIndex)
{
	UMaterialFunctionInterface* AssetToOpen = nullptr;
	if (InAssociation == EMaterialParameterAssociation::BlendParameter)
	{
		AssetToOpen = FunctionInstance->Blends[InIndex];
	}
	else if (InAssociation == EMaterialParameterAssociation::LayerParameter)
	{
		AssetToOpen = FunctionInstance->Layers[InIndex];
	}
	if (AssetToOpen != nullptr)
	{
		GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetToOpen);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SMaterialSubstrateTree::ShowSubParameters(FRecursiveCreateWidgetsContext* InContext, TSharedPtr<FSortedParamData> ParentParameter)
{
	for (FUnsortedParamData Property : InContext->NonLayerProperties)
	{
		UDEditorParameterValue* Parameter = Property.Parameter;
		if (Parameter->ParameterInfo.Index == ParentParameter->ParameterInfo.Index
			&& Parameter->ParameterInfo.Association == ParentParameter->ParameterInfo.Association)
		{
			TSharedPtr<FSortedParamData> GroupProperty(new FSortedParamData());
			GroupProperty->StackDataType = EStackDataType::Group;
			GroupProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			GroupProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			GroupProperty->Group = Property.ParameterGroup;
			GroupProperty->NodeKey = FString::FromInt(GroupProperty->ParameterInfo.Index) + FString::FromInt(GroupProperty->ParameterInfo.Association) + Property.ParameterGroup.GroupName.ToString();

			bool bAddNewGroup = true;
			for (TSharedPtr<struct FSortedParamData> GroupChild : ParentParameter->Children)
			{
				if (GroupChild->NodeKey == GroupProperty->NodeKey)
				{
					bAddNewGroup = false;
				}
			}
			if (bAddNewGroup)
			{
				ParentParameter->Children.Add(GroupProperty);
			}

			TSharedPtr<FSortedParamData> ChildProperty(new FSortedParamData());
			ChildProperty->StackDataType = EStackDataType::Property;
			ChildProperty->Parameter = Parameter;
			ChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			ChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			ChildProperty->ParameterNode = Property.ParameterNode;
			ChildProperty->PropertyName = Property.UnsortedName;
			ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association) +  Property.ParameterGroup.GroupName.ToString() + Property.UnsortedName.ToString();


			UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
			if (!CompMaskParam)
			{
				TArray<TSharedRef<IDetailTreeNode>> ParamChildren;
				Property.ParameterNode->GetChildren(ParamChildren);
				for (int32 ParamChildIdx = 0; ParamChildIdx < ParamChildren.Num(); ParamChildIdx++)
				{
					TSharedPtr<FSortedParamData> ParamChildProperty(new FSortedParamData());
					ParamChildProperty->StackDataType = EStackDataType::PropertyChild;
					ParamChildProperty->ParameterNode = ParamChildren[ParamChildIdx];
					ParamChildProperty->ParameterHandle = ParamChildProperty->ParameterNode->CreatePropertyHandle();
					ParamChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
					ParamChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
					ParamChildProperty->Parameter = ChildProperty->Parameter;
					ChildProperty->Children.Add(ParamChildProperty);
				}
			}
			for (TSharedPtr<struct FSortedParamData> GroupChild : ParentParameter->Children)
			{
				if (GroupChild->Group.GroupName == Property.ParameterGroup.GroupName
					&& GroupChild->ParameterInfo.Association == ChildProperty->ParameterInfo.Association
					&&  GroupChild->ParameterInfo.Index == ChildProperty->ParameterInfo.Index)
				{
					GroupChild->Children.Add(ChildProperty);
				}
			}

		}
	}
}


void SMaterialSubstrateTree::CollectStackItemsRecursively(TSharedPtr<FSortedParamData> Item, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer)
{
	for (TSharedPtr<FSortedParamData> Child : Item->Children)
	{
		if (Child->StackDataType == EStackDataType::Stack)
		{
			OutGroupsContainer.Add(Child);
		}

		CollectStackItemsRecursively(Child, OutGroupsContainer);
	}
}

void SMaterialSubstrateTree::CollectAssetStackItemsRecursively(TSharedPtr<FSortedParamData> Item, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer, TArray<uint32>& OutNodeIdsContainer)
{
	uint32 NodeID = Item->ParameterInfo.Index;
	for (TSharedPtr<FSortedParamData> Child : Item->Children)
	{
		if (Child->StackDataType == EStackDataType::Asset)
		{
			OutGroupsContainer.Add(Child);
			OutNodeIdsContainer.Add(NodeID);
		}

		CollectAssetStackItemsRecursively(Child, OutGroupsContainer, OutNodeIdsContainer);
	}
}

void SMaterialSubstrateTree::CollectStackItemsForMaterialFunctionAsset(EMaterialParameterAssociation InAssociation, int32 InAssetIndex, TArray<TSharedPtr<FSortedParamData>>& OutGroupsContainer)
{
	TArray<TSharedPtr<FSortedParamData>> CollectedAssetItems;
	TArray<uint32> CollectedNodeIds;
	for (TSharedPtr<FSortedParamData> Child : LayerProperties)
	{
		CollectAssetStackItemsRecursively(Child, CollectedAssetItems, CollectedNodeIds);
	}

	for (TSharedPtr<FSortedParamData> AssetItem : CollectedAssetItems)
	{
		if (AssetItem->ParameterInfo.Association == InAssociation)
		{
			if (AssetItem->ParameterInfo.Index == InAssetIndex)
			{
				OutGroupsContainer.Add(AssetItem);
			}
		}
	}
}


void SMaterialSubstrateTree::OnDeleteSelectedTreeViewItems()
{
	TArray<FSortedParamDataPtr> SelectedItemsArray = GetSelectedItems();
	if (SelectedItemsArray.Num() > 0)
	{
		FSortedParamDataPtr StackParameterData = SelectedItemsArray[0];
		RemoveNodeLayer(StackParameterData->ParameterInfo.Index);
	}
}

bool SMaterialSubstrateTree::CanDeleteSelectedTreeViewItems() const
{
	bool bCanRemoveLayer = false;
	TArray<FSortedParamDataPtr> SelectedItemsArray = GetSelectedItems();
	if (SelectedItemsArray.Num() > 0)
	{
		FSortedParamDataPtr StackParameterData = SelectedItemsArray[0];
		bCanRemoveLayer = FunctionInstance->CanRemoveLayerNode(StackParameterData->ParameterInfo.Index);
	}
	return bCanRemoveLayer;
}

void SMaterialSubstrateTree::OnRenameSelectedTreeViewItems()
{
	TArray<FSortedParamDataPtr> SelectedItemsArray = GetSelectedItems();
	if (SelectedItemsArray.Num() > 0)
	{
		FSortedParamDataPtr StackParameterData = SelectedItemsArray[0];

		TSharedPtr<SMaterialSubstrateTreeItem> TreeItem = StaticCastSharedPtr<SMaterialSubstrateTreeItem>(WidgetFromItem(StackParameterData));
		TreeItem->Rename();
	}
}

bool SMaterialSubstrateTree::CanRenameSelectedTreeViewItem() const
{
	bool bCanRenameLayer = false;
	TArray<FSortedParamDataPtr> SelectedItemsArray = GetSelectedItems();
	if (SelectedItemsArray.Num() > 0)
	{
		FSortedParamDataPtr StackParameterData = SelectedItemsArray[0];
		// only allow Material Evaluation layers to be renamed for now
		bCanRenameLayer = FunctionInstance->GetNodeDepth(StackParameterData->ParameterInfo.Index) == 1;
	}
	return bCanRenameLayer;
}

#undef LOCTEXT_NAMESPACE
