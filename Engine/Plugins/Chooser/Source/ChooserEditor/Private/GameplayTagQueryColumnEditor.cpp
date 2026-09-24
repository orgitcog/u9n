// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagQueryColumnEditor.h"

#include <ChooserColumnHeader.h>

#include "GameplayTagQueryColumn.h"
#include "ObjectChooserWidgetFactories.h"
#include "SGameplayTagQueryEntryBox.h"
#include "SGameplayTagWidget.h"
#include "SSimpleComboButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "FGameplayTagQueryColumnEditor"

namespace UE::ChooserEditor
{

TSharedRef<SWidget> CreateGameplayTagQueryColumnWidget(UChooserTable* Chooser, FChooserColumnBase* Column, int Row)
{
	FGameplayTagQueryColumn* GameplayTagQueryColumn = static_cast<struct FGameplayTagQueryColumn*>(Column);
	
	if (Row == ColumnWidget_SpecialIndex_Fallback)
	{
		return SNullWidget::NullWidget;
	}
	else if (Row == ColumnWidget_SpecialIndex_Header)
	{
		// create column header widget
		const FSlateBrush* ColumnIcon = FCoreStyle::Get().GetBrush("Icons.Filter");
		const FText ColumnTooltip = LOCTEXT("Gameplay Tag Query Tooltip", "Gameplay Tag Query: cells pass if the input gameplay tag collection matches the query specified in the column properties. Note that empty queries never pass.");
		const FText ColumnName = LOCTEXT("Gameplay Tag Query","Gameplay Tag Query");
		
		TSharedPtr<SWidget> DebugWidget = nullptr;
		if (Chooser->GetEnableDebugTesting())
		{
			DebugWidget = SNew(SSimpleComboButton)
				.IsEnabled_Lambda([Chooser]()
				{
					 return !Chooser->HasDebugTarget();
				})
				.Text_Lambda([GameplayTagQueryColumn]()
				{
					FText Text = FText::FromString(GameplayTagQueryColumn->TestValue.ToStringSimple(false));
					if (Text.IsEmpty())
					{
						Text = LOCTEXT("None", "None");
					}
					return Text;
				})	
				.OnGetMenuContent_Lambda([Chooser, GameplayTagQueryColumn]()
				{
					TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum> EditableContainers;
					EditableContainers.Emplace(Chooser, &(GameplayTagQueryColumn->TestValue));
					return TSharedRef<SWidget>(SNew(SGameplayTagWidget, EditableContainers));
				});
		}

		return MakeColumnHeaderWidget(Chooser, Column, ColumnName, ColumnTooltip, ColumnIcon, DebugWidget);
	}

	// create cell widget
	return SNew(SGameplayTagQueryEntryBox)
		.TagQuery_Lambda([GameplayTagQueryColumn, Row]()
		{
			if (Row < GameplayTagQueryColumn->RowValues.Num())
			{
				return GameplayTagQueryColumn->RowValues[Row];
			}
			return FGameplayTagQuery::EmptyQuery;
		})
		.ReadOnly(false)
		.OnTagQueryChanged_Lambda([GameplayTagQueryColumn, Row](const FGameplayTagQuery& UpdatedQuery)
		{
			if (Row < GameplayTagQueryColumn->RowValues.Num())
			{
				GameplayTagQueryColumn->RowValues[Row] = UpdatedQuery;
			}
		});
}
	
void RegisterGameplayTagQueryWidgets()
{
	FObjectChooserWidgetFactories::RegisterColumnWidgetCreator(FGameplayTagQueryColumn::StaticStruct(), CreateGameplayTagQueryColumnWidget);
	// No need to make and register a creator for gameplay tag containers - it's already registered in GameplayTagColumnEditor
}
	
}

#undef LOCTEXT_NAMESPACE
