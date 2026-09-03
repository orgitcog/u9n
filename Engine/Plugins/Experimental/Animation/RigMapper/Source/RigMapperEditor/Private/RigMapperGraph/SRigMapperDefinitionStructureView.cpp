// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRigMapperDefinitionStructureView.h"

#include "RigMapperDefinition.h"
#include "RigMapperDefinitionEditorGraphNode.h"
#include "SlateOptMacros.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "RigMapperDefinitionStructureView"

const int32 SRigMapperDefinitionStructureView::NumNodeTypes = 4;
const int32 SRigMapperDefinitionStructureView::NumFeatureTypes = 3;
const FString SRigMapperDefinitionStructureView::InputsNodeName = LOCTEXT("RigMapperDefinitionStructureViewInputs","Inputs").ToString();
const FString SRigMapperDefinitionStructureView::FeaturesNodeName = LOCTEXT("RigMapperDefinitionStructureViewFeatures", "Features").ToString();
const FString SRigMapperDefinitionStructureView::MultiplyNodeName = LOCTEXT("RigMapperDefinitionStructureViewMultiplyFeatures", "Multiply").ToString();
const FString SRigMapperDefinitionStructureView::WsNodeName = LOCTEXT("RigMapperDefinitionStructureViewWeightedSumsFeatures", "Weighted Sums").ToString();
const FString SRigMapperDefinitionStructureView::SdkNodeName = LOCTEXT("RigMapperDefinitionStructureViewSDKsFeatures", "SDKs").ToString();
const FString SRigMapperDefinitionStructureView::OutputNodeName = LOCTEXT("RigMapperDefinitionStructureViewOutputs", "Outputs").ToString();
const FString SRigMapperDefinitionStructureView::NullOutputNodeName = LOCTEXT("RigMapperDefinitionStructureViewNullOutputs", "Null Outputs").ToString();


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SRigMapperDefinitionStructureView::Construct(const FArguments& InArgs, URigMapperDefinition* InDefinition)
{
	Definition = InDefinition;
	GenerateParentNodes();
	GenerateChildrenNodes();

	TreeView = SNew(STreeView<TSharedPtr<FString>>)
		.SelectionMode(ESelectionMode::Multi)
		.HighlightParentNodesForSelection(true)
		.OnGenerateRow(this, &SRigMapperDefinitionStructureView::OnGenerateTreeRow)
		.OnGetChildren(this, &SRigMapperDefinitionStructureView::OnGetTreeNodeChildren)
		.OnSelectionChanged(this, &SRigMapperDefinitionStructureView::HandleTreeNodesSelectionChanged)
		.TreeItemsSource(&FilteredRootNodes);

	SearchBoxFilter = MakeShared<TTextFilter<TSharedPtr<FString>>>(TTextFilter<TSharedPtr<FString>>::FItemToStringArray::CreateSP(this, &SRigMapperDefinitionStructureView::TransformElementToString));
	
	for (const TPair<TSharedPtr<FString>, TArray<TSharedPtr<FString>>>& NodeAndChildren : ParentsAndChildrenNodes)
	{
		TreeView->SetItemExpansion(NodeAndChildren.Key, true);	
	}
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(7.0f, 6.0f)
		.AutoHeight()
		[
			SAssignNew(SearchBox, SSearchBox)
			.OnTextChanged(this, &SRigMapperDefinitionStructureView::OnFilterTextChanged)
		]

		+ SVerticalBox::Slot()
		[
			TreeView.ToSharedRef()
		]
	]; 
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION


TSharedPtr<FString> SRigMapperDefinitionStructureView::GetParentAndChildrenNodes(ERigMapperNodeType NodeType, TArray<TSharedPtr<FString>>& OutChildren)
{
	if (const TSharedPtr<FString>* ParentNode = ParentNodesMapping.Find(NodeType))
	{
		if (const TArray<TSharedPtr<FString>>* ChildrenNodes = ParentsAndChildrenNodes.Find(*ParentNode))
		{
			OutChildren = *ChildrenNodes;
			return *ParentNode;
		}
	}
	return nullptr;
}

TArray<TSharedPtr<FString>>* SRigMapperDefinitionStructureView::GetChildrenNodes(ERigMapperNodeType NodeType)
{
	if (const TSharedPtr<FString>* ParentNode = ParentNodesMapping.Find(NodeType))
	{
		return ParentsAndChildrenNodes.Find(*ParentNode);
	}
	return nullptr;
}

const TArray<TSharedPtr<FString>>* SRigMapperDefinitionStructureView::GetChildrenNodes(ERigMapperNodeType NodeType) const
{
	if (const TSharedPtr<FString>* ParentNode = ParentNodesMapping.Find(NodeType))
	{
		return ParentsAndChildrenNodes.Find(*ParentNode);
	}
	return nullptr;
}

