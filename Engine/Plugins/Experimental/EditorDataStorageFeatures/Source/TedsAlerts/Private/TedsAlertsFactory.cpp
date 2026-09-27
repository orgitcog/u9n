// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAlertsFactory.h"

#include "Elements/Columns/TypedElementHiearchyColumns.h"
#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"
#include "TedsAlerts.h"
#include "Templates/UnrealTemplate.h"

const FName UTedsAlertsFactory::AlertChainTableName = "Alerts chain";
const FName UTedsAlertsFactory::UnsortedAlertChainTableName = "Alerts chain (unsorted)";
const FName UTedsAlertsFactory::AlertConditionName = FName(TEXT("Alerts"));

void UTedsAlertsFactory::RegisterTables(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	ChainTable = DataStorage.RegisterTable<FTedsAlertColumn, FTedsAlertChainTag>(AlertChainTableName);
	UnsortedChainTable = DataStorage.RegisterTable<FTedsUnsortedAlertChainTag>(ChainTable, UnsortedAlertChainTableName);
}

void UTedsAlertsFactory::RegisterQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	RegisterSubQueries(DataStorage);
	RegisterSortUnsortedAlertsQuery(DataStorage);
	RegisterOnAddQueries(DataStorage);
	RegisterOnRemoveQueries(DataStorage);
	RegisterParentUpdatesQueries(DataStorage);
	RegisterChildAlertUpdatesQueries(DataStorage);
}

UE::Editor::DataStorage::TableHandle UTedsAlertsFactory::GetAlertChainTable() const
{
	return ChainTable;
}

UE::Editor::DataStorage::TableHandle UTedsAlertsFactory::GetUnsortedAlertChainTable() const
{
	return UnsortedChainTable;
}

UE::Editor::DataStorage::QueryHandle UTedsAlertsFactory::GetSortedAlertsQuery() const
{
	return SortedAlertsQuery;
}

UE::Editor::DataStorage::QueryHandle UTedsAlertsFactory::GetUnsortedAlertsQuery() const
{
	return UnsortedAlertsQuery;
}

void UTedsAlertsFactory::RegisterSubQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	SortedAlertsQuery = DataStorage.RegisterQuery(
		Select()
			.ReadWrite<FTedsAlertColumn>()
		.Where()
			.None<FTedsUnsortedAlertChainTag>()
		.Compile());

	UnsortedAlertsQuery = DataStorage.RegisterQuery(
		Select()
			.ReadWrite<FTedsAlertColumn>()
		.Where()
			.All<FTedsUnsortedAlertChainTag>()
		.Compile());
	
	AlertActionQuery = DataStorage.RegisterQuery(
		Select()
			.ReadWrite<FTedsAlertActionColumn>()
		.Compile());

	ChildAlertColumnReadWriteQuery = DataStorage.RegisterQuery(
		Select()
			.ReadWrite<FTedsChildAlertColumn>()
		.Compile());

	ParentReadOnlyQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTableRowParentColumn>()
		.Compile());
}

