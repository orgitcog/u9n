// Copyright Epic Games, Inc. All Rights Reserved.

#include "Compatibility/SceneOutlinerTedsBridge.h"

#include "ActorTreeItem.h"
#include "Columns/TedsOutlinerColumns.h"
#include "Elements/Columns/TypedElementLabelColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementTransformColumns.h"
#include "Elements/Columns/TypedElementTypeInfoColumns.h"
#include "Elements/Columns/TypedElementVisibilityColumns.h"
#include "Elements/Common/EditorDataStorageFeatures.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageCompatibilityInterface.h"
#include "Elements/Interfaces/TypedElementDataStorageUiInterface.h"
#include "ILevelEditor.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerColumn.h"
#include "ISceneOutlinerTreeItem.h"
#include "LevelEditor.h"
#include "SSceneOutliner.h"
#include "SceneOutlinerFwd.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "TedsTableViewerColumn.h"
#include "TedsTableViewerUtils.h"
#include "Elements/Interfaces/Capabilities/TypedElementUiTextCapability.h"
#include "TedsOutlinerItem.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerTedsBridge"

FAutoConsoleCommand BindColumnsToSceneOutlinerConsoleCommand(
	TEXT("TEDS.UI.BindColumnsToSceneOutliner"),
	TEXT("Bind one or more columns to the most recently used Scene Outliner. Several prebuild configurations are offered as well.")
	TEXT("An example input to show a label column is 'TEDS.UI.BindColumnsToSceneOutliner /Script/TypedElementFramework.TypedElementLabelColumn'."),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			using namespace UE::Editor::DataStorage::Queries;

			if (ICoreProvider* DataStorage = GetMutableDataStorageFeature<ICoreProvider>(StorageFeatureName))
			{
				static QueryHandle Queries[] =
				{
					DataStorage->RegisterQuery(Select().ReadWrite<FTypedElementLabelColumn>().Compile()),
					DataStorage->RegisterQuery(Select().ReadOnly<FTypedElementLocalTransformColumn>().Compile()),
					DataStorage->RegisterQuery(Select().ReadOnly<FTypedElementPackagePathColumn>().Compile()),
					DataStorage->RegisterQuery(
						Select()
							.ReadWrite<FTypedElementLabelColumn>()
							.ReadOnly<FTypedElementLocalTransformColumn>()
						.Compile()),
					DataStorage->RegisterQuery(
						Select()
							.ReadOnly<FTypedElementLabelColumn>()
							.ReadOnly<FTypedElementLabelHashColumn>()
						.Compile())
				};

				FSceneOutlinerTedsQueryBinder& Binder = FSceneOutlinerTedsQueryBinder::GetInstance();
				const TWeakPtr<ILevelEditor> LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor")).GetLevelEditorInstance();
				const TSharedPtr<ISceneOutliner> SceneOutliner = LevelEditor.IsValid() ? LevelEditor.Pin()->GetMostRecentlyUsedSceneOutliner() : nullptr;
				if (SceneOutliner.IsValid())
				{
					if (!Args.IsEmpty())
					{
						if (Args[0].IsNumeric())
						{
							int32 QueryIndex = FCString::Atoi(*Args[0]);
							if (QueryIndex < sizeof(Queries) / sizeof(QueryHandle))
							{
								Binder.AssignQuery(Queries[QueryIndex], SceneOutliner);
								return;
							}
						}
						else
						{
							uint32 AdditionCount = 0;
							Select Query;
							for (const FString& Arg : Args)
							{
								FTopLevelAssetPath Path;
								// TrySetPath has an ensure that checks if the path starts with an '/' and otherwise throws
								// an assert.
								if (!Arg.IsEmpty() && Arg[0] == '/' && Path.TrySetPath(Arg))
								{
									const UScriptStruct* ColumnType = TypeOptional(Path);
									if (ColumnType && ColumnType->IsChildOf(FColumn::StaticStruct()))
									{
										Query.ReadOnly(ColumnType);
										++AdditionCount;
									}
								}
							}
							if (AdditionCount > 0)
							{
								static QueryHandle CustomQuery = InvalidQueryHandle;
								if (CustomQuery != InvalidQueryHandle)
								{
									DataStorage->UnregisterQuery(CustomQuery);
								}
								CustomQuery = DataStorage->RegisterQuery(Query.Compile());
								Binder.AssignQuery(CustomQuery, SceneOutliner);
								return;
							}
						}
					}
					Binder.AssignQuery(InvalidQueryHandle, SceneOutliner);
				}
			}
		}));

