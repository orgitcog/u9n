// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/ScopeExit.h"
#include "UObject/UnrealType.h"
#include "Templates/UniquePtr.h"
#include "HAL/CriticalSection.h"
#include "Dataflow/DataflowContextCache.h"
#include "Dataflow/DataflowContextAssetStore.h"
#include "Dataflow/DataflowContextEvaluator.h"
#include "Serialization/Archive.h"

class  UDataflow;
struct FDataflowNode;
struct FDataflowOutput;
struct FDataflowConnection;

#define DATAFLOW_EDITOR_EVALUATION WITH_EDITOR

namespace UE::Dataflow
{
	struct FRenderingParameter 
	{
		FRenderingParameter() {}
		FRenderingParameter(FString InRenderName, FName InTypeName, const TArray<FName>& InOutputs, const FName& InViewMode = FName("3DView"))
			: Name(InRenderName), Type(InTypeName), Outputs(InOutputs), ViewMode(InViewMode) {}
		FRenderingParameter(FString InRenderName, FName InTypeName, TArray<FName>&& InOutputs, const FName& InViewMode = FName("3DView"))
			: Name(InRenderName), Type(InTypeName), Outputs(InOutputs), ViewMode(InViewMode) {}
		
		bool operator==(const FRenderingParameter& Other) const = default;

		FString Name = FString("");
		FName Type = FName("");
		TArray<FName> Outputs;
		FName ViewMode = FName("");
	};

	//////////////////////////////////////////////////////////////////////////////////

	/*
	* Performance data per connection 
	*/
	struct FContextPerfData
	{
		void Reset();
		void Accumulate(const FDataflowConnection* Connection, uint64 TotalTime, uint64 ExternalTime);

		struct FData
		{
			float InclusiveTimeMs = 0;
			float ExclusiveTimeMs = 0;
			FTimestamp LastTimestamp = FTimestamp(0); // timestamp of the owner node when data was last collected
		};
		TMap<FGuid, FData> DataPerOutput;
		bool bEnabled = false;
	};

	//////////////////////////////////////////////////////////////////////////////////

	/*
	* Connection context callstack
	* Used to detected loops and use for error handling
	*/
	struct FContextCallstack
	{
		void Push(const FDataflowConnection* Connection);
		void Pop(const FDataflowConnection* Connection, uint64& OutTotalTime, uint64& OutExternalTime);
		const FDataflowConnection* Top() const;

		int32 Num() const;
		const FDataflowConnection* operator[](int32 Index);
		bool Contains(const FDataflowConnection* Connection) const;

		struct FEntry
		{
			const FDataflowConnection* Connection;
			uint64 StartTime = 0;
			uint64 ExternalTime = 0;

			bool operator==(const FDataflowConnection* OtherConnection) const
			{
				return (OtherConnection == Connection);
			}
		};
		TArray<FEntry> Callstack;
	};

	//////////////////////////////////////////////////////////////////////////////////
	class FScopedOptionalLock final
	{
	public:
		UE_NODISCARD_CTOR explicit FScopedOptionalLock(FCriticalSection* InCriticalSection)
			: CriticalSection(InCriticalSection)
		{
			if (CriticalSection)
			{
				CriticalSection->Lock();
			}
		}

		~FScopedOptionalLock()
		{
			Unlock();
		}

		void Unlock()
		{
			if (CriticalSection)
			{
				CriticalSection->Unlock();
				CriticalSection = nullptr;
			}
		}

	private:
		FScopedOptionalLock() = delete;
		FScopedOptionalLock(const FScopedOptionalLock& InScopeLock) = delete;
		FScopedOptionalLock& operator=(const FScopedOptionalLock& InScopeLock) = delete;

	private:
		FCriticalSection* CriticalSection;
	};

	//////////////////////////////////////////////////////////////////////////////////

	/*
	* Dataflow context base class
	*/
	class FContext: public IContextCacheStore, public IContextAssetStoreInterface
	{
	protected:
		FContext(FContext&&) = delete;
		FContext& operator=(FContext&&) = delete;
		
		FContext(const FContext&) = delete;
		FContext& operator=(const FContext&) = delete;

		FContextCache DataStore;
		TUniquePtr<FCriticalSection> DataLock;

		FContextEvaluator AsyncEvaluator;

