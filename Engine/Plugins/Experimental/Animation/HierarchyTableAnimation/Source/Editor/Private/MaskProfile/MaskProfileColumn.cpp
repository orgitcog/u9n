// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaskProfile/MaskProfileColumn.h"
#include "Widgets/Input/SSpinBox.h"
#include "MaskProfile/HierarchyTableTypeMask.h"
#include "HierarchyTable.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "FHierarchyTableColumn_Mask"

TSharedRef<SWidget> FHierarchyTableColumn_Mask::CreateEntryWidget(TObjectPtr<UHierarchyTable> HierarchyTable, int32 EntryIndex)
{
	return SNew(SSpinBox<float>)
		.IsEnabled_Lambda([HierarchyTable, EntryIndex]() { return HierarchyTable->GetTableEntry(EntryIndex)->IsOverridden(); })
		.MinDesiredWidth(100.0f)
		.MinValue(0.0f)
		.MaxValue(1.0f)
		.Value_Lambda([HierarchyTable, EntryIndex]()
			{
				return HierarchyTable->GetTableEntry(EntryIndex)->GetValue<FHierarchyTable_ElementType_Mask>()->Value;
			})
		.OnValueChanged_Lambda([HierarchyTable, EntryIndex](float NewValue)
			{
				HierarchyTable->GetMutableTableEntry(EntryIndex)->GetMutableValue<FHierarchyTable_ElementType_Mask>()->Value = NewValue;
			})
		.OnBeginSliderMovement_Lambda([HierarchyTable]()
			{
				GEditor->BeginTransaction(LOCTEXT("SetMaskValue", "Set Mask Value"));
				HierarchyTable->Modify();
			})
		.OnEndSliderMovement_Lambda([](float)
			{
				GEditor->EndTransaction();
			});
}

TSharedRef<SWidget> FHierarchyTableColumn_Mask::CreateHeaderWidget()
{
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE