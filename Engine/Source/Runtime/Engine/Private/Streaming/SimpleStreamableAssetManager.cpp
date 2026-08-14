// Copyright Epic Games, Inc. All Rights Reserved.

#include "Streaming/SimpleStreamableAssetManager.h"

#include "Engine/StreamableRenderAsset.h"
#include "Engine/TextureStreamingTypes.h"
#include "Engine/Texture.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "PrimitiveSceneProxy.h"

#include "ProfilingDebugging/CountersTrace.h"

#include "Streaming/TextureInstanceView.h"
#include "Streaming/TextureInstanceView.inl"

TRACE_DECLARE_INT_COUNTER(RegisteredObjects, TEXT("StreamableAssets/SimpleStreamableAssetManager/RegisteredObjects"));
TRACE_DECLARE_INT_COUNTER(RegisteredAssets, TEXT("StreamableAssets/SimpleStreamableAssetManager/RegisteredAssets"));

TRACE_DECLARE_INT_COUNTER(AddedObjects, TEXT("StreamableAssets/SimpleStreamableAssetManager/AddedObjects"));
TRACE_DECLARE_INT_COUNTER(RemovedObjects, TEXT("StreamableAssets/SimpleStreamableAssetManager/RemovedObjects"));
TRACE_DECLARE_INT_COUNTER(UpdatedObjects, TEXT("StreamableAssets/SimpleStreamableAssetManager/UpdateObjects"));



FSimpleStreamableAssetManager* FSimpleStreamableAssetManager::Instance = nullptr;
int32 FSimpleStreamableAssetManager::GUseSimpleStreamableAssetManager = 0;
int32 FSimpleStreamableAssetManager::GSimpleStreamableAssetManagerSparseArrayGrowSize = 64;
int32 FSimpleStreamableAssetManager::GSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration = 1;

FAutoConsoleVariableRef FSimpleStreamableAssetManager::CVarUseSimpleStreamableAssetManager(
TEXT("s.StreamableAssets.UseSimpleStreamableAssetManager"),
FSimpleStreamableAssetManager::GUseSimpleStreamableAssetManager,
TEXT("Whether to use FSimpleStreamableAssetManager.\n")
TEXT("If 0 (current default), StreamingAsset works with LevelStreamingManager by collecting UPrimitiveComponents mostly operating on GT.\n")
TEXT("If 1, The FSimpleStreamableAssetManager is Enabled and works by integrating with SceneProxy that is responsible for feeding the system."),
ECVF_SetByGameSetting | ECVF_ReadOnly
);

FAutoConsoleVariableRef FSimpleStreamableAssetManager::CVarUseSimpleStreamableAssetManagerSparseArrayGrowSize(
TEXT("s.StreamableAssets.SimpleStreamableAssetManager.SparseArrayGrowSize"),
FSimpleStreamableAssetManager::GSimpleStreamableAssetManagerSparseArrayGrowSize,
TEXT("The growth size of SparseArray used for tracking objects pointing specific assets"),
ECVF_SetByGameSetting
);
FAutoConsoleVariableRef FSimpleStreamableAssetManager::CVarUseSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration(
TEXT("s.StreamableAssets.SimpleStreamableAssetManager.SortAssetsOnRegistration"),
FSimpleStreamableAssetManager::GSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration,
TEXT("If true when object will be added, referenced assets will be sorted \n")
     TEXT("we will make sure we register asset only once with highest Texel Factor \n")
     TEXT("It will be beneficial only if multiple materials use same texture"),
ECVF_SetByGameSetting
);


void FSimpleStreamableAssetManager::Init()
{
	check(Instance == nullptr);
	Instance = new FSimpleStreamableAssetManager();
}

void FSimpleStreamableAssetManager::Shutdown()
{
	check(Instance != nullptr);
	delete Instance;
}

void FSimpleStreamableAssetManager::Process()
{
	if (IsEnabled())
	{
		check(Instance != nullptr);
		FScopedLock ScopedLock(GetCriticalSection(), IsEnabled());
		Instance->UpdateTask_Async();
	}
}

void FSimpleStreamableAssetManager::UnregisterAsset(UStreamableRenderAsset* InAsset)
{
	if (IsEnabled())
	{
		check(Instance != nullptr);
		Instance->RemovedAssetsRecords.Push(FAssetRecord{
			.AssetRegistrationIndex = InAsset->SimpleStreamableAssetManagerIndex
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
			, .StreamableRenderAsset_ForDebug = InAsset
#endif
		});
		InAsset->SimpleStreamableAssetManagerIndex = INDEX_NONE;
	}
}

