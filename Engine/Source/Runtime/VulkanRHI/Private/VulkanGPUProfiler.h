// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanGPUProfiler.h: Vulkan Utility definitions.
=============================================================================*/

#pragma once

#include "Containers/Queue.h"
#include "GPUProfiler.h"
#include "VulkanConfiguration.h"

class FVulkanCommandBuffer;
class FVulkanContextCommon;
class FVulkanDevice;

#if (RHI_NEW_GPU_PROFILER == 0)

using FVulkanSyncPoint = FGraphEvent;
using FVulkanSyncPointRef = TRefCountPtr<FVulkanSyncPoint>;

class FVulkanGPUTiming : public FGPUTiming
{
public:
	FVulkanGPUTiming(FVulkanContextCommon* InContext, FVulkanDevice* InDevice)
		: Device(InDevice)
		, Context(InContext)
		, bIsTiming(false)
		, bEndTimestampIssued(false)
	{
	}

	~FVulkanGPUTiming();

	/**
	 * Start a GPU timing measurement.
	 */
	void StartTiming(FVulkanContextCommon* InContext = nullptr);

	/**
	 * End a GPU timing measurement.
	 * The timing for this particular measurement will be resolved at a later time by the GPU.
	 */
	void EndTiming(FVulkanContextCommon* InContext = nullptr);

	/**
	 * Retrieves the most recently resolved timing measurement.
	 * The unit is the same as for FPlatformTime::Cycles(). Returns 0 if there are no resolved measurements.
	 *
	 * @return	Value of the most recently resolved timing, or 0 if no measurements have been resolved by the GPU yet.
	 */
	uint64 GetTiming(bool bGetCurrentResultsAndBlock = false);

	/**
	 * Initializes all Vulkan resources.
	 */
	void Initialize(uint32 PoolSize = 8);

	/**
	 * Releases all Vulkan resources.
	 */
	void Release();

	bool IsComplete() const
	{
		check(bEndTimestampIssued);
		return true;
	}

	bool IsTiming() const
	{
		return bIsTiming;
	}

	static void CalibrateTimers(FVulkanDevice& Device);

private:

	void DiscardOldestQuery();

	/**
	 * Initializes the static variables, if necessary.
	 */
	static void PlatformStaticInitialize(void* UserData);

	FVulkanDevice* Device = nullptr;
	FVulkanContextCommon* Context = nullptr;

	/** Whether we are currently timing the GPU: between StartTiming() and EndTiming(). */
	bool bIsTiming;
	bool bEndTimestampIssued;
	uint64 LastTime = 0;

	struct FPendingQuery
	{
		FVulkanSyncPointRef StartSyncPoint;
		uint64 StartResult = 0;
		FVulkanSyncPointRef EndSyncPoint;
		uint64 EndResult = 0;
	};
	const uint32 MaxPendingQueries = 4;
	uint32 NumPendingQueries = 0;
	TQueue<FPendingQuery*> PendingQueries;
	FPendingQuery* ActiveQuery = nullptr;
};

/** A single perf event node, which tracks information about a appBeginDrawEvent/appEndDrawEvent range. */
class FVulkanEventNode : public FGPUProfilerEventNode
{
public:
	FVulkanEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent, FVulkanContextCommon* InContext, FVulkanDevice* InDevice) :
		FGPUProfilerEventNode(InName, InParent),
		Timing(InContext, InDevice)
	{
		// Initialize Buffered timestamp queries 
		Timing.Initialize();
	}

	virtual ~FVulkanEventNode()
	{
		Timing.Release(); 
	}

	/** 
	 * Returns the time in ms that the GPU spent in this draw event.  
	 * This blocks the CPU if necessary, so can cause hitching.
	 */
	virtual float GetTiming() override final;


	virtual void StartTiming() override final
	{
		Timing.StartTiming();
	}

	virtual void StopTiming() override final
	{
		Timing.EndTiming();
	}

	FVulkanGPUTiming Timing;
};

/** An entire frame of perf event nodes, including ancillary timers. */
class FVulkanEventNodeFrame : public FGPUProfilerEventNodeFrame
{
public:

	FVulkanEventNodeFrame(FVulkanContextCommon* InContext, FVulkanDevice* InDevice)
		: RootEventTiming(InContext, InDevice)
	{
		RootEventTiming.Initialize();
	}

	~FVulkanEventNodeFrame()
	{
		RootEventTiming.Release();
	}

	/** Start this frame of per tracking */
	virtual void StartFrame() override final;

	/** End this frame of per tracking, but do not block yet */
	virtual void EndFrame() override final;

	/** Calculates root timing base frequency (if needed by this RHI) */
	virtual float GetRootTimingResults() override final;

	virtual bool PlatformDisablesVSync() const { return true; }

	/** Timer tracking inclusive time spent in the root nodes. */
	FVulkanGPUTiming RootEventTiming;
};

/** 
 * Encapsulates GPU profiling logic and data. 
 * There's only one global instance of this struct so it should only contain global data, nothing specific to a frame.
 */
struct FVulkanGPUProfiler : public FGPUProfiler
{
	/** GPU hitch profile histories */
	TIndirectArray<FVulkanEventNodeFrame> GPUHitchEventNodeFrames;

	FVulkanGPUProfiler(FVulkanContextCommon* InContext, FVulkanDevice* InDevice);

	virtual ~FVulkanGPUProfiler();

	virtual FGPUProfilerEventNode* CreateEventNode(const TCHAR* InName, FGPUProfilerEventNode* InParent) override final
	{
		FVulkanEventNode* EventNode = new FVulkanEventNode(InName, InParent, CmdContext, Device);
		return EventNode;
	}

	void BeginFrame();

	void EndFrameBeforeSubmit();
	void EndFrame();

	bool bCommandlistSubmitted;
	FVulkanDevice* Device;
	FVulkanContextCommon* CmdContext;

	bool bBeginFrame;
};

#endif // (RHI_NEW_GPU_PROFILER == 0)