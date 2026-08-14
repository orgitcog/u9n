// Copyright Epic Games, Inc. All Rights Reserved.

#include "TraceServices/Model/Regions.h"
#include "Model/RegionsPrivate.h"

#include "AnalysisServicePrivate.h"
#include "Algo/ForEach.h"
#include "Common/FormatArgs.h"
#include "Common/Utils.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "RegionProvider"

namespace TraceServices
{

thread_local FProviderLock::FThreadLocalState GRegionsProviderLockState;

// ####################### FRegionTimeline ##########################

const TCHAR* FRegionTimeline::GetCategory() const
{
	Provider.ReadAccessCheck();
	return Category;
}
	
int32 FRegionTimeline::GetLaneCount() const
{
	Provider.ReadAccessCheck();
	return Lanes.Num();
}

const FRegionLane* FRegionTimeline::GetLane(int32 Index) const
{
	Provider.ReadAccessCheck();
	
	if (Index >= 0 && Index < Lanes.Num())
	{
		return &(Lanes[Index]);
	}
	return nullptr;
}

bool FRegionTimeline::EnumerateRegions(double IntervalStart, double IntervalEnd,
	TFunctionRef<bool(const FTimeRegion&)> Callback) const
{
	Provider.ReadAccessCheck();
	
	if (IntervalStart > IntervalEnd)
	{
		return false;
	}

	for (const FRegionLane& Lane : Lanes)
	{
		if (!Lane.EnumerateRegions(IntervalStart, IntervalEnd, Callback))
		{
			return false;
		}
	}

	return true;
}

void FRegionTimeline::EnumerateLanes(TFunctionRef<void(const FRegionLane&, const int32)> Callback) const
{
	Provider.ReadAccessCheck();
	
	for (int32 LaneIndex = 0; LaneIndex < Lanes.Num(); ++LaneIndex)
	{
		Callback(Lanes[LaneIndex], LaneIndex);
	}
}

FTimeRegion* FRegionTimeline::InsertNewRegion(double BeginTime, const TCHAR* Name, uint64 Id, const TCHAR* RegionCategory)
{
	Provider.EditAccessCheck();
	
	FTimeRegion Region;
	Region.BeginTime = BeginTime;
	Region.Text = Name;
	Region.Id = Id;
	Region.Category = RegionCategory;
	Region.Depth = CalculateRegionDepth(Region);

	if (Region.Depth == Lanes.Num())
	{
		Lanes.Emplace(Session.GetLinearAllocator());
	}
	FRegionLane& NewLane = Lanes[Region.Depth];	
	NewLane.Regions.EmplaceBack(Region);
	FTimeRegion* NewOpenRegion = &(Lanes[Region.Depth].Regions.Last());

	return NewOpenRegion;
}

int32 FRegionTimeline::CalculateRegionDepth(const FTimeRegion& Region) const
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
		
		const FTimeRegion& LastRegion = Lanes[NewDepth].Regions.Last();
		if (LastRegion.EndTime <= Region.BeginTime)
		{
			break;
		}
		NewDepth++;
	}

	ensureMsgf(NewDepth < DepthLimit, TEXT("Regions are nested too deep."));

	return NewDepth;
}	
	
// ##################### RegionProvider #####################	

FRegionProvider::FRegionProvider(IAnalysisSession& InSession)
	: Session(InSession), AllRegions(*this, InSession, nullptr)
{
}

void FRegionProvider::EnumerateTimelinesByCategory(
	TFunctionRef<void(const IRegionTimeline&, const TCHAR*)> Callback) const
{
	ReadAccessCheck();
	
	for (auto& KV: RegionsPerCategory)
	{
		Callback(KV.Value, KV.Key);
	}
}

const IRegionTimeline& FRegionProvider::GetDefaultTimeline() const
{
	ReadAccessCheck();
	
	return AllRegions;
}