uint32 FSimpleStreamableAssetManager::GetAllocatedSize()
{
	constexpr uint32 StaticSize = sizeof(Instance) + sizeof(GUseSimpleStreamableAssetManager) + sizeof(CVarUseSimpleStreamableAssetManager);
	if (IsEnabled())
	{
		check(Instance != nullptr);
		FScopedLock SimpleScopedLock(GetCriticalSection(), IsEnabled());
		return StaticSize + Instance->GetAllocatedSize_Impl();
	}
	return StaticSize;
}

void FSimpleStreamableAssetManager::GetAssetReferenceBounds(const UStreamableRenderAsset* Asset, TArray<FBox>& AssetBoxes)
{
	check(Instance != nullptr);
	Instance->GetAssetReferenceBounds_Impl(Asset, AssetBoxes);
}

void FSimpleStreamableAssetManager::UpdateBoundSizes(
	const TArray<FStreamingViewInfo>& ViewInfos,
	const TArray<FStreamingViewInfoExtra, TInlineAllocator<4>>& ViewInfoExtras,
	float LastUpdateTime,
	const FRenderAssetStreamingSettings& Settings)
{
	check(Instance != nullptr);
	Instance->UpdateBoundSizes_Impl(ViewInfos, ViewInfoExtras, LastUpdateTime, Settings);
}

void FSimpleStreamableAssetManager::GetRenderAssetScreenSize(
		EStreamableRenderAssetType AssetType,
		const UStreamableRenderAsset* InAsset,
		float& MaxSize,
		float& MaxSize_VisibleOnly,
		int32& MaxNumForcedLODs,
		const float MaxAssetSize,
		const int32 MaxAllowedMip,
		const TCHAR* LogPrefix)
{
	check(Instance != nullptr);
	Instance->GetRenderAssetScreenSize_Impl(AssetType, InAsset, MaxSize, MaxSize_VisibleOnly, MaxNumForcedLODs, MaxAssetSize, MaxAllowedMip, LogPrefix);
}

void FSimpleStreamableAssetManager::GetDistanceAndRange(const FUpdate& Record, float& MinDistanceSq, float& MinRangeSq, float& MaxRangeSq)
{
	// RenderAssetInstanceBounds is Object Bounds since there is no parent support
	MinDistanceSq = FMath::Max<float>(0.0f, Record.MinDistance - Record.ObjectBounds.SphereRadius);
	MinDistanceSq *= MinDistanceSq;
	MinRangeSq = FMath::Max<float>(0, Record.MinDistance);
	MinRangeSq *= MinRangeSq;
	MaxRangeSq = FMath::Max<float>(0, Record.MaxDistance);
	MaxRangeSq *= MaxRangeSq;
}

void FSimpleStreamableAssetManager::Register(FRegister&& Record)
{
	check(Instance != nullptr);
	Instance->RegisterRecords.Push(MoveTemp(Record));
}
void FSimpleStreamableAssetManager::Unregister(FUnregister&& Record)
{
	check(Instance != nullptr);
	Instance->UnregisterRecords.Push(MoveTemp(Record));
}
void FSimpleStreamableAssetManager::Update(FUpdate&& Record)
{
	check(Instance != nullptr);
	Instance->UpdateRecords.Push(MoveTemp(Record));
}

void FSimpleStreamableAssetManager::GetAssetReferenceBounds_Impl(const UStreamableRenderAsset* Asset, TArray<FBox>& AssetBoxes) const
{
	const int32 AssetIndex = Asset->SimpleStreamableAssetManagerIndex;

	if (AssetIndex != INDEX_NONE)
	{
		const TSimpleSparseArray<FAssetBoundElement>& AssetElements = AssetIndexToBounds4Index[AssetIndex];

		AssetBoxes.Reserve(AssetElements.Num());
		
		for (const FAssetBoundElement& Element : AssetElements.GetSparseView())
		{
			const int32 ObjectRegistrationIndex = Element.ObjectRegistrationIndex;
			if (ObjectRegistrationIndex != INDEX_NONE)
			{
				const FBoxSphereBounds Bounds = GetBounds(ObjectRegistrationIndex);
				AssetBoxes.Add(Bounds.GetBox());
			}
		}
	}
}