class FSceneOutlinerTedsBridge
{
public:
	~FSceneOutlinerTedsBridge();

	void Initialize(
		UE::Editor::DataStorage::ICoreProvider& InStorage,
		UE::Editor::DataStorage::IUiProvider& InStorageUi,
		UE::Editor::DataStorage::ICompatibilityProvider& InStorageCompatibility,
		const TSharedPtr<ISceneOutliner>& InOutliner);
	
	void AssignQuery(UE::Editor::DataStorage::QueryHandle Query,
		const UE::Editor::DataStorage::IUiProvider::FPurposeID& CellWidgetPurpose, const UE::Editor::DataStorage::IUiProvider::FPurposeID& HeaderWidgetPurpose);
	
	void RegisterDealiaser(const FTreeItemIDDealiaser& InDealiaser);
	FTreeItemIDDealiaser GetDealiaser();
	
private:
	void ClearColumns(ISceneOutliner& InOutliner);

	TArray<FName> AddedColumns;
	TWeakPtr<ISceneOutliner> Outliner;
	UE::Editor::DataStorage::ICoreProvider* Storage{ nullptr };
	UE::Editor::DataStorage::IUiProvider* StorageUi{ nullptr };
	UE::Editor::DataStorage::ICompatibilityProvider* StorageCompatibility{ nullptr };
	FTreeItemIDDealiaser Dealiaser;
	UE::Editor::DataStorage::IUiProvider::FPurposeID CellWidgetPurpose;
};

class FOutlinerColumn : public ISceneOutlinerColumn
{
public:
	FOutlinerColumn(
		UE::Editor::DataStorage::QueryHandle InQuery,
		UE::Editor::DataStorage::ICoreProvider& InStorage, 
		UE::Editor::DataStorage::IUiProvider& InStorageUi, 
		UE::Editor::DataStorage::ICompatibilityProvider& InStorageCompatibility,
		FName InNameId, 
		TArray<TWeakObjectPtr<const UScriptStruct>> InColumnTypes,
		TSharedPtr<FTypedElementWidgetConstructor> InHeaderWidgetConstructor,
		TSharedPtr<FTypedElementWidgetConstructor> InCellWidgetConstructor,
		FName InFallbackColumnName,
		TWeakPtr<ISceneOutliner> InOwningOutliner,
		const FTreeItemIDDealiaser& InDealiaser)
		: Storage(InStorage)
		, StorageUi(InStorageUi)
		, StorageCompatibility(InStorageCompatibility)
		, QueryHandle(InQuery)
		, NameId(InNameId)
		, OwningOutliner(InOwningOutliner)
		, Dealiaser(InDealiaser)
	{
		MetaData.AddOrSetMutableData(TEXT("Name"), NameId.ToString());

		using namespace UE::Editor::DataStorage;

		TableViewerColumnImpl = MakeUnique<FTedsTableViewerColumn>(NameId, InCellWidgetConstructor, InColumnTypes,
			InHeaderWidgetConstructor, FComboMetaDataView(FGenericMetaDataView(MetaData)).Next(FQueryMetaDataView(Storage.GetQueryDescription(QueryHandle))));

		TableViewerColumnImpl->SetIsRowVisibleDelegate(
		FTedsTableViewerColumn::FIsRowVisible::CreateRaw(this, &FOutlinerColumn::IsRowVisible)
		);
		
		// Try to find a fallback column from the regular item, for handling cases like folders which are not in TEDS but want to use TEDS columns
		FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
		FallbackColumn = SceneOutlinerModule.FactoryColumn(InFallbackColumnName, *OwningOutliner.Pin());
	};
	
