// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerTextWidget.h"

#include "ActorEditorUtils.h"
#include "Columns/SlateDelegateColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiStyleOverrideCapability.h"
#include "Settings/EditorStyleSettings.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "OutlinerTextWidget"

void UOutlinerTextWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorageUi.RegisterWidgetFactory<FOutlinerTextWidgetConstructor>(DataStorageUi.FindPurpose(
		IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Text").GeneratePurposeID()),
		TColumn<FTypedElementLabelColumn>());
}

FOutlinerTextWidgetConstructor::FOutlinerTextWidgetConstructor()
	: Super(FOutlinerTextWidgetConstructor::StaticStruct())
{
}

FOutlinerTextWidgetConstructor::FOutlinerTextWidgetConstructor(const UScriptStruct* InTypeInfo)
	: Super(InTypeInfo)
{
}

TSharedPtr<SWidget> FOutlinerTextWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi,
	UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow, const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	using namespace UE::Editor::DataStorage;
	if (!DataStorage->IsRowAvailable(TargetRow))
	{
		return SNew(STextBlock)
			.Text(LOCTEXT("MissingRowReferenceColumn", "Unable to retrieve row reference."));
	}

	TSharedPtr<SWidget> Result = SNullWidget::NullWidget;

	const bool* IsEditable = Arguments.FindForColumn<FTypedElementLabelColumn>(IsEditableName).TryGetExact<bool>();

	if (IsEditable && *IsEditable)
	{
		Result = CreateEditableWidget(DataStorage, DataStorageUi, TargetRow, WidgetRow, Arguments);
	}
	else
	{
		Result = CreateNonEditableWidget(DataStorage, DataStorageUi, TargetRow, WidgetRow, Arguments);
	}
	return Result;
}

void FOutlinerTextWidgetConstructor::OnCommitText(
	const FText& NewText,
	ETextCommit::Type,
	UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow)
{
	FString NewLabelText = NewText.ToString();

	// This callback happens on the game thread so it's safe to directly call into the data storage.
	if (FTypedElementLabelHashColumn* LabelHashColumn = DataStorage->GetColumn<FTypedElementLabelHashColumn>(TargetRow))
	{
		LabelHashColumn->LabelHash = CityHash64(reinterpret_cast<const char*>(*NewLabelText), NewLabelText.Len() * sizeof(**NewLabelText));
	}
	if (FTypedElementLabelColumn* LabelColumn = DataStorage->GetColumn<FTypedElementLabelColumn>(TargetRow))
	{
		LabelColumn->Label = MoveTemp(NewLabelText);
	}
	DataStorage->AddColumn<FTypedElementSyncBackToWorldTag>(TargetRow);
}

bool FOutlinerTextWidgetConstructor::OnVerifyText(const FText& Label, FText& ErrorMessage)
{
	// Note: The use of actor specific functionality should be minimized, but this function acts generic enough that the 
	// use of actor is just in names.
	return FActorEditorUtils::ValidateActorName(Label, ErrorMessage);
}

TSharedPtr<SWidget> FOutlinerTextWidgetConstructor::CreateEditableWidget(
	UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi,
	UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	return CreateEditableTextBlock(DataStorage, TargetRow, WidgetRow);
}

TSharedPtr<SWidget> FOutlinerTextWidgetConstructor::CreateNonEditableWidget(
	UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi,
	UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	return CreateNonEditableTextBlock(DataStorage, TargetRow, WidgetRow);
}

TSharedPtr<SInlineEditableTextBlock> FOutlinerTextWidgetConstructor::CreateEditableTextBlock(
	UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow) const
{
	using namespace UE::Editor::DataStorage;
	FAttributeBinder TargetRowBinder(TargetRow, DataStorage);
	FAttributeBinder WidgetRowBinder(WidgetRow, DataStorage);

	const bool bShouldUseMiddleEllipsis = GetDefault<UEditorStyleSettings>()->bEnableMiddleEllipsis;

	TSharedPtr<SInlineEditableTextBlock> TextBlock = SNew(SInlineEditableTextBlock)
		.OnTextCommitted_Static(&FOutlinerTextWidgetConstructor::OnCommitText, DataStorage, TargetRow)
		.OnVerifyTextChanged_Static(&FOutlinerTextWidgetConstructor::OnVerifyText)
		.Text(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label))
		.ToolTipText(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label))
		.OverflowPolicy(bShouldUseMiddleEllipsis ? ETextOverflowPolicy::MiddleEllipsis : TOptional<ETextOverflowPolicy>())
		.IsSelected(WidgetRowBinder.BindEvent(&FExternalWidgetSelectionColumn::IsSelected));

	DataStorage->AddColumn<FWidgetEnterEditModeColumn>(TargetRow, FWidgetEnterEditModeColumn{
		.OnEnterEditMode = FSimpleDelegate::CreateLambda([TextBlock]()
		{
			TextBlock->EnterEditingMode();
		})
	});

	TextBlock->AddMetadata(MakeShared<TTypedElementUiStyleOverrideCapability<SInlineEditableTextBlock>>(*TextBlock));
	return TextBlock;
}

TSharedPtr<STextBlock> FOutlinerTextWidgetConstructor::CreateNonEditableTextBlock(
	UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow) const
{
	using namespace UE::Editor::DataStorage;
	FAttributeBinder TargetRowBinder(TargetRow, DataStorage);

	TSharedPtr<STextBlock> TextBlock = SNew(STextBlock)
		.IsEnabled(false)
		.Text(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label))
		.ToolTipText(TargetRowBinder.BindText(&FTypedElementLabelColumn::Label));

	return TextBlock;
}

#undef LOCTEXT_NAMESPACE