	public:
		FContext() : AsyncEvaluator(*this) {}
		virtual ~FContext() = default;

		static FName StaticType() { return FName("FContext"); }

		virtual bool IsA(FName InType) const { return InType==StaticType(); }

		virtual FName GetType() const { return FContext::StaticType(); }

		template<class T>
		const T* AsType() const
		{
			if (IsA(T::StaticType()))
			{
				return static_cast<const T*>(this);
			}
			return nullptr;
		}

		template<class T>
		T* AsType()
		{
			if (IsA(T::StaticType()))
			{
				return static_cast<T*>(this);
			}
			return nullptr;
		}

		bool IsThreaded() const { return DataLock.IsValid(); }
		DATAFLOWCORE_API void SetThreaded(bool bValue);

		DATAFLOWCORE_API bool IsAsyncEvaluating() const;
		DATAFLOWCORE_API void CancelAsyncEvaluation();
		DATAFLOWCORE_API void GetAsyncEvaluationStats(int32& OutNumPendingTasks, int32& OutNumRunningTasks, int32& OutNumCompletedTasks) const;

		DATAFLOWCORE_API virtual void SetDataImpl(FContextCacheKey Key, TUniquePtr<FContextCacheElementBase>&& DataStoreEntry);
		DATAFLOWCORE_API virtual const TUniquePtr<FContextCacheElementBase>* GetDataImpl(FContextCacheKey Key) const;
		DATAFLOWCORE_API virtual bool HasDataImpl(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) const;
		DATAFLOWCORE_API virtual bool IsEmptyImpl() const;
		DATAFLOWCORE_API virtual int32 GetKeys(TSet<FContextCacheKey>& InKeys) const;

		DATAFLOWCORE_API void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output, FOnPostEvaluationFunction PostEvaluationFunction);

		DATAFLOWCORE_API virtual void Evaluate(const FDataflowNode* Node, const FDataflowOutput* Output );
		DATAFLOWCORE_API virtual bool Evaluate(const FDataflowOutput& Connection );

		DATAFLOWCORE_API void ClearAllData();

