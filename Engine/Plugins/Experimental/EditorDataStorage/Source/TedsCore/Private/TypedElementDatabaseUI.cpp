// Copyright Epic Games, Inc. All Rights Reserved.

#include "TypedElementDatabaseUI.h"

#include "Algo/BinarySearch.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/WidgetPurposeColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementSlateWidgetColumns.h"
#include "Elements/Framework/TypedElementColumnUtils.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "GenericPlatform/GenericPlatformMemory.h"
#include "Widgets/SlateControlledConstruction.h"
#include "Widgets/STedsWidget.h"

DEFINE_LOG_CATEGORY(LogEditorDataStorageUI);

#define LOCTEXT_NAMESPACE "TypedElementDatabaseUI"

namespace Internal
{
	// Source: https://en.cppreference.com/w/cpp/utility/variant/visit
	template<class... Ts>
	struct TOverloaded : Ts...
	{ 
		using Ts::operator()...; 
	};

	template<class... Ts> TOverloaded(Ts...) -> TOverloaded<Ts...>;

	// Check if the two columns are equal, or if InRequestedColumn is a dynamic specialization of InMatchedColumn
	bool CheckSingleColumnMatch(const UScriptStruct* InMatchedColumn, const UScriptStruct* InRequestedColumn)
	{
		if (InMatchedColumn == InRequestedColumn)
		{
			return true;
		}
		
		else if (UE::Editor::DataStorage::ColumnUtils::IsDynamicTemplate(InMatchedColumn) &&
			UE::Editor::DataStorage::ColumnUtils::IsDerivedFromDynamicTemplate(InRequestedColumn))
		{
			return InRequestedColumn->IsChildOf(InMatchedColumn);
		}

		return false;
	}

	static const UE::Editor::DataStorage::IUiProvider::FPurposeID DefaultWidgetPurposeID(
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("General", "Cell", "Default").GeneratePurposeID());
	
	static const UE::Editor::DataStorage::IUiProvider::FPurposeID GeneralWidgetPurposeID(
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("General", "Cell", NAME_None).GeneratePurposeID());
	
}

void UEditorDataStorageUi::Initialize(
	UE::Editor::DataStorage::ICoreProvider* StorageInterface,
	UE::Editor::DataStorage::ICompatibilityProvider* StorageCompatibilityInterface)
{
	checkf(StorageInterface, TEXT("TEDS' compatibility manager is being initialized with an invalid storage target."));

	Storage = StorageInterface;
	StorageCompatibility = StorageCompatibilityInterface;
	CreateStandardArchetypes();
	RegisterQueries();
}

void UEditorDataStorageUi::Deinitialize()
{
}

UE::Editor::DataStorage::RowHandle UEditorDataStorageUi::RegisterWidgetPurpose(const FPurposeID& PurposeID, const FPurposeInfo& InPurposeInfo)
{
	using namespace UE::Editor::DataStorage;

	// If a purpose is already registered against this name, let the user know
	const FMapKey Key = FMapKey(PurposeID);
	RowHandle ExistingRow = Storage->LookupMappedRow(Key);
	if (Storage->IsRowAvailable(ExistingRow))
	{
		ensureMsgf(false, TEXT("Existing purpose found registered with name: %s"), *PurposeID.ToString());
		return InvalidRowHandle;
	}

	// Add the row and register the mapping
	RowHandle PurposeRowHandle = Storage->AddRow(WidgetPurposeTable);
	Storage->MapRow(Key, PurposeRowHandle);

	// Setup the relevant columns
	if (FWidgetPurposeColumn* PurposeColumn = Storage->GetColumn<FWidgetPurposeColumn>(PurposeRowHandle))
	{
		PurposeColumn->PurposeType = InPurposeInfo.Type;
		PurposeColumn->PurposeID = PurposeID;
	}

	if (FWidgetPurposeNameColumn* PurposeNameColumn = Storage->GetColumn<FWidgetPurposeNameColumn>(PurposeRowHandle))
	{
		PurposeNameColumn->Namespace = InPurposeInfo.Namespace;
		PurposeNameColumn->Name = InPurposeInfo.Name;
		PurposeNameColumn->Frame = InPurposeInfo.Frame;
	}

	if (!InPurposeInfo.Description.IsEmpty())
	{
		Storage->AddColumn(PurposeRowHandle, FDescriptionColumn{ .Description = InPurposeInfo.Description });
	}

	// If the parent purpose already exists, simply reference it. Otherwise add an unresolved parent column to resolve it later
	if (InPurposeInfo.ParentPurposeID.IsSet())
	{
		RowHandle ParentRowHandle = FindPurpose(InPurposeInfo.ParentPurposeID);

		if (Storage->IsRowAvailable(ParentRowHandle))
		{
			Storage->AddColumn(PurposeRowHandle, FTableRowParentColumn{ .Parent = ParentRowHandle });
		}
		else
		{
			Storage->AddColumn(PurposeRowHandle, FUnresolvedTableRowParentColumn{ .ParentIdKey = InPurposeInfo.ParentPurposeID});
		}
	}
	return PurposeRowHandle;
}