const IRegionTimeline* FRegionProvider::GetTimelineForCategory(const TCHAR* Category) const
{
	ReadAccessCheck();

	if (!Category)
	{
		return &AllRegions;
	}
	
	if (const FRegionTimeline* Timeline = RegionsPerCategory.Find(Category))
	{
		return Timeline;
	}
	return nullptr;
}
	
uint64 FRegionProvider::GetRegionCount() const
{
	ReadAccessCheck();

	uint64 RegionCount = 0;
	AllRegions.EnumerateLanes([&RegionCount](const FRegionLane& Lane, const int32 Index) {
		RegionCount += Lane.Num();
	});
	return RegionCount;
}

const FRegionLane* FRegionProvider::GetLane(int32 index) const
{
	ReadAccessCheck();
	return AllRegions.GetLane(index);
}

void FRegionProvider::AppendRegionBegin(const TCHAR* Name, double Time, const TCHAR* Category)
{
	EditAccessCheck();

	check(Name)

	TArray<FTimeRegion*>* OpenRegions = OpenRegionsByName.Find(Name);

	if (OpenRegions)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region begin event (BeginTime=%f, Name=\"%s\") was encountered while a region with same name is already open."), Time, Name)
		}

		// Automatically end the previous region.
		AppendRegionEnd(Name, Time);
	}

	TArray<FTimeRegion*> NewRegions = InsertNewRegion(Time, Name, 0, Category);
	OpenRegionsByName.Add(NewRegions[0]->Text, NewRegions);

	UpdateCounter++;
}

void FRegionProvider::AppendRegionBeginWithId(const TCHAR* Name, uint64 Id, double Time, const TCHAR* Category)
{
	EditAccessCheck();

	check(Name && Id)
	TArray<FTimeRegion*>* OpenRegions = OpenRegionsById.Find(Id);

	if (OpenRegions)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region begin event (BeginTime=%f, Name=\"%s\", Id=%llu) was encountered while a region with same name is already open."), Time, Name, Id)
		}

		// Automatically end the previous region.
		AppendRegionEndWithId(Id, Time);
	}

	TArray<FTimeRegion*> NewRegions = InsertNewRegion(Time, Name, Id, Category);
	OpenRegionsById.Add(Id, NewRegions);
	
	UpdateCounter++;
}

void FRegionProvider::AppendRegionEnd(const TCHAR* Name, double Time)
{
	EditAccessCheck();

	check(Name)
	TArray<FTimeRegion*>* OpenRegionsPos = OpenRegionsByName.Find(Name);

	if (!OpenRegionsPos)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region end event (EndTime=%f, Name=\"%s\") was encountered without having seen a matching region begin event first."), Time, Name)
		}

		AppendRegionBegin(Name, Time);
		OpenRegionsPos = OpenRegionsByName.Find(Name);
		check(OpenRegionsPos);
	}

	for (FTimeRegion* OpenRegion: *OpenRegionsPos )
	{
		check(OpenRegion);
		OpenRegion->EndTime = Time;
	}	

	OpenRegionsByName.Remove(Name);
	
	UpdateCounter++;
}

void FRegionProvider::AppendRegionEndWithId(uint64 Id, double Time)
{
	EditAccessCheck();

	if (!Id)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region end event with id 0 was encountered, ignoring (EndTime=%f."), Time)
		}
		return;
	}
	TArray<FTimeRegion*>* OpenRegionsPos = OpenRegionsById.Find(Id);

	if (!OpenRegionsPos)
	{
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region end event (EndTime=%f, Id=%llu) was encountered without having seen a matching region begin event first."), Time, Id)
		}

		// Automatically create a new region.
		// Generates a display name if we're missing a begin and are closing by ID
		FString GeneratedName = FString::Printf(TEXT("Unknown Region (missing begin, Id=%llu)"), Id);
		AppendRegionBeginWithId(*GeneratedName, Id, Time);
		OpenRegionsPos = OpenRegionsById.Find(Id);
		check(OpenRegionsPos);
	}

	for (FTimeRegion* OpenRegion: *OpenRegionsPos )
	{
		check(OpenRegion);
		OpenRegion->EndTime = Time;
	}

	OpenRegionsById.Remove(Id);
	
	UpdateCounter++;
}

