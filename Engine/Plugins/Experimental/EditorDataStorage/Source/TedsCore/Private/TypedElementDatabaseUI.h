// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Elements/Common/TypedElementQueryConditions.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "Logging/LogMacros.h"
#include "UObject/ObjectMacros.h"

#include "TypedElementDatabaseUI.generated.h"

namespace UE::Editor::DataStorage
{
	class ICoreProvider;
	class ICompatibilityProvider;
} // namespace UE::Editor::DataStorage


TEDSCORE_API DECLARE_LOG_CATEGORY_EXTERN(LogEditorDataStorageUI, Log, All);

UCLASS()
class TEDSCORE_API UEditorDataStorageUi final
	: public UObject
	, public UE::Editor::DataStorage::IUiProvider
{
	GENERATED_BODY()

public:
	~UEditorDataStorageUi() override = default;

	void Initialize(
		UE::Editor::DataStorage::ICoreProvider* StorageInterface, 
		UE::Editor::DataStorage::ICompatibilityProvider* StorageCompatibilityInterface);
	void Deinitialize();

	virtual UE::Editor::DataStorage::RowHandle RegisterWidgetPurpose(const FPurposeID& PurposeID, const FPurposeInfo& InPurposeInfo) override;
	virtual UE::Editor::DataStorage::RowHandle RegisterWidgetPurpose(const FPurposeInfo& InPurposeInfo) override;

	virtual bool RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow, const UScriptStruct* Constructor) override;
	virtual bool RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow, const UScriptStruct* Constructor,
		UE::Editor::DataStorage::Queries::FConditions Columns) override;
	virtual bool RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow,
		TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) override;
	virtual bool RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, 
		UE::Editor::DataStorage::Queries::FConditions Columns) override;
	
	virtual void CreateWidgetConstructors(UE::Editor::DataStorage::RowHandle PurposeRow, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;
	virtual void CreateWidgetConstructors(UE::Editor::DataStorage::RowHandle PurposeRow, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;
	virtual void ConstructWidgets(UE::Editor::DataStorage::RowHandle PurposeRow, const UE::Editor::DataStorage::FMetaDataView& Arguments,
			const WidgetCreatedCallback& ConstructionCallback) override;

	// Deprecated FName overloads
	void RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description) override;
	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor,
		UE::Editor::DataStorage::Queries::FConditions Columns) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor) override;
	bool RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor,
		UE::Editor::DataStorage::Queries::FConditions Columns) override;
	void CreateWidgetConstructors(FName Purpose,
		const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;
	void CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback) override;
	void ConstructWidgets(FName Purpose, const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback) override;
	// ~FName overloads
	
	TSharedPtr<SWidget> ConstructWidget(UE::Editor::DataStorage::RowHandle Row, FTypedElementWidgetConstructor& Constructor,
		const UE::Editor::DataStorage::FMetaDataView& Arguments) override;

	void ListWidgetPurposes(const WidgetPurposeCallback& Callback) const override;

	bool SupportsExtension(FName Extension) const override;
	void ListExtensions(TFunctionRef<void(FName)> Callback) const override;

	/** Create the container widget that every TEDS UI widget is stored in */
	virtual TSharedPtr<UE::Editor::DataStorage::ITedsWidget> CreateContainerTedsWidget(UE::Editor::DataStorage::RowHandle UiRowHandle) const override;
	
	/** Get the table where TEDS UI widgets are stored */
	virtual UE::Editor::DataStorage::TableHandle GetWidgetTable() const override;
	
	/** Get the ID of the default TEDS UI widget purpose used to register default widgets for different types of data (e.g FText -> STextBlock) */
	virtual FPurposeID GetDefaultWidgetPurposeID() const;
	
	/** Get the ID of the general TEDS UI purpose used to register general purpose widgets for columns */
	virtual FPurposeID GetGeneralWidgetPurposeID() const;
	
	/** Find the row handle for a purpose by looking it up using the purpose ID */
	virtual UE::Editor::DataStorage::RowHandle FindPurpose(const FPurposeID& PurposeID) const override;

private:
	void CreateStandardArchetypes();
	void RegisterQueries();

	bool CreateSingleWidgetConstructor(
		UE::Editor::DataStorage::RowHandle FactoryRow,
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes,
		const WidgetConstructorCallback& Callback);

	void CreateWidgetInstance(
		FTypedElementWidgetConstructor& Constructor,
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	void CreateWidgetInstance(
		const UScriptStruct* ConstructorType,
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetCreatedCallback& ConstructionCallback);

	bool CreateWidgetConstructors_LongestMatch(
		const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories, 
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, 
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_ExactMatch(
		const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);
	void CreateWidgetConstructors_SingleMatch(
		const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories,
		TArray<TWeakObjectPtr<const UScriptStruct>>& Columns,
		const UE::Editor::DataStorage::FMetaDataView& Arguments,
		const WidgetConstructorCallback& Callback);

	// Register a widget factory for the provided purpose
	UE::Editor::DataStorage::RowHandle RegisterWidgetFactoryRow(UE::Editor::DataStorage::RowHandle PurposeRowHandle) const;

	// Register a unique factory for the provided purpose, clearing the info if there was any factory previously registered for the purpose
	UE::Editor::DataStorage::RowHandle RegisterUniqueWidgetFactoryRow(UE::Editor::DataStorage::RowHandle PurposeRowHandle) const;

	// Get all factories for the provided purpose
	void GetFactories(UE::Editor::DataStorage::RowHandle PurposeRowHandle, TArray<UE::Editor::DataStorage::RowHandle>& OutFactories) const;

	// The the query conditions for the provided factory
	const UE::Editor::DataStorage::Queries::FConditions& GetFactoryConditions(UE::Editor::DataStorage::RowHandle FactoryRow) const ;

	UE::Editor::DataStorage::TableHandle WidgetTable{ UE::Editor::DataStorage::InvalidTableHandle };
	UE::Editor::DataStorage::TableHandle WidgetPurposeTable{ UE::Editor::DataStorage::InvalidTableHandle };
	UE::Editor::DataStorage::TableHandle WidgetFactoryTable{ UE::Editor::DataStorage::InvalidTableHandle };
	
	UE::Editor::DataStorage::ICoreProvider* Storage{ nullptr };
	UE::Editor::DataStorage::ICompatibilityProvider* StorageCompatibility{ nullptr };
};