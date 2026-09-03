// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IO/IoDispatcherBackend.h"
#include "Templates/SharedPointer.h"
#include "Containers/StringFwd.h"

struct FIoContainerHeader; 

namespace UE::IoStore
{

class FOnDemandIoStore;
struct FOnDemandContainer;
struct FOnDemandInstallCacheUsage;

struct FOnDemandInstallCacheStorageUsage
{
	uint64 MaxSize = 0;
	uint64 TotalSize = 0;
	uint64 ReferencedBlockSize = 0;
};

class IOnDemandInstallCache 
	: public IIoDispatcherBackend
{
public:
	virtual										~IOnDemandInstallCache() = default;
	virtual bool								IsChunkCached(const FIoHash& ChunkHash) = 0;
	virtual FIoStatus							PutChunk(FIoBuffer&& Chunk, const FIoHash& ChunkHash) = 0;
	virtual FIoStatus							Purge(TSet<FIoHash>&& ChunksToInstall) = 0;
	virtual FIoStatus							PurgeAllUnreferenced(bool bDefrag, const uint64* BytesToPurge = nullptr) = 0;
	virtual FIoStatus							DefragAll(const uint64* BytesToFree = nullptr) = 0;
	virtual FIoStatus							Verify() = 0;
	virtual FIoStatus							Flush() = 0;
	virtual FOnDemandInstallCacheUsage			GetCacheUsage() = 0;
};

struct FOnDemandInstallCacheConfig
{
	FString RootDirectory;
	uint64	DiskQuota		= 1ull << 30;
	uint64	JournalMaxSize	= 2ull << 20;
	bool	bDropCache		= false;
};

TSharedPtr<IOnDemandInstallCache> MakeOnDemandInstallCache(
	FOnDemandIoStore& IoStore,
	const FOnDemandInstallCacheConfig& Config);

} // namespace UE::IoStore