void UTedsAlertsFactory::RegisterSortUnsortedAlertsQuery(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorage.RegisterQuery(
		Select("Sort unsorted alerts", FProcessor(EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Update)),
			[](IQueryContext& NewAlertContext, RowHandle NewAlertRow, FTedsAlertColumn& NewAlert)
			{
				RowHandle NextRow = NewAlert.NextAlert;
				while (NewAlertContext.IsRowAssigned(NextRow))
				{
					// Walk the chain and add link at the appropriate spot. This can include the row with the active alert.
					NewAlertContext.RunSubquery(0, NextRow, CreateSubqueryCallbackBinding([NewAlertRow, &NewAlert, &NewAlertContext, &NextRow]
						(ISubqueryContext& Context, RowHandle TargetRow, FTedsAlertColumn& TargetAlert)
						{
							if (NewAlert.AlertType > TargetAlert.AlertType ||
								(NewAlert.AlertType == TargetAlert.AlertType && NewAlert.Priority >= TargetAlert.Priority))
							{
								// if the row is not the active alert, then simply link up.
								// Note that "NextAlert" at this point still contains the row that has the active alert.
								if (NewAlert.NextAlert == TargetRow)
								{
									if (TargetAlert.Message.IsEmpty())
									{
										// This is a placeholder so override it.
										AssignAlert(Context, TargetRow, TargetAlert, NewAlertContext, NewAlertRow, NewAlert, 1);
									}
									else
									{
										// The active alert needs to be replaced so swap the active alert with the new alert.
										SwapAlerts(Context, TargetRow, TargetAlert, NewAlertContext, NewAlertRow, NewAlert, 1);
									}
									// (Re)calculate child alerts.
									Context.ActivateQueries(AlertConditionName);
								}
								else
								{
									// Found a spot in the chain. To avoid having to track the previous entry, just swap the 
									// alert at that spot with the new one and chain them up.
									SwapAlerts(Context, TargetRow, TargetAlert, NewAlertContext, NewAlertRow, NewAlert, 1);
								}
								NextRow = InvalidRowHandle;
							}
							else
							{
								if (TargetAlert.NextAlert == InvalidRowHandle)
								{
									// End of the chain so append at the end.
									AppendAlert(TargetAlert, NewAlertContext, NewAlertRow, NewAlert);
									NextRow = InvalidRowHandle;
								}
								else
								{
									// Go to the next row in the chain to check that one.
									NextRow = TargetAlert.NextAlert;
								}
							}
						}));
				}
			})
		.Where()
			.Any<FTedsUnsortedAlertChainTag>()
		.DependsOn()
			.SubQuery(SortedAlertsQuery)
			.SubQuery(AlertActionQuery)
		.Compile());
}

void UTedsAlertsFactory::RegisterOnAddQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorage.RegisterQuery(
		Select("Register alert with parent on alert add", FObserver::OnAdd<FTedsAlertColumn>(),
			[](IQueryContext& Context, RowHandle Row)
			{
				Context.ActivateQueries(AlertConditionName);
			})
		.Where()
			.All<FTableRowParentColumn>() // Only need to do an update pass if there are parents.
			.None<FTedsAlertChainTag>()
		.Compile());

	DataStorage.RegisterQuery(
		Select("Register alert with parent on parent add", FObserver::OnAdd<FTableRowParentColumn>(),
			[](IQueryContext& Context, RowHandle Row)
			{
				Context.ActivateQueries(AlertConditionName);
			})
		.Where()
			.Any<FTedsAlertColumn, FTedsChildAlertColumn>()
			.None<FTedsAlertChainTag>()
		.Compile());
}

void UTedsAlertsFactory::RegisterOnRemoveQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;
	
	DataStorage.RegisterQuery(
		Select("Remove active alert", FObserver::OnRemove<FTedsAlertColumn>(),
			[](IQueryContext& Context, RowHandle Row, const FTedsAlertColumn& Alert)
			{
				// Delete all entries in the alert chain.
				RowHandle NextRow = Alert.NextAlert;
				while (Context.RunSubquery(0, NextRow, CreateSubqueryCallbackBinding(
					[&NextRow](ISubqueryContext& Context, RowHandle Row, FTedsAlertColumn& Alert)
					{
						NextRow = Alert.NextAlert;
						Context.RemoveRow(Row);
					})).Count > 0);


				// Remove any alerts that haven't been processed yet.
				Context.RunSubquery(1, CreateSubqueryCallbackBinding(
					[&Alert](ISubqueryContext& Context, RowHandle Row, FTedsAlertColumn& PendingAlert)
					{
						if (Alert.Name == PendingAlert.Name)
						{
							Context.RemoveRow(Row);
						}
					}));

				// Update any alert parents.
				Context.ActivateQueries(AlertConditionName);
			})
		.Where()
			.None<FTedsAlertChainTag>()
		.DependsOn()
			.SubQuery(SortedAlertsQuery)
			.SubQuery(UnsortedAlertsQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select("Update alert upon parent removal", FObserver::OnRemove<FTableRowParentColumn>(),
			[](IQueryContext& Context, RowHandle Row)
			{
				Context.ActivateQueries(AlertConditionName);
			})
		.Where()
			.Any<FTedsAlertColumn, FTedsChildAlertColumn>()
			.None<FTedsAlertChainTag>()
		.Compile());
}

