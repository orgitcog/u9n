// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/TextFilter.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"

enum class ERigMapperNodeType : uint8;
class SSearchBox;
class URigMapperDefinition;

/**
 * 
 */
class RIGMAPPEREDITOR_API SRigMapperDefinitionStructureView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SRigMapperDefinitionStructureView)
		{
		}

	SLATE_END_ARGS()

	DECLARE_DELEGATE_FiveParams(FOnSelectionChanged, ESelectInfo::Type, TArray<FString>, TArray<FString>, TArray<FString>, TArray<FString>);
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs, URigMapperDefinition* InDefinition);

	bool SelectNode(const FString& NodeName, ERigMapperNodeType NodeType, bool bSelected);
	void RebuildTree();
	void ClearSelection() const;
	bool IsNodeOrChildSelected(ERigMapperNodeType NodeType, int32 ArrayIndex) const;
	bool IsSelectionEmpty() const;
	
public:
	FOnSelectionChanged OnSelectionChanged;

private:
	void GenerateParentNodes();
	void GenerateChildrenNodes();
	void HandleTreeNodesSelectionChanged(TSharedPtr<FString> Node, ESelectInfo::Type SelectInfo);
	void TransformElementToString(TSharedPtr<FString> String, TArray<FString>& Strings);
	void OnFilterTextChanged(const FText& Text);
	void FilterNodes(const TArray<TSharedPtr<FString>>& ParentNodes, TArray<TSharedPtr<FString>>& FilteredNodes);

	TSharedRef<ITableRow> OnGenerateTreeRow(TSharedPtr<FString> NodeName, const TSharedRef<STableViewBase>& TableViewBase);
	void OnGetTreeNodeChildren(TSharedPtr<FString> NodeName, TArray<TSharedPtr<FString>>& Children);

	TSharedPtr<FString> GetParentAndChildrenNodes(ERigMapperNodeType NodeType, TArray<TSharedPtr<FString>>& OutChildren);
	TArray<TSharedPtr<FString>>* GetChildrenNodes(ERigMapperNodeType NodeType);
	const TArray<TSharedPtr<FString>>* GetChildrenNodes(ERigMapperNodeType NodeType) const;
	
private:
	static const int32 NumNodeTypes;
	static const int32 NumFeatureTypes;
	
	static const FString InputsNodeName;
	static const FString FeaturesNodeName;
	static const FString MultiplyNodeName;
	static const FString WsNodeName;
	static const FString SdkNodeName;
	static const FString OutputNodeName;
	static const FString NullOutputNodeName;

	TObjectPtr<URigMapperDefinition> Definition;

	TSharedPtr<STreeView<TSharedPtr<FString>>> TreeView;
	
	TSharedPtr<SSearchBox> SearchBox;
	TSharedPtr<TTextFilter<TSharedPtr<FString>>> SearchBoxFilter;

	TArray<TSharedPtr<FString>> RootNodes;
	TArray<TSharedPtr<FString>> FilteredRootNodes;
	
	TMap<TSharedPtr<FString>, TArray<TSharedPtr<FString>>> ParentsAndChildrenNodes;
	TMap<ERigMapperNodeType, TSharedPtr<FString>> ParentNodesMapping;
};