	FName GetColumnID() override
	{
		return NameId;
	}

	virtual void Tick(double InCurrentTime, float InDeltaTime) override
	{
		TableViewerColumnImpl->Tick();
		if (FallbackColumn)
		{
			FallbackColumn->Tick(InCurrentTime, InDeltaTime);
		}
	}

	bool IsRowVisible(const UE::Editor::DataStorage::RowHandle InRowHandle) const
	{
		TSharedPtr<ISceneOutliner> OutlinerPinned = OwningOutliner.Pin();

		if (!OutlinerPinned)
		{
			return false;
		}
		
		// Try to grab the TEDS Outliner item from the row handle
		FSceneOutlinerTreeItemPtr Item = OutlinerPinned->GetTreeItem(InRowHandle);

		// If it doesn't exist, this could be a legacy item that uses something other than the row id as the ID, so check if we have a dealiaser
		if (!Item)
		{
			if (Dealiaser.IsBound())
			{
				Item = OutlinerPinned->GetTreeItem(Dealiaser.Execute(InRowHandle));
			}
		}

		if (!Item)
		{
			return false;
		}

		// Check if the item is visible in the tree
		return OutlinerPinned->GetTree().IsItemVisible(Item);
	}

	virtual SHeaderRow::FColumn::FArguments ConstructHeaderRowColumn() override
	{
		return TableViewerColumnImpl->ConstructHeaderRowColumn();
	}

	// TODO: Sorting is currently handled through the fallback column if it exists because we have no way to sort columns through TEDS
	virtual void SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const override
	{
		if (FallbackColumn)
		{
			FallbackColumn->SortItems(RootItems, SortMode);
		}
	}

	virtual bool SupportsSorting() const override
	{
		return FallbackColumn ? FallbackColumn->SupportsSorting() : false;
	}

	void SetHighlightText(SWidget& Widget)
	{
		TSharedPtr<ISceneOutliner> OutlinerPinned = OwningOutliner.Pin();

		if (!OutlinerPinned)
		{
			return;
		}

		if (TSharedPtr<ITypedElementUiTextCapability> TextCapability = Widget.GetMetaData<ITypedElementUiTextCapability>())
		{
			TextCapability->SetHighlightText(OutlinerPinned->GetFilterHighlightText());
		}
	
		if (FChildren* ChildWidgets = Widget.GetChildren())
		{
			ChildWidgets->ForEachWidget([this](SWidget& ChildWidget)
				{
					SetHighlightText(ChildWidget);
				});
		}
	}
	
	const TSharedRef<SWidget> ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row) override
	{
		using namespace UE::Editor::DataStorage;
		using namespace UE::Editor::Outliner;
		
		RowHandle TargetRowHandle = InvalidRowHandle;

		TSharedPtr<SWidget> RowWidget;

		if (const FTedsOutlinerTreeItem* TedsItem = TreeItem->CastTo<FTedsOutlinerTreeItem>())
		{
			TargetRowHandle = TedsItem->GetRowHandle();
			
		}
		else if (const FActorTreeItem* ActorItem = TreeItem->CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				TargetRowHandle = StorageCompatibility.FindRowWithCompatibleObject(Actor);
			}
		}
		else if (FallbackColumn)
		{
			RowWidget = FallbackColumn->ConstructRowWidget(TreeItem, Row);
		}

		if(Storage.IsRowAssigned(TargetRowHandle))
		{
			RowWidget = TableViewerColumnImpl->ConstructRowWidget(TargetRowHandle,
				[&](UE::Editor::DataStorage::ICoreProvider& DataStorage, const RowHandle& WidgetRow)
				{
					DataStorage.AddColumn(WidgetRow, FTedsOutlinerColumn{ .Outliner = OwningOutliner });
				});
		}

		if (RowWidget)
		{
			SetHighlightText(*RowWidget);
			return RowWidget.ToSharedRef();
		}

		return SNullWidget::NullWidget;
	}

	virtual void PopulateSearchStrings( const ISceneOutlinerTreeItem& Item, TArray< FString >& OutSearchStrings ) const override
	{
		// TODO: We don't currently have a way to convert TEDS widgets into searchable strings, but we can rely on the fallback column if it exists
		if (FallbackColumn)
		{
			FallbackColumn->PopulateSearchStrings(Item, OutSearchStrings);
		}
	}

	// The table viewer implementation that we internally use to create our widgets
	TUniquePtr<UE::Editor::DataStorage::FTedsTableViewerColumn> TableViewerColumnImpl;
	
	UE::Editor::DataStorage::ICoreProvider& Storage;
	UE::Editor::DataStorage::IUiProvider& StorageUi;
	UE::Editor::DataStorage::ICompatibilityProvider& StorageCompatibility;
	UE::Editor::DataStorage::QueryHandle QueryHandle;
	UE::Editor::DataStorage::FMetaData MetaData;
	FName NameId;
	TSharedPtr<ISceneOutlinerColumn> FallbackColumn;
	TWeakPtr<ISceneOutliner> OwningOutliner;
	FTreeItemIDDealiaser Dealiaser;
};