void UTedsAlertsFactory::RegisterParentUpdatesQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorage.RegisterQuery(
		Select(
			"Trigger alert update if alert's parent changed", 
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Default)),
			[](IQueryContext& Context, FTedsAlertColumn& Alert, const FTableRowParentColumn& Parent)
			{
				if (Alert.CachedParent != Parent.Parent)
				{
					Alert.CachedParent = Parent.Parent;
					Context.ActivateQueries(AlertConditionName);
				}
			})
		.Where()
			.Any<FTypedElementSyncBackToWorldTag, FTypedElementSyncFromWorldTag>()
			.None<FTedsAlertChainTag>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			"Trigger alert update if child alert's parent changed",
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Default)),
			[](IQueryContext& Context, FTedsChildAlertColumn& ChildAlert, const FTableRowParentColumn& Parent)
			{
				if (ChildAlert.CachedParent != Parent.Parent)
				{
					ChildAlert.CachedParent = Parent.Parent;
					Context.ActivateQueries(AlertConditionName);
				}
			})
		.Where()
			.Any<FTypedElementSyncBackToWorldTag, FTypedElementSyncFromWorldTag>()
			.None<FTedsAlertChainTag>()
		.Compile());
}

void UTedsAlertsFactory::RegisterChildAlertUpdatesQueries(UE::Editor::DataStorage::ICoreProvider& DataStorage)
{
	using namespace UE::Editor::DataStorage::Queries;

	DataStorage.RegisterQuery(
		Select(
			"Add missing child alerts",
			FPhaseAmble(FPhaseAmble::ELocation::Preamble, EQueryTickPhase::PostPhysics)
				.MakeActivatable(AlertConditionName),
			[](IQueryContext& Context, RowHandle Row, FTedsAlertColumn& Alert, const FTableRowParentColumn& Parent)
			{
				if (Context.IsRowAssigned(Parent.Parent))
				{
					AddChildAlertsToHierarchy(Context, Parent.Parent, 0);
				}
			})
		.Where()
			.None<FTedsAlertChainTag>()
		.DependsOn()
			.SubQuery(ParentReadOnlyQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			"Clear child alerts",
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::PreUpdate))
				.MakeActivatable(AlertConditionName),
			[](IQueryContext& Context, RowHandle Row, FTedsChildAlertColumn& ChildAlert)
			{
				ResetChildAlertCounters(ChildAlert);
			})
		.Where()
			.None<FTedsAlertChainTag>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			"Increment child alerts",
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::Update))
				.MakeActivatable(AlertConditionName),
			[](IQueryContext& Context, RowHandle Row, FTedsAlertColumn& Alert)
			{
				IncrementParents(Context, Alert.CachedParent, Alert.AlertType, 0);
			})
		.Where()
			.None<FTedsAlertChainTag>()
		.DependsOn()
			.SubQuery(ChildAlertColumnReadWriteQuery)
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			"Remove unused child alerts",
			FProcessor(EQueryTickPhase::PostPhysics, DataStorage.GetQueryTickGroupName(EQueryTickGroups::PostUpdate))
				.MakeActivatable(AlertConditionName),
			[](IQueryContext& Context, RowHandle Row, FTedsChildAlertColumn& ChildAlert)
			{
				for (size_t It = 0; It < static_cast<size_t>(FTedsAlertColumnType::MAX); ++It)
				{
					if (ChildAlert.Counts[It] != 0)
					{
						return;
					}
				}
				Context.RemoveColumns<FTedsChildAlertColumn>(Row);
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
			})
		.Compile());
}