uint32 FSimpleStreamableAssetManager::GetAllocatedSize_Impl() const
{
	uint32 AllocatedSize = sizeof(FSimpleStreamableAssetManager);

	AllocatedSize += ObjectUsedIndices.GetAllocatedSize();
	for (const TArray<FAssetRecord>& Assets : ObjectRegistrationIndexToAssetProperty)
	{
		AllocatedSize += Assets.GetAllocatedSize();
	}
	
	AllocatedSize += ObjectBounds4.GetAllocatedSize();

	AllocatedSize += AssetUsedIndices.GetAllocatedSize();

	for (const TSimpleSparseArray<FAssetBoundElement>& AssetBoundElements : AssetIndexToBounds4Index)
	{
		AllocatedSize += AssetBoundElements.GetAllocatedSize();
	}

	AllocatedSize += BoundsViewInfos.GetAllocatedSize();

	return AllocatedSize;
}

void FSimpleStreamableAssetManager::UpdateTask_Async()
{
	SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_UpdateTask_Async, FColor::Silver);
		
	const int32 LastRegisteredObjectCount = RegisteredObjectCount;
	const int32 LastRegisteredAssetsCount = UsedAssetIndices;
	

	TArray<TLocklessGrowingStorage<FUpdate>::FStorageShard*> Pending_UpdateRecords{};
	TArray<TLocklessGrowingStorage<FRegister>::FStorageShard*> Pending_RegisterRecords{};
	TArray<TLocklessGrowingStorage<FUnregister>::FStorageShard*> Pending_UnregisterRecords{};
	TArray<TLocklessGrowingStorage<FAssetRecord>::FStorageShard*> Pending_RemovedAssetRecords{};

	{
		/** It is important to extract Shards in this order
		 * Since the process in not blocking we want to ensure we have
		 * request first  add for updated primitives
		 * becouse those two can come from different/competeing threads 
		 */
		SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_MoveData, FColor::Silver);
		Pending_UpdateRecords = UpdateRecords.ExtractShards();
		Pending_RegisterRecords = RegisterRecords.ExtractShards();
		Pending_UnregisterRecords = UnregisterRecords.ExtractShards();
		Pending_RemovedAssetRecords = RemovedAssetsRecords.ExtractShards();
	}
	
	/** Assign a indice to each proxy **/
	{
		SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_AssignObjectIndex, FColor::Silver);
		
		for (TLocklessGrowingStorage<FRegister>::FStorageShard* Shard : Pending_RegisterRecords)
		{
			TArrayView<FRegister> View = Shard->GetData();
			const int32 NeedToReserve = RegisteredObjectCount + View.Num() - MaxObjects;
			if (NeedToReserve > 0)
			{
				SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_Resize, FColor::Silver);
				ObjectUsedIndices.Add(false, NeedToReserve);
				ObjectRegistrationIndexToAssetProperty.AddZeroed(NeedToReserve);
				MaxObjects += NeedToReserve;

				// We store 4 Object bounds in one entry
				const int32 MaxObject4BoundsNeeded = MaxObjects/4 + int32(MaxObjects % 4 != 0);
				const int32 Object4BoundsNeedToReserve = MaxObject4BoundsNeeded - ObjectBounds4.Num();
				if (Object4BoundsNeedToReserve > 0)
				{
					ObjectBounds4.AddDefaulted(Object4BoundsNeedToReserve);
				}
			}

			// Cheap index assignment for proxy
			for (FRegister& Entry : View)
			{
				// Lets Skip reserving indices if no streamable assets
				if (Entry.Assets.Num() > 0)
				{
					const int32 ObjectIndex = ObjectUsedIndices.FindAndSetFirstZeroBit(FreeObjectIndexHint);
					check(ObjectIndex != INDEX_NONE);
					FreeObjectIndexHint = ObjectIndex + 1;
					++RegisteredObjectCount;
					*Entry.ObjectRegistrationIndex = ObjectIndex;
				}
			}
		}
	}
	
	{
		SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_Process, FColor::Silver);

		{
			SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_Unregister, FColor::Silver);
			for (TLocklessGrowingStorage<FUnregister>::FStorageShard* Shard : Pending_UnregisterRecords)
			{
				TArrayView<FUnregister> View = Shard->GetData();
				for (FUnregister& Record : View)
				{
					UnregisterRecord(Record);
				}
			}
		}
		
		{
			SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_Register, FColor::Silver);
			for (TLocklessGrowingStorage<FRegister>::FStorageShard* Shard : Pending_RegisterRecords)
			{
				TArrayView<FRegister> View = Shard->GetData();
				for (FRegister& Entry : View)
				{
					RegisterRecord(Entry);
				}
			}
		}
		
		{
			
			SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_Update, FColor::Silver);
			for (TLocklessGrowingStorage<FUpdate>::FStorageShard* Shard : Pending_UpdateRecords)
			{
				TArrayView<FUpdate> View = Shard->GetData();
				for (FUpdate& Entry : View)
				{
					UpdateRecord(Entry);
				}
			}
		}

		{
			SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_RemoveAssets, FColor::Silver);
			for (TLocklessGrowingStorage<FAssetRecord>::FStorageShard* Shard : Pending_RemovedAssetRecords)
			{
				TArrayView<FAssetRecord> View = Shard->GetData();
				for (const FAssetRecord& Record : View)
				{
					const int32 AssetIndex = Record.AssetRegistrationIndex;
					if (AssetIndex != INDEX_NONE)
					{
						AssetUsedIndices[AssetIndex] = false;
						--UsedAssetIndices;
						FreeAssetIndexHint = FMath::Min(FreeAssetIndexHint, AssetIndex);
						
						AssetIndexToBounds4Index[AssetIndex].Empty();
					}
				}
			}
		}
	}
	
	if (LastRegisteredObjectCount != RegisteredObjectCount)
	{
		TRACE_COUNTER_SET(RegisteredObjects, RegisteredObjectCount);
	}

	if (LastRegisteredAssetsCount != UsedAssetIndices)
	{
		TRACE_COUNTER_SET(RegisteredAssets, UsedAssetIndices);
	}
	
	{
#if COUNTERSTRACE_ENABLED
		int32 Updates = 0;	
#endif
		for (TLocklessGrowingStorage<FUpdate>::FStorageShard* Shard : Pending_UpdateRecords)
		{
#if COUNTERSTRACE_ENABLED
			Updates += Shard->GetData().Num();
#endif
			delete Shard;
		}
		TRACE_COUNTER_SET(UpdatedObjects, Updates);
		
#if COUNTERSTRACE_ENABLED
		int32 Added = 0;
#endif
		for (TLocklessGrowingStorage<FRegister>::FStorageShard* Shard : Pending_RegisterRecords)
		{
#if COUNTERSTRACE_ENABLED
			Added += Shard->GetData().Num();
#endif
			delete Shard;
		}
		TRACE_COUNTER_SET(AddedObjects, Added);

#if COUNTERSTRACE_ENABLED
		int32 Removed = 0;
#endif
		for (TLocklessGrowingStorage<FUnregister>::FStorageShard* Shard : Pending_UnregisterRecords)
		{
#if COUNTERSTRACE_ENABLED
			Removed += Shard->GetData().Num();
#endif
			delete Shard;
		}
		TRACE_COUNTER_SET(RemovedObjects, Removed);

		for (TLocklessGrowingStorage<FAssetRecord>::FStorageShard* Shard : Pending_RemovedAssetRecords)
		{
			delete Shard;
		}
	}
}

