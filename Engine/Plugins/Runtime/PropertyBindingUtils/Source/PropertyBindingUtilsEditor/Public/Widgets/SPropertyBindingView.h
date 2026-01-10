// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "UObject/WeakInterfacePtr.h"

#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FPropertyBindingBindingCollection;

namespace UE::PropertyBinding
{
	namespace Private
	{
		class SBindingViewRow;
	}

	class SBindingView : public SCompoundWidget
	{
	private:
		friend ::UE::PropertyBinding::Private::SBindingViewRow;
		struct FItem
		{
			FPropertyBindingPath SourcePath;
			FPropertyBindingPath TargetPath;
			TWeakObjectPtr<const UScriptStruct> FunctionNodeStruct;
		};

	public:
		DECLARE_DELEGATE_RetVal(const FPropertyBindingBindingCollection*, FOnGetBindingCollection);

		SLATE_BEGIN_ARGS(SBindingView){}
			SLATE_EVENT(FOnGetBindingCollection, GetBindingCollection)
			SLATE_ARGUMENT(TScriptInterface<const IPropertyBindingBindingCollectionOwner>, CollectionOwner)
		SLATE_END_ARGS()

		PROPERTYBINDINGUTILSEDITOR_API void Construct(const FArguments& InArgs);
		PROPERTYBINDINGUTILSEDITOR_API virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	private:
		const FPropertyBindingBindingCollection* GetBindingCollection();
		void RequestRefresh();

		TSharedRef<ITableRow> HandleGenerateRow(TSharedPtr<FItem> Value, const TSharedRef<STableViewBase>& OwnerTable);

	private:
		FOnGetBindingCollection OnGetBindingCollection;
		TWeakInterfacePtr<const IPropertyBindingBindingCollectionOwner> CollectionOwner;
		TSharedPtr<SListView<TSharedPtr<FItem>>> ListView;
		TArray<TSharedPtr<FItem>> Values;
};

} // namespace UE::PropertyBinding