void UTedsAlertsFactory::AssignAlert(
	UE::Editor::DataStorage::ISubqueryContext& TargetContext, UE::Editor::DataStorage::RowHandle TargetRow, FTedsAlertColumn& Target,
	UE::Editor::DataStorage::IQueryContext& SourceContext, UE::Editor::DataStorage::RowHandle SourceRow, FTedsAlertColumn& Source,
	int32 AlertActionQueryIndex)
{
	Target = MoveTemp(Source);
	Target.NextAlert = UE::Editor::DataStorage::InvalidRowHandle;
	
	FTedsAlertActionColumn* TargetAction = GetAction(SourceContext, TargetRow, AlertActionQueryIndex);
	FTedsAlertActionColumn* SourceAction = GetAction(SourceContext, SourceRow, AlertActionQueryIndex);
	if (SourceAction)
	{
		if (TargetAction)
		{
			*TargetAction = MoveTemp(*SourceAction);
		}
		else
		{
			SourceContext.AddColumn(TargetRow, MoveTemp(*SourceAction));
		}
	}
	else
	{
		if (TargetAction)
		{
			TargetContext.RemoveColumns<FTedsAlertActionColumn>(TargetRow);
		}
	}

	// Notify UI.
	TargetContext.AddColumns<FTypedElementSyncBackToWorldTag>(TargetRow);
	// Fully absorbed into the placeholder so the addition to the chain is no longer needed.
	SourceContext.RemoveRow(SourceRow);
}

void UTedsAlertsFactory::SwapAlerts(
	UE::Editor::DataStorage::ISubqueryContext& OriginalContext, UE::Editor::DataStorage::RowHandle OriginalRow, FTedsAlertColumn& Original,
	UE::Editor::DataStorage::IQueryContext& NewContext, UE::Editor::DataStorage::RowHandle NewRow, FTedsAlertColumn& New,
	int32 AlertActionQueryIndex)
{
	Swap(Original, New);
	
	// Swap the alert action columns if they exist.
	{
		FTedsAlertActionColumn* OriginalAction = GetAction(NewContext, OriginalRow, AlertActionQueryIndex);
		FTedsAlertActionColumn* NewAction = GetAction(NewContext, NewRow, AlertActionQueryIndex);
		if (NewAction)
		{
			if (OriginalAction)
			{
				Swap(*OriginalAction, *NewAction);
			}
			else
			{
				OriginalContext.AddColumn(OriginalRow, MoveTemp(*NewAction));
				NewContext.RemoveColumns<FTedsAlertActionColumn>(NewRow);
			}
		}
		else
		{
			if (OriginalAction)
			{
				NewContext.AddColumn(NewRow, MoveTemp(*OriginalAction));
				OriginalContext.RemoveColumns<FTedsAlertActionColumn>(OriginalRow);
			}
		}
	}

	Original.NextAlert = NewRow;
	// Notify UI. Most of the time this is not needed as only the active alert should be used in the UI,
	// but the TEDS Debugger might be showing the other alerts, so make sure they're updated to prevent
	// present invalid data or worse in the case of a action which can cause a crash.
	OriginalContext.AddColumns<FTypedElementSyncBackToWorldTag>(OriginalRow);
	NewContext.AddColumns<FTypedElementSyncBackToWorldTag>(NewRow);
	NewContext.RemoveColumns<FTedsUnsortedAlertChainTag>(NewRow);
}

