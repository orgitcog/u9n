// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "UObject/RemoteObjectTypes.h"
#include "UObject/Class.h"
#include "Async/Async.h"

DECLARE_LOG_CATEGORY_EXTERN(LogRemoteExec, Display, All);

struct FRemoteTransactionId
{
	FRemoteTransactionId() = default;
	explicit constexpr FRemoteTransactionId(uint32 InId)
		: Id(InId)
	{
	}

	static FRemoteTransactionId Invalid()
	{
		return FRemoteTransactionId();
	}

	bool operator==(const FRemoteTransactionId& Rhs) const { return Id == Rhs.Id; }
	bool operator!=(const FRemoteTransactionId& Rhs) const { return Id != Rhs.Id; }

	uint32 GetIdNumber() const
	{
		return Id;
	}
	bool IsValid() const
	{
		return Id != 0;
	}
	FString ToString() const
	{
		return FString::FromInt(Id);
	}
	void operator = (FRemoteTransactionId Other)
	{
		Id = Other.Id;
	}

	friend COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FRemoteTransactionId& Id);

private:
	uint32 Id = 0;
};

struct FRemoteWorkPriority
{
	// the data is packed in a uint64 to allow passing by value in a register
	// [63:32] : FRemoteServerId RootServerId
	// [31:24] : uint8 WorkDepth
	// [23:0]  : FRemoteTransactionId RootWorkTransactionId
	//
	// this allows comparison of priority to be a single uint64 compare-less-than
	// 
	// the WorkDepth is stored subtracted from 0xFF so that less-than means higher priority
	//   0xFF is the root depth
	//   0xFE is one deeper
	//   
	uint64 PackedData = 0;
	static_assert(sizeof(FRemoteServerId) == 4);
	static_assert(sizeof(FRemoteTransactionId) == 4);

	FRemoteServerId GetRootServerId() const { return FRemoteServerId(static_cast<uint32>(PackedData >> 32)); }
	uint32 GetWorkDepth() const { return (PackedData >> 24) & 0xFFu; }
	FRemoteTransactionId GetRootWorkTransactionId() const { return FRemoteTransactionId(PackedData & 0xFFFFFFu); }

	bool IsValid() const { return GetRootServerId().IsValid(); }

	COREUOBJECT_API static FRemoteWorkPriority CreateRootWorkPriority(FRemoteServerId ServerId, FRemoteTransactionId TransactionId);

	// creates a copy of this priority with the WorkDepth decreased by 1
	COREUOBJECT_API FRemoteWorkPriority CreateDependentWorkPriority() const;

	COREUOBJECT_API FString ToString() const;
};

COREUOBJECT_API FArchive& operator<<(FArchive& Ar, FRemoteWorkPriority& Priority);
COREUOBJECT_API bool IsHigherPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs);
COREUOBJECT_API bool IsEqualPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs);
COREUOBJECT_API bool IsHigherOrEqualPriority(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs);
COREUOBJECT_API bool operator==(FRemoteWorkPriority Lhs, FRemoteWorkPriority Rhs);

class FRemoteSubsystemBase
{
protected:
	TArray<TTuple<FRemoteTransactionId, void*>> Requests;

public:

	FRemoteSubsystemBase() = default;
	virtual ~FRemoteSubsystemBase() = default;

	int32 GetRequestCount() const { return Requests.Num(); }

	// implemented in FRemoteSubsystem<>
	virtual void* CreateRequest(FRemoteTransactionId RequestId, FRemoteWorkPriority Priority) = 0;
	virtual void DestroyRequest(FRemoteTransactionId RequestId) = 0;
	virtual void SetActiveRequest(FRemoteTransactionId RequestId) = 0;
	virtual void ClearActiveRequest() = 0;

	// for subsystem implementor to override:
	virtual const TCHAR* NameForDebug() = 0;
	virtual void BeginRequest() = 0;
	virtual void TickSubsystem() = 0;
	virtual void TickRequest() = 0;
	virtual bool AreDependenciesSatisfied() const = 0;
	virtual void BeginMultiServerCommit(TArray<FRemoteServerId>& OutMultiServerCommitRemoteServers) = 0;
	virtual void ExecuteMultiServerCommit() = 0;
	virtual void AbortMultiServerCommit() = 0;
	virtual void CommitMultiServerCommit() = 0;

	virtual void EndRequest(bool bTransactionCommitted) = 0;
};

template<typename RequestType>
class FRemoteSubsystem : public FRemoteSubsystemBase
{
public:
	RequestType* ActiveRequest = nullptr;

	RequestType* GetRequestByIndex(int32 RequestIndex) const
	{
		const TTuple<FRemoteTransactionId, void*> RequestData = Requests[RequestIndex];
		return static_cast<RequestType*>(RequestData.Get<1>());
	}

