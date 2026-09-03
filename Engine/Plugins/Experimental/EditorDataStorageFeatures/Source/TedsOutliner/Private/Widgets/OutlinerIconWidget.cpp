// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/OutlinerIconWidget.h"

#include "Elements/Columns/TypedElementOverrideColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Framework/TypedElementAttributeBinding.h"
#include "TedsTableViewerUtils.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"

#define LOCTEXT_NAMESPACE "OutlinerIconWidget"

namespace UE::OutlinerIconWidget::Local
{
	static const FSlateBrush* GetOverrideBadgeFirstLayer(const EOverriddenState& OverriddenState)
	{
		switch (OverriddenState)
		{
		case EOverriddenState::Added:
			return FAppStyle::GetBrush("SceneOutliner.OverrideAddedBase");

		case EOverriddenState::AllOverridden:
			// Not implemented yet
			break;

		case EOverriddenState::HasOverrides:
			return FAppStyle::GetBrush("SceneOutliner.OverrideInsideBase");

		case EOverriddenState::NoOverrides:
			// No icon for no overrides
			break;

		case EOverriddenState::SubObjectsHasOverrides:
			// Not implemented yet
			break;
		}

		return FAppStyle::GetBrush("NoBrush");
	}

	static const FSlateBrush* GetOverrideBadgeSecondLayer(const EOverriddenState& OverriddenState)
	{
		switch (OverriddenState)
		{
		case EOverriddenState::Added:
			return FAppStyle::GetBrush("SceneOutliner.OverrideAdded");

		case EOverriddenState::AllOverridden:
			// Not implemented yet
			break;

		case EOverriddenState::HasOverrides:
			return FAppStyle::GetBrush("SceneOutliner.OverrideInside");

		case EOverriddenState::NoOverrides:
			// No icon for no overrides
			break;

		case EOverriddenState::SubObjectsHasOverrides:
			// Not implemented yet
			break;
		}

		return FAppStyle::GetBrush("NoBrush");
	}

	static FText GetOverrideTooltip(const EOverriddenState& OverriddenState)
	{
		switch (OverriddenState)
		{
		case EOverriddenState::Added:
			return LOCTEXT("OverrideAddedTooltip", "This entity has been added.");

		case EOverriddenState::AllOverridden:
			// Not implemented yet
			break;

		case EOverriddenState::HasOverrides:
			return LOCTEXT("OverrideInsideTooltip", "At least one property or child has an override.");

		case EOverriddenState::NoOverrides:
			// No icon for no overrides
			break;

		case EOverriddenState::SubObjectsHasOverrides:
			// Not implemented yet
			break;
		}

		return FText::GetEmpty();
	}
}

void UOutlinerIconWidgetFactory::RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
	UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorageUi.RegisterWidgetFactory<FOutlinerIconWidgetConstructor>(
		DataStorageUi.FindPurpose(IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", "Icon").GeneratePurposeID()),
		TColumn<FTypedElementClassTypeInfoColumn>() || TColumn<FTypedElementScriptStructTypeInfoColumn>());
}

FOutlinerIconWidgetConstructor::FOutlinerIconWidgetConstructor()
	: Super(FOutlinerIconWidgetConstructor::StaticStruct())
{
}

TSharedPtr<SWidget> FOutlinerIconWidgetConstructor::CreateWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
	UE::Editor::DataStorage::IUiProvider* DataStorageUi, UE::Editor::DataStorage::RowHandle TargetRow,
	UE::Editor::DataStorage::RowHandle WidgetRow, const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	using namespace UE::Editor::DataStorage;
	if (!DataStorage->IsRowAvailable(TargetRow))
	{
		return SNullWidget::NullWidget;
	}

	FAttributeBinder Binder(TargetRow, DataStorage);

	TSharedRef<SLayeredImage> LayeredImageWidget = SNew(SLayeredImage)
		.Image(TableViewerUtils::GetIconForRow(DataStorage, TargetRow))
		.ToolTipText(Binder.BindData(&FObjectOverrideColumn::OverriddenState, [](const EOverriddenState& OverriddenState)
			{
				return UE::OutlinerIconWidget::Local::GetOverrideTooltip(OverriddenState);
			}))
		.ColorAndOpacity(FSlateColor::UseForeground());

	LayeredImageWidget->AddLayer(Binder.BindData(&FObjectOverrideColumn::OverriddenState, [](const EOverriddenState& OverriddenState)
		{
			return UE::OutlinerIconWidget::Local::GetOverrideBadgeFirstLayer(OverriddenState);
		}));

	LayeredImageWidget->AddLayer(Binder.BindData(&FObjectOverrideColumn::OverriddenState, [](const EOverriddenState& OverriddenState)
		{
			return UE::OutlinerIconWidget::Local::GetOverrideBadgeSecondLayer(OverriddenState);
		}));

	return LayeredImageWidget;
}

#undef LOCTEXT_NAMESPACE
