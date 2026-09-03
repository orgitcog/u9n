// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h" // TraceServices
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "Templates/ValueOrError.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace MassInsightsAnalysis
{

struct MASSINSIGHTSANALYSIS_API FMassInsights
{
	double BeginTime = std::numeric_limits<double>::infinity();
	double EndTime = std::numeric_limits<double>::infinity();
	const TCHAR* Text = nullptr;
	// ID will be zero if the region is identified by Name only
	uint64 ID = 0;
	int32 Depth = -1;
};

class MASSINSIGHTSANALYSIS_API FMassInsightsLane
{
	friend class FMassInsightsProvider;

public:
	FMassInsightsLane(TraceServices::ILinearAllocator& InAllocator) : Regions(InAllocator, 512) {}

	int32 Num() const { return static_cast<int32>(Regions.Num()); }

	/**
	 * Call Callback for every region overlapping the interval defined by IntervalStart and IntervalEnd
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FMassInsights&)> Callback) const;

private:
	TraceServices::TPagedArray<FMassInsights> Regions;
};

enum class EFragmentType : uint8
{
	Unknown,
	Fragment,
	Tag,
	Shared
};

struct MASSINSIGHTSANALYSIS_API FMassFragmentInfo
{
	uint64 Id;
	FString Name;
	uint32 Size;
	EFragmentType Type;

	const TCHAR* GetName() const { return *Name; }
};

struct MASSINSIGHTSANALYSIS_API FMassArchetypeInfo
{
	uint64 Id;
	TArray<const FMassFragmentInfo*> Fragments;

	TConstArrayView<const FMassFragmentInfo*> GetFragments() const
	{
		return MakeArrayView(Fragments);
	}
};
	
enum class EMassEntityEventType : uint8_t
{
	Created,
	ArchetypeChange,
	Destroyed
};

struct FMassEntityEventRecord
{
	double Time;
	uint64 Entity;
	uint64 ArchetypeID;
	EMassEntityEventType Operation;
};

class MASSINSIGHTSANALYSIS_API IMassInsightsProvider
	: public TraceServices::IProvider
{
protected:
	
public:
	virtual ~IMassInsightsProvider() override = default;

	virtual int32 GetFragmentCount() const = 0;
	virtual const FMassFragmentInfo* FindFragmentById(uint64 FragmentId) const = 0;
	
	virtual const FMassArchetypeInfo* FindArchetypeById(uint64 ArchetypeId) const = 0;

	virtual void EnumerateFragments(TFunctionRef<void(const FMassFragmentInfo& FragmentInfo, int32 Index)> Callback, int32 BeginIndex) const = 0;

	virtual TValueOrError<FMassEntityEventRecord, void> GetEntityEvent(uint64 EventIndex) const = 0;

	virtual uint64 GetEntityEventCount() const = 0;
	// Enumerate upto Count number of events starting at the StartIndex
	// Enumeration will end early if there is not enough events or if Callback returns false
	virtual void EnumerateEntityEvents(
		uint64 StartIndex,
		uint64 Count,
		TFunctionRef<void(const FMassEntityEventRecord&, uint64 EventIndex)> Callback) const = 0;

	/**
	 * @return the amount of currently known regions (including open-ended ones)
	 */
	virtual uint64 GetRegionCount() const = 0;

	/**
	 * @return the number of lanes
	 */
	virtual int32 GetLaneCount() const = 0;

	/**
	 * Direct access to a certain lane at a given index/depth.
	 * The pointer is valid only in the current read scope.
	 * @return a pointer to the lane at the specified depth index or nullptr if Index > GetLaneCount()-1
	 */
	virtual const FMassInsightsLane* GetLane(int32 Index) const = 0;

	/**
	 * Enumerates all regions that overlap a certain time interval. Will enumerate by depth but does not expose lanes.
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	virtual bool EnumerateRegions(double  IntervalStart, double IntervalEnd, TFunctionRef<bool(const FMassInsights&)> Callback) const = 0;

	/**
	 * Will call Callback(Lane, Depth) for each lane in order.
	 */
	virtual void EnumerateLanes(TFunctionRef<void(const FMassInsightsLane&, const int32)> Callback) const = 0;

	/**
	 * @return A monotonically increasing counter that that changes each time new data is added to the provider.
	 * This can be used to detect when to update any (UI-)state dependent on the provider during analysis.
	 */
	virtual uint64 GetUpdateCounter() const = 0;
};

/*
* The interface to a provider that can consume mutations of region events from a session.
*/
class IEditableMassInsightsProvider
	: public TraceServices::IEditableProvider
{
public:
	virtual ~IEditableMassInsightsProvider() override = default;
	
	virtual void AddFragment(const FMassFragmentInfo& FragmentInfo) = 0;
	virtual void AddArchetype(const FMassArchetypeInfo& ArchetypeInfo) = 0;
	
	virtual void BulkAddEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs) = 0;
	// Notification of moved entities to the given ArchetypeID
	virtual void BulkMoveEntity(double Time, TConstArrayView<uint64> Entities, TConstArrayView<uint64> ArchetypeIDs) = 0;
	// Notification of destroyed entities
	virtual void BulkDestroyEntity(double Time, TConstArrayView<uint64> Entities) = 0;

	/*
	* Append a new begin event of a region from the trace session.
	*
	* @param Name		The string name of the region.
	* @param Time		The time in seconds of the begin event of this region.
	*/
	virtual void AppendRegionBegin(const TCHAR* Name, double Time) = 0;

	/*
	* Append a new begin event of a region from the trace session.
	*
	* @param Name		The string name of the region.
	* @param Name		The ID of the region. Used to uniquely identify regions with the same name
	* @param Time		The time in seconds of the begin event of this region.
	*/
	virtual void AppendRegionBegin(const TCHAR* Name, uint64 ID, double Time) = 0;
	
	/*
	* Append a new end event of a region from the trace session (by Name).
	*
	* @param Name		The string name of the region.
	* @param Time		The time in seconds of the end event of this region.
	*/
	virtual void AppendRegionEnd(const TCHAR* Name, double Time) = 0;

	/*
	* Append a new end event of a region from the trace session (by ID).
	*
	* @param ID			The ID of the region.
	* @param Time		The time in seconds of the end event of this region.
	*/
	virtual void AppendRegionEnd(const uint64 ID, double Time) = 0;
	
	/**
	 * Called from the analyzer once all events have been processed.
	 * Allows postprocessing and error reporting for regions that were never closed.
	 */
	virtual void OnAnalysisSessionEnded() = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

MASSINSIGHTSANALYSIS_API FName GetMassInsightsProviderName();
MASSINSIGHTSANALYSIS_API const IMassInsightsProvider& ReadMassInsightsProvider(const TraceServices::IAnalysisSession& Session);
MASSINSIGHTSANALYSIS_API IEditableMassInsightsProvider& EditMassInsightsProvider(TraceServices::IAnalysisSession& Session);
	
} // namespace MassInsightsAnalysis