void FSimpleStreamableAssetManager::RegisterRecord(FRegister& Record)
{
	const int32 ObjectIndex = *Record.ObjectRegistrationIndex;

	// In case Add/Remove pair we do not have valid registration index
	if (ObjectIndex == INDEX_NONE)
	{
		return;
	}

	TArray<FStreamingRenderAssetPrimitiveInfo>& Assets = Record.Assets;
	
	//* Remove not needed */
	for (int32 Index = 0; Index < Assets.Num(); Index++)
	{
		const FStreamingRenderAssetPrimitiveInfo& Info = Assets[Index];
		if (!Info.RenderAsset || !Info.RenderAsset->IsStreamable())
		{
			Assets.RemoveAtSwap(Index--, EAllowShrinking::No);
		}
		else
		{
#if DO_CHECK
			ensure(Info.TexelFactor >= 0.f
				|| Info.RenderAsset->IsA<UStaticMesh>()
				|| Info.RenderAsset->IsA<USkeletalMesh>()
				|| (Info.RenderAsset->IsA<UTexture>() && Info.RenderAsset->GetLODGroupForStreaming() == TEXTUREGROUP_Terrain_Heightmap));
#endif

			// Other wise check that everything is setup right. If the component is not yet registered, then the bound data is irrelevant.
			const bool bCanBeStreamedByDistance = Info.CanBeStreamedByDistance(true);

			if (!Record.bForceMipStreaming && !bCanBeStreamedByDistance && Info.TexelFactor >= 0.f)
			{
				Assets.RemoveAtSwap(Index--, EAllowShrinking::No);
			}
		}
	}

	int32 AssetCount = Assets.Num();

	if (GSimpleStreamableAssetManagerEnsureAssetUniqueOnRegistration && AssetCount > 0)
	{
		// Sort by Texture to merge duplicate texture entries.
		// Then sort by TexelFactor
		Assets.Sort([](const FStreamingRenderAssetPrimitiveInfo& Lhs, const FStreamingRenderAssetPrimitiveInfo& Rhs)
		{
				if (Lhs.RenderAsset == Rhs.RenderAsset)
				{
					return Lhs.TexelFactor > Rhs.TexelFactor;
				}
				return Lhs.RenderAsset < Rhs.RenderAsset;
		});

		int32 ProcessedIndex = 0;
		int32 EmplaceIndex = 1;
		for (int32 Index = 1; Index < Assets.Num(); Index++)
		{
			// We found new asset 
			if (Assets[ProcessedIndex].RenderAsset != Assets[Index].RenderAsset)
			{
				
				if (EmplaceIndex != Index) // We need to fill the gap in array
				{
					Assets[EmplaceIndex] = MoveTemp(Assets[Index]);
				}
				++ProcessedIndex;
				++EmplaceIndex;
			}
			// For landscape entries negative TexelFactor values are used we need to ensure we get min while sorted max 
			else if (Assets[ProcessedIndex].TexelFactor < 0 && Assets[ProcessedIndex].RenderAsset == Assets[Index].RenderAsset)
			{
				Assets[ProcessedIndex].TexelFactor = FMath::Min(Assets[ProcessedIndex].TexelFactor, Assets[Index].TexelFactor);
			}
		}

		AssetCount = EmplaceIndex;
	}
		
	if (AssetCount > 0)
	{
		float MinDistanceSq = 0, MinRangeSq = 0, MaxRangeSq = FLT_MAX;
        GetDistanceAndRange(Record, MinDistanceSq, MinRangeSq, MaxRangeSq);
        SetBounds(ObjectIndex, Record.ObjectBounds, PackedRelativeBox_Identity, Record.LastRenderedTime,Record.ObjectBounds.Origin, MinDistanceSq, MinRangeSq, MaxRangeSq);
       	
		const bool bForceLOD = Record.bForceMipStreaming;
		AddRenderAssetElements(TArrayView<FStreamingRenderAssetPrimitiveInfo>(Assets.GetData(), AssetCount), ObjectIndex, bForceLOD);
	}
}

