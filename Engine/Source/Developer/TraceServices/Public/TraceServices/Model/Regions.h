// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Common/PagedArray.h" // TraceServices
#include "HAL/Platform.h"
#include "Templates/Function.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "UObject/NameTypes.h"

namespace TraceServices
{

struct TRACESERVICES_API FTimeRegion
{
	double BeginTime = std::numeric_limits<double>::infinity();
	double EndTime = std::numeric_limits<double>::infinity();
	const TCHAR* Text = nullptr;
	/// Id will be zero if the region is identified by Name only
	uint64 Id = 0;
	int32 Depth = -1;
	/// this will be nullptr for regions without a Category
	const TCHAR* Category = nullptr;
};

class TRACESERVICES_API FRegionLane
{
	friend class FRegionTimeline;

public:
	FRegionLane(ILinearAllocator& InAllocator) : Regions(InAllocator, 512) {}

	int32 Num() const { return static_cast<int32>(Regions.Num()); }

	/**
	 * Call Callback for every region overlapping the interval defined by IntervalStart and IntervalEnd
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const;

private:
	TPagedArray<FTimeRegion> Regions;
};

/*
 *	Sorts a set of Timing Regions into a stack of individual lanes without overlaps for display
 */
class TRACESERVICES_API IRegionTimeline
{
public:
	/**
	 * @return the category of this timeline if filtered or nullptr
	 */
	virtual const TCHAR* GetCategory() const = 0;
	
	/**
	 * @return the number of lanes
	 */
	virtual int32 GetLaneCount() const = 0;

	/**
	 * Direct access to a certain lane at a given index/depth.
	 * The pointer is valid only in the current read scope.
	 * @return a pointer to the lane at the specified depth index or nullptr if Index > GetLaneCount()-1
	 */
	virtual const FRegionLane* GetLane(int32 Index) const = 0;

