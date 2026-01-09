// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerFolderLabelWidget.h"

#include "Columns/ActorFolderColumns.h"
#include "Columns/SlateDelegateColumns.h"
#include "Columns/TedsOutlinerColumns.h"
#include "Compatibility/SceneOutlinerTedsBridge.h"
#include "Elements/Columns/TypedElementCompatibilityColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementFolderColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "ISceneOutliner.h"
#include "SceneOutlinerHelpers.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "FOutlinerFolderLabelWidgetConstructor"


void UOutlinerFolderLabelWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;
	DataStorageUi.RegisterWidgetFactory<FOutlinerFolderLabelWidgetConstructor>(
		DataStorageUi.FindPurpose(IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", NAME_None).GeneratePurposeID()),
		TColumn<FTypedElementLabelColumn>() && TColumn<FFolderTag>());
}

FOutlinerFolderLabelWidgetConstructor::FOutlinerFolderLabelWidgetConstructor()
	: FSimpleWidgetConstructor(StaticStruct())
{
}

TSharedPtr<SWidget> FOutlinerFolderLabelWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	using namespace UE::Editor::DataStorage;
	if (DataStorage->IsRowAvailable(TargetRow))
	{
		FAttributeBinder Binder(TargetRow, DataStorage);

		

		return SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(16.0f)
				.HeightOverride(16.0f)
				[
					SNew(SImage)
						.Image_Lambda([DataStorage, TargetRow, WidgetRow]()
						{
							bool bIsExpanded = DataStorage->HasColumns<FFolderExpandedTag>(TargetRow);

							TWeakPtr<ISceneOutlinerTreeItem> OutlinerTreeItem = GetTreeItemForRow(DataStorage, TargetRow, WidgetRow);
							
							// If this item does not have any children, we want to treat it as not expanded and show the closed folder icon
							if (TSharedPtr<ISceneOutlinerTreeItem> Item = OutlinerTreeItem.Pin())
							{
								bIsExpanded &= !Item->GetChildren().IsEmpty();
							}

							return bIsExpanded
								? FAppStyle::GetBrush("SceneOutliner.FolderOpen")
								: FAppStyle::GetBrush("SceneOutliner.FolderClosed");
						})
						// Disabled for now since folder colors are not properly supported yet
						//.ColorAndOpacity(Binder.BindData(&FSlateColorColumn::Color))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SSpacer)
					.Size(FVector2D(6.0f, 0.0f))
			]
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				CreateLabel(DataStorage, TargetRow, WidgetRow, Arguments)
			];
	}
	else
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("MissingRowReferenceColumn", "Unable to retrieve row reference."));
	}
}

TSharedRef<SWidget> FOutlinerFolderLabelWidgetConstructor::CreateLabel(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	using namespace UE::Editor::DataStorage;
	TSharedRef<SWidget> Result = SNullWidget::NullWidget;
	
	const bool* IsEditable = Arguments.FindForColumn<FTypedElementLabelColumn>(IsEditableName).TryGetExact<bool>();

	if (IsEditable && *IsEditable)
	{
		FAttributeBinder WidgetRowBinder(WidgetRow, DataStorage);
			
		TSharedPtr<SInlineEditableTextBlock> TextBlock = SNew(SInlineEditableTextBlock)
			.OnTextCommitted_Lambda(
				[DataStorage, TargetRow](const FText& NewText, ETextCommit::Type CommitInfo)
				{
					// This callback happens on the game thread so it's safe to directly call into the data storage.
					FString NewLabelText = NewText.ToString();
					if (FTypedElementLabelHashColumn* LabelHashColumn = DataStorage->GetColumn<FTypedElementLabelHashColumn>(TargetRow))
					{
						LabelHashColumn->LabelHash = CityHash64(reinterpret_cast<const char*>(*NewLabelText), NewLabelText.Len() * sizeof(**NewLabelText));
					}
					if (FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
					{
						LabelColumn->Label = MoveTemp(NewLabelText);
					}
					DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(TargetRow);
				})
			.OnVerifyTextChanged_Lambda([DataStorage, TargetRow](const FText& Label, FText& ErrorMessage)
				{
					const FFolderCompatibilityColumn* FolderCompatibilityColumn = DataStorage->GetColumn<FFolderCompatibilityColumn>(TargetRow);
					const FTypedElementWorldColumn* WorldColumn = DataStorage->GetColumn<FTypedElementWorldColumn>(TargetRow);

					if (FolderCompatibilityColumn && WorldColumn)
					{
						return SceneOutliner::FSceneOutlinerHelpers::ValidateFolderName(FolderCompatibilityColumn->Folder, WorldColumn->World.Get(),
							Label, ErrorMessage);
					}

					ErrorMessage = LOCTEXT("MissingColumns", "Could not find folder information to rename.");
				
					return false;
				})
			.Text_Static(&FOutlinerFolderLabelWidgetConstructor::GetDisplayText, DataStorage, TargetRow, WidgetRow)
			.ToolTipText_Static(&FOutlinerFolderLabelWidgetConstructor::GetTooltipText, DataStorage, TargetRow)
			.ColorAndOpacity_Static(&FOutlinerFolderLabelWidgetConstructor::GetForegroundColor, DataStorage, TargetRow, WidgetRow)
			.IsSelected(WidgetRowBinder.BindEvent(&FExternalWidgetSelectionColumn::IsSelected))
			.OnEnterEditingMode_Lambda([DataStorage, WidgetRow]()
			{
				DataStorage->AddColumn<FIsInEditingModeTag>(WidgetRow);
			})
			.OnExitEditingMode_Lambda([DataStorage, WidgetRow]()
			{
				DataStorage->RemoveColumn<FIsInEditingModeTag>(WidgetRow);
			});

		DataStorage->AddColumn<FWidgetEnterEditModeColumn>(TargetRow, FWidgetEnterEditModeColumn{
				.OnEnterEditMode = FSimpleDelegate::CreateLambda([TextBlock]()
				{
					TextBlock->EnterEditingMode();
				})
			});

		Result = TextBlock.ToSharedRef();
	}
	else
	{
		FAttributeBinder TargetRowBinder(TargetRow, DataStorage);
		
		TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
			.IsEnabled(false)
			.Text(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label))
			.ToolTipText(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label));
		
		Result = TextBlock.ToSharedRef();
	}

	return Result;
}