void FRegionProvider::OnAnalysisSessionEnded()
{
	EditAccessCheck();

	auto printOpenRegionMessage = [this](const auto& KV)
	{
		const TArray<FTimeRegion*> Regions = KV.Value;
		const auto Region = Regions[0];
		
		++NumWarnings;
		if (NumWarnings <= MaxWarningMessages)
		{
			UE_LOG(LogTraceServices, Warning, TEXT("[Regions] A region (BeginTime=%f, Name=\"%s\", Id=%llu) was never closed."), Region->BeginTime, Region->Text, Region->Id)
		}
	};
	Algo::ForEach(OpenRegionsById, printOpenRegionMessage);
	Algo::ForEach(OpenRegionsByName, printOpenRegionMessage);

	if (NumWarnings > 0)
	{
		UE_LOG(LogTraceServices, Warning, TEXT("[Regions] %u warnings"), NumWarnings);
	}
	if (NumErrors > 0)
	{
		UE_LOG(LogTraceServices, Error, TEXT("[Regions] %u errors"), NumErrors);
	}

	uint64 TotalRegionCount = GetRegionCount();
	UE_LOG(LogTraceServices, Log, TEXT("[Regions] Analysis completed (%llu regions, %d lanes)."), TotalRegionCount, AllRegions.Lanes.Num());
}


TArray<FTimeRegion*> FRegionProvider::InsertNewRegion(double BeginTime, const TCHAR* Name, uint64 Id,
                                                      const TCHAR* Category)
{
	const TCHAR* StoredName = Session.StoreString(Name);
	const TCHAR* StoredCategory = Category ? Session.StoreString(Category) : nullptr;
	
	TArray<FTimeRegion*> Regions;
	Regions.Add(AllRegions.InsertNewRegion(BeginTime, StoredName, Id, StoredCategory));

	// skip per category timeline if no category is set
	const TCHAR* Key = !StoredCategory ? UncategorizedName : StoredCategory;
	
	FRegionTimeline* Timeline = RegionsPerCategory.Find(Key);
	if (!Timeline)
	{
		FRegionTimeline NewTimeline{*this, Session, Key};
		Timeline = &RegionsPerCategory.Emplace(Key, NewTimeline);
	}
	Regions.Add(Timeline->InsertNewRegion(BeginTime, StoredName, Id, StoredCategory));
	return Regions;
}

void FRegionProvider::EnumerateLanes(TFunctionRef<void(const FRegionLane&, int32)> Callback) const
{
	ReadAccessCheck();
	AllRegions.EnumerateLanes(Callback);
}

bool FRegionProvider::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const
{
	ReadAccessCheck();
	return AllRegions.EnumerateRegions(IntervalStart, IntervalEnd, Callback);
}

// ###################### FRegionLane #########################

bool FRegionLane::EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const
{
	const FInt32Interval OverlapRange = GetElementRangeOverlappingGivenRange<FTimeRegion>(Regions, IntervalStart, IntervalEnd,
		[](const FTimeRegion& r) { return r.BeginTime; },
		[](const FTimeRegion& r) { return r.EndTime; });

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

FName GetRegionProviderName()
{
	static const FName Name("RegionProvider");
	return Name;
}
const IRegionProvider& ReadRegionProvider(const IAnalysisSession& Session)
{
	return *Session.ReadProvider<IRegionProvider>(GetRegionProviderName());
}

IEditableRegionProvider& EditRegionProvider(IAnalysisSession& Session)
{
	return *Session.EditProvider<IEditableRegionProvider>(GetRegionProviderName());
}

} // namespace TraceServices

#undef LOCTEXT_NAMESPACE