//
// USceneOutlinerTedsBridgeFactory
// 

void USceneOutlinerTedsBridgeFactory::RegisterWidgetPurposes(UE::Editor::DataStorage::IUiProvider& DataStorageUi) const
{
	UE::Editor::DataStorage::IUiProvider::FPurposeID GeneralRowLabelPurposeID =
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("General", "RowLabel", NAME_None).GeneratePurposeID();

	UE::Editor::DataStorage::IUiProvider::FPurposeID GeneralHeaderPurposeID =
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("General", "Header", NAME_None).GeneratePurposeID();

	DataStorageUi.RegisterWidgetPurpose(
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "Header", NAME_None,
			UE::Editor::DataStorage::IUiProvider::EPurposeType::UniqueByNameAndColumn,
			LOCTEXT("HeaderWidgetPurpose", "Widgets for headers in any Scene Outliner for specific columns or column combinations."),
			GeneralHeaderPurposeID));
	
	DataStorageUi.RegisterWidgetPurpose(
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "Cell", NAME_None,
			UE::Editor::DataStorage::IUiProvider::EPurposeType::UniqueByNameAndColumn,
			LOCTEXT("CellWidgetPurpose", "Widgets for cells in any Scene Outliner for specific columns or column combinations."),
			DataStorageUi.GetGeneralWidgetPurposeID()));


	DataStorageUi.RegisterWidgetPurpose(
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "RowLabel", NAME_None,
			UE::Editor::DataStorage::IUiProvider::EPurposeType::UniqueByNameAndColumn,
			LOCTEXT("ItemCellWidgetPurpose", "Widgets for cells in any Scene Outliner that are specific to the Item label column."),
			GeneralRowLabelPurposeID));
}



//
// FSceneOutlinerTedsQueryBinder
// 

const FName FSceneOutlinerTedsQueryBinder::CellWidgetTableName(TEXT("Editor_SceneOutlinerCellWidgetTable"));
const FName FSceneOutlinerTedsQueryBinder::HeaderWidgetPurpose(TEXT("SceneOutliner.Header"));
const FName FSceneOutlinerTedsQueryBinder::CellWidgetPurpose(TEXT("SceneOutliner.Cell"));
const FName FSceneOutlinerTedsQueryBinder::ItemLabelCellWidgetPurpose(TEXT("SceneOutliner.RowLabel"));

FSceneOutlinerTedsQueryBinder::FSceneOutlinerTedsQueryBinder()
{
	using namespace UE::Editor::DataStorage;
	Storage = GetMutableDataStorageFeature<UE::Editor::DataStorage::ICoreProvider>(StorageFeatureName);
	StorageUi = GetMutableDataStorageFeature<UE::Editor::DataStorage::IUiProvider>(UiFeatureName);
	StorageCompatibility = GetMutableDataStorageFeature<UE::Editor::DataStorage::ICompatibilityProvider>(CompatibilityFeatureName);

	SetupDefaultColumnMapping();
}