void UTedsAlertsFactory::AppendAlert(
	FTedsAlertColumn& LastAlert,
	UE::Editor::DataStorage::IQueryContext& AdditionalAlertContext,
	UE::Editor::DataStorage::RowHandle AdditionalAlertRow,
	FTedsAlertColumn& AdditionalAlert)
{
	AdditionalAlert.NextAlert = UE::Editor::DataStorage::InvalidRowHandle;
	LastAlert.NextAlert = AdditionalAlertRow;
	AdditionalAlertContext.RemoveColumns<FTedsUnsortedAlertChainTag>(AdditionalAlertRow);
}

FTedsAlertActionColumn* UTedsAlertsFactory::GetAction(UE::Editor::DataStorage::IQueryContext& Context,
	UE::Editor::DataStorage::RowHandle Row, int32 ChildAlertQueryIndex)
{
	using namespace UE::Editor::DataStorage::Queries;
	
	FTedsAlertActionColumn* Result = nullptr;
	Context.RunSubquery(ChildAlertQueryIndex, Row, CreateSubqueryCallbackBinding(
		[&Result](ISubqueryContext& Context, FTedsAlertActionColumn& Action)
		{
			Result = &Action;
		}));
	return Result;
}

void UTedsAlertsFactory::AddChildAlertsToHierarchy(
	UE::Editor::DataStorage::IQueryContext& Context, UE::Editor::DataStorage::RowHandle Parent, int32 ParentQueryIndex)
{
	using namespace UE::Editor::DataStorage;

	bool bHasParent = true;
	do
	{
		RowHandle NextParent = Parent;
		bHasParent = MoveToNextParent(NextParent, Context, ParentQueryIndex);

		// Check if a child alert column exists and add one if not.
		if (!Context.HasColumn<FTedsChildAlertColumn>(Parent))
		{
			FTedsChildAlertColumn ChildAlert;
			ResetChildAlertCounters(ChildAlert);
			ChildAlert.CachedParent = NextParent;
			Context.AddColumn(Parent, MoveTemp(ChildAlert));
		}
		Parent = NextParent;
	} while (bHasParent);
}

void UTedsAlertsFactory::IncrementParents(
	UE::Editor::DataStorage::IQueryContext& Context, UE::Editor::DataStorage::RowHandle Row, FTedsAlertColumnType AlertType,
	int32 ChildAlertQueryIndex)
{
	using namespace UE::Editor::DataStorage::Queries;

	while (Context.IsRowAvailable(Row))
	{
		FQueryResult Result = Context.RunSubquery(ChildAlertQueryIndex, Row, CreateSubqueryCallbackBinding(
			[&ParentRow = Row, AlertType](
				ISubqueryContext& Context, RowHandle Row, FTedsChildAlertColumn& ChildAlert)
			{
				ChildAlert.Counts[static_cast<size_t>(AlertType)]++;
				Context.AddColumns<FTypedElementSyncBackToWorldTag>(Row);
				ParentRow = ChildAlert.CachedParent;
			}));
		checkf(Result.Count > 0, TEXT("Expected to be able to setup the child alert, but it was missing on the parent column."));
	}
}

bool UTedsAlertsFactory::MoveToNextParent(
	UE::Editor::DataStorage::RowHandle& Parent, UE::Editor::DataStorage::IQueryContext& Context, int32 SubQueryIndex)
{
	using namespace UE::Editor::DataStorage::Queries;

	FQueryResult Result = Context.RunSubquery(SubQueryIndex, Parent, CreateSubqueryCallbackBinding(
		[&Parent](const FTableRowParentColumn& NextParent)
		{
			Parent = NextParent.Parent;
		}));
	Parent = Result.Count != 0 ? Parent : InvalidRowHandle;
	return Result.Count != 0;
}

void UTedsAlertsFactory::ResetChildAlertCounters(FTedsChildAlertColumn& ChildAlert)
{
	for (size_t It = 0; It < static_cast<size_t>(FTedsAlertColumnType::MAX); ++It)
	{
		ChildAlert.Counts[It] = 0;
	}
}