bool SRigMapperDefinitionStructureView::SelectNode(const FString& NodeName, ERigMapperNodeType NodeType, bool bSelected)
{
	if (const TArray<TSharedPtr<FString>>* ChildrenNodes = GetChildrenNodes(NodeType))
	{
		if (const TSharedPtr<FString>* TreeNode = ChildrenNodes->FindByPredicate(
			[&NodeName](const TSharedPtr<FString>& Item) { return Item && *Item.Get() == NodeName; }))
		{
			TreeView->SetItemSelection(*TreeNode, bSelected);
			TreeView->RequestNavigateToItem(*TreeNode);
			return true;
		}
	}
	return false;
}

void SRigMapperDefinitionStructureView::RebuildTree()
{
	TreeView->ClearSelection();
	GenerateChildrenNodes();
	
	FilteredRootNodes.Reset();
	FilterNodes(RootNodes, FilteredRootNodes);

	TreeView->RequestTreeRefresh();
}

void SRigMapperDefinitionStructureView::ClearSelection() const
{
	TreeView->ClearSelection();
}

bool SRigMapperDefinitionStructureView::IsNodeOrChildSelected(ERigMapperNodeType NodeType, int32 ArrayIndex) const
{
	if (NodeType == ERigMapperNodeType::Invalid)
	{
		return IsNodeOrChildSelected(ERigMapperNodeType::Multiply, ArrayIndex) ||
				IsNodeOrChildSelected(ERigMapperNodeType::WeightedSum, ArrayIndex) ||
				IsNodeOrChildSelected(ERigMapperNodeType::SDK, ArrayIndex);
	}
	
	const TArray<TSharedPtr<FString>>& Selection = TreeView->GetSelectedItems();
	
	if (const TArray<TSharedPtr<FString>>* ChildrenNodes = GetChildrenNodes(NodeType))
	{
		if (ArrayIndex == INDEX_NONE)
		{
			for (const TSharedPtr<FString>& Node : Selection)
			{
				if (ChildrenNodes->Contains(Node))
				{
					return true;
				}
			}
		}
		return ChildrenNodes->IsValidIndex(ArrayIndex) && Selection.Contains((*ChildrenNodes)[ArrayIndex]);	
	}
	return false;
}

bool SRigMapperDefinitionStructureView::IsSelectionEmpty() const
{
	return TreeView->GetNumItemsSelected() == 0;
}

void SRigMapperDefinitionStructureView::GenerateParentNodes()
{
	RootNodes.Reset(NumFeatureTypes);
	ParentNodesMapping.Empty(NumFeatureTypes + NumNodeTypes);
	ParentsAndChildrenNodes.Empty(NumFeatureTypes + NumNodeTypes);

	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::Input, RootNodes.Add_GetRef(MakeShared<FString>(InputsNodeName))));

	TArray<TSharedPtr<FString>>& FeatureEntries = ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::Invalid, RootNodes.Add_GetRef(MakeShared<FString>(FeaturesNodeName))));
	FeatureEntries.Reserve(NumNodeTypes);

	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::Multiply, FeatureEntries.Add_GetRef(MakeShared<FString>(MultiplyNodeName))));

	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::WeightedSum, FeatureEntries.Add_GetRef(MakeShared<FString>(WsNodeName))));
	
	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::SDK, FeatureEntries.Add_GetRef(MakeShared<FString>(SdkNodeName))));

	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::Output, RootNodes.Add_GetRef(MakeShared<FString>(OutputNodeName))));

	ParentsAndChildrenNodes.Add(ParentNodesMapping.Add(
		ERigMapperNodeType::NullOutput, RootNodes.Add_GetRef(MakeShared<FString>(NullOutputNodeName))));

	FilteredRootNodes = RootNodes;
}

void SRigMapperDefinitionStructureView::GenerateChildrenNodes()
{
	if (Definition && !ParentsAndChildrenNodes.IsEmpty() && !ParentNodesMapping.IsEmpty())
	{
		TArray<TSharedPtr<FString>>* InputEntries = GetChildrenNodes(ERigMapperNodeType::Input);
		InputEntries->Reset(Definition->Inputs.Num());
		for (const FString& Input : Definition->Inputs)
		{
			InputEntries->Add(MakeShared<FString>(Input));
		}

		TArray<TSharedPtr<FString>>* MultiplyEntries = GetChildrenNodes(ERigMapperNodeType::Multiply);
		MultiplyEntries->Reset(Definition->Features.Multiply.Num());
		for (const FRigMapperMultiplyFeature& Feature : Definition->Features.Multiply)
		{
			MultiplyEntries->Add(MakeShared<FString>(Feature.Name));
		}

		TArray<TSharedPtr<FString>>* WsEntries = GetChildrenNodes(ERigMapperNodeType::WeightedSum);
		WsEntries->Reset(Definition->Features.WeightedSums.Num());
		for (const FRigMapperFeature& Feature : Definition->Features.WeightedSums)
		{
			WsEntries->Add(MakeShared<FString>(Feature.Name));
		}

		TArray<TSharedPtr<FString>>* SdkEntries = GetChildrenNodes(ERigMapperNodeType::SDK);
		SdkEntries->Reset(Definition->Features.SDKs.Num());
		for (const FRigMapperSdkFeature& Feature : Definition->Features.SDKs)
		{
			SdkEntries->Add(MakeShared<FString>(Feature.Name));
		}

		TArray<TSharedPtr<FString>>* OutputEntries = GetChildrenNodes(ERigMapperNodeType::Output);
		OutputEntries->Reset(Definition->Outputs.Num());
		for (const TPair<FString, FString>& Output : Definition->Outputs)
		{
			OutputEntries->Add(MakeShared<FString>(Output.Key));
		}

		TArray<TSharedPtr<FString>>* NullOutputEntries = GetChildrenNodes(ERigMapperNodeType::NullOutput);
		NullOutputEntries->Reset(Definition->NullOutputs.Num());
		for (const FString& NullOutput : Definition->NullOutputs)
		{
			NullOutputEntries->Add(MakeShared<FString>(NullOutput));
		}
	}
}