UE::Editor::DataStorage::RowHandle UEditorDataStorageUi::RegisterWidgetPurpose(const FPurposeInfo& InPurposeInfo)
{
	return RegisterWidgetPurpose(InPurposeInfo.GeneratePurposeID(), InPurposeInfo);
}

bool UEditorDataStorageUi::RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow, const UScriptStruct* Constructor)
{
	checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
		TEXT("Attempting to register a widget constructor '%s' that isn't derived from FTypedElementWidgetConstructor."),
		*Constructor->GetFullName());
	
	if (FWidgetPurposeColumn* PurposeInfo = Storage->GetColumn<FWidgetPurposeColumn>(PurposeRow))
	{
		switch (PurposeInfo->PurposeType)
		{
		case EPurposeType::Generic:
			{
				UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
				Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorTypeInfoColumn{.Constructor = Constructor});
				return true;
			}
		case EPurposeType::UniqueByName:
			{
				UE::Editor::DataStorage::RowHandle FactoryRow = RegisterUniqueWidgetFactoryRow(PurposeRow);
				Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorTypeInfoColumn{.Constructor = Constructor});
				return true;
			}
		case EPurposeType::UniqueByNameAndColumn:
			{
				UE_LOG(LogEditorDataStorageUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%llu' requires at least one column for matching."), 
				*Constructor->GetName(), PurposeRow);
				return false;
			}
		default:
			{
				checkf(false, TEXT("Unexpected UE::Editor::DataStorage::IUiProvider::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
	}
	
	UE_LOG(LogEditorDataStorageUI, Warning, 
				TEXT("Unable to register widget factory '%s' as purpose '%llu' isn't registered."), *Constructor->GetName(), PurposeRow);
	return false;
}

bool UEditorDataStorageUi::RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow, const UScriptStruct* Constructor,
	UE::Editor::DataStorage::Queries::FConditions Columns)
{
	if (!Columns.IsEmpty())
	{
		checkf(Constructor->IsChildOf(FTypedElementWidgetConstructor::StaticStruct()),
			TEXT("Attempting to register a widget constructor '%s' that isn't deriving from FTypedElementWidgetConstructor."),
			*Constructor->GetFullName());
		
		if (FWidgetPurposeColumn* PurposeInfo = Storage->GetColumn<FWidgetPurposeColumn>(PurposeRow))
		{
			switch (PurposeInfo->PurposeType)
			{
			case EPurposeType::Generic:
				{
					UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
					Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorTypeInfoColumn{.Constructor = Constructor});
					return true;
				}
			case EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					UE::Editor::DataStorage::RowHandle FactoryRow = RegisterUniqueWidgetFactoryRow(PurposeRow);
					Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorTypeInfoColumn{.Constructor = Constructor});
				}
				else
				{
					return false;
				}
			case EPurposeType::UniqueByNameAndColumn:
				{
					if (!Columns.IsEmpty())
					{
						Columns.Compile(UE::Editor::DataStorage::Queries::FEditorStorageQueryConditionCompileContext(Storage));
							
						UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
						Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorTypeInfoColumn{.Constructor = Constructor});
						Storage->AddColumn(FactoryRow, FWidgetFactoryConditionsColumn{.Conditions = Columns});
							
						return true;
					}
					else
					{
						return false;
					}
				}
			default:
				checkf(false, TEXT("Unexpected UE::Editor::DataStorage::IUiProvider::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		
		UE_LOG(LogEditorDataStorageUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%llu' isn't registered."), *Constructor->GetName(), PurposeRow);
		return false;
	}
	else
	{
		return RegisterWidgetFactory(PurposeRow, Constructor);
	}
}

bool UEditorDataStorageUi::RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow,
	TUniquePtr<FTypedElementWidgetConstructor>&& Constructor)
{
	checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));

	if (FWidgetPurposeColumn* PurposeInfo = Storage->GetColumn<FWidgetPurposeColumn>(PurposeRow))
	{
		switch (PurposeInfo->PurposeType)
		{
		case EPurposeType::Generic:
			{
				UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
				Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorColumn{.Constructor = MoveTemp(Constructor)});
				return true;
			}
		case EPurposeType::UniqueByName:
			{
				UE::Editor::DataStorage::RowHandle FactoryRow = RegisterUniqueWidgetFactoryRow(PurposeRow);
				Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorColumn{.Constructor = MoveTemp(Constructor)});
				return true;
			}
		case EPurposeType::UniqueByNameAndColumn:
			{
				UE_LOG(LogEditorDataStorageUI, Warning,
				TEXT("Unable to register widget factory '%s' as purpose '%llu' requires at least one column for matching."),
				*Constructor->GetTypeInfo()->GetName(), PurposeRow);
				return false;
			}
		default:
			{
				checkf(false, TEXT("Unexpected UE::Editor::DataStorage::IUiProvider::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
	}

	UE_LOG(LogEditorDataStorageUI, Warning,
		TEXT("Unable to register widget factory '%s' as purpose '%llu' isn't registered."), *Constructor->GetTypeInfo()->GetName(), PurposeRow);

	return false;
}

bool UEditorDataStorageUi::RegisterWidgetFactory(UE::Editor::DataStorage::RowHandle PurposeRow,
	TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, UE::Editor::DataStorage::Queries::FConditions Columns)
{
	checkf(Constructor->GetTypeInfo(), TEXT("Widget constructor being registered that doesn't have valid type information."));

	if (!Columns.IsEmpty())
	{
		if (FWidgetPurposeColumn* PurposeInfo = Storage->GetColumn<FWidgetPurposeColumn>(PurposeRow))
		{
			switch (PurposeInfo->PurposeType)
			{
			case EPurposeType::Generic:
				{
					UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
					Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorColumn{.Constructor = MoveTemp(Constructor)});
				}
				return true;
			case EPurposeType::UniqueByName:
				if (!Columns.IsEmpty())
				{
					UE::Editor::DataStorage::RowHandle FactoryRow = RegisterUniqueWidgetFactoryRow(PurposeRow);
					Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorColumn{.Constructor = MoveTemp(Constructor)});
				}
				else
				{
					return false;
				}
			case EPurposeType::UniqueByNameAndColumn:
				{
					if (!Columns.IsEmpty())
					{
						Columns.Compile(UE::Editor::DataStorage::Queries::FEditorStorageQueryConditionCompileContext(Storage));
							
						UE::Editor::DataStorage::RowHandle FactoryRow = RegisterWidgetFactoryRow(PurposeRow);
						Storage->AddColumn(FactoryRow, FWidgetFactoryConstructorColumn{.Constructor = MoveTemp(Constructor)});
						Storage->AddColumn(FactoryRow, FWidgetFactoryConditionsColumn{.Conditions = Columns});

						return true;
					}
					else
					{
						return false;
					}
				}
					
			default:
				checkf(false, TEXT("Unexpected UE::Editor::DataStorage::IUiProvider::EPurposeType found provided when registering widget factory."));
				return false;
			}
		}
		
		UE_LOG(LogEditorDataStorageUI, Warning,
		TEXT("Unable to register widget factory '%s' as purpose '%llu' isn't registered."), *Constructor->GetTypeInfo()->GetName(), PurposeRow);

		return false;
	}
	else
	{
		return RegisterWidgetFactory(PurposeRow, MoveTemp(Constructor));
	}
	
}