void FSimpleStreamableAssetManager::UpdateRecord(FUpdate& Record)
{
	const int32 ObjectIndex = *Record.ObjectRegistrationIndex;
	if (ObjectIndex != INDEX_NONE) // Filter out updates for not registered proxies
	{
		float MinDistanceSq = 0, MinRangeSq = 0, MaxRangeSq = FLT_MAX;
		GetDistanceAndRange(Record, MinDistanceSq, MinRangeSq, MaxRangeSq);
		SetBounds(ObjectIndex, Record.ObjectBounds, PackedRelativeBox_Identity, Record.LastRenderedTime,Record.ObjectBounds.Origin, MinDistanceSq, MinRangeSq, MaxRangeSq);
	}
}

void FSimpleStreamableAssetManager::UnregisterRecord(FUnregister& Record)
{
	const int32 ObjectIndex = *Record.ObjectRegistrationIndex;
	if (ObjectIndex != INDEX_NONE)
	{
		RemoveRenderAssetElements(ObjectIndex);
		*Record.ObjectRegistrationIndex = INDEX_NONE;
	}
}



void FSimpleStreamableAssetManager::SetBounds(int32 BoundsIndex, const FBoxSphereBounds& Bounds, uint32 PackedRelativeBox, float LastRenderTime, const FVector4& RangeOrigin, float MinDistanceSq, float MinRangeSq, float MaxRangeSq)
{
	const int32 ObjectBounds4Index = BoundsIndex / 4;

	if (ObjectBounds4Index >= ObjectBounds4.Num())
	{
		check(ObjectBounds4Index == ObjectBounds4.Num());
		ObjectBounds4.Add(FBounds4{});
	}
	// We store 4 Objects in one entry
	ObjectBounds4[ObjectBounds4Index].Set(BoundsIndex % 4, Bounds, PackedRelativeBox, LastRenderTime, RangeOrigin, MinDistanceSq, MinRangeSq, MaxRangeSq);
}

