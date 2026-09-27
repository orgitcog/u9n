// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataRegistry.h"

#include "Misc/ScopeRWLock.h"
#include "DataRegistryTypes.h"
#include "GenerationTools.h"
#include "UObject/UObjectGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BoneContainer.h"
#include "AssetRegistry/AssetData.h"

namespace // Private
{

UE::AnimNext::FDataRegistry* GAnimationDataRegistry = nullptr;
constexpr int32 BASIC_TYPE_ALLOC_BLOCK = 1000;
FDelegateHandle PostGarbageCollectHandle;

} // end namespace

namespace UE::AnimNext
{

/*static*/ void FDataRegistry::Init()
{
	if (GAnimationDataRegistry == nullptr)
	{
		GAnimationDataRegistry = new FDataRegistry();

		PostGarbageCollectHandle = FCoreUObjectDelegates::GetPostGarbageCollect().AddStatic(&FDataRegistry::HandlePostGarbageCollect);
	}
}

/*static*/ void FDataRegistry::Destroy()
{
	if (GAnimationDataRegistry != nullptr)
	{
		FCoreUObjectDelegates::GetPostGarbageCollect().Remove(PostGarbageCollectHandle);

		// When the module unloads, we cannot use ScriptStructs and our ParamTypes to free the memory
		// we allocated. To avoid crashing on an invalid access, we clear the type map which means that
		// destructors won't run for allocated data. If they were to free memory, it will leak on app exit.
		{
			FRWScopeLock ScopedDataTypeDefsLock(GAnimationDataRegistry->DataTypeDefsLock, SLT_Write);
			GAnimationDataRegistry->DataTypeDefs.Empty();
		}

		GAnimationDataRegistry->ReleaseReferencePoseData(); // release any registered poses

		check(GAnimationDataRegistry->AllocatedBlocks.Num() == 0); // any other data should have been released at this point
		check(GAnimationDataRegistry->StoredData.Num() == 0);

		delete GAnimationDataRegistry;
		GAnimationDataRegistry = nullptr;
	}
}

FDataRegistry* FDataRegistry::Get()
{
	checkf(GAnimationDataRegistry, TEXT("Animation Data Registry is not instanced. It is only valid to access this while the engine module is loaded."));
	return GAnimationDataRegistry;
}


/*static*/ void FDataRegistry::HandlePostGarbageCollect()
{
	// Compact the registry on GC
	if (GAnimationDataRegistry)
	{
		FRWScopeLock Lock(GAnimationDataRegistry->SkeletalMeshReferencePosesLock, SLT_Write);

		for (auto Iter = GAnimationDataRegistry->SkeletalMeshReferencePoses.CreateIterator(); Iter; ++Iter)
		{
			const TObjectKey<USkeletalMeshComponent>& SkeletalMeshComponentPtr = Iter.Key();
			if (SkeletalMeshComponentPtr.ResolveObjectPtr() == nullptr)
			{
				Iter.RemoveCurrent();
			}
		}
	}
}

FDataHandle FDataRegistry::RegisterReferencePose(USkeletalMeshComponent* SkeletalMeshComponent)
{
	FDataHandle Handle = AllocateData<FAnimNextReferencePose>(1);

	FAnimNextReferencePose& AnimationReferencePose = Handle.GetRef<FAnimNextReferencePose>();

	FGenerationTools::GenerateReferencePose(SkeletalMeshComponent, SkeletalMeshComponent->GetSkeletalMeshAsset(), AnimationReferencePose);

	// register even if it fails to generate (register an empty ref pose)

	const TObjectKey<USkeletalMeshComponent> Key(SkeletalMeshComponent);
	const uint32 KeyHash = GetTypeHash(Key);

	{
		FRWScopeLock Lock(SkeletalMeshReferencePosesLock, SLT_Write);

		// First check if another thread grabbed the write lock before us to do the same work
		if (const FReferencePoseData* ExistingPoseData = SkeletalMeshReferencePoses.FindByHash(KeyHash, Key))
		{
			// Another thread beat us to it, discard the work we did and re-use what is cached
			return ExistingPoseData->AnimationDataHandle;
		}

		// Only register the delegate if we are the one to add to the map
		FDelegateHandle DelegateHandle = SkeletalMeshComponent->RegisterOnLODRequiredBonesUpdate_Member(FOnLODRequiredBonesUpdate::CreateRaw(this, &FDataRegistry::OnLODRequiredBonesUpdate));

		SkeletalMeshReferencePoses.AddByHash(KeyHash, Key, FReferencePoseData(Handle, DelegateHandle));
	}

	return Handle;
}

void FDataRegistry::OnLODRequiredBonesUpdate(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODLevel, const TArray<FBoneIndexType>& LODRequiredBones)
{
	// TODO : Check if the LDO bomes are different from the currently calculated ReferencePose data (for now just delete the cached data)
	RemoveReferencePose(SkeletalMeshComponent);
}

FDataHandle FDataRegistry::GetOrGenerateReferencePose(USkeletalMeshComponent* SkeletalMeshComponent)
{
	FDataHandle ReturnHandle;

	{
		FRWScopeLock Lock(SkeletalMeshReferencePosesLock, SLT_ReadOnly);

		if (const FReferencePoseData* ReferencePoseData = SkeletalMeshReferencePoses.Find(SkeletalMeshComponent))
		{
			ReturnHandle = ReferencePoseData->AnimationDataHandle;
		}
	}
	
	if (ReturnHandle.IsValid() == false)
	{
		ReturnHandle = RegisterReferencePose(SkeletalMeshComponent);
	}

	return ReturnHandle;
}

void FDataRegistry::RemoveReferencePose(USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (SkeletalMeshComponent != nullptr)
	{
		FRWScopeLock Lock(SkeletalMeshReferencePosesLock, SLT_Write);

		FReferencePoseData ReferencePoseData;
		if (SkeletalMeshReferencePoses.RemoveAndCopyValue(SkeletalMeshComponent, ReferencePoseData))
		{
			SkeletalMeshComponent->UnregisterOnLODRequiredBonesUpdate_Member(ReferencePoseData.DelegateHandle);
		}
	}
}



void FDataRegistry::RegisterData(const FName& Id, const FDataHandle& AnimationDataHandle)
{
	FRWScopeLock Lock(StoredDataLock, SLT_Write);
	StoredData.Add(Id, AnimationDataHandle);
}

void FDataRegistry::UnregisterData(const FName& Id)
{
	FRWScopeLock Lock(StoredDataLock, SLT_Write);
	StoredData.Remove(Id);
}

FDataHandle FDataRegistry::GetRegisteredData(const FName& Id) const
{
	FDataHandle ReturnHandle;

	{
		FRWScopeLock Lock(StoredDataLock, SLT_ReadOnly);

		if (const FDataHandle* HandlePtr = StoredData.Find(Id))
		{
			ReturnHandle = *HandlePtr;
		}
	}

	return ReturnHandle;
}

void FDataRegistry::FreeAllocatedBlock(Private::FAllocatedBlock* AllocatedBlock)
{
	FRWScopeLock ScopedAllocatedBlocksLock(AllocatedBlocksLock, SLT_Write);

	if (ensure(AllocatedBlock != nullptr && AllocatedBlocks.Find(AllocatedBlock)))
	{
		if (AllocatedBlock->Memory != nullptr)
		{
			void* Memory = AllocatedBlock->Memory;

			{
				FRWScopeLock ScopedDataTypeDefsLock(DataTypeDefsLock, SLT_ReadOnly);
				// We are empty on app exit and thus cannot run destructors (param type is invalid)
				// When this occurs, we leak memory on purpose
				const FDataTypeDef* TypeDef = !DataTypeDefs.IsEmpty() ? DataTypeDefs.Find(AllocatedBlock->Type) : nullptr;
				if (!DataTypeDefs.IsEmpty()
					&& ensure(TypeDef != nullptr))
				{
					TypeDef->DestroyTypeFn((uint8*)AllocatedBlock->Memory, AllocatedBlock->NumElem);
				}
			}

			FMemory::Free(AllocatedBlock->Memory); // TODO : This should come from preallocated chunks, use malloc / free for now
			AllocatedBlock->Memory = nullptr;

			AllocatedBlocks.Remove(AllocatedBlock);
			delete AllocatedBlock; // TODO : avoid memory fragmentation
		}
	}
}

// Remove any ReferencePoses and unregister all the SkeletalMeshComponent delegates (if any still alive)
void FDataRegistry::ReleaseReferencePoseData()
{
	FRWScopeLock Lock(SkeletalMeshReferencePosesLock, SLT_Write);

	for (auto Iter = GAnimationDataRegistry->SkeletalMeshReferencePoses.CreateIterator(); Iter; ++Iter)
	{
		const TObjectKey<USkeletalMeshComponent>& SkeletalMeshComponentPtr = Iter.Key();
		const FReferencePoseData& ReferencePoseData = Iter.Value();

		if (USkeletalMeshComponent* SkeletalMeshComponent = SkeletalMeshComponentPtr.ResolveObjectPtr())
		{
			SkeletalMeshComponent->UnregisterOnLODRequiredBonesUpdate_Member(ReferencePoseData.DelegateHandle);
		}
	}

	SkeletalMeshReferencePoses.Empty();
}

} // end namespace UE::AnimNext
