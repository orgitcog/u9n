// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/ProviderLock.h"
#include "Containers/ChunkedArray.h"
#include "Containers/PagedArray.h"
#include "Templates/SharedPointer.h"
#include "MassInsightsAnalysis/Model/MassInsights.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMassInsights, Display, All)

namespace TraceServices
{
	class FAnalysisSessionLock;
	class FStringStore;
}

namespace MassInsightsAnalysis
{

extern thread_local TraceServices::FProviderLock::FThreadLocalState GMassInsightsProviderLockState;

class FMassInsightsProvider
	: public IMassInsightsProvider
	, public IEditableMassInsightsProvider
{
	struct Constants
	{
		static constexpr uint64 EntityEventsPageSize = 65536;
	};
public:
	explicit FMassInsightsProvider(TraceServices::IAnalysisSession& Session);
	virtual ~FMassInsightsProvider() override {}

	//////////////////////////////////////////////////
	// Read operations

	virtual void BeginRead() const override       { Lock.BeginRead(GMassInsightsProviderLockState); }
	virtual void EndRead() const override         { Lock.EndRead(GMassInsightsProviderLockState); }
	virtual void ReadAccessCheck() const override { Lock.ReadAccessCheck(GMassInsightsProviderLockState); }

	virtual int32 GetFragmentCount() const override;
	virtual const FMassFragmentInfo* FindFragmentById(uint64 FragmentId) const override;
	virtual const FMassArchetypeInfo* FindArchetypeById(uint64 ArchetypeId) const override;
	
	virtual void EnumerateFragments(TFunctionRef<void(const FMassFragmentInfo& FragmentInfo, int32 Index)> Callback, int32 BeginIndex) const override;

	virtual uint64 GetEntityEventCount() const override;
	virtual TValueOrError<FMassEntityEventRecord, void> GetEntityEvent(uint64 EventIndex) const override;
	virtual void EnumerateEntityEvents(
		uint64 StartIndex,
		uint64 Count,
		TFunctionRef<void(const FMassEntityEventRecord&, uint64 /*EventIndex*/)> Callback) const override;
	
	virtual uint64 GetRegionCount() const override;
	virtual int32 GetLaneCount()  const override;

	virtual const FMassInsightsLane* GetLane(int32 Index) const override;

	virtual bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FMassInsights&)> Callback) const override;
	virtual void EnumerateLanes(TFunctionRef<void(const FMassInsightsLane&, const int32)> Callback) const override;

	virtual uint64 GetUpdateCounter() const override { ReadAccessCheck(); return UpdateCounter; }

	//////////////////////////////////////////////////
	// Edit operations

	virtual void BeginEdit() const override       { Lock.BeginWrite(GMassInsightsProviderLockState); }
	virtual void EndEdit() const override         { Lock.EndWrite(GMassInsightsProviderLockState); }
	virtual void EditAccessCheck() const override { Lock.WriteAccessCheck(GMassInsightsProviderLockState); }

	virtual void AddFragment(const FMassFragmentInfo& FragmentInfo) override;
	virtual void AddArchetype(const FMassArchetypeInfo& ArchetypeInfo) override;
	
	virtual void BulkAddEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs) override;
	virtual void BulkMoveEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs) override;
	virtual void BulkDestroyEntity(double Time, TConstArrayView<uint64> Entities) override;
	
	virtual void AppendRegionBegin(const TCHAR* Name, uint64 ID, double Time) override;
	virtual void AppendRegionBegin(const TCHAR* Name, double Time) override;
	virtual void AppendRegionEnd(const TCHAR* ID, double Time) override;
	virtual void AppendRegionEnd(const uint64 ID, double Time) override;

	virtual void OnAnalysisSessionEnded() override;

	//////////////////////////////////////////////////

private:
	// Update the depth member of a region to allow overlapping regions to be displayed on separate lanes.
	int32 CalculateRegionDepth(const FMassInsights& Item) const;
	void AppendRegionEndInternal(FMassInsights* OpenRegion, double Time);

private:
	mutable TraceServices::FProviderLock Lock;

	TraceServices::IAnalysisSession&	Session;

	TMap<uint64, const FMassFragmentInfo*> FragmentInfoByID;
	TChunkedArray<FMassFragmentInfo> FragmentInfos;

	// Allocation of ranges of fragments, mostly for ArchetypeInfo
	TChunkedArray<const FMassFragmentInfo*> FragmentInfoRanges;

	TMap<uint32, const FMassArchetypeInfo*> ArchetypeByID;
	TChunkedArray<FMassArchetypeInfo> ArchetypeInfos;
	
	// Timeseries updates
	
	// Sorted by Cycle
	TraceServices::TPagedArray<FMassEntityEventRecord> EntityEvents;

	// Open regions inside lanes
	TMap<FStringView, FMassInsights*> OpenRegionsByName;
	TMap<uint64_t, FMassInsights*> OpenRegionsByID;

	// Closed regions
	TArray<FMassInsightsLane> Lanes;

	// Counter incremented each time region data changes during analysis
	uint64 UpdateCounter = -1;

	static constexpr uint32 MaxWarningMessages = 100;
	static constexpr uint32 MaxErrorMessages = 100;

	uint32 NumWarnings = 0;
	uint32 NumErrors = 0;
};

} // namespace MassInsightsAnalysis