void UEditorDataStorageUi::CreateWidgetConstructors(UE::Editor::DataStorage::RowHandle PurposeRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback)
{
	while (Storage->HasColumns<FWidgetPurposeColumn>(PurposeRow))
	{
		TArray<UE::Editor::DataStorage::RowHandle> Factories;
		GetFactories(PurposeRow, Factories);

		// If no factories were found for this purpose, move on to the parent purpose
		if (Factories.IsEmpty())
		{
			if (FTableRowParentColumn* ParentColumn = Storage->GetColumn<FTableRowParentColumn>(PurposeRow))
			{
				PurposeRow = ParentColumn->Parent;
			}
			else
			{
				PurposeRow = UE::Editor::DataStorage::InvalidRowHandle;
			}
			continue;
		}

		for (UE::Editor::DataStorage::RowHandle FactoryRow : Factories)
		{
			if (!CreateSingleWidgetConstructor(FactoryRow, Arguments, {}, Callback))
			{
				return;
			}
		}

		// Don't want to go up the parent chain if we created any widgets for this purpose
		break;
	}
}

void UEditorDataStorageUi::CreateWidgetConstructors(UE::Editor::DataStorage::RowHandle PurposeRow, EMatchApproach MatchApproach,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	// Sort by name so that removing the matched columns can be done in a single pass.
	// Sorting by ptr does not work because dynamic column ptrs are different than their base template
	Columns.Sort(
		[](const TWeakObjectPtr<const UScriptStruct>& Lhs, const TWeakObjectPtr<const UScriptStruct>& Rhs)
		{
			return Lhs->GetName() < Rhs->GetName();
		});
	
	while (Storage->HasColumns<FWidgetPurposeColumn>(PurposeRow))
	{
		TArray<UE::Editor::DataStorage::RowHandle> Factories;
		GetFactories(PurposeRow, Factories);

		if (!Factories.IsEmpty())
		{
			// There is currently no way to cache the sorted results back into TEDS, so we sort every time this function is called
			Factories.StableSort(
				[this](UE::Editor::DataStorage::RowHandle Lhs, UE::Editor::DataStorage::RowHandle Rhs)
				{
					const UE::Editor::DataStorage::Queries::FConditions& LhsConditions = GetFactoryConditions(Lhs);
					const UE::Editor::DataStorage::Queries::FConditions& RhsConditions = GetFactoryConditions(Rhs);
				
					int32 LeftSize = LhsConditions.MinimumColumnMatchRequired();
					int32 RightSize = RhsConditions.MinimumColumnMatchRequired();

					// If two factories are the same size, we want factories containing dynamic templates to be at the end so
					// they are de-prioritized when matching and factories with dynamic specializations are matched first.
					// e.g A widget factory for ColumnA("Apple") or ColumnA("Orange") should be considered before a generic one for ColumnA.
					if (LeftSize == RightSize)
					{
						return !LhsConditions.UsesDynamicTemplates();
					}
				
					return LeftSize > RightSize;
				});

			switch (MatchApproach)
			{
			case EMatchApproach::LongestMatch:

				// For longest match, we don't want to continue matching with the parent purpose if the user requested us to stop
				if (!CreateWidgetConstructors_LongestMatch(Factories, Columns, Arguments, Callback))
				{
					return;
				}
				break;
			case EMatchApproach::ExactMatch:
				CreateWidgetConstructors_ExactMatch(Factories, Columns, Arguments, Callback);
				break;
			case EMatchApproach::SingleMatch:
				CreateWidgetConstructors_SingleMatch(Factories, Columns, Arguments, Callback);
				break;
			default:
				checkf(false, TEXT("Unsupported match type (%i) for CreateWidgetConstructors."), 
					static_cast<std::underlying_type_t<EMatchApproach>>(MatchApproach));
			}
		}

		// No need to go up the parent chain if there are no more columns to match
		if (Columns.IsEmpty())
		{
			return;
		}

		// If we have a parent purpose, try matching against factories belonging to it next
		if (FTableRowParentColumn* ParentColumn = Storage->GetColumn<FTableRowParentColumn>(PurposeRow))
		{
			PurposeRow = ParentColumn->Parent;
		}
		else
		{
			PurposeRow = UE::Editor::DataStorage::InvalidRowHandle;
		}
	}
}

