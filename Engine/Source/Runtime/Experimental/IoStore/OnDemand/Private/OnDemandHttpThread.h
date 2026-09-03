// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Mutex.h"
#include "Containers/StringFwd.h"
#include "Delegates/Delegate.h"
#include "HAL/Event.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/Runnable.h"
#include "IO/IoAllocators.h"
#include "IO/IoBuffer.h"
#include "IO/IoOffsetLength.h"
#include "Misc/SingleThreadRunnable.h"
#include "Templates/UniquePtr.h"
#include "ThreadSafeIntrusiveQueue.h"

class FRunnableThread;

namespace UE::IoStore
{

class FMultiEndpointHttpClient;
struct FHttpRequest;
struct FOnDemandChunkInfo;

/** Describes the system making the request */
enum class EHttpRequestType : uint8
{
	Streaming = 0,	/** IAS */
	Installed,		/** IAD */

	NUM_SOURCES
};

class FOnDemandHttpThread final
	: public FRunnable
	, public FSingleThreadRunnable
{
public:
	using FCompletionCallback = TUniqueFunction<void(uint32 /*StatusCode*/ , FStringView/*ErrorReason*/, FIoBuffer&& /*Data*/)>;
	using FRequestHandle = void*;
	DECLARE_MULTICAST_DELEGATE(FOnTickIdle);

	FOnDemandHttpThread();
	virtual ~FOnDemandHttpThread();

	static FOnDemandHttpThread& Get();
	
	/**
	 * Issue a http request to read from a chunk.
	 * 
	 * @param ChunkInfo		Info about the chunk to be read
	 * @param ReadRange		The range from within the chunk to read. A default FIoOffsetAndLength will read the entire chunk.
	 * @param Priority		The priority of the request. Currently maps to EIoDispatcherPriority but may be expanded on in the future.
	 * @CompletionCallback	A callback that will be invoked when the http request has completed. This will be invoked on the
	 *						thread that the http requests are being processed on and will block new requests being queued so
	 *						care should be taken to do as little work in the callback as possible.
	 * @return				A handle representing the request which can be used to modify it while the request is still in
	 *						flight. The handle will no longer be valid after the CompletionCallback has been invoked.
	 */
	FRequestHandle IssueRequest(const FOnDemandChunkInfo& ChunkInfo, const FIoOffsetAndLength& ReadRange, int32 Priority, FCompletionCallback&& CompletionCallback, EHttpRequestType Type);
	
	/**
	 * Issue a http request to read from a chunk.
	 *
	 * @param ChunkInfo		Info about the chunk to be read
	 * @param ReadRange		The range from within the chunk to read. A default FIoOffsetAndLength will read the entire chunk.
	 * @param Priority		The priority of the request. Currently maps to EIoDispatcherPriority but may be expanded on in the future.
	 * @CompletionCallback	A callback that will be invoked when the http request has completed. This will be invoked on the
	 *						thread that the http requests are being processed on and will block new requests being queued so
	 *						care should be taken to do as little work in the callback as possible.
	 * @return				A handle representing the request which can be used to modify it while the request is still in
	 *						flight. The handle will no longer be valid after the CompletionCallback has been invoked.
	 */
	FRequestHandle IssueRequest(FOnDemandChunkInfo&& ChunkInfo, const FIoOffsetAndLength& ReadRange, int32 Priority, FCompletionCallback&& CompletionCallback, EHttpRequestType Type);

	void ReprioritizeRequest(FRequestHandle Request, int32 NewPriority);
	void CancelRequest(FRequestHandle Request);

	FOnTickIdle& OnTickIdle()
	{
		return OnTickIdleDelegate;
	}

	virtual class FSingleThreadRunnable* GetSingleThreadInterface() override { return this; }

private:

	virtual bool Init() override;
	virtual uint32 Run() override;
	virtual void Stop() override;
	virtual void Exit() override;
	virtual void Tick() override;

	void TickRequests();
	void DrainHttpRequests();

	bool TryCreateHttpClient();
	void UpdateThreadPriorityIfNeeded();

	FHttpRequest* AllocateRequest(FOnDemandChunkInfo&& ChunkInfo);

	void DestroyRequest(FHttpRequest* Request);

private:
	TUniquePtr<FRunnableThread> Thread;

	FEventRef TickThreadEvent;

	FOnTickIdle OnTickIdleDelegate;
	TUniquePtr<FMultiEndpointHttpClient> HttpClient;

	TThreadSafeIntrusiveQueue<FHttpRequest> HttpRequests;

	EThreadPriority ThreadPriority = EThreadPriority::TPri_Normal;

	std::atomic_bool bStopRequested = false;
	std::atomic_bool bHttpEnabled = true;

	UE::FMutex AllocatorMutex;
	TSingleThreadedSlabAllocator<FHttpRequest, 32> RequestAllocator;
};

} //namespace UE::IoStore
