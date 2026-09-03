// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Elements/Common/TypedElementQueryConditions.h"

#include "ExportedTextWidget.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

class UScriptStruct;

UCLASS()
class TEDSUI_API UExportedTextWidgetFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	virtual ~UExportedTextWidgetFactory() override = default;

	virtual void RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
		UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};

USTRUCT()
struct TEDSUI_API FExportedTextWidgetConstructor : public FSimpleWidgetConstructor
{
	GENERATED_BODY()

public:
	FExportedTextWidgetConstructor();
	virtual ~FExportedTextWidgetConstructor() override = default;

	virtual TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;
	virtual const UE::Editor::DataStorage::Queries::FConditions* GetQueryConditions(const UE::Editor::DataStorage::ICoreProvider* Storage) const override;

	virtual FText CreateWidgetDisplayNameText(
		UE::Editor::DataStorage::ICoreProvider* DataStorage, UE::Editor::DataStorage::RowHandle Row) const override;
	
	virtual TSharedPtr<SWidget> CreateWidget(
		UE::Editor::DataStorage::ICoreProvider* DataStorage,
		UE::Editor::DataStorage::IUiProvider* DataStorageUi,
		UE::Editor::DataStorage::RowHandle TargetRow,
		UE::Editor::DataStorage::RowHandle WidgetRow,
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;

protected:
	// The column this exported text widget is operating on
	UE::Editor::DataStorage::Queries::FConditions MatchedColumn;
};

USTRUCT(meta = (DisplayName = "Exported text widget"))
struct TEDSUI_API FExportedTextWidgetTag : public FEditorDataStorageTag
{
	GENERATED_BODY()
};