void UEditorDataStorageUi::ConstructWidgets(UE::Editor::DataStorage::RowHandle PurposeRow, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	// Find the first purpose in the parent chain with at least one registered factory
	TArray<UE::Editor::DataStorage::RowHandle> Factories;

	while (Storage->HasColumns<FWidgetPurposeColumn>(PurposeRow) && Factories.IsEmpty())
	{
		GetFactories(PurposeRow, Factories);
		
		// If no factories were found for this purpose, move on to the parent purpose
		if (FTableRowParentColumn* ParentColumn = Storage->GetColumn<FTableRowParentColumn>(PurposeRow))
		{
			PurposeRow = ParentColumn->Parent;
		}
		else
		{
			PurposeRow = UE::Editor::DataStorage::InvalidRowHandle;
		}
	}
	
	for (UE::Editor::DataStorage::RowHandle FactoryRow : Factories)
	{
		if (FWidgetFactoryConstructorColumn* ConstructorColumn = Storage->GetColumn<FWidgetFactoryConstructorColumn>(FactoryRow))
		{
			CreateWidgetInstance(*ConstructorColumn->Constructor, Arguments, ConstructionCallback);
		}
		else if (FWidgetFactoryConstructorTypeInfoColumn* ConstructorTypeInfoColumn = Storage->GetColumn<FWidgetFactoryConstructorTypeInfoColumn>(FactoryRow))
		{
			if (const UScriptStruct* ConstructorType = ConstructorTypeInfoColumn->Constructor.Get())
			{
				CreateWidgetInstance(ConstructorType, Arguments, ConstructionCallback);
			}
		}
	}
}

void UEditorDataStorageUi::RegisterWidgetPurpose(FName Purpose, EPurposeType Type, FText Description)
{
	RegisterWidgetPurpose(UE::Editor::DataStorage::FMapKey(Purpose), FPurposeInfo(Purpose, Type, Description));
}

bool UEditorDataStorageUi::RegisterWidgetFactory(FName Purpose, const UScriptStruct* Constructor)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));

	if (Storage->IsRowAvailable(PurposeRow))
	{
		return RegisterWidgetFactory(PurposeRow, Constructor);
	}
	
	UE_LOG(LogEditorDataStorageUI, Warning, 
		TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());

	return false;
}

bool UEditorDataStorageUi::RegisterWidgetFactory(
	FName Purpose, const UScriptStruct* Constructor, UE::Editor::DataStorage::Queries::FConditions Columns)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	
	if (Storage->IsRowAvailable(PurposeRow))
	{
		return RegisterWidgetFactory(PurposeRow, Constructor, Columns);
	}
	
	UE_LOG(LogEditorDataStorageUI, Warning, 
			TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), *Constructor->GetName(), *Purpose.ToString());

	return false;
}

