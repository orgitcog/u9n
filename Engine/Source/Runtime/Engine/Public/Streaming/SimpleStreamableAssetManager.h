// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "Misc/SpinLock.h"
#include "Engine/StreamableRenderAsset.h"

class FPrimitiveSceneProxy;
class IPrimitiveComponent;

struct FStreamingRenderAssetPrimitiveInfo;
struct FBoundsViewInfo;
struct FStreamingViewInfo;
struct FStreamingViewInfoExtra;
struct FRenderAssetStreamingSettings;
struct FBounds4;

#define SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER !(UE_BUILD_SHIPPING)

template<typename T, uint32 N>
struct TLocklessStaticStorage
{
public:
	constexpr static uint32 MaxElements = N;
	uint32 Reserve() { return ReservedIndex.fetch_add(1, std::memory_order_acq_rel); }
		
	void Store(T&& In, uint32 ItemReservedIndex)
	{
		// can assert if Relased is equal ReservedIndex and lower than N
		Storage[ItemReservedIndex] = MoveTemp(In);
	}

	void Release(uint32 ToRelease)
	{
		uint32 Expected = ToRelease;
		while (!ReleasedIndex.compare_exchange_weak(Expected, ToRelease + 1, std::memory_order_acq_rel)) 
		{ 
			Expected = ToRelease; 
		}
	}

	void WaitForWrites() const
	{
		while(ReleasedIndex.load(std::memory_order_acquire) < std::min(MaxElements, ReservedIndex.load(std::memory_order_acquire))){}
	}

	TArrayView<T> GetData() { return TArrayView<T>(Storage, ReleasedIndex.load()); };
	
private:
	std::atomic_uint32_t ReservedIndex = 0;
	std::atomic_uint32_t ReleasedIndex = 0;
	T Storage[MaxElements]{};
};

template<typename T>
class TLocklessGrowingStorage
{
public:
	using FStorageShard = TLocklessStaticStorage<T, 512>;

	void Push(T&& In)
	{
		while (true)
		{
			FStorageShard* Storage = Shard.load(std::memory_order_acquire);
			if (Storage)
			{
				const uint32 StorageIndex = Storage->Reserve();
				if (StorageIndex < FStorageShard::MaxElements)
				{
					Storage->Store(MoveTemp(In), StorageIndex);
					Storage->Release(StorageIndex);
					return;
				}
			}
			uint32 Expected = 0;
			if (StorageAllocatorGuard.compare_exchange_strong(Expected, 1, std::memory_order_acq_rel))
			{
				FStorageShard* NewStorage = new FStorageShard();
				if (Shard.compare_exchange_strong(Storage, NewStorage, std::memory_order_acq_rel))
				{
					if (Storage)
					{
						LockedStorage.Add(Storage);
					}
				}
				else
				{
					delete NewStorage;
				}
				StorageAllocatorGuard.store(0, std::memory_order_release);
			}
		}
	}
	TArray<FStorageShard*> ExtractShards()
	{
		TArray<FStorageShard*> Shards;
		uint32 Expected = 0;
		while (!StorageAllocatorGuard.compare_exchange_strong(Expected, 1, std::memory_order_acq_rel)){ Expected =  0; }
		Shards = MoveTemp(LockedStorage);
		FStorageShard* GrabbedStorage = Shard.exchange(nullptr);
		StorageAllocatorGuard.store(0, std::memory_order_release);
		if (GrabbedStorage)
		{
			GrabbedStorage->WaitForWrites();
			Shards.Add(GrabbedStorage);
		}
			
		return Shards;
	}

	~TLocklessGrowingStorage()
	{
		TArray<FStorageShard*> Shards = ExtractShards();
		for (FStorageShard* ShardElement : Shards)
		{
			delete ShardElement;
		}
	}
private:
		
	std::atomic_uint32_t StorageAllocatorGuard = 0;
	std::atomic<FStorageShard*> Shard = nullptr;
	TArray<FStorageShard*> LockedStorage;
};

