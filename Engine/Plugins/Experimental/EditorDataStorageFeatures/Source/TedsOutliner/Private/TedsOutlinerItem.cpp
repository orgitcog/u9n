// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsOutlinerItem.h"

#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "HAL/PlatformApplicationMisc.h"
#include "TedsOutlinerImpl.h"

#include "UObject/PropertyBagRepository.h"

#include "Columns/TedsOutlinerColumns.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Common//EditorDataStorageFeatures.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"


#include "UObject/InstanceDataObjectUtils.h"

#define LOCTEXT_NAMESPACE "TedsOutliner"

namespace UE::Editor::Outliner
{
const FSceneOutlinerTreeItemType FTedsOutlinerTreeItem::Type(&ISceneOutlinerTreeItem::Type);

FTedsOutlinerTreeItem::FTedsOutlinerTreeItem(const DataStorage::RowHandle& InRowHandle,
	const TWeakPtr<const FTedsOutlinerImpl>& InTedsOutlinerImpl)
	: ISceneOutlinerTreeItem(Type)
	, RowHandle(InRowHandle)
	, TedsOutlinerImpl(InTedsOutlinerImpl)
{
	
}

bool FTedsOutlinerTreeItem::IsValid() const
{
	return true; // TEDS-Outliner TODO: check with TEDS if the item is valid?
}

FSceneOutlinerTreeItemID FTedsOutlinerTreeItem::GetID() const
{
	return FSceneOutlinerTreeItemID(RowHandle);
}

FString FTedsOutlinerTreeItem::GetDisplayString() const
{
	if (const TSharedPtr<const FTedsOutlinerImpl> TedsOutlinerImplPin = TedsOutlinerImpl.Pin())
	{
		if (const FTypedElementLabelColumn* LabelColumn = TedsOutlinerImplPin->GetStorage()->GetColumn<FTypedElementLabelColumn>(RowHandle))
		{
			return LabelColumn->Label;
		}
		if (const FNameColumn* NameColumn = TedsOutlinerImplPin->GetStorage()->GetColumn<FNameColumn>(RowHandle))
		{
			return NameColumn->Name.ToString();
		}
	}
	

	return TEXT("TEDS Item");
}

bool FTedsOutlinerTreeItem::CanInteract() const
{
	return Flags.bInteractive;
}

TSharedRef<SWidget> FTedsOutlinerTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner,
	const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return TedsOutlinerImpl.IsValid() ? TedsOutlinerImpl.Pin()->CreateLabelWidgetForItem(RowHandle, InRow, CanInteract()) : SNullWidget::NullWidget;
}

void FTedsOutlinerTreeItem::GenerateContextMenu(UToolMenu* Menu, SSceneOutliner& Outliner)
{
	using namespace UE::Editor::DataStorage;
	ICoreProvider* DataStorageInterface = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName);

	if (FTedsOutlinerContextMenuColumn* Column = DataStorageInterface->GetColumn<FTedsOutlinerContextMenuColumn>(RowHandle))
	{
		if (Column->OnCreateContextMenu.IsBound())
		{
			Column->OnCreateContextMenu.Execute(Menu, Outliner);
		}
	}
}

DataStorage::RowHandle FTedsOutlinerTreeItem::GetRowHandle() const
{
	return RowHandle;
}
} // namespace UE::Editor::Outliner

#undef LOCTEXT_NAMESPACE