bool UEditorDataStorageUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	if (Storage->IsRowAvailable(PurposeRow))
	{
		return RegisterWidgetFactory(PurposeRow, MoveTemp(Constructor));
	}
	
	UE_LOG(LogEditorDataStorageUI, Warning, 
			TEXT("Unable to register widget factory as purpose '%s' isn't registered."), *Purpose.ToString());
	return false;
}

bool UEditorDataStorageUi::RegisterWidgetFactory(FName Purpose, TUniquePtr<FTypedElementWidgetConstructor>&& Constructor, 
	UE::Editor::DataStorage::Queries::FConditions Columns)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	
	if (Storage->IsRowAvailable(PurposeRow))
	{
		return RegisterWidgetFactory(PurposeRow, MoveTemp(Constructor), Columns);
	}
		
	UE_LOG(LogEditorDataStorageUI, Warning, TEXT("Unable to register widget factory '%s' as purpose '%s' isn't registered."), 
			*Constructor->GetTypeInfo()->GetName(), *Purpose.ToString());
	return false;
}

void UEditorDataStorageUi::CreateWidgetConstructors(FName Purpose,
	const UE::Editor::DataStorage::FMetaDataView& Arguments, const WidgetConstructorCallback& Callback)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	CreateWidgetConstructors(PurposeRow, Arguments, Callback);
}

void UEditorDataStorageUi::CreateWidgetConstructors(FName Purpose, EMatchApproach MatchApproach, 
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	CreateWidgetConstructors(PurposeRow, MatchApproach, Columns, Arguments, Callback);
}

void UEditorDataStorageUi::ConstructWidgets(FName Purpose, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	UE::Editor::DataStorage::RowHandle PurposeRow = Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(Purpose));
	ConstructWidgets(PurposeRow, Arguments, ConstructionCallback);
}

bool UEditorDataStorageUi::CreateSingleWidgetConstructor(
	UE::Editor::DataStorage::RowHandle FactoryRow,
	const UE::Editor::DataStorage::FMetaDataView& Arguments,
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumnTypes,
	const WidgetConstructorCallback& Callback)
{
	if (FWidgetFactoryConstructorColumn* ConstructorColumn = Storage->GetColumn<FWidgetFactoryConstructorColumn>(FactoryRow))
	{
		const UScriptStruct* TargetType = ConstructorColumn->Constructor->GetTypeInfo();
		checkf(TargetType, TEXT("Expected valid type information from a widget constructor."));
		TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
			FMemory::Malloc(TargetType->GetStructureSize(), TargetType->GetMinAlignment())));
		if (Result)
		{
			TargetType->InitializeStruct(Result.Get());
			TargetType->CopyScriptStruct(Result.Get(), ConstructorColumn->Constructor.Get());
			Result->Initialize(Arguments, MoveTemp(MatchedColumnTypes), FactoryRow);
			return Callback(MoveTemp(Result), Result->GetMatchedColumns());
		}
		return true;
	}
	else if (FWidgetFactoryConstructorTypeInfoColumn* ConstructorTypeInfoColumn = Storage->GetColumn<FWidgetFactoryConstructorTypeInfoColumn>(FactoryRow))
	{
		if (const UScriptStruct* Target = ConstructorTypeInfoColumn->Constructor.Get())
		{
			TUniquePtr<FTypedElementWidgetConstructor> Result(reinterpret_cast<FTypedElementWidgetConstructor*>(
				FMemory::Malloc(Target->GetStructureSize(), Target->GetMinAlignment())));
			if (Result)
			{
				Target->InitializeStruct(Result.Get());
				Result->Initialize(Arguments, MoveTemp(MatchedColumnTypes), FactoryRow);
				const TArray<TWeakObjectPtr<const UScriptStruct>>& MatchedColumns = Result->GetMatchedColumns();
				return Callback(MoveTemp(Result), MatchedColumns);
			}
			return true;
		}
	}
	return false;
}

void UEditorDataStorageUi::CreateWidgetInstance(
	FTypedElementWidgetConstructor& Constructor, 
	const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	UE::Editor::DataStorage::RowHandle Row = Storage->AddRow(WidgetTable);
	Storage->AddColumns(Row, Constructor.GetAdditionalColumnsList());
	TSharedPtr<SWidget> Widget = Constructor.ConstructFinalWidget(Row, Storage, this, Arguments);
	if (Widget)
	{
		ConstructionCallback(Widget.ToSharedRef(), Row);
	}
	else
	{
		Storage->RemoveRow(Row);
	}
}

void UEditorDataStorageUi::CreateWidgetInstance(const UScriptStruct* ConstructorType, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetCreatedCallback& ConstructionCallback)
{
	FTypedElementWidgetConstructor* Constructor = reinterpret_cast<FTypedElementWidgetConstructor*>(
						FMemory_Alloca_Aligned(ConstructorType->GetStructureSize(), ConstructorType->GetMinAlignment()));
	if (Constructor)
	{
		ConstructorType->InitializeStruct(Constructor);
		CreateWidgetInstance(*Constructor, Arguments, ConstructionCallback);
		ConstructorType->DestroyStruct(&Constructor);
	}
	else
	{
		checkf(false, TEXT("Remaining memory is too small to create a widget constructor from a description."));
	}
}

