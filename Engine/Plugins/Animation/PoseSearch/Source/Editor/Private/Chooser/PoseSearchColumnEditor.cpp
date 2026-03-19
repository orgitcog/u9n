// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchColumnEditor.h"

#include <ChooserColumnHeader.h>

#include "PoseSearch/Chooser/PoseSearchChooserColumn.h"
#include "SPropertyAccessChainWidget.h"
#include "ObjectChooserWidgetFactories.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "PoseSearchColumnEditor"

namespace UE::PoseSearchEditor
{

TSharedRef<SWidget> CreatePoseSearchColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FPoseSearchColumn* PoseSearchColumn = static_cast<FPoseSearchColumn*>(Column);
	
	if (Row == UE::ChooserEditor::ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	if (Row == UE::ChooserEditor::ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		const FSlateBrush* ColumnIcon = FAppStyle::Get().GetBrush("Icons.Search");
		const FText ColumnTooltip = LOCTEXT("Pose Match Tooltip", "Pose Match: Selects a single result based on the animation with the best matching pose, and outputs the StartTime for the frame with that pose. Animation Assets must contain \"Pose Match Branch In\" Notify State. AutoPopulate will fill in Column data with result Animation Assets.");
		const FText ColumnName = LOCTEXT("Pose Match","Pose Match");
        		
		TSharedPtr<SWidget> DebugWidget = nullptr;

		return UE::ChooserEditor::MakeColumnHeaderWidget(Chooser, Column, ColumnName, ColumnTooltip, ColumnIcon, DebugWidget);
	}

	// create cell widget
	return
		SNew(STextBlock)
			.Text_Lambda([Row, PoseSearchColumn]()
			{
				if (PoseSearchColumn->RowValues.IsValidIndex(Row))
				{
					if (UObject* ResultAsset = PoseSearchColumn->RowValues[Row].ResultAsset)
					{
						return FText::FromString(ResultAsset->GetName());
					}
				}
				return FText();
			});
}
	
void RegisterPoseSearchChooserWidgets()
{
	UE::ChooserEditor::FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FPoseSearchColumn::StaticStruct(), CreatePoseSearchColumnWidget);
}
	
}

#undef LOCTEXT_NAMESPACE
