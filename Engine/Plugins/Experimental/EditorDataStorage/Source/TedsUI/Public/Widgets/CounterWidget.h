// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Framework/Text/TextLayout.h"
#include "Internationalization/Text.h"

#include "CounterWidget.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
	class IUiProvider;
} // namespace UE::Editor::DataStorage

class SWindow;
class UScriptStruct;

UCLASS()
class TEDSUI_API UCounterWidgetFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	static UE::Editor::DataStorage::IUiProvider::FPurposeID LevelEditorWidgetPurpose;

	UCounterWidgetFactory();
	~UCounterWidgetFactory() override = default;

	void RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage) override;
	void RegisterWidgetPurposes(UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
	void RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
		UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;

	static void EnableCounterWidgets();

private:
	static void SetupMainWindowIntegrations(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog);

	static bool bAreCounterWidgetsEnabled;
	static bool bHasBeenSetup;
};

/**
 * Constructor for the counter widget. The counter widget accepts a "count"-query. The query will be periodically
 * run and the result is written to a textbox widget after it's been formatted using LabelText. An example for 
 * LabelText is "{0} {0}|plural(one=MyCounter, other=MyCounters)" which will use "MyCounter" if there's exactly one
 * entry found and otherwise "MyCounters".
 */
USTRUCT()
struct TEDSUI_API FCounterWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FCounterWidgetConstructor();
	~FCounterWidgetConstructor() override = default;

	TConstArrayView<const UScriptStruct*> GetAdditionalColumnsList() const override;

	FText ToolTipText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Tooltip", "Shows the total number found in the editor.") };
	FText LabelText{ NSLOCTEXT("TypedElementUI_CounterWidget", "Label", "Counted") };
	UE::Editor::DataStorage::QueryHandle Query;

protected:
	TSharedPtr<SWidget> CreateWidget(const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
	bool SetColumns(UE::Editor::DataStorage::ICoreProvider* DataStorage, UE::Editor::DataStorage::RowHandle Row) override;
};

USTRUCT(meta = (DisplayName = "Counter widget"))
struct TEDSUI_API FCounterWidgetColumn : public FEditorDataStorageColumn
{
	GENERATED_BODY()

	FTextFormat LabelTextFormatter;
	UE::Editor::DataStorage::QueryHandle Query;
};