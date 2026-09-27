// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementDataStorageFactory.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"

#include "PackagePathWidget.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
} // namespace UE::Editor::DataStorage

class SWidget;
class UScriptStruct;

UCLASS()
class TEDSUI_API UPackagePathWidgetFactory : public UEditorDataStorageFactory
{
	GENERATED_BODY()

public:
	~UPackagePathWidgetFactory() override = default;

	void RegisterWidgetConstructors(UE::Editor::DataStorage::ICoreProvider& DataStorage,
		UE::Editor::DataStorage::IUiProvider& DataStorageUi) const override;
};

USTRUCT()
struct TEDSUI_API FPackagePathWidgetConstructor : public FTypedElementWidgetConstructor
{
	GENERATED_BODY()

public:
	FPackagePathWidgetConstructor();
	~FPackagePathWidgetConstructor() override = default;

protected:
	explicit FPackagePathWidgetConstructor(const UScriptStruct* InTypeInfo);

	TSharedPtr<SWidget> CreateWidget(const UE::Editor::DataStorage::FMetaDataView& Arguments) override;
	bool FinalizeWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
		UE::Editor::DataStorage::IUiProvider* DataStorageUi,
		UE::Editor::DataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};

USTRUCT()
struct TEDSUI_API FLoadedPackagePathWidgetConstructor : public FPackagePathWidgetConstructor
{
	GENERATED_BODY()

public:
	FLoadedPackagePathWidgetConstructor();
	~FLoadedPackagePathWidgetConstructor() override = default;

protected:
	bool FinalizeWidget(UE::Editor::DataStorage::ICoreProvider* DataStorage,
		UE::Editor::DataStorage::IUiProvider* DataStorageUi,
		UE::Editor::DataStorage::RowHandle Row, const TSharedPtr<SWidget>& Widget) override;
};
