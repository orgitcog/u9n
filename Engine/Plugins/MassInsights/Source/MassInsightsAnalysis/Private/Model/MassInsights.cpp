// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassInsightsAnalysis/Model/MassInsights.h"
#include "MassInsightsPrivate.h"

#include "Algo/ForEach.h"
#include "Internationalization/Internationalization.h"

DEFINE_LOG_CATEGORY(LogMassInsights)

namespace MassInsightsAnalysis
{

thread_local TraceServices::FProviderLock::FThreadLocalState GMassInsightsProviderLockState;
	
FMassInsightsProvider::FMassInsightsProvider(TraceServices::IAnalysisSession& InSession)
	: Session(InSession)
	, EntityEvents(InSession.GetLinearAllocator(), Constants::EntityEventsPageSize)
{
}

int32 FMassInsightsProvider::GetFragmentCount() const
{
	ReadAccessCheck();
	return FragmentInfoByID.Num();
}

const FMassFragmentInfo* FMassInsightsProvider::FindFragmentById(uint64 FragmentId) const
{
	ReadAccessCheck();
	
	const FMassFragmentInfo* const* Result = FragmentInfoByID.Find(FragmentId);
	if (Result != nullptr)
	{
		return *Result;
	}
	return nullptr;
}

const FMassArchetypeInfo* FMassInsightsProvider::FindArchetypeById(uint64 ArchetypeId) const
{
	ReadAccessCheck();

	const FMassArchetypeInfo* const* Result = ArchetypeByID.Find(ArchetypeId);
	if (Result != nullptr)
	{
		return *Result;
	}
	return nullptr;
}

void FMassInsightsProvider::EnumerateFragments(TFunctionRef<void(const FMassFragmentInfo& FragmentInfo, int32 Index)> Callback, int32 BeginIndex) const
{
	ReadAccessCheck();
	for (int32 Index = BeginIndex, End = FragmentInfos.Num(); Index < End; Index++)
	{
		const FMassFragmentInfo& FragmentInfo = FragmentInfos[Index];
		Callback(FragmentInfo, Index);
	}
}

uint64 FMassInsightsProvider::GetEntityEventCount() const
{
	ReadAccessCheck();

	return EntityEvents.Num();
}

TValueOrError<FMassEntityEventRecord, void> FMassInsightsProvider::GetEntityEvent(uint64 EventIndex) const
{
	ReadAccessCheck();
	
	if (EventIndex < EntityEvents.Num())
	{
		return MakeValue(EntityEvents[EventIndex]);
	}

	return MakeError();
}

void FMassInsightsProvider::EnumerateEntityEvents(
	uint64 StartIndex,
	uint64 Count,
	TFunctionRef<void(const FMassEntityEventRecord&, uint64 EventIndex)> Callback) const
{
	ReadAccessCheck();

	uint64 EndIndex = FPlatformMath::Min(StartIndex + Count, EntityEvents.Num());
	for (uint64 Index = StartIndex; Index < EndIndex; Index++)
	{
		Callback(EntityEvents[Index], Index);
	}
}

uint64 FMassInsightsProvider::GetRegionCount() const
{
	ReadAccessCheck();

	uint64 RegionCount = 0;
	for (const FMassInsightsLane& Lane : Lanes)
	{
		RegionCount += Lane.Num();
	}
	return RegionCount;
}

int32 FMassInsightsProvider::GetLaneCount() const
{
	ReadAccessCheck();
	return Lanes.Num();
}

const FMassInsightsLane* FMassInsightsProvider::GetLane(int32 index) const
{
	ReadAccessCheck();

	if (index < Lanes.Num())
	{
		return &(Lanes[index]);
	}
	return nullptr;
}

void FMassInsightsProvider::AppendRegionBegin(const TCHAR* Name, double Time)
{
	EditAccessCheck();
	// Regions identified by Name don't have an ID
	AppendRegionBegin(Name, 0, Time);
}

void FMassInsightsProvider::AddFragment(const FMassFragmentInfo& FragmentInfo)
{
	EditAccessCheck();

	if (FragmentInfoByID.Find(FragmentInfo.Id) == nullptr)
	{
		uint64 Id = FragmentInfo.Id;
		int32 AllocatedIndex = FragmentInfos.AddElement(FragmentInfo);
		const FMassFragmentInfo* AllocatedFragment = &FragmentInfos[AllocatedIndex];
		FragmentInfoByID.Add(Id, AllocatedFragment);
	}
}

void FMassInsightsProvider::AddArchetype(const FMassArchetypeInfo& ArchetypeInfo)
{
	EditAccessCheck();

	if (ArchetypeByID.Find(ArchetypeInfo.Id) == nullptr)
	{
		uint32 Id = ArchetypeInfo.Id;
		int32 AllocatedIndex = ArchetypeInfos.AddElement(ArchetypeInfo);
		const FMassArchetypeInfo* AllocatedArchetype = &ArchetypeInfos[AllocatedIndex];
		ArchetypeByID.Add(Id, AllocatedArchetype);
	}
}

void FMassInsightsProvider::BulkAddEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs)
{
	EditAccessCheck();

	FMassEntityEventRecord Row;
	Row.Time = Time;
	Row.Operation = EMassEntityEventType::Created;

	for (int32 Index = 0, End = Entities.Num(); Index < End; Index++)
	{
		Row.ArchetypeID = ArchetypeIDs[Index];
		Row.Entity = Entities[Index];
		EntityEvents.EmplaceBack(Row);
	}
}
	