void FSceneOutlinerTedsQueryBinder::SetupDefaultColumnMapping()
{
	// Map the type column from the TEDS to the default Outliner type column, so we can show type info for objects not in TEDS
	TEDSToOutlinerDefaultColumnMapping.Add(FTypedElementClassTypeInfoColumn::StaticStruct(), FSceneOutlinerBuiltInColumnTypes::ActorInfo());
	TEDSToOutlinerDefaultColumnMapping.Add(FVisibleInEditorColumn::StaticStruct(), FSceneOutlinerBuiltInColumnTypes::Gutter());
}

FName FSceneOutlinerTedsQueryBinder::FindOutlinerColumnFromTEDSColumns(TConstArrayView<TWeakObjectPtr<const UScriptStruct>> TEDSColumns) const
{
	// Currently, the algorithm naively looks through the mapping and returns the first match
	for(const TWeakObjectPtr<const UScriptStruct>& Column : TEDSColumns)
	{
		if (const FName* FoundDefaultColumn = TEDSToOutlinerDefaultColumnMapping.Find(Column))
		{
			return *FoundDefaultColumn;
		}
	}

	return FName();
}

void FSceneOutlinerTedsQueryBinder::RefreshLevelEditorOutliners() const
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FTabManager> LevelEditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	static const FName TabIDS[] = { LevelEditorTabIds::LevelEditorSceneOutliner, LevelEditorTabIds::LevelEditorSceneOutliner2, LevelEditorTabIds::LevelEditorSceneOutliner3, LevelEditorTabIds::LevelEditorSceneOutliner4 };

	for (const FName& TabID : TabIDS)
	{
		if (LevelEditorTabManager->FindExistingLiveTab(TabID).IsValid())
		{
			LevelEditorTabManager->TryInvokeTab(TabID)->RequestCloseTab();
			LevelEditorTabManager->TryInvokeTab(TabID);
		}
	}
}

FSceneOutlinerTedsQueryBinder& FSceneOutlinerTedsQueryBinder::GetInstance()
{
	static FSceneOutlinerTedsQueryBinder Binder;
	return Binder;
}

TSharedPtr<FSceneOutlinerTedsBridge>* FSceneOutlinerTedsQueryBinder::FindOrAddQueryMapping(const TSharedPtr<ISceneOutliner>& Outliner)
{
	TSharedPtr<FSceneOutlinerTedsBridge>* QueryMapping = SceneOutliners.Find(Outliner);
	if (QueryMapping == nullptr)
	{
		QueryMapping = &SceneOutliners.Add(Outliner, MakeShared<FSceneOutlinerTedsBridge>());
		(*QueryMapping)->Initialize(*Storage, *StorageUi, *StorageCompatibility, Outliner);
	}

	return QueryMapping;
}

TSharedPtr<FSceneOutlinerTedsBridge>* FSceneOutlinerTedsQueryBinder::FindQueryMapping(const TSharedPtr<ISceneOutliner>& Outliner)
{
	return SceneOutliners.Find(Outliner);
}


void FSceneOutlinerTedsQueryBinder::AssignQuery(UE::Editor::DataStorage::QueryHandle Query, const TSharedPtr<ISceneOutliner>& Outliner,
	const UE::Editor::DataStorage::IUiProvider::FPurposeID& InWidgetPurpose, const UE::Editor::DataStorage::IUiProvider::FPurposeID& InHeaderPurpose)
{
	CleanupStaleOutliners();

	TSharedPtr<FSceneOutlinerTedsBridge>* QueryMapping = FindOrAddQueryMapping(Outliner);
	(*QueryMapping)->AssignQuery(Query, InWidgetPurpose, InHeaderPurpose);
}