class FSimpleStreamableAssetManager
{
public:
	struct FScopedLock
	{
		FScopedLock(FCriticalSection* InCriticalSection, bool bInShouldLock)
			: CriticalSection(InCriticalSection)
			, bShouldLock(bInShouldLock)
		{
			if (bShouldLock)
			{
				CriticalSection->Lock();
			}
		}
		~FScopedLock()
		{
			if (bShouldLock)
			{
				CriticalSection->Unlock();
			}
		}
	private:
		FCriticalSection* CriticalSection = nullptr;
		bool bShouldLock = false;
	};
	
	struct FUnregister
	{
		TSharedPtr<int32, ESPMode::ThreadSafe> ObjectRegistrationIndex{};
		
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
		union
		{
			UPTRINT ObjectKey = 0;
			const FPrimitiveSceneProxy* SceneProxy_ForDebug;
		};
#endif
		
		FUnregister() = default;
		FUnregister(const FUnregister&) = default;
		FUnregister& operator=(const FUnregister&) = default;
		FUnregister(FUnregister&&) = default;
		FUnregister& operator=(FUnregister&&) = default;

		template<typename TObject>
		FUnregister(const TObject* Object)
			: ObjectRegistrationIndex(Object->SimpleStreamableAssetManagerIndex)
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
			, ObjectKey(reinterpret_cast<UPTRINT>(Object))
#endif
		{}
	};

	struct FUpdate
	{
		TSharedPtr<int32, ESPMode::ThreadSafe> ObjectRegistrationIndex{};
		FBoxSphereBounds ObjectBounds{};
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
		union
		{
			UPTRINT ObjectKey = 0;
			const FPrimitiveSceneProxy* SceneProxy_ForDebug;
		};
#endif
		float MinDistance = 0.0f;
		float MaxDistance = FLT_MAX;
		float LastRenderedTime = -1000.0f;
		uint8 bForceMipStreaming : 1 = false;

		FUpdate() = default;
		FUpdate(const FUpdate&) = default;
		FUpdate& operator=(const FUpdate&) = default;
		FUpdate(FUpdate&&) = default;
		FUpdate& operator=(FUpdate&&) = default;

		FUpdate(
			const void* InObject,
			const TSharedPtr<int32, ESPMode::ThreadSafe>& InObjectRegistrationIndex,
			const FBoxSphereBounds& InBounds,
			const float InMinDistance, const float InMaxDistance, const float InLastRenderedTime, bool InForceMipStreaming)
				: ObjectRegistrationIndex(InObjectRegistrationIndex)
				, ObjectBounds(InBounds)
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
				, ObjectKey(reinterpret_cast<UPTRINT>(InObject))
#endif
				, MinDistance(InMinDistance)
				, MaxDistance(InMaxDistance)
				, LastRenderedTime(InLastRenderedTime)
				, bForceMipStreaming(InForceMipStreaming)
		{}
	};
		
	
	struct FRegister : public FUpdate
	{
		TArray<FStreamingRenderAssetPrimitiveInfo> Assets{};

		FRegister() = default;
		FRegister(const FRegister&) = default;
		FRegister& operator=(const FRegister&) = default;
		FRegister(FRegister&&) = default;
		FRegister& operator=(FRegister&&) = default;
		
		template<typename TObject, typename TPrimitive>
		FRegister(const TObject* InObject, const TPrimitive* InPrimitive);
	};

	struct FAssetBoundElement
	{
		int32 ObjectRegistrationIndex = INDEX_NONE;
		float TexelFactor = 0.0f;
		uint32 bForceLOD : 1 = 0;
	};

	struct FAssetRecord
	{
		int32 AssetRegistrationIndex = INDEX_NONE;
		int32 AssetElementIndex = INDEX_NONE; 
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
		const UStreamableRenderAsset* StreamableRenderAsset_ForDebug = nullptr;
#endif
		friend bool operator==(const FAssetRecord& A, const FAssetRecord& B) { return A.AssetRegistrationIndex == B.AssetRegistrationIndex; }
	};