void FMassInsightsProvider::BulkMoveEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs)
{
	EditAccessCheck();

	FMassEntityEventRecord Row;
	Row.Time = Time;
	Row.Operation = EMassEntityEventType::ArchetypeChange;
	
	for (int32 Index = 0, End = Entities.Num(); Index < End; Index++)
	{
		Row.ArchetypeID = ArchetypeIDs[Index];
		Row.Entity = Entities[Index];
		EntityEvents.EmplaceBack(Row);
	}
}
	
void FMassInsightsProvider::BulkDestroyEntity(double Time, TConstArrayView<uint64> Entities)
{
	EditAccessCheck();

	FMassEntityEventRecord Row;
	Row.Time = Time;
	Row.Operation = EMassEntityEventType::Destroyed;
	Row.ArchetypeID = 0;
	
	for (int32 Index = 0, End = Entities.Num(); Index < End; Index++)
	{
		Row.Entity = Entities[Index];
		EntityEvents.EmplaceBack(Row);
	}
}

void FMassInsightsProvider::AppendRegionBegin(const TCHAR* Name, uint64 ID, double Time)
{
	EditAccessCheck();
	// lookup by ID if the region has an ID
	FMassInsights** OpenRegion = ID ? OpenRegionsByID.Find(ID) : OpenRegionsByName.Find(Name);
	
	if (OpenRegion)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogMassInsights, Warning, TEXT("[Regions] A region begin event (%s) was encountered while a region with same name is already open."), Name)
		}
	}
	else
	{
		FMassInsights Region;
		Region.BeginTime = Time;
		Region.Text = Session.StoreString(Name);
		Region.ID = ID;
		Region.Depth = CalculateRegionDepth(Region);

		if (Region.Depth == Lanes.Num())
		{
			Lanes.Emplace(Session.GetLinearAllocator());
		}

		Lanes[Region.Depth].Regions.EmplaceBack(Region);
		FMassInsights* NewOpenRegion = &(Lanes[Region.Depth].Regions.Last());
		
		if (ID)
		{
			OpenRegionsByID.Add(Region.ID, NewOpenRegion);
		} else
		{
			OpenRegionsByName.Add(Region.Text, NewOpenRegion);
		}
		UpdateCounter++;
	}

	// Update session time
	{
		TraceServices::FAnalysisSessionEditScope _(Session);
		Session.UpdateDurationSeconds(Time);
	}
}

void FMassInsightsProvider::AppendRegionEnd(const uint64 ID, double Time)
{
	EditAccessCheck();

	FMassInsights** OpenRegionPos = OpenRegionsByID.Find(ID);
	FMassInsights* OpenRegion = nullptr;
	if (OpenRegionPos)
		OpenRegion = *OpenRegionPos;

	AppendRegionEndInternal(OpenRegion, Time);
}
	
void FMassInsightsProvider::AppendRegionEnd(const TCHAR* Name, double Time)
{
	EditAccessCheck();

	FMassInsights** OpenRegionPos = OpenRegionsByName.Find(Name);
	FMassInsights* OpenRegion = nullptr;
	if (OpenRegionPos)
	{
		OpenRegion = *OpenRegionPos;		
	}
	else
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogMassInsights, Warning, TEXT("[Regions] A region end event (%s) was encountered without having seen a matching region start event first."), Name)
		}
	}

	AppendRegionEndInternal(OpenRegion, Time);
}