		template<typename T>
		void SetData(FContextCacheKey InKey, const FProperty* InProperty, T&& InValue, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{
			TUniquePtr<FContextCacheElementBase> DataStoreEntry;
			bool bMadeEntry = false;
			if constexpr (TIsTArray<typename TDecay<T>::Type>::Value)
			{
				typedef typename TDecay<T>::Type ArrayType;
				if constexpr (TIsUObjectPtrElement<typename ArrayType::ElementType>::Value)
				{
					bMadeEntry = true;
					DataStoreEntry = MakeUnique<TContextCacheElementUObjectArray<typename ArrayType::ElementType>>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);
				}
				else if constexpr (TIsReflectedStruct<ArrayType>::Value)
				{
					DataStoreEntry = MakeUnique<FContextCacheElementUStructArray>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);
				}
			}
			if (!bMadeEntry)
			{
				if constexpr (TIsUObjectPtrElement<T>::Value)
				{
					DataStoreEntry = MakeUnique<TContextCacheElementUObject<T>>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);
				}
				else if constexpr (TIsReflectedStruct<T>::Value)
				{
					DataStoreEntry = MakeUnique<FContextCacheElementUStruct>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);
				}
				else
				{
					DataStoreEntry = MakeUnique<TContextCacheElement<T>>(InNodeGuid, InProperty, Forward<T>(InValue), InNodeHash, InTimestamp);
				}
			}

			SetDataImpl(InKey, MoveTemp(DataStoreEntry));
		}

		void SetDataFromStructView(FContextCacheKey InKey, const FProperty* InProperty, const FConstStructView& StructView, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{
			TUniquePtr<FContextCacheElementBase> DataStoreEntry = MakeUnique<FContextCacheElementUStruct>(InNodeGuid, InProperty, StructView, InNodeHash, InTimestamp);
			SetDataImpl(InKey, MoveTemp(DataStoreEntry));
		}

		void SetDataFromStructArrayView(FContextCacheKey InKey, const FProperty* InProperty, const FConstStructArrayView& StructArrayView, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{
			TUniquePtr<FContextCacheElementBase> DataStoreEntry = MakeUnique<FContextCacheElementUStructArray>(InNodeGuid, InProperty, StructArrayView, InNodeHash, InTimestamp);
			SetDataImpl(InKey, MoveTemp(DataStoreEntry));
		}

		void SetDataReference(FContextCacheKey Key, const FProperty* Property, FContextCacheKey ReferenceKey, const FTimestamp& InTimestamp)
		{
			// find the reference key to get 
			if (const TUniquePtr<FContextCacheElementBase>* CacheElement = GetDataImpl(ReferenceKey))
			{
				TUniquePtr<FContextCacheElementBase> CacheReferenceElement = (*CacheElement)->CreateReference(ReferenceKey);
				CacheReferenceElement->SetTimestamp(InTimestamp);

				SetDataImpl(Key, MoveTemp(CacheReferenceElement));
			}
			else
			{
				ensure(false); // could not find the original cache element 
			}
		}

		// this is useful when there's a need to have to have cache entry but  the type is not known and there no connected output
		// ( like reroute nodes with unconnected input for example ) 
		// in that case posting an invalid reference, will allow the evaluatino to go through and the node reading it will get a default value instead
		void SetNullData(FContextCacheKey InKey, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp)
		{
			TUniquePtr<FContextCacheElementNull> CacheNullElement = MakeUnique<FContextCacheElementNull>(InNodeGuid, InProperty, InNodeHash, InTimestamp);
			SetDataImpl(InKey, MoveTemp(CacheNullElement));
		}

		DATAFLOWCORE_API int32 GetArraySizeFromData(const FContextCacheKey InKey) const;

		DATAFLOWCORE_API void SetArrayElementFromData(const FContextCacheKey InArrayKey, int32 Index, const FContextCacheKey InElementKey, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp);

		DATAFLOWCORE_API bool CopyDataToAnotherContext(const FContextCacheKey InSourceKey, FContext& TargetContext, const FContextCacheKey InTargetKey, const FProperty* InProperty, const FGuid& InNodeGuid, uint32 InNodeHash, const FTimestamp& InTimestamp) const;

		template<class T>
		const T& GetData(FContextCacheKey Key, const FProperty* InProperty, const T& Default = T()) const
		{
			if (const TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetTypedData<T>(*this, InProperty, Default);
			}
			return Default;
		}

		const void* GetUntypedData(FContextCacheKey Key, const FProperty* InProperty) const
		{
			if (const TUniquePtr<FContextCacheElementBase>* Cache = GetDataImpl(Key))
			{
				return (*Cache)->GetUntypedData(*this, InProperty);
			}
			return nullptr;
		}

		bool HasData(FContextCacheKey Key, FTimestamp InTimestamp = FTimestamp::Invalid) const
		{
			FContextCacheKey IntKey = (FContextCacheKey)Key;
			return HasDataImpl(Key, InTimestamp);
		}

		bool IsEmpty() const
		{
			return IsEmptyImpl();
		}

		virtual void Serialize(FArchive& Ar)
		{
			FTimestamp Timestamp = FTimestamp::Invalid;
			Ar << Timestamp;
			Ar << DataStore;
		}

		// Begin - IContextCacheStore
		virtual const TUniquePtr<FContextCacheElementBase>* FindCacheElement(FContextCacheKey InKey) const override
		{
			return GetDataImpl(InKey);
		}
		virtual bool HasCacheElement(FContextCacheKey InKey, FTimestamp InTimestamp = FTimestamp::Invalid) const override
		{
			return HasDataImpl(InKey, InTimestamp);
		}
		// End - IContextCacheStore

		// Begin - IContextAssetStoreInterface
		DATAFLOWCORE_API virtual UObject* AddAsset(const FString& AssetPath, const UClass* AssetClass) override;
		DATAFLOWCORE_API virtual UObject* CommitAsset(const FString& AssetPath) override;
		DATAFLOWCORE_API virtual void ClearAssets() override;
		// End - IContextAssetStoreInterface

		DATAFLOWCORE_API FTimestamp GetTimestamp(FContextCacheKey Key) const;

		DATAFLOWCORE_API void PushToCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API void PopFromCallstack(const FDataflowConnection* Connection);
		DATAFLOWCORE_API bool IsInCallstack(const FDataflowConnection* Connection) const;

		DATAFLOWCORE_API FContextPerfData::FData GetPerfDataForNode(const FDataflowNode& Node) const;
		DATAFLOWCORE_API void ResetPerfDataForNode(const FDataflowNode& Node);
		DATAFLOWCORE_API void EnablePerfData(bool bEnable);
		DATAFLOWCORE_API bool IsPerfDataEnabled() const;

		DATAFLOWCORE_API bool IsCacheEntryAfterTimestamp(FContextCacheKey InKey, const FTimestamp InTimestamp) const;

		DATAFLOWCORE_API void Info(const FString& InInfo, const FDataflowNode* InNode = nullptr, const FDataflowOutput* InOutput = nullptr);
		DATAFLOWCORE_API int32 GetNumInfo() const;
		DATAFLOWCORE_API void Warning(const FString& InWarning, const FDataflowNode* InNode = nullptr, const FDataflowOutput* InOutput = nullptr);
		DATAFLOWCORE_API int32 GetNumWarnings() const;
		DATAFLOWCORE_API void Error(const FString& InError, const FDataflowNode* InNode = nullptr, const FDataflowOutput* InOutput = nullptr);
		DATAFLOWCORE_API int32 GetNumErrors() const;

		bool NodeHasWarning(const FDataflowNode* InNode) { return NodesWithWarning.Contains(InNode); };
		bool NodeHasError(const FDataflowNode* InNode) { return NodesWithError.Contains(InNode); };
		bool NodeFailed(const FDataflowNode* InNode) { return NodesFailed.Contains(InNode); };

		DATAFLOWCORE_API void ClearNodesData();
		DATAFLOWCORE_API void ClearNodeData(const FDataflowNode* InNode);

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodeBeginEvaluateMulticast, const FDataflowNode* Node, const FDataflowOutput* Output);
		FOnNodeBeginEvaluateMulticast OnNodeBeginEvaluateMulticast;

		DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNodeFinishEvaluateMulticast, const FDataflowNode* Node, const FDataflowOutput* Output);
		FOnNodeFinishEvaluateMulticast OnNodeFinishEvaluateMulticast;

		DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContextHasInfoMulticast, const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Info);
		FOnContextHasInfoMulticast OnContextHasInfo;

		DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContextHasWarningMulticast, const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Info);
		FOnContextHasWarningMulticast OnContextHasWarning;

		DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnContextHasErrorMulticast, const FDataflowNode* Node, const FDataflowOutput* Output, const FString& Info);
		FOnContextHasErrorMulticast OnContextHasError;

	protected:
		DATAFLOWCORE_API void BeginContextEvaluation(const FDataflowNode* Node, const FDataflowOutput* Output);

	private:

		void CheckIntrinsicInputs(const FDataflowOutput& Connection);

#if DATAFLOW_EDITOR_EVALUATION
		FContextCallstack Callstack;
		FContextPerfData PerfData;
#endif
		TSet<const FDataflowNode*> NodesWithInfo;
		TSet<const FDataflowNode*> NodesWithWarning;
		TSet<const FDataflowNode*> NodesWithError;
		TSet<const FDataflowNode*> NodesFailed;

		/** used to store the dependent asset created during the evaluation of the graph */
		FContextAssetStore AssetStore;
	};

	struct FContextScopedCallstack
	{
	public:
		DATAFLOWCORE_API FContextScopedCallstack(FContext& InContext, const FDataflowConnection* InConnection);
		DATAFLOWCORE_API ~FContextScopedCallstack();

		bool IsLoopDetected() const { return bLoopDetected; }

	private:
		bool bLoopDetected;
		FContext& Context;
		const FDataflowConnection* Connection;
	};

#define DATAFLOW_CONTEXT_INTERNAL(PARENTTYPE, TYPENAME)														\
	typedef PARENTTYPE Super;																				\
	static FName StaticType() { return FName(#TYPENAME); }													\
	virtual bool IsA(FName InType) const override { return InType==StaticType() || Super::IsA(InType); }	\
	virtual FName GetType() const override { return StaticType(); }

	class FContextSingle : public FContext
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextSingle);
	};
	
	class FContextThreaded : public FContext
	{
	public:
		DATAFLOW_CONTEXT_INTERNAL(FContext, FContextThreaded);

		FContextThreaded()
			: FContext()
		{
			SetThreaded(true);
		}
	};
}