FBoxSphereBounds FSimpleStreamableAssetManager::GetBounds(int32 BoundsIndex) const
{
	FBoxSphereBounds Bounds(ForceInitToZero);
	const int32 ObjectBounds4Index = BoundsIndex / 4;
	const int32 ObjectBounds4Offset = BoundsIndex % 4;

	check(BoundsIndex >= 0 && ObjectBounds4Index < ObjectBounds4.Num());

	const FBounds4& TheBounds4 = ObjectBounds4[ObjectBounds4Index];

	Bounds.Origin.X = TheBounds4.OriginX[ObjectBounds4Offset];
	Bounds.Origin.Y = TheBounds4.OriginY[ObjectBounds4Offset];
	Bounds.Origin.Z = TheBounds4.OriginZ[ObjectBounds4Offset];

	Bounds.BoxExtent.X = TheBounds4.ExtentX[ObjectBounds4Offset];
	Bounds.BoxExtent.Y = TheBounds4.ExtentY[ObjectBounds4Offset];
	Bounds.BoxExtent.Z = TheBounds4.ExtentZ[ObjectBounds4Offset];

	Bounds.SphereRadius = Bounds.BoxExtent.Length();

	return Bounds;
}

void FSimpleStreamableAssetManager::AddRenderAssetElements(const TArrayView<FStreamingRenderAssetPrimitiveInfo>& RenderAssetInstanceInfos, int32 ObjectRegistrationIndex, bool bForceMipStreaming)
{
	TSet<FAssetRecord> ObjectAssets;
	ObjectAssets.Reserve(RenderAssetInstanceInfos.Num());
	for (const FStreamingRenderAssetPrimitiveInfo& AssetInfo : RenderAssetInstanceInfos)
	{
		UStreamableRenderAsset* RenderAsset = AssetInfo.RenderAsset;
		if (RenderAsset == nullptr)
		{
			continue;
		}
		
		const int32 AssetIndex = [&]
		{
			if (RenderAsset->SimpleStreamableAssetManagerIndex != INDEX_NONE)
			{
				return RenderAsset->SimpleStreamableAssetManagerIndex;
			}
			else
			{
				if (UsedAssetIndices == AssetUsedIndices.Num())
				{
					AssetUsedIndices.Add(false, 32);
				}
					
				const int32 NewAssetIndex = AssetUsedIndices.FindAndSetFirstZeroBit(FreeAssetIndexHint);
				check(NewAssetIndex != INDEX_NONE);
				FreeAssetIndexHint = NewAssetIndex + 1;
				++UsedAssetIndices;
				RenderAsset->SimpleStreamableAssetManagerIndex = NewAssetIndex;
				
				return NewAssetIndex;
			}
		}();

		check(AssetIndex != INDEX_NONE);

		if (AssetIndex >= AssetIndexToBounds4Index.Num())
		{
			check(AssetIndex == AssetIndexToBounds4Index.Num());
			AssetIndexToBounds4Index.Add({});
		}
		
		const int32 AssetElementIndex = AssetIndexToBounds4Index[AssetIndex].Add(FAssetBoundElement{ .ObjectRegistrationIndex = ObjectRegistrationIndex, .TexelFactor =  AssetInfo.TexelFactor, .bForceLOD = bForceMipStreaming});

		ObjectAssets.Add(FAssetRecord{ .AssetRegistrationIndex = AssetIndex, .AssetElementIndex = AssetElementIndex
#if SIMPLE_STREAMABLE_ASSET_MANAGER_ALLOW_DEBUG_POINTER
			, .StreamableRenderAsset_ForDebug = RenderAsset
#endif
			});			
	}

	ObjectRegistrationIndexToAssetProperty[ObjectRegistrationIndex] = ObjectAssets.Array();
}