void FSceneOutlinerTedsQueryBinder::AssignQuery(UE::Editor::DataStorage::QueryHandle Query, const TSharedPtr<ISceneOutliner>& Outliner)
{
	UE::Editor::DataStorage::IUiProvider::FPurposeID CellPurpose =
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "Cell", NAME_None).GeneratePurposeID();
	
	UE::Editor::DataStorage::IUiProvider::FPurposeID HeaderPurpose =
		UE::Editor::DataStorage::IUiProvider::FPurposeInfo("SceneOutliner", "Header", NAME_None).GeneratePurposeID();

	AssignQuery(Query, Outliner, CellPurpose, HeaderPurpose);

}

void FSceneOutlinerTedsQueryBinder::RegisterTreeItemIDDealiaser(const TSharedPtr<ISceneOutliner>& Outliner, const FTreeItemIDDealiaser& InDealiaser)
{
	TSharedPtr<FSceneOutlinerTedsBridge>* QueryMapping = FindOrAddQueryMapping(Outliner);
	(*QueryMapping)->RegisterDealiaser(InDealiaser);
}

FTreeItemIDDealiaser FSceneOutlinerTedsQueryBinder::GetTreeItemIDDealiaser(const TSharedPtr<ISceneOutliner>& Widget)
{
	TSharedPtr<FSceneOutlinerTedsBridge>* QueryMapping = FindQueryMapping(Widget);

	if (QueryMapping)
	{
		return (*QueryMapping)->GetDealiaser();
	}

	return FTreeItemIDDealiaser();
}