TSharedPtr<SWidget> UEditorDataStorageUi::ConstructWidget(UE::Editor::DataStorage::RowHandle Row, FTypedElementWidgetConstructor& Constructor,
	const UE::Editor::DataStorage::FMetaDataView& Arguments)
{
	return Constructor.ConstructFinalWidget(Row, Storage, this, Arguments);
}

void UEditorDataStorageUi::ListWidgetPurposes(const WidgetPurposeCallback& Callback) const
{
	using namespace UE::Editor::DataStorage::Queries;
	
	static QueryHandle PurposeQueryHandle =
		Storage->RegisterQuery(Select().ReadOnly<FNameColumn, FWidgetPurposeColumn, FDescriptionColumn>().Compile());

	Storage->RunQuery(PurposeQueryHandle, CreateDirectQueryCallbackBinding(
		[Callback](const FNameColumn& NameColumn, const FWidgetPurposeColumn& PurposeColumn, const FDescriptionColumn& DescriptionColumn)
	{
			Callback(NameColumn.Name, PurposeColumn.PurposeType, DescriptionColumn.Description);
	}));
}

bool UEditorDataStorageUi::SupportsExtension(FName Extension) const
{
	return false;
}

void UEditorDataStorageUi::ListExtensions(TFunctionRef<void(FName)> Callback) const
{
	
}

TSharedPtr<UE::Editor::DataStorage::ITedsWidget> UEditorDataStorageUi::CreateContainerTedsWidget(UE::Editor::DataStorage::RowHandle UiRowHandle) const
{
	return SNew(UE::Editor::DataStorage::Widgets::STedsWidget)
			.UiRowHandle(UiRowHandle);
}

UE::Editor::DataStorage::TableHandle UEditorDataStorageUi::GetWidgetTable() const
{
	return WidgetTable;
}

UE::Editor::DataStorage::IUiProvider::FPurposeID UEditorDataStorageUi::GetDefaultWidgetPurposeID() const
{
	return Internal::DefaultWidgetPurposeID;	
}

UE::Editor::DataStorage::IUiProvider::FPurposeID UEditorDataStorageUi::GetGeneralWidgetPurposeID() const
{
	return Internal::GeneralWidgetPurposeID;
}

UE::Editor::DataStorage::RowHandle UEditorDataStorageUi::FindPurpose(const FPurposeID& PurposeID) const
{
	return Storage->LookupMappedRow(UE::Editor::DataStorage::FMapKey(PurposeID));
}

void UEditorDataStorageUi::CreateStandardArchetypes()
{
	WidgetTable = Storage->RegisterTable(MakeArrayView(
		{
			FTypedElementSlateWidgetReferenceColumn::StaticStruct(),
			FTypedElementSlateWidgetReferenceDeletesRowTag::StaticStruct(),
			FSlateColorColumn::StaticStruct()
		}), FName(TEXT("Editor_WidgetTable")));

	WidgetPurposeTable = Storage->RegisterTable(UE::Editor::DataStorage::TTypedElementColumnTypeList<FWidgetPurposeColumn, FWidgetPurposeNameColumn>(), FName("Editor_WidgetPurposeTable"));
	WidgetFactoryTable = Storage->RegisterTable(UE::Editor::DataStorage::TTypedElementColumnTypeList<FWidgetFactoryColumn>(), FName("Editor_WidgetFactoryTable"));
}

void UEditorDataStorageUi::RegisterQueries()
{
	using namespace UE::Editor::DataStorage::Queries;
	
	Storage->RegisterQuery(
		Select(
			TEXT("Add display name to widget factory with constructor"),
			FObserver::OnAdd<FWidgetFactoryConstructorTypeInfoColumn>().SetExecutionMode(EExecutionMode::GameThread),
			[](IQueryContext& Context, RowHandle Row, const FWidgetFactoryConstructorTypeInfoColumn& ConstructorColumn)
			{
				Context.AddColumn(Row, FNameColumn{ .Name = ConstructorColumn.Constructor->GetFName() });
			})
		.Where()
			.All<FWidgetFactoryColumn>()
		.Compile());
	
	Storage->RegisterQuery(
		Select(
			TEXT("Add display name to widget factory with constructor type info"),
			FObserver::OnAdd<FWidgetFactoryConstructorColumn>().SetExecutionMode(EExecutionMode::GameThread),
			[this](IQueryContext& Context, RowHandle Row, const FWidgetFactoryConstructorColumn& ConstructorColumn)
			{
				Context.AddColumn(Row, FNameColumn{ .Name = ConstructorColumn.Constructor->GetTypeInfo()->GetFName() });
			})
		.Where()
			.All<FWidgetFactoryColumn>()
		.Compile());
}