void FSimpleStreamableAssetManager::RemoveRenderAssetElements(int32 ObjectRegistrationIndex)
{
	check(ObjectRegistrationIndex != INDEX_NONE)
	--RegisteredObjectCount;
	ObjectUsedIndices[ObjectRegistrationIndex] = false;			
	FreeObjectIndexHint = FMath::Min(FreeObjectIndexHint, ObjectRegistrationIndex);
	
	TArray<FAssetRecord>& ObjectAssets = ObjectRegistrationIndexToAssetProperty[ObjectRegistrationIndex];
	
	for (const FAssetRecord& Asset : ObjectAssets)
	{
		const int32& AssetIndex = Asset.AssetRegistrationIndex;
		const int32& AssetElementIndex = Asset.AssetElementIndex;
		check(AssetIndex != INDEX_NONE);
		check(AssetElementIndex != INDEX_NONE);

		AssetIndexToBounds4Index[AssetIndex].Reset(AssetElementIndex);
	}
	ObjectAssets.Reset();
#if DO_CHECK
	// We store 4 Objects in one Bounds entry, there is no real need for clearing the entry
	// we will clean it only for ease of debugging if checks enabled 
	ObjectBounds4[ObjectRegistrationIndex / 4].Clear(ObjectRegistrationIndex % 4);
#endif
}

void FSimpleStreamableAssetManager::GetRenderAssetScreenSize_Impl(
		EStreamableRenderAssetType AssetType,
		const UStreamableRenderAsset* InAsset,
		float& MaxSize,
		float& MaxSize_VisibleOnly,
		int32& MaxNumForcedLODs,
		const float MaxAssetSize,
		const int32 MaxAllowedMip,
		const TCHAR* LogPrefix) const
{
	if (AssetType != EStreamableRenderAssetType::Texture || MaxSize_VisibleOnly < MaxAssetSize || LogPrefix)
	{
		const int32 AssetIndex = InAsset->SimpleStreamableAssetManagerIndex;
		// Asset might be registered, but it might not be yet used by any proxy
		if (AssetIndex != INDEX_NONE)
		{
			const TSimpleSparseArray<FAssetBoundElement>& AssetBoundElements = AssetIndexToBounds4Index[AssetIndex];
						
			for (const FAssetBoundElement& AssetBoundElement : AssetBoundElements.GetSparseView())	
			{
				const int32 ObjectRegistrationIndex =  AssetBoundElement.ObjectRegistrationIndex; 

				if (ObjectRegistrationIndex != INDEX_NONE)
				{
					const FBoundsViewInfo& BoundsViewInfo = BoundsViewInfos[ObjectRegistrationIndex];
			
					const float TexelFactor = AssetBoundElement.TexelFactor;
					const bool bForcedLODs = AssetBoundElement.bForceLOD;
					FRenderAssetInstanceAsyncView::ProcessElement(AssetType, BoundsViewInfo, AssetType != EStreamableRenderAssetType::Texture ? TexelFactor : TexelFactor * BoundsViewInfo.ComponentScale, bForcedLODs, MaxSize, MaxSize_VisibleOnly, MaxNumForcedLODs);

					if (MaxSize_VisibleOnly >= MaxAssetSize || MaxNumForcedLODs >= MaxAllowedMip)
					{
						return;
					}
				}
			}
		}
	}
}

void FSimpleStreamableAssetManager::UpdateBoundSizes_Impl(
	const TArray<FStreamingViewInfo>& ViewInfos,
	const TArray<FStreamingViewInfoExtra, TInlineAllocator<4>>& ViewInfoExtras,
	float LastUpdateTime,
	const FRenderAssetStreamingSettings& Settings)
{
	SCOPED_NAMED_EVENT(FSimpleStreamableAssetManager_UpdateBounds, FColor::Silver);
	
	constexpr float MaxTexelFactor = 1.0f; 
	float MaxLevelRenderAssetScreenSize = 0.0f;
	FRenderAssetInstanceAsyncView::UpdateBoundSizes(
		ViewInfos,
		ViewInfoExtras,
		LastUpdateTime,
		MaxTexelFactor,
		Settings,
		ObjectBounds4,
		BoundsViewInfos,
		MaxLevelRenderAssetScreenSize
	);
}