FText FOutlinerFolderLabelWidgetConstructor::GetDisplayText(UE::Editor::DataStorage::ICoreProvider* DataStorage, UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow)
{
	if (const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
	{
		FText Label = FText::FromString(LabelColumn->Label);
		
		const FFolderCompatibilityColumn* FolderCompatibilityColumn = DataStorage->GetColumn<FFolderCompatibilityColumn>(TargetRow);
		const FTypedElementWorldColumn* WorldColumn = DataStorage->GetColumn<FTypedElementWorldColumn>(TargetRow);
		const bool IsInEditingMode = DataStorage->HasColumns<FIsInEditingModeTag>(WidgetRow);

		if (FolderCompatibilityColumn && WorldColumn)
		{
			if (!IsInEditingMode)
			{
				FText IsCurrentSuffixText =
					SceneOutliner::FSceneOutlinerHelpers::IsFolderCurrent(FolderCompatibilityColumn->Folder, WorldColumn->World.Get())
						?	FText(LOCTEXT("IsCurrentSuffix", " (Current)"))
						: FText::GetEmpty();
				
				return FText::Format(LOCTEXT("LevelInstanceDisplay", "{0}{1}"), Label, IsCurrentSuffixText);
			}
		}

		return Label;
	}

	return FText();
}

FText FOutlinerFolderLabelWidgetConstructor::GetTooltipText(UE::Editor::DataStorage::ICoreProvider* DataStorage, UE::Editor::DataStorage::RowHandle TargetRow)
{
	const FFolderCompatibilityColumn* FolderCompatibilityColumn = DataStorage->GetColumn<FFolderCompatibilityColumn>(TargetRow);
	const FTypedElementWorldColumn* WorldColumn = DataStorage->GetColumn<FTypedElementWorldColumn>(TargetRow);
	const FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow);

	if (LabelColumn && WorldColumn && FolderCompatibilityColumn)
	{
		FText Description = SceneOutliner::FSceneOutlinerHelpers::IsFolderCurrent(FolderCompatibilityColumn->Folder, WorldColumn->World.Get())
				? LOCTEXT("ActorFolderIsCurrentDescription", "\nThis is your current folder. New actors you create will appear here.")
				: FText::GetEmpty();
	
		return FText::Format(LOCTEXT("DataLayerTooltipText", "{0}{1}"), FText::FromString(LabelColumn->Label), Description);
	}
	return FText();
}

FSlateColor FOutlinerFolderLabelWidgetConstructor::GetForegroundColor(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow)
{
	// CommonLabelData has some Outliner specific color logic
	TWeakPtr<ISceneOutlinerTreeItem> OutlinerTreeItem = GetTreeItemForRow(DataStorage, TargetRow, WidgetRow);
	
	if (TSharedPtr<ISceneOutlinerTreeItem> Item = OutlinerTreeItem.Pin())
	{
		FSceneOutlinerCommonLabelData CommonLabelData;
		CommonLabelData.WeakSceneOutliner = Item->WeakSceneOutliner;

		if (TOptional<FLinearColor> BaseColor = CommonLabelData.GetForegroundColor(*Item))
		{
			return BaseColor.GetValue();
		}
		
		const FFolderCompatibilityColumn* FolderCompatibilityColumn = DataStorage->GetColumn<FFolderCompatibilityColumn>(TargetRow);
		const FTypedElementWorldColumn* WorldColumn = DataStorage->GetColumn<FTypedElementWorldColumn>(TargetRow);

		if (FolderCompatibilityColumn && WorldColumn)
		{
			if (SceneOutliner::FSceneOutlinerHelpers::IsFolderCurrent(FolderCompatibilityColumn->Folder, WorldColumn->World.Get()))
			{
				return FAppStyle::Get().GetSlateColor("Colors.AccentGreen");
			}
		}
	}

	return FSlateColor::UseForeground();
}

TWeakPtr<ISceneOutlinerTreeItem> FOutlinerFolderLabelWidgetConstructor::GetTreeItemForRow(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow, UE::Editor::DataStorage::RowHandle WidgetRow)
{
	if (FTedsOutlinerColumn* TedsOutlinerColumn = DataStorage->GetColumn<FTedsOutlinerColumn>(WidgetRow))
	{
		if (TSharedPtr<ISceneOutliner> Outliner = TedsOutlinerColumn->Outliner.Pin())
		{
			FSceneOutlinerTedsQueryBinder& Binder = FSceneOutlinerTedsQueryBinder::GetInstance();
          
			if (FTreeItemIDDealiaser Dealiaser = Binder.GetTreeItemIDDealiaser(Outliner);
			   Dealiaser.IsBound())
			{
				return Outliner->GetTreeItem(Dealiaser.Execute(TargetRow));
			}
		}
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE // "FOutlinerFolderLabelWidgetConstructor"