	struct FObjectBoundsRecord
	{
		int32 BoundsIndex = INDEX_NONE;
	};
private:
	template <typename T>
	struct TSimpleSparseArray
	{
		int32 Add(T&& InElement)
		{
			if (UsedElementsCount == FreeElementIndexHint)
			{
				UsedElements.Add(false, GSimpleStreamableAssetManagerSparseArrayGrowSize);
				Elements.AddDefaulted(GSimpleStreamableAssetManagerSparseArrayGrowSize);
			}
			const int32 Index = UsedElements.FindAndSetFirstZeroBit(FreeElementIndexHint);
			check(Index != INDEX_NONE);
			FreeElementIndexHint = Index + 1;
			++UsedElementsCount;
			Elements[Index] = MoveTemp(InElement);
			return Index;
		}
		
		void Reset(int32 Index)
		{
			if (UsedElements.Num() > Index && UsedElements[Index])
			{
				UsedElements[Index] = false;
				FreeElementIndexHint = FMath::Min(FreeElementIndexHint, Index);
				Elements[Index] = T{};
				--UsedElementsCount;
			}
		}
		
		void Empty()
		{
			FreeElementIndexHint = 0;
			UsedElementsCount = 0;
			UsedElements.Empty();
			Elements.Empty();
		}

		int32 Num() const { return UsedElementsCount; }

		SIZE_T GetAllocatedSize(void) const
		{
			return sizeof(TSimpleSparseArray)
				+ UsedElements.GetAllocatedSize()
				+ Elements.GetAllocatedSize();
		}

		TConstArrayView<T> GetSparseView() const
		{
			const int32 Max = UsedElements.FindLast(true);
			return TConstArrayView<T>{ Elements.GetData(), Max + 1};
		}
		private:
		int32 FreeElementIndexHint = 0;
		int32 UsedElementsCount = 0;
		TBitArray<> UsedElements;
		TArray<T> Elements;
	};

	static FAutoConsoleVariableRef CVarUseSimpleStreamableAssetManager;
	ENGINE_API static int32 GUseSimpleStreamableAssetManager;

	static FAutoConsoleVariableRef CVarUseSimpleStreamableAssetManagerSparseArrayGrowSize;
	ENGINE_API static int32 GSimpleStreamableAssetManagerSparseArrayGrowSize;

	static FAutoConsoleVariableRef CVarUseSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration;
	ENGINE_API static int32 GSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration;
	
	static FSimpleStreamableAssetManager* Instance;
	FCriticalSection CriticalSection;

	TLocklessGrowingStorage<FAssetRecord> RemovedAssetsRecords;
	TLocklessGrowingStorage<FRegister> RegisterRecords;
	TLocklessGrowingStorage<FUnregister> UnregisterRecords;
	TLocklessGrowingStorage<FUpdate> UpdateRecords;

	// ** Variables to manage Object registration ** //
	int32 RegisteredObjectCount = 0;
	int32 MaxObjects = 0;

	int32 FreeObjectIndexHint = 0;
	TBitArray<> ObjectUsedIndices;
	TArray<TArray<FAssetRecord>> ObjectRegistrationIndexToAssetProperty;
	TArray<FBounds4> ObjectBounds4;

	void RegisterRecord(FRegister& Record);
	void UpdateRecord(FUpdate& Record);
	void UnregisterRecord(FUnregister& Record);

	// ** Variables to manage Bounds registration ** //
	void SetBounds(int32 BoundsIndex, const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, float LastRenderTime, const FVector4& RangeOrigin, float MinDistanceSq, float MinRangeSq, float MaxRangeSq);
	FBoxSphereBounds GetBounds(int32 BoundsIndex) const;

	// ** Variables to manage Assets registration ** //
	int32 FreeAssetIndexHint = 0;
	int32 UsedAssetIndices = 0;
	TBitArray<> AssetUsedIndices;
	TArray<TSimpleSparseArray<FAssetBoundElement>> AssetIndexToBounds4Index;
	void AddRenderAssetElements(const TArrayView<FStreamingRenderAssetPrimitiveInfo>& RenderAssetInstanceInfos, int32 ObjectRegistrationIndex, bool bForceMipStreaming);
	void RemoveRenderAssetElements(int32 ObjectRegistrationIndex);

