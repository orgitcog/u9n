// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassInsightsTraceAnalysis.h"

#include "Common/ProviderLock.h"
#include "HAL/LowLevelMemTracker.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Model/MassInsightsPrivate.h"

namespace MassInsightsAnalysis
{

FMassInsightsTraceAnalyzer::FMassInsightsTraceAnalyzer(TraceServices::IAnalysisSession& InSession,
                                                         FMassInsightsProvider& InRegionProvider)
	: Session(InSession)
	, MassInsightsProvider(InRegionProvider)
{
}

void FMassInsightsTraceAnalyzer::OnAnalysisBegin(const FOnAnalysisContext& Context)
{
	auto& Builder = Context.InterfaceBuilder;
	
	Builder.RouteEvent(RouteId_RegisterMassFragment, "MassTrace", "RegisterMassFragment");
	Builder.RouteEvent(RouteId_RegisterMassArchetype, "MassTrace", "RegisterMassArchetype");
	
	Builder.RouteEvent(RouteId_MassBulkAddEntity, "MassTrace", "MassBulkAddEntity");
	Builder.RouteEvent(RouteId_MassEntityMoved, "MassTrace", "MassEntityMoved");
	Builder.RouteEvent(RouteId_MassBulkEntityDestroyed, "MassTrace", "MassBulkEntityDestroyed");
	
	Builder.RouteEvent(RouteId_MassPhaseBegin, "MassTrace", "MassPhaseBegin");
	Builder.RouteEvent(RouteId_MassPhaseEnd, "MassTrace", "MassPhaseEnd");
}

void FMassInsightsTraceAnalyzer::OnAnalysisEnd()
{
	TraceServices::FProviderEditScopeLock RegionProviderScopedLock(static_cast<TraceServices::IEditableProvider&>(MassInsightsProvider));
	MassInsightsProvider.OnAnalysisSessionEnded();
}

bool FMassInsightsTraceAnalyzer::OnEvent(uint16 RouteId, EStyle Style, const FOnEventContext& Context)
{
	const auto& EventData = Context.EventData;
	switch (RouteId)
	{
		case RouteId_RegisterMassFragment:
			{
				uint64 FragmentId = EventData.GetValue<uint64>("FragmentId");
				FString FragmentName;
				EventData.GetString("FragmentName", FragmentName);

				uint32 FragmentSize = EventData.GetValue<uint32>("FragmentSize");
				EFragmentType FragmentType = static_cast<EFragmentType>(EventData.GetValue<uint8>("FragmentType"));
				FMassFragmentInfo FragmentInfo
				{
					.Id = FragmentId,
					.Name = MoveTemp(FragmentName),
					.Size = FragmentSize,
					.Type = FragmentType,
				};

				TraceServices::FProviderEditScopeLock Lock(MassInsightsProvider);
				
				MassInsightsProvider.AddFragment(FragmentInfo);
				break;
			}
		case RouteId_RegisterMassArchetype:
			{
				uint64 Id = EventData.GetValue<uint64>("ArchetypeID");
				const auto FragmentArrayView = EventData.GetArrayView<uint64>("Fragments");
				FMassArchetypeInfo ArchetypeInfo
				{
					.Id = Id,
				};

				ArchetypeInfo.Fragments.Reserve(FragmentArrayView.Num());
				{
					TraceServices::FProviderReadScopeLock ReadLock(MassInsightsProvider);
					for (int32 Index = 0; Index < FragmentArrayView.Num(); ++Index)
					{
						const uint64 FragmentId = FragmentArrayView[Index];
						ArchetypeInfo.Fragments.Add(MassInsightsProvider.FindFragmentById(FragmentId));
					}
					Algo::Sort(ArchetypeInfo.Fragments, [](const FMassFragmentInfo* Lhs, const FMassFragmentInfo* Rhs)
					{
						if (Lhs->Type == Rhs->Type)
						{
							return Lhs->Name < Rhs->Name;
						}
						return Lhs->Type < Rhs->Type;
					});
				}
				{
					TraceServices::FProviderEditScopeLock EditLock(MassInsightsProvider);
					MassInsightsProvider.AddArchetype(ArchetypeInfo);
				}
				break;
			}
		case RouteId_MassBulkAddEntity:
			{
				uint64 Cycle = EventData.GetValue<uint64>("Cycle");
				const auto Entities = EventData.GetArrayView<uint64>("Entities");
				const auto ArchetypeIDs = EventData.GetArrayView<uint64>("ArchetypeIDs");
				
				TraceServices::FProviderEditScopeLock EditLock(MassInsightsProvider);
				MassInsightsProvider.BulkAddEntity(Context.EventTime.AsSeconds(Cycle), Entities, ArchetypeIDs);
				break;
			}
		case RouteId_MassEntityMoved:
			{
				uint64 Cycle = EventData.GetValue<uint64>("Cycle");
				uint64 Entity = EventData.GetValue<uint64>("Entity");
				uint64 Archetype = EventData.GetValue<uint64>("NewArchetypeID");

				TraceServices::FProviderEditScopeLock EditLock(MassInsightsProvider);
				MassInsightsProvider.BulkMoveEntity(
					Context.EventTime.AsSeconds(Cycle),
					MakeConstArrayView(&Entity, 1),
					MakeConstArrayView(&Archetype, 1));
				break;
			}
		case RouteId_MassBulkEntityDestroyed:
			{
				uint64 Cycle = EventData.GetValue<uint64>("Cycle");
				const auto Entities = EventData.GetArrayView<uint64>("Entities");

				TraceServices::FProviderEditScopeLock EditLock(MassInsightsProvider);
				MassInsightsProvider.BulkDestroyEntity(
					Context.EventTime.AsSeconds(Cycle),
					Entities);
				break;
			}
		case RouteId_MassPhaseBegin:
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			FString PhaseName;
			EventData.GetString("PhaseName", PhaseName);
			
			const uint64 PhaseId = EventData.GetValue<uint64>("PhaseId", 0);
			// a missing or 0 RegionID indicates that the region is identified by name only
			if (PhaseId > 0)
			{
				TraceServices::FProviderEditScopeLock RegionProviderScopedLock(MassInsightsProvider);
				MassInsightsProvider.AppendRegionBegin(*PhaseName, PhaseId, Context.EventTime.AsSeconds(Cycle));
			}
			else
			{
				TraceServices::FProviderEditScopeLock RegionProviderScopedLock(MassInsightsProvider);
				MassInsightsProvider.AppendRegionBegin(*PhaseName, Context.EventTime.AsSeconds(Cycle));
			}

			break;
		}

		case RouteId_MassPhaseEnd:
		{
			uint64 Cycle = EventData.GetValue<uint64>("Cycle");
			uint64_t PhaseID = EventData.GetValue<uint64>("PhaseId", 0);

			TraceServices::FProviderEditScopeLock RegionProviderScopedLock(MassInsightsProvider);
			if (PhaseID > 0)
			{
				MassInsightsProvider.AppendRegionEnd(PhaseID, Context.EventTime.AsSeconds(Cycle));
			}
			else
			{
				FString PhaseName = TEXT("Invalid");
				EventData.GetString("PhaseName", PhaseName);
				MassInsightsProvider.AppendRegionEnd(*PhaseName, Context.EventTime.AsSeconds(Cycle));
			}
			break;
		}
	}

	return true;
}

} // namespace MassInsightsAnalysis