	RequestType* TryGetRequest(FRemoteTransactionId RequestId) const
	{
		for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); RequestIndex++)
		{
			const TTuple<FRemoteTransactionId, void*>& RequestData = Requests[RequestIndex];
			if (RequestData.Get<0>() == RequestId)
				return static_cast<RequestType*>(RequestData.Get<1>());
		}

		return nullptr;
	}

	int32 TryGetRequestIndex(FRemoteTransactionId RequestId) const
	{
		int32 Result = INDEX_NONE;

		for (int32 RequestIndex = 0; RequestIndex < Requests.Num(); RequestIndex++)
		{
			const TTuple<FRemoteTransactionId, void*>& RequestData = Requests[RequestIndex];
			if (RequestData.Get<0>() == RequestId)
			{
				Result = RequestIndex;
				break;
			}
		}

		return Result;
	}

	void* CreateRequest(FRemoteTransactionId RequestId, FRemoteWorkPriority Priority) final
	{
		RequestType* Request = new RequestType{};
		Request->RequestId = RequestId;
		Request->Priority = Priority;
		Requests.Add(TTuple<FRemoteTransactionId, void*>(RequestId, Request));
		return Request;
	}

	void DestroyRequest(FRemoteTransactionId RequestId) final
	{
		int32 RequestIndex = TryGetRequestIndex(RequestId);
		check(RequestIndex != INDEX_NONE);
		RequestType* Request = GetRequestByIndex(RequestIndex);
		Requests.RemoveAt(RequestIndex, EAllowShrinking::No);
		delete Request;
	}

	void SetActiveRequest(FRemoteTransactionId RequestId) final
	{
		ActiveRequest = TryGetRequest(RequestId);
		check(ActiveRequest);
	}

	void ClearActiveRequest() final
	{
		ActiveRequest = nullptr;
	}
};

namespace UE::RemoteExecutor
{
	/** Delegate that pumps the network and executes RPC handlers */
	extern COREUOBJECT_API TDelegate<void()> TickNetworkDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, FRemoteWorkPriority, const TArray<FRemoteServerId>&)> BeginMultiServerCommitDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>&)> ReadyMultiServerCommitDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>&)> AbandonMultiServerCommitDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, const TArray<FRemoteServerId>&)> EndMultiServerCommitDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, FRemoteServerId)> ReadyRemoteMultiServerCommitDelegate;
	extern COREUOBJECT_API TDelegate<void(FRemoteTransactionId, FRemoteServerId)> AbortRemoteMultiServerCommitDelegate;

	/** Fetch the next deferred RPC request **/
	extern COREUOBJECT_API TDelegate<TOptional<TTuple<FName, FRemoteWorkPriority, bool, TFunction<void(void)>>>(void)> FetchNextDeferredRPCDelegate;

	/** Delegate executed when a ExecuteTransactional starts an AutoRTFM transaction */
	extern COREUOBJECT_API TMulticastDelegate<void(FRemoteTransactionId, FName)> OnTransactionStartingDelegate;

	/** Delegate executed when a ExecuteTransactional observes a completed AutoRTFM transaction */
	extern COREUOBJECT_API TMulticastDelegate<void(FRemoteTransactionId)> OnTransactionCompletedDelegate;

	/** Delegate executed when a ExecuteTransactional observes an aborted AutoRTFM transaction */
	extern COREUOBJECT_API TMulticastDelegate<void(FRemoteTransactionId)> OnTransactionAbortedDelegate;

	COREUOBJECT_API void RegisterRemoteSubsystem(FRemoteSubsystemBase* Subsystem);

	COREUOBJECT_API void ExecuteTransactional(FName WorkName, const TFunctionRef<void(void)>& Work);
	COREUOBJECT_API void ExecuteTransactionalWithExplicitPriority(FName WorkName, FRemoteWorkPriority WorkPriority, const TFunctionRef<void(void)>& Work);
	COREUOBJECT_API void ExecutePendingWork();

	COREUOBJECT_API void AbortTransactionRequiresDependencies(FStringView Description);
	COREUOBJECT_API void RollbackTransactionRequiresDependencies(FStringView Description);
	COREUOBJECT_API void AbortTransactionAndAbandonWork(FStringView Description);

	COREUOBJECT_API void TransactionRequiresMultiServerCommit(FStringView Description);
	
	COREUOBJECT_API void BeginRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId, FRemoteWorkPriority RequestPriority);
	COREUOBJECT_API void ReadyRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId);
	COREUOBJECT_API void AbandonRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId);
	COREUOBJECT_API void EndRemoteMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId);
	COREUOBJECT_API void EnqueueRemoteMultiServerCommitAction(FRemoteServerId ServerId, FRemoteTransactionId RequestId, const TFunction<void()>& Action);

	COREUOBJECT_API void ReadyMultiServerCommitResponse(FRemoteServerId ServerId, FRemoteTransactionId RequestId);
	COREUOBJECT_API void AbortMultiServerCommit(FRemoteServerId ServerId, FRemoteTransactionId RequestId);
	
	/** Establishes a synchronization barrier across all servers */
	COREUOBJECT_API void GlobalServerSync(FName SyncName);
	COREUOBJECT_API void GlobalServerSyncAndReturnBorrowedObjects(FName SyncName);
	COREUOBJECT_API void GlobalServerSyncAndRecomputeObjectOwners(FName SyncName);

	COREUOBJECT_API FRemoteWorkPriority CreateRootWorkPriority();
}