bool UEditorDataStorageUi::CreateWidgetConstructors_LongestMatch(const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (auto FactoryIt = WidgetFactories.CreateConstIterator(); FactoryIt && !Columns.IsEmpty(); ++FactoryIt)
	{
		const UE::Editor::DataStorage::Queries::FConditions& Conditions = GetFactoryConditions(*FactoryIt);
		
		if (Conditions.MinimumColumnMatchRequired() > Columns.Num())
		{
			// There are more columns required for this factory than there are in the requested columns list so skip this
			// factory.
			continue;
		}

		MatchedColumns.Reset();
		
		if (Conditions.Verify(MatchedColumns, Columns))
		{
			// Empty conditions match against everything - so we update the matched columns list to reflect that
			if (Conditions.IsEmpty())
			{
				MatchedColumns = Columns;
			}
			// Remove the found columns from the requested list.
			MatchedColumns.Sort(
				[](const TWeakObjectPtr<const UScriptStruct>& Lhs, const TWeakObjectPtr<const UScriptStruct>& Rhs)
				{
					return Lhs->GetName() < Rhs->GetName();
				});
			
			MatchedColumns.SetNum(Algo::Unique(MatchedColumns), EAllowShrinking::No);

			// We need to keep track of the columns the user requested that ended up matching separately because MatchedColumns could contain the
			// base template for a dynamic column in the requested columns that actually matched - and the widget constructor wants the latter
			TArray<TWeakObjectPtr<const UScriptStruct>> RequestedColumnsThatMatched;
			
			TWeakObjectPtr<const UScriptStruct>* ColumnsIt = Columns.GetData();
			TWeakObjectPtr<const UScriptStruct>* ColumnsEnd = ColumnsIt + Columns.Num();
			int32 ColumnIndex = 0;
			for (const TWeakObjectPtr<const UScriptStruct>& MatchedColumn : MatchedColumns)
			{
				// Remove all the columns that were matched from the provided column list.
				while (!Internal::CheckSingleColumnMatch(MatchedColumn.Get(), ColumnsIt->Get()))
				{
					++ColumnIndex;
					++ColumnsIt;
					if (ColumnsIt == ColumnsEnd)
					{
						ensureMsgf(false, TEXT("A previously found matching column can't be found in the original array."));
						return false;
					}
				}
				RequestedColumnsThatMatched.Add(*ColumnsIt);
				Columns.RemoveAt(ColumnIndex, EAllowShrinking::No);
				--ColumnsEnd;
			}
			
			if (!CreateSingleWidgetConstructor(*FactoryIt, Arguments, MoveTemp(RequestedColumnsThatMatched), Callback))
			{
				return false;
			}
		}
	}

	return true;
}

void UEditorDataStorageUi::CreateWidgetConstructors_ExactMatch(const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	int32 ColumnCount = Columns.Num();
	TArray<TWeakObjectPtr<const UScriptStruct>> MatchedColumns;
	for (UE::Editor::DataStorage::RowHandle FactoryRow : WidgetFactories)
	{
		const UE::Editor::DataStorage::Queries::FConditions& Conditions = GetFactoryConditions(FactoryRow);
		
		// If there are more matches required that there are columns, then there will never be an exact match.
		// Less than the column count can still result in a match that covers all columns.
		if (Conditions.MinimumColumnMatchRequired() > ColumnCount)
		{
			continue;
		}

		MatchedColumns.Reset();

		if (Conditions.Verify(MatchedColumns, Columns))
		{
			// Empty conditions match against everything - so we update the matched columns list to reflect that
			if (Conditions.IsEmpty())
			{
				MatchedColumns = Columns;
			}
			
			Algo::SortBy(MatchedColumns, [](const TWeakObjectPtr<const UScriptStruct>& Column) { return Column.Get(); });
			MatchedColumns.SetNum(Algo::Unique(MatchedColumns), EAllowShrinking::No);
			if (MatchedColumns.Num() == Columns.Num())
			{
				Columns.Reset();
				CreateSingleWidgetConstructor(FactoryRow, Arguments, MoveTemp(MatchedColumns), Callback);
				return;
			}
		}
	}
}