void SRigMapperDefinitionStructureView::HandleTreeNodesSelectionChanged(TSharedPtr<FString> Node, ESelectInfo::Type SelectInfo)
{
	TArray<FString> SelectedInputs;
	TArray<FString> SelectedFeatures;
	TArray<FString> SelectedOutputs;
	TArray<FString> SelectedNullOutputs;

	if (SelectInfo != ESelectInfo::Direct)
	{
		const TArray<TSharedPtr<FString>>& SelectedItems = TreeView->GetSelectedItems();

		auto GetSelectedNodeNames = [&SelectedItems](const TArray<TSharedPtr<FString>>* PtrArray)
		{
			TArray<FString> SelectedNames;

			for (const TSharedPtr<FString>& Item : SelectedItems)
			{
				if (PtrArray->Contains(Item))
				{
					SelectedNames.Add(*Item.Get());
				}
			}
			return SelectedNames;
		};

		SelectedInputs = GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::Input));

		SelectedFeatures = GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::Multiply));
		SelectedFeatures.Append(GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::WeightedSum)));
		SelectedFeatures.Append(GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::SDK)));
	
		SelectedOutputs = GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::Output));
		SelectedNullOutputs = GetSelectedNodeNames(GetChildrenNodes(ERigMapperNodeType::NullOutput));
	}

	if (OnSelectionChanged.IsBound())
	{
		OnSelectionChanged.Execute(SelectInfo, SelectedInputs, SelectedFeatures, SelectedOutputs, SelectedNullOutputs);
	}
}


void SRigMapperDefinitionStructureView::TransformElementToString(TSharedPtr<FString> String, TArray<FString>& Strings)
{
	if (String.IsValid())
	{
		Strings = { *String.Get() };
	}
}

void SRigMapperDefinitionStructureView::OnFilterTextChanged(const FText& Text)
{
	SearchBoxFilter->SetRawFilterText(Text);

	FilteredRootNodes.Reset();
	FilterNodes(RootNodes, FilteredRootNodes);
	TreeView->RequestTreeRefresh();
}

void SRigMapperDefinitionStructureView::FilterNodes(const TArray<TSharedPtr<FString>>& ParentNodes, TArray<TSharedPtr<FString>>& FilteredNodes)
{
	for (TSharedPtr<FString> ParentNode : ParentNodes)
	{
		bool ChildPassedFilter = false;
		
		if (ParentNode.IsValid() && ParentsAndChildrenNodes.Contains(ParentNode))
		{
			const TArray<TSharedPtr<FString>>& Children = ParentsAndChildrenNodes[ParentNode];

			if (!SearchBoxFilter->GetRawFilterText().IsEmpty())
			{
				ChildPassedFilter = Children.ContainsByPredicate([this](const TSharedPtr<FString>& InItem)
				{
					return SearchBoxFilter->PassesFilter(InItem);
				});
			}

			if (ChildPassedFilter)
			{
				FilteredNodes.AddUnique(ParentNode);
				TreeView->SetItemExpansion(ParentNode, true);
			}
		}
		if (!ChildPassedFilter && SearchBoxFilter->PassesFilter(ParentNode))
		{
			FilteredNodes.Add(ParentNode);
		}
	}
}

TSharedRef<ITableRow> SRigMapperDefinitionStructureView::OnGenerateTreeRow(TSharedPtr<FString> NodeName, const TSharedRef<STableViewBase>& TableViewBase)
{
	return SNew(STableRow<TSharedPtr<FString>>, TableViewBase)
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
		.Padding(4)
		[
			SNew(STextBlock)
			.Text(FText::FromString(*NodeName.Get()))
		];
}

void SRigMapperDefinitionStructureView::OnGetTreeNodeChildren(TSharedPtr<FString> NodeName, TArray<TSharedPtr<FString>>& Children)
{
	if (NodeName.IsValid() && ParentsAndChildrenNodes.Contains(NodeName))
	{
		const TArray<TSharedPtr<FString>>& UnfilteredChildren = ParentsAndChildrenNodes[NodeName];

		Children.Reset(UnfilteredChildren.Num());
		FilterNodes(UnfilteredChildren, Children);
	}
}

#undef LOCTEXT_NAMESPACE