	// ** Background task data** //
	TArray<FBoundsViewInfo> BoundsViewInfos;
	
	void GetRenderAssetScreenSize_Impl(
		EStreamableRenderAssetType AssetType,
		const UStreamableRenderAsset* InAsset,
		float& MaxSize,
		float& MaxSize_VisibleOnly,
		int32& MaxNumForcedLODs,
		const float MaxAssetSize,
		const int32 MaxAllowedMip,
		const TCHAR* LogPrefix) const;

	void UpdateBoundSizes_Impl(
		const TArray<FStreamingViewInfo>& ViewInfos,
		const TArray<FStreamingViewInfoExtra, TInlineAllocator<4>>& ViewInfoExtras,
		float LastUpdateTime,
		const FRenderAssetStreamingSettings& Settings);

	static void GetDistanceAndRange(
		const FUpdate& Record,
		float& MinDistanceSq, float& MinRangeSq, float& MaxRangeSq);

	void UpdateTask_Async();
	
	void GetAssetReferenceBounds_Impl(const UStreamableRenderAsset* Asset, TArray<FBox>& AssetBoxes) const;
	uint32 GetAllocatedSize_Impl() const;
public:
	static FCriticalSection* GetCriticalSection() { return Instance ? &Instance->CriticalSection : nullptr; }

	ENGINE_API static void Register(FRegister&& Record);
	ENGINE_API static void Unregister(FUnregister&& Record);
	ENGINE_API static void Update(FUpdate&& Record);

	static bool IsEnabled() { return GUseSimpleStreamableAssetManager != 0; }
	static void Init();
	static void Shutdown();
	static void Process();
	
	static void UnregisterAsset(UStreamableRenderAsset* InAsset);

	static uint32 GetAllocatedSize();

	static void UpdateBoundSizes(
		const TArray<FStreamingViewInfo>& ViewInfos,
		const TArray<FStreamingViewInfoExtra, TInlineAllocator<4>>& ViewInfoExtras,
		float LastUpdateTime,
		const FRenderAssetStreamingSettings& Settings);

	static void GetRenderAssetScreenSize(
		EStreamableRenderAssetType AssetType,
		const UStreamableRenderAsset* InAsset,
		float& MaxSize,
		float& MaxSize_VisibleOnly,
		int32& MaxNumForcedLODs,
		const float MaxAssetSize,
		const int32 MaxAllowedMip,
		const TCHAR* LogPrefix);

	static void GetAssetReferenceBounds(const UStreamableRenderAsset* Asset, TArray<FBox>& AssetBoxes);
};


template<typename TObject, typename TPrimitive>
FSimpleStreamableAssetManager::FRegister::FRegister(const TObject* InObject, const TPrimitive* InPrimitive)
	: FUpdate(
		InObject,
		InObject->SimpleStreamableAssetManagerIndex,
		InPrimitive->Bounds,
		InObject->GetMinDrawDistance(),
		InObject->GetMaxDrawDistance(),
		InObject->GetPrimitiveSceneInfo()->LastRenderTime,
		InObject->IsForceMipStreaming())
{
	if (InObject->IsSupportingStreamableRenderAssetsGathering())
	{
		InObject->GetStreamableRenderAssetInfo(InPrimitive->Bounds, Assets);
	}
	else if (const IPrimitiveComponent* Interface = InPrimitive->GetPrimitiveComponentInterface())
	{
		Interface->GetStreamableRenderAssetInfo(Assets);
	}
	else
	{
		// Primitive has to support at least one of the two ways of providing assets
		checkNoEntry(); 
	}
}

inline int32 GetTypeHash(const FSimpleStreamableAssetManager::FAssetRecord& Object) { return Object.AssetRegistrationIndex; }