void UEditorDataStorageUi::CreateWidgetConstructors_SingleMatch(const TArray<UE::Editor::DataStorage::RowHandle>& WidgetFactories,
	TArray<TWeakObjectPtr<const UScriptStruct>>& Columns, const UE::Editor::DataStorage::FMetaDataView& Arguments,
	const WidgetConstructorCallback& Callback)
{
	auto FactoryIt = WidgetFactories.rbegin();
	auto FactoryEnd = WidgetFactories.rend();

	// Start from the back as the widgets with lower counts will be last.
	for (int32 ColumnIndex = Columns.Num() - 1; ColumnIndex >= 0; --ColumnIndex)
	{
		for (; FactoryIt != FactoryEnd; ++FactoryIt)
		{
			const UE::Editor::DataStorage::Queries::FConditions& Conditions = GetFactoryConditions(*FactoryIt);
			
			TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnData = Conditions.GetColumns();
			if (ColumnData.Num() > 1)
			{
				// Moved passed the point where factories only have a single column.
				return;
			}
			else if (ColumnData.Num() == 0)
			{
				// Need to move further to find factories with exactly one column.
				continue;
			}

			if (Internal::CheckSingleColumnMatch(ColumnData[0].Get(), Columns[ColumnIndex].Get()))
			{
				// We need to keep a copy of the actually requested column because the matched column could be the base template for the actually matched
				// dynamic column and the widget constructor wants the latter
				TWeakObjectPtr<const UScriptStruct> RequestedColumn = Columns[ColumnIndex];
				Columns.RemoveAt(ColumnIndex);
				CreateSingleWidgetConstructor(*FactoryIt, Arguments, {RequestedColumn}, Callback);
				
				
				// Match was found so move on to the next column in the column.
				break;
			} 
		}
	}
}

UE::Editor::DataStorage::RowHandle UEditorDataStorageUi::RegisterWidgetFactoryRow(UE::Editor::DataStorage::RowHandle PurposeRowHandle) const
{
	UE::Editor::DataStorage::RowHandle FactoryRowHandle = Storage->AddRow(WidgetFactoryTable);
	Storage->GetColumn<FWidgetFactoryColumn>(FactoryRowHandle)->PurposeRowHandle = PurposeRowHandle;

	return FactoryRowHandle;
}

UE::Editor::DataStorage::RowHandle UEditorDataStorageUi::RegisterUniqueWidgetFactoryRow(UE::Editor::DataStorage::RowHandle InPurposeRowHandle) const
{
	using namespace UE::Editor::DataStorage::Queries;
	
	static QueryHandle FactoryQueryHandle = Storage->RegisterQuery(Select().ReadOnly<FWidgetFactoryColumn>().Compile());

	RowHandle FactoryRowHandle = InvalidRowHandle;

	// Find the first matching factory belonging to this purpose, we know there is only going to be one
	Storage->RunQuery(FactoryQueryHandle, CreateDirectQueryCallbackBinding(
		[&FactoryRowHandle, InPurposeRowHandle](RowHandle FoundFactoryRowHandle, const FWidgetFactoryColumn& PurposeReferenceColumn)
		{
			if (PurposeReferenceColumn.PurposeRowHandle == InPurposeRowHandle)
			{
				FactoryRowHandle = FoundFactoryRowHandle;
			}
		}));

	// If there was a factory already registered for this purpose, overwrite its information
	if (Storage->IsRowAvailable(FactoryRowHandle))
	{
		Storage->RemoveColumns<FWidgetFactoryConstructorColumn, FWidgetFactoryConstructorTypeInfoColumn>(FactoryRowHandle);
		Storage->GetColumn<FWidgetFactoryColumn>(FactoryRowHandle)->PurposeRowHandle = InPurposeRowHandle;
		return FactoryRowHandle;
	}
	// Otherwise just register the factory row as usual
	else
	{
		return RegisterWidgetFactoryRow(InPurposeRowHandle);
	}
}

void UEditorDataStorageUi::GetFactories(UE::Editor::DataStorage::RowHandle PurposeRowHandle,
	TArray<UE::Editor::DataStorage::RowHandle>& OutFactories) const
{
	using namespace UE::Editor::DataStorage::Queries;
	
	static QueryHandle FactoryQueryHandle = Storage->RegisterQuery(Select().ReadOnly<FWidgetFactoryColumn>().Compile());
	
	Storage->RunQuery(FactoryQueryHandle, CreateDirectQueryCallbackBinding(
		[PurposeRowHandle, &OutFactories](RowHandle RowHandle, const FWidgetFactoryColumn& PurposeReferenceColumn)
		{
			if (PurposeReferenceColumn.PurposeRowHandle == PurposeRowHandle)
			{
				OutFactories.Add(RowHandle);
			}
		}));
}

const UE::Editor::DataStorage::Queries::FConditions& UEditorDataStorageUi::GetFactoryConditions(UE::Editor::DataStorage::RowHandle FactoryRow) const
{
	using namespace UE::Editor::DataStorage::Queries;

	if (FWidgetFactoryConditionsColumn* FactoryColumn = Storage->GetColumn<FWidgetFactoryConditionsColumn>(FactoryRow))
	{
		FactoryColumn->Conditions.Compile(FEditorStorageQueryConditionCompileContext(Storage));
		return FactoryColumn->Conditions;
	}
	// If this factory does not have any query conditions, just return a default empty FConditions struct
	else
	{
		static FConditions DefaultConditions;
		DefaultConditions.Compile(FEditorStorageQueryConditionCompileContext(Storage));
		return DefaultConditions;
	}
	
}

#undef LOCTEXT_NAMESPACE // "TypedElementDatabaseUI"