void FMassInsightsProvider::AppendRegionEndInternal(FMassInsights* OpenRegion, double Time)
{
	if (OpenRegion)
	{
		OpenRegion->EndTime = Time;

		if (OpenRegion->ID)
		{
			OpenRegionsByID.Remove(OpenRegion->ID);
		}
		OpenRegionsByName.Remove(OpenRegion->Text);
		UpdateCounter++;
	}

	// Update session time
	{
		TraceServices::FAnalysisSessionEditScope _(Session);
		Session.UpdateDurationSeconds(Time);
	}
}
	
void FMassInsightsProvider::OnAnalysisSessionEnded()
{
	EditAccessCheck();

	auto printOpenRegionMessage = [this](const auto& KV)
	{
		const FMassInsights* Region = KV.Value;
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogMassInsights, Warning, TEXT("[Regions] A region begin event (%s) was never closed."), Region->Text)
		}
	};
	Algo::ForEach(OpenRegionsByID, printOpenRegionMessage);
	Algo::ForEach(OpenRegionsByName, printOpenRegionMessage);

	if (NumWarnings > 0 || NumErrors > 0)
	{
		UE_LOG(LogMassInsights, Error, TEXT("[Regions] %u warnings; %u errors"), NumWarnings, NumErrors);
	}

	const uint64 TotalRegionCount = GetRegionCount();
	UE_LOG(LogMassInsights, Log, TEXT("[Regions] Analysis completed (%llu regions, %d lanes)."), TotalRegionCount, Lanes.Num());
}

int32 FMassInsightsProvider::CalculateRegionDepth(const FMassInsights& Region) const
{
	constexpr int32 DepthLimit = 100;

	int32 NewDepth = 0;

	// Find first free lane/depth
	while (NewDepth < DepthLimit)
	{
		if (!Lanes.IsValidIndex(NewDepth))
		{
			break;
		}

		const FMassInsights& LastRegion = Lanes[NewDepth].Regions.Last();
		if (LastRegion.EndTime <= Region.BeginTime)
		{
			break;
		}
		NewDepth++;
	}

	ensureMsgf(NewDepth < DepthLimit, TEXT("Regions are nested too deep."));

	return NewDepth;
}

void FMassInsightsProvider::EnumerateLanes(TFunctionRef<void(const FMassInsightsLane&, int32)> Callback) const
{
	ReadAccessCheck();

	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		Callback(Lanes[LaneIndex], LaneIndex);
	}
}

bool FMassInsightsProvider::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FMassInsights&)> Callback) const
{
	ReadAccessCheck();

	if (IntervalStart > IntervalEnd)
	{
		return false;
	}

	for (const FMassInsightsLane& Lane : Lanes)
	{
		if (!Lane.EnumerateRegions(IntervalStart, IntervalEnd, Callback))
		{
			return false;
		}
	}

	return true;
}

bool FMassInsightsLane::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FMassInsights&)> Callback) const
{
	const FInt32Interval OverlapRange = GetElementRangeOverlappingGivenRange<FMassInsights>(Regions, IntervalStart, IntervalEnd,
		[](const FMassInsights& r) { return r.BeginTime; },
		[](const FMassInsights& r) { return r.EndTime; });

	if (OverlapRange.Min == -1)
	{
		return true;
	}

	for (int32 Index = OverlapRange.Min; Index <= OverlapRange.Max; ++Index)
	{
		if (!Callback(Regions[Index]))
		{
			return false;
		}
	}

	return true;
}
	
FName GetMassInsightsProviderName()
{
	static const FName Name("MassInsightsProvider");
	return Name;
}

const IMassInsightsProvider& ReadMassInsightsProvider(const TraceServices::IAnalysisSession& Session)
{
	return *Session.ReadProvider<IMassInsightsProvider>(GetMassInsightsProviderName());
}

IEditableMassInsightsProvider& EditMassInsightsProvider(TraceServices::IAnalysisSession& Session)
{
	return *Session.EditProvider<IEditableMassInsightsProvider>(GetMassInsightsProviderName());
}

} // namespace MassInsightsAnalysis
