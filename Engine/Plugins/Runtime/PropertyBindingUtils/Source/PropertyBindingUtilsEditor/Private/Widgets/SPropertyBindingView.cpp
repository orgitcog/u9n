// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SPropertyBindingView.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollection.h"

#define LOCTEXT_NAMESPACE "SPropertyBindingViewer"

namespace UE::PropertyBinding
{
namespace Private
{
	static const FLazyName ColumnId_SourceStruct = "SourceStruct";
	static const FLazyName ColumnId_SourcePath = "SourcePath";
	static const FLazyName ColumnId_TargetStruct = "TargetStruct";
	static const FLazyName ColumnId_TargetPath = "TargetPath";

	class SBindingViewRow : public SMultiColumnTableRow<TSharedPtr<SBindingView::FItem>>
	{
	public:
		SLATE_BEGIN_ARGS(SBindingViewRow) {}
		SLATE_END_ARGS()

		void Construct(const FArguments& Args, TSharedPtr<SBindingView::FItem> InItem, TWeakInterfacePtr<const IPropertyBindingBindingCollectionOwner> InCollectionOwner, const TSharedRef<STableViewBase>& OwnerTableView)
		{
			Item = InItem;
			CollectionOwner = InCollectionOwner;
			SMultiColumnTableRow<TSharedPtr<SBindingView::FItem>>::Construct(
				FSuperRowType::FArguments()
				.Padding(1.0f),
				OwnerTableView
			);
		}

		FText HandleGetStructName(FGuid StructID) const
		{
			if (const IPropertyBindingBindingCollectionOwner* CollectionOwnerPtr = CollectionOwner.Get())
			{
				TInstancedStruct<FPropertyBindingBindableStructDescriptor> StructDesc;
				if (CollectionOwnerPtr->GetBindableStructByID(StructID, StructDesc))
				{
					return FText::FromString(StructDesc.Get().ToString());
				}
			}
			return FText::GetEmpty();
		}

		virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override
		{
			if (ColumnName == ColumnId_SourceStruct)
			{
				const FGuid StructID = Item->SourcePath.GetStructID();
				return SNew(STextBlock)
					.Text(this, &SBindingViewRow::HandleGetStructName, StructID);
			}
			else if (ColumnName == ColumnId_SourcePath)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->SourcePath.ToString()));		
			}
			else if (ColumnName == ColumnId_TargetStruct)
			{
				const FGuid StructID = Item->TargetPath.GetStructID();
				return SNew(STextBlock)
					.Text(this, &SBindingViewRow::HandleGetStructName, StructID);
			}
			else if (ColumnName == ColumnId_TargetPath)
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Item->TargetPath.ToString()));
			}

			return SNullWidget::NullWidget;
		}

	private:
		TSharedPtr<SBindingView::FItem> Item;
		TWeakInterfacePtr<const IPropertyBindingBindingCollectionOwner> CollectionOwner;
	};
}

void SBindingView::Construct(const FArguments& InArgs)
{
	OnGetBindingCollection = InArgs._GetBindingCollection;
	CollectionOwner = InArgs._CollectionOwner;

	ListView = SNew(SListView<TSharedPtr<FItem>>)
		.SelectionMode(ESelectionMode::None)
		.ListItemsSource(&Values)
		.OnGenerateRow(this, &SBindingView::HandleGenerateRow)
		.HeaderRow
		(
			SNew(SHeaderRow)
			+ SHeaderRow::Column(Private::ColumnId_SourceStruct)
			.DefaultLabel(LOCTEXT("SourceStructColumnLabel", "Source"))
			
			+ SHeaderRow::Column(Private::ColumnId_SourcePath)
			.DefaultLabel(LOCTEXT("SourcePathColumnLabel", "Path"))

			+ SHeaderRow::Column(Private::ColumnId_TargetStruct)
			.DefaultLabel(LOCTEXT("TargetStructColumnLabel", "Target"))

			+ SHeaderRow::Column(Private::ColumnId_TargetPath)
			.DefaultLabel(LOCTEXT("TargetPathColumnLabel", "Path"))
		);

	ChildSlot
	[
		ListView.ToSharedRef()
	];
}

const FPropertyBindingBindingCollection* SBindingView::GetBindingCollection()
{
	return OnGetBindingCollection.IsBound() ? OnGetBindingCollection.Execute() : nullptr;
}

void SBindingView::RequestRefresh()
{
	ListView->RequestListRefresh();
}

void SBindingView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FPropertyBindingBindingCollection* Collection = GetBindingCollection();
	if (Collection)
	{
		int32 Index = 0;
		bool bRequestRefresh = false;
		Collection->ForEachBinding(
			[&Index, &bRequestRefresh, Self=this](const FPropertyBindingBinding & Binding)
			{
				if (Self->Values.Num() <= Index)
				{
					Self->Values.Add(MakeShared<FItem>(Binding.GetSourcePath(), Binding.GetTargetPath(), Binding.GetPropertyFunctionNode().GetScriptStruct()));
					bRequestRefresh = true;
				}
				else
				{
					const TSharedPtr<FItem>& Item = Self->Values[Index];
					const bool bChanged = Item->FunctionNodeStruct.Get() != Binding.GetPropertyFunctionNode().GetScriptStruct()
						|| Item->SourcePath != Binding.GetSourcePath()
						|| Item->TargetPath != Binding.GetTargetPath();
					if (bChanged)
					{
						Self->Values.SetNum(Index);
						Self->Values.Add(MakeShared<FItem>(Binding.GetSourcePath(), Binding.GetTargetPath(), Binding.GetPropertyFunctionNode().GetScriptStruct()));
						bRequestRefresh = true;
					}
				}
				++Index;
			});

		if (Values.Num() != Index)
		{
			Values.SetNum(Index);
			bRequestRefresh = true;
		}

		if (bRequestRefresh)
		{
			RequestRefresh();
		}
	}
	else
	{
		if (!Values.IsEmpty())
		{
			Values.Empty();
			RequestRefresh();
		}
	}
}

TSharedRef<ITableRow> SBindingView::HandleGenerateRow(TSharedPtr<FItem> Value, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(Private::SBindingViewRow, Value, CollectionOwner, OwnerTable);
}

} // namespace UE::PropertyBinding

#undef LOCTEXT_NAMESPACE