void FSceneOutlinerTedsQueryBinder::CleanupStaleOutliners()
{
	for (TMap<TWeakPtr<ISceneOutliner>, TSharedPtr<FSceneOutlinerTedsBridge>>::TIterator It(SceneOutliners); It; ++It)
	{
		// Remove any query mappings where the target Outliner doesn't exist anymore
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

//
// FSceneOutlinerTedsBridge
//

FSceneOutlinerTedsBridge::~FSceneOutlinerTedsBridge()
{
	TSharedPtr<ISceneOutliner> OutlinerPinned = Outliner.Pin();
	if (OutlinerPinned)
	{
		ClearColumns(*OutlinerPinned);
	}
}

void FSceneOutlinerTedsBridge::Initialize(
	UE::Editor::DataStorage::ICoreProvider& InStorage,
	UE::Editor::DataStorage::IUiProvider& InStorageUi,
	UE::Editor::DataStorage::ICompatibilityProvider& InStorageCompatibility,
	const TSharedPtr<ISceneOutliner>& InOutliner)
{
	Storage = &InStorage;
	StorageUi = &InStorageUi;
	StorageCompatibility = &InStorageCompatibility;
	Outliner = InOutliner;
}

void FSceneOutlinerTedsBridge::RegisterDealiaser(const FTreeItemIDDealiaser& InDealiaser)
{
	Dealiaser = InDealiaser;
}

FTreeItemIDDealiaser FSceneOutlinerTedsBridge::GetDealiaser()
{
	return Dealiaser;
}


void FSceneOutlinerTedsBridge::AssignQuery(UE::Editor::DataStorage::QueryHandle Query,
	const UE::Editor::DataStorage::IUiProvider::FPurposeID& InCellWidgetPurposes, const UE::Editor::DataStorage::IUiProvider::FPurposeID& HeaderWidgetPurpose)
{
	using MatchApproach = UE::Editor::DataStorage::IUiProvider::EMatchApproach;
	constexpr uint8 DefaultPriorityIndex = 100;
	FSceneOutlinerTedsQueryBinder& Binder = FSceneOutlinerTedsQueryBinder::GetInstance();
	CellWidgetPurpose = InCellWidgetPurposes;

	if (TSharedPtr<ISceneOutliner> OutlinerPinned = Outliner.Pin())
	{
		const UE::Editor::DataStorage::FQueryDescription& Description = Storage->GetQueryDescription(Query);
		UE::Editor::DataStorage::FQueryMetaDataView MetaDataView(Description);
		UE::Editor::DataStorage::RowHandle CellPurposeRow = StorageUi->FindPurpose(CellWidgetPurpose);
		UE::Editor::DataStorage::RowHandle HeaderPurposeRow = StorageUi->FindPurpose(HeaderWidgetPurpose);
		
		ClearColumns(*OutlinerPinned);

		if (Description.Action == UE::Editor::DataStorage::FQueryDescription::EActionType::Select)
		{
			int32 SelectionCount = Description.SelectionTypes.Num();
			AddedColumns.Reset(SelectionCount);

			TArray<TWeakObjectPtr<const UScriptStruct>> ColumnTypes = UE::Editor::DataStorage::TableViewerUtils::CreateVerifiedColumnTypeArray(Description.SelectionTypes);

			int32 IndexOffset = 0;
			auto ColumnConstructor = [this, Query, MetaDataView, &IndexOffset, &OutlinerPinned, Binder, HeaderPurposeRow](
				TUniquePtr<FTypedElementWidgetConstructor> Constructor, TConstArrayView<TWeakObjectPtr<const UScriptStruct>> ColumnTypes)
				{
					TSharedPtr<FTypedElementWidgetConstructor> CellConstructor(Constructor.Release());

					/* If we have a fallback column for this query, remove it, take over it's priority and 
					 * replace it with the TEDS column. But also allow the TEDS-Outliner column to fallback to it for
					 * data not in TEDS yet.
				 	 */
					FName FallbackColumn = Binder.FindOutlinerColumnFromTEDSColumns(ColumnTypes);
					const FSceneOutlinerColumnInfo* FallbackColumnInfo = OutlinerPinned->GetSharedData().ColumnMap.Find(FallbackColumn);
					uint8 ColumnPriority = FallbackColumnInfo ? FallbackColumnInfo->PriorityIndex : static_cast<uint8>(FMath::Clamp(DefaultPriorityIndex + static_cast<uint8>(FMath::Clamp(IndexOffset, 0, 255)), 0, 255));

					OutlinerPinned->RemoveColumn(FallbackColumn);

					FName NameId = UE::Editor::DataStorage::TableViewerUtils::FindLongestMatchingName(ColumnTypes, IndexOffset);
					FText DisplayName = CellConstructor.Get()->CreateWidgetDisplayNameText(Storage);
					AddedColumns.Add(NameId);
					OutlinerPinned->AddColumn(NameId,
						FSceneOutlinerColumnInfo(
							ESceneOutlinerColumnVisibility::Visible, 
							ColumnPriority,
							FCreateSceneOutlinerColumn::CreateLambda(
								[this, Query, MetaDataView, NameId, &ColumnTypes, CellConstructor, &OutlinerPinned, FallbackColumn, HeaderPurposeRow](ISceneOutliner&)
								{
									TSharedPtr<FTypedElementWidgetConstructor> HeaderConstructor = 
										UE::Editor::DataStorage::TableViewerUtils::CreateHeaderWidgetConstructor(*StorageUi, MetaDataView, ColumnTypes, HeaderPurposeRow);
									return MakeShared<FOutlinerColumn>(
										Query, *Storage, *StorageUi, *StorageCompatibility, NameId,
										TArray<TWeakObjectPtr<const UScriptStruct>>(ColumnTypes.GetData(), ColumnTypes.Num()), 
										MoveTemp(HeaderConstructor), CellConstructor, FallbackColumn, OutlinerPinned, Dealiaser);

								}),
							true,
							TOptional<float>(),
							DisplayName
						)
					);
					++IndexOffset;
					return true;
				};

			StorageUi->CreateWidgetConstructors(CellPurposeRow, MatchApproach::LongestMatch, ColumnTypes, 
				MetaDataView, ColumnConstructor);
		}
	}
}

void FSceneOutlinerTedsBridge::ClearColumns(ISceneOutliner& InOutliner)
{
	for (FName ColumnName : AddedColumns)
	{
		InOutliner.RemoveColumn(ColumnName);
	}
}

#undef LOCTEXT_NAMESPACE