	/**
	 * Enumerates all regions that overlap a certain time interval. Will enumerate by depth but does not expose lanes.
	 * 
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	virtual bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion&)> Callback) const = 0;
	
	/**
	 * Will call Callback(Lane, Depth) for each lane in order.
	 */
	virtual void EnumerateLanes(TFunctionRef<void(const FRegionLane&, const int32)> Callback) const = 0;
};
	
class TRACESERVICES_API IRegionProvider
	: public IProvider
{
public:
	virtual ~IRegionProvider() override = default;

	/**
	*	Enumerates all timelines, this includes the uncategorized timeline with uncategorized regions and individual timelines for each Category
	*	@param Callback a callback called for each timeline. Second parameter contains Category or nullptr for the uncategorized timeline.
	*/
	virtual void EnumerateTimelinesByCategory(TFunctionRef<void(const IRegionTimeline& /*Region*/, const TCHAR* /*Category*/)> Callback) const = 0;

	/**
	*	@return the default timeline containing all regions without filtering 
	*/
	virtual const IRegionTimeline& GetDefaultTimeline() const = 0;
	
	/**
	*	@return the timeline for a given Category or the timeline with uncategorized regions if Category is nullptr. Returns nullptr if the category was invalid/not found
	*/
	virtual const IRegionTimeline* GetTimelineForCategory(const TCHAR* Category) const = 0;

	/**
	 * @return returns the string used to store regions with no explicit category set ("__Uncategorized__")
	 */
	virtual const TCHAR* GetUncategorizedRegionCategoryName() const = 0;
	
	/**
	 * @return the amount of currently known regions (including open-ended ones)
	 */
	virtual uint64 GetRegionCount() const = 0;

	/**
	 * @return the number of lanes of the default timeline
	 */
	UE_DEPRECATED(5.6, "Use GetDefaultTimeline().GetLaneCount() or EnumerateTimelinesByCategory() to access timelines instead.")
	virtual int32 GetLaneCount() const = 0;

	/**
	 * Direct access to a certain lane at a given index/depth of the default timeline.
	 * The pointer is valid only in the current read scope.
	 * @return a pointer to the lane at the specified depth index or nullptr if Index > GetLaneCount()-1
	 */
	UE_DEPRECATED(5.6, "Use GetDefaultTimeline().GetLane() or EnumerateTimelinesByCategory() to access timelines instead.")
	virtual const FRegionLane* GetLane(int32 Index) const = 0;

	/**
	 * Enumerates all regions that overlap a certain time interval. Will enumerate by depth but does not expose lanes.
	 * 
	 * @param Callback a callback called for each region. Return false to abort iteration.
	 * @returns true if the enumeration finished, false if it was aborted by the callback returning false
	 */
	UE_DEPRECATED(5.6, "Use GetDefaultTimeline().EnumerateRegions() or EnumerateTimelinesByCategory() to access timelines instead.")
	virtual bool EnumerateRegions(double IntervalStart, double IntervalEnd, TFunctionRef<bool(const FTimeRegion& /*Region*/)> Callback) const = 0;

	/**
	 * Will call Callback(Lane, Depth) for each lane in order.
	 */
	
	UE_DEPRECATED(5.6, "Use GetDefaultTimeline().EnumerateLanes() or EnumerateTimelinesByCategory() to access timelines instead.")
	virtual void EnumerateLanes(TFunctionRef<void(const FRegionLane& /*Lane*/, const int32 /*Depth*/)> Callback) const = 0;

	/**
	 * @return a monotonically increasing counter that is updated each time new data is added to the provider.
	 * This can be used to detect when to update any (UI-)state dependent on the provider during analysis.
	 */
	virtual uint64 GetUpdateCounter() const = 0;
};

/**
 * The interface to a provider that can consume mutations of region events from a session.
 */
class TRACESERVICES_API IEditableRegionProvider
	: public IEditableProvider
{
public:
	virtual ~IEditableRegionProvider() override = default;

	/**
	 * Append a new begin event of a region from the trace session.
	 * Prefer opening/closing regions with an Id, since string names are not unique.
	 *
	 * @param Name		The string name of the region.
	 * @param Time		The time in seconds of the begin event of this region.
	 * @param Category	The category associated with this region, use nullptr if no category has been set
	 */
	virtual void AppendRegionBegin(const TCHAR* Name, double Time, const TCHAR* Category = nullptr) = 0;

	/**
	 * Append a new begin event of a region from the trace session.
	 * Id will be used to uniquely identify the new region.
	 *
	 * @param Name		The string name of the region.
	 * @param Id		The Id of the region. Used to uniquely identify regions with the same name.
	 * @param Time		The time in seconds of the begin event of this region.
	 * @param Category	The category associated with this region, use nullptr if no category has been set
	 */
	virtual void AppendRegionBeginWithId(const TCHAR* Name, uint64 Id, double Time, const TCHAR* Category = nullptr) = 0;

	/**
	 * Append a new end event of a region from the trace session.
	 * Prefer opening/closing regions with an Id, since string names are not unique.
	 *
	 * @param Name		The string name of the region.
	 * @param Time		The time in seconds of the end event of this region.
	 */
	virtual void AppendRegionEnd(const TCHAR* Name, double Time) = 0;

	/**
	 * Append a new end event of a region from the trace session.
	 * The region is identified by Id.
	 * @param Id		The Id of the region.
	 * @param Time		The time in seconds of the end event of this region.
	 */
	virtual void AppendRegionEndWithId(const uint64 Id, double Time) = 0;

	/**
	 * Called from the analyzer once all events have been processed.
	 * Allows post-processing and error reporting for regions that were never closed.
	 */
	virtual void OnAnalysisSessionEnded() = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

TRACESERVICES_API FName GetRegionProviderName();
TRACESERVICES_API const IRegionProvider& ReadRegionProvider(const IAnalysisSession& Session);
TRACESERVICES_API IEditableRegionProvider& EditRegionProvider(IAnalysisSession& Session);

} // namespace TraceServices
