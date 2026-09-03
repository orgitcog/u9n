// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/BinarySearch.h"
#include "Async/Async.h"
#include "Async/Mutex.h"
#include "Containers/AnsiString.h"
#include "Containers/BitArray.h"
#include "IO/IoBuffer.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoChunkId.h"
#include "IO/IoHash.h"
#include "IO/IoStoreOnDemandInternals.h"
#include "IasHostGroup.h"
#include "Misc/AES.h"
#include "Misc/EnumClassFlags.h"

#include <atomic>

enum class EForkProcessRole : uint8;

struct FIoContainerHeader;
using FSharedContainerHeader		= TSharedPtr<FIoContainerHeader>;

namespace UE::IoStore
{

class FOnDemandContentInstaller;
class FOnDemandHttpThread;
struct FOnDemandEndpointConfig;
enum class ELogOnDemandCacheUsage : uint8;

using FSharedStreamingBackend		= TSharedPtr<class IOnDemandIoDispatcherBackend>;
using FSharedPackageStoreBackend	= TSharedPtr<class IOnDemandPackageStoreBackend>;
using FSharedInstallCache			= TSharedPtr<class IOnDemandInstallCache>;

///////////////////////////////////////////////////////////////////////////////
enum class EOnDemandContainerFlags : uint8
{
	None					= 0,
	PendingEncryptionKey	= (1 << 0),
	Mounted					= (1 << 1),
	StreamOnDemand			= (1 << 2),
	InstallOnDemand			= (1 << 3),
	Encrypted				= (1 << 4),
	WithSoftReferences		= (1 << 5),
	PendingHostGroup		= (1 << 6),
	Last = PendingHostGroup
};
ENUM_CLASS_FLAGS(EOnDemandContainerFlags);

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandChunkEntry
{
	static const FOnDemandChunkEntry Null;

	FIoHash	Hash;
	uint32	RawSize = 0;
	uint32	EncodedSize = 0;
	uint32	BlockOffset = ~uint32(0);
	uint32	BlockCount = 0;
	uint8	CompressionFormatIndex = 0;
};
static_assert(sizeof(FOnDemandChunkEntry) == 40);

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandTagSet
{
	FString			Tag;
	TArray<uint32>	PackageIndicies;
};

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandChunkEntryReferences
{
	UPTRINT		ContentHandleId = 0;
	TBitArray<>	Indices;
};

using FSharedOnDemandContainer = TSharedPtr<struct FOnDemandContainer, ESPMode::ThreadSafe>;

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandContainerChunkEntryReferences
{
	FSharedOnDemandContainer	Container;
	TBitArray<>					Indices;
};

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandContainer
{
	FString									UniqueName() const;
	inline int32							FindChunkEntryIndex(const FIoChunkId& ChunkId) const;
	inline const FOnDemandChunkEntry*		FindChunkEntry(const FIoChunkId& ChunkId, int32* OutIndex = nullptr) const;
	inline FOnDemandChunkEntryReferences&	FindOrAddChunkEntryReferences(const FOnDemandInternalContentHandle& ContentHandle);
	inline TBitArray<>						GetReferencedChunkEntries() const;
	inline bool								IsReferenced(int32 ChunkEntryIndex) const;
	inline FAnsiString						GetTestUrl();
	bool									HasAnyFlags(EOnDemandContainerFlags Contains) const { return EnumHasAnyFlags(Flags, Contains); }
	bool									HasAllFlags(EOnDemandContainerFlags Contains) const { return EnumHasAllFlags(Flags, Contains); }

	FAES::FAESKey							EncryptionKey;
	FSharedContainerHeader					Header;
	FIASHostGroup							HostGroup;
	FString									EncryptionKeyGuid;
	FString									Name;
	FString									MountId;
	FAnsiString								ChunksDirectory;
	TArray<FName>							CompressionFormats;
	TArray<uint32>							BlockSizes;
	TArray<FIoBlockHash>					BlockHashes;
	TArray<FOnDemandTagSet>					TagSets;
	TUniquePtr<uint8[]>						ChunkEntryData;
	TArrayView<FIoChunkId>					ChunkIds;
	TArrayView<FOnDemandChunkEntry> 		ChunkEntries;
	TArray<FOnDemandChunkEntryReferences>	ChunkEntryReferences;
	FIoContainerId							ContainerId;
	uint32									BlockSize = 0;
	FName									HostGroupName = FOnDemandHostGroup::DefaultName;
	EOnDemandContainerFlags 				Flags = EOnDemandContainerFlags::None;
};

int32 FOnDemandContainer::FindChunkEntryIndex(const FIoChunkId& ChunkId) const
{
	if (const int32 Index = Algo::LowerBound(ChunkIds, ChunkId); Index < ChunkIds.Num())
	{
		if (ChunkIds[Index] == ChunkId)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

const FOnDemandChunkEntry* FOnDemandContainer::FindChunkEntry(const FIoChunkId& ChunkId, int32* OutIndex) const
{
	if (int32 Index = FindChunkEntryIndex(ChunkId); Index != INDEX_NONE)
	{
		if (OutIndex != nullptr)
		{
			*OutIndex = Index;
		}
		return &ChunkEntries[Index];
	}

	return nullptr;
}

FOnDemandChunkEntryReferences& FOnDemandContainer::FindOrAddChunkEntryReferences(const FOnDemandInternalContentHandle& ContentHandle)
{
	const UPTRINT ContentHandleId = ContentHandle.HandleId(); 
	for (FOnDemandChunkEntryReferences& Refs : ChunkEntryReferences)
	{
		if (Refs.ContentHandleId == ContentHandleId)
		{
			return Refs;
		}
	}

	FOnDemandChunkEntryReferences& NewRef = ChunkEntryReferences.AddDefaulted_GetRef();
	NewRef.ContentHandleId = ContentHandleId;
	NewRef.Indices.SetNum(ChunkEntries.Num(), false);
	return NewRef;
}

TBitArray<> FOnDemandContainer::GetReferencedChunkEntries() const
{
	TBitArray<> Indices;
	for (const FOnDemandChunkEntryReferences& Refs : ChunkEntryReferences)
	{
		check(Refs.Indices.Num() == ChunkEntries.Num());
		Indices.CombineWithBitwiseOR(Refs.Indices, EBitwiseOperatorFlags::MaxSize);
	}

	return Indices;
}

bool FOnDemandContainer::IsReferenced(int32 ChunkEntryIndex) const
{
	for (const FOnDemandChunkEntryReferences& Refs : ChunkEntryReferences)
	{
		if (Refs.Indices[ChunkEntryIndex])
		{
			return true;
		}
	}

	return false;
}

FAnsiString FOnDemandContainer::GetTestUrl()
{
	if (ChunkEntries.IsEmpty())
	{
		return FAnsiString();
	}

	TAnsiStringBuilder<41> HashString;
	HashString << ChunkEntries[0].Hash;

	TAnsiStringBuilder<256> Url;
	Url << "/" << ChunksDirectory
		<< "/" << HashString.ToView().Left(2)
		<< "/" << HashString << ANSITEXTVIEW(".iochunk");

	return Url.ToString();
}

///////////////////////////////////////////////////////////////////////////////
struct FOnDemandChunkInfo
{
	FOnDemandChunkInfo()
		: Entry(FOnDemandChunkEntry::Null)
	{ }

	FOnDemandChunkInfo(const FOnDemandChunkInfo& Other)
		: SharedContainer(Other.SharedContainer)
		, Entry(Other.Entry)
	{

	}

	FOnDemandChunkInfo(FOnDemandChunkInfo&& Other)
		: SharedContainer(MoveTemp(Other.SharedContainer))
		, Entry(Other.Entry)
	{

	}


	const									FIoHash& Hash() const { return Entry.Hash; }
	uint32									RawSize() const { return Entry.RawSize; }
	uint32									EncodedSize() const { return Entry.EncodedSize; }
	uint32									BlockSize() const { return SharedContainer->BlockSize; }
	FName									CompressionFormat() const { return SharedContainer->CompressionFormats[Entry.CompressionFormatIndex]; }
	FMemoryView								EncryptionKey() const { return FMemoryView(SharedContainer->EncryptionKey.Key, FAES::FAESKey::KeySize); }
	inline TConstArrayView<uint32>			Blocks() const;
	inline TConstArrayView<FIoBlockHash>	BlockHashes() const;
	FAnsiStringView							ChunksDirectory() const { return SharedContainer->ChunksDirectory; }
	const FOnDemandChunkEntry&				ChunkEntry() const { return Entry; }
	inline const FIASHostGroup&				HostGroup() const;

	bool									IsValid() const { return SharedContainer.IsValid(); }
	operator								bool() const { return IsValid(); }
	
	inline static FOnDemandChunkInfo		Find(FSharedOnDemandContainer Container, const FIoChunkId& ChunkId);

	inline void								GetUrl(FAnsiStringBuilderBase& Url) const;

private:
	friend class FOnDemandIoStore;
	friend class FOnDemandContentInstaller;

	FOnDemandChunkInfo(FSharedOnDemandContainer InContainer, const FOnDemandChunkEntry& InEntry)
		: SharedContainer(InContainer)
		, Entry(InEntry)
	{ }

	FSharedOnDemandContainer	SharedContainer;
	const FOnDemandChunkEntry&	Entry;
};

TConstArrayView<uint32> FOnDemandChunkInfo::Blocks() const
{
	return TConstArrayView<uint32>(SharedContainer->BlockSizes.GetData() + Entry.BlockOffset, Entry.BlockCount);
}

TConstArrayView<FIoBlockHash> FOnDemandChunkInfo::BlockHashes() const
{
	return SharedContainer->BlockHashes.IsEmpty()
		? TConstArrayView<FIoBlockHash>()
		: TConstArrayView<FIoBlockHash>(SharedContainer->BlockHashes.GetData() + Entry.BlockOffset, Entry.BlockCount);
}

const FIASHostGroup& FOnDemandChunkInfo::HostGroup() const
{
	return SharedContainer->HostGroup;
}

FOnDemandChunkInfo FOnDemandChunkInfo::Find(FSharedOnDemandContainer Container, const FIoChunkId& ChunkId)
{
	check(Container.IsValid());
	if (const FOnDemandChunkEntry* Entry = Container->FindChunkEntry(ChunkId))
	{
		return FOnDemandChunkInfo(Container, *Entry);
	}

	return FOnDemandChunkInfo();
}

void FOnDemandChunkInfo::GetUrl(FAnsiStringBuilderBase& Url) const
{
	TAnsiStringBuilder<41> HashString;
	HashString << Hash();

	Url.Reset();
	Url << "/" << ChunksDirectory()
		<< "/" << HashString.ToView().Left(2)
		<< "/" << HashString << ANSITEXTVIEW(".iochunk");
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandIoStore
	: public TSharedFromThis<FOnDemandIoStore, ESPMode::ThreadSafe>
	, public IOnDemandIoStore
{
	struct FMountRequest
	{
		FOnDemandMountArgs		Args;
		FOnDemandMountCompleted	OnCompleted;
		double					DurationInSeconds = 0.0;
	};

	using FSharedMountRequest	= TSharedRef<FMountRequest>;

public:
	UE_NONCOPYABLE(FOnDemandIoStore);

										FOnDemandIoStore();
										~FOnDemandIoStore();

	FIoStatus							InitializeStreamingBackend(const FOnDemandEndpointConfig& EndpointConfig);

	FOnDemandChunkInfo					GetStreamingChunkInfo(const FIoChunkId& ChunkId);
	FOnDemandChunkInfo					GetInstalledChunkInfo(const FIoChunkId& ChunkId, EIoErrorCode& OutErrorCode);

#if !UE_BUILD_SHIPPING
	/** Used to access a number of FIoChunkId that exist in the system when running tests and is not intended for production use */
	TArray<FIoChunkId>					DebugFindStreamingChunkIds(int32 NumToFind);
#endif //!UE_BUILD_SHIPPING
	void								GetReferencedContent(TArray<FSharedOnDemandContainer>& OutContainers, TArray<TBitArray<>>& OutChunkEntryIndices, bool bPackageStore = false);
	void								GetReferencedContentByHandle(TMap<FOnDemandWeakContentHandle, TArray<FOnDemandContainerChunkEntryReferences>>& OutReferencesByHandle) const;

	// IOnDemandIoStore interface
	virtual FIoStatus					Initialize() override;
	virtual FIoStatus					InitializePostHotfix() override;
	virtual void						Mount(FOnDemandMountArgs&& Args, FOnDemandMountCompleted&& OnCompleted) override;
	virtual FOnDemandInstallRequest 	Install(FOnDemandInstallArgs&& Args,
											FOnDemandInstallCompleted&& OnCompleted,
											FOnDemandInstallProgressed&& OnProgress = nullptr) override;
	virtual void						Purge(FOnDemandPurgeArgs&& Args, FOnDemandPurgeCompleted&& OnCompleted) override;
	virtual void						Defrag(FOnDemandDefragArgs&& Args, FOnDemandDefragCompleted&& OnCompleted) override;
	virtual void						Verify(FOnDemandVerifyCacheCompleted&& OnCompleted) override;
	virtual FIoStatus					Unmount(FStringView MountId) override;
	virtual TIoStatusOr<uint64> 		GetInstallSize(const FOnDemandGetInstallSizeArgs& Args) const override;
	virtual FIoStatus					GetInstallSizesByMountId(const FOnDemandGetInstallSizeArgs& Args, TMap<FString, uint64>& OutSizesByMountId) const override;
	virtual FOnDemandCacheUsage			GetCacheUsage(const FOnDemandGetCacheUsageArgs& Args) const override;
	virtual bool						IsOnDemandStreamingEnabled() const override;
	virtual void						SetStreamingOptions(EOnDemandStreamingOptions Options) override;
	virtual void						ReportAnalytics(TArray<FAnalyticsEventAttribute>& OutAnalyticsArray) const override;
	virtual TUniquePtr<IAnalyticsRecording> StartAnalyticsRecording() const override;
	virtual void						CancelInstallRequest(FSharedInternalInstallRequest InstallRequest) override;
	virtual void						UpdateInstallRequestPriority(FSharedInternalInstallRequest InstallRequest, int32 NewPriority) override;
	virtual void						ReleaseContent(FOnDemandInternalContentHandle& ContentHandle) override;
	TArray<FSharedOnDemandContainer>	GetContainers(EOnDemandContainerFlags ContainerFlags = EOnDemandContainerFlags::None) const;

private:
	friend class FOnDemandContentInstaller;

	FIoStatus							GetContainersAndPackagesForInstall(
											FStringView MountId,
											const TArray<FString>& TagSets,
											const TArray<FPackageId>& PackageIds,
											TSet<FSharedOnDemandContainer>& OutContainersForInstallation,
											TSet<FPackageId>& OutPackageIdsToInstall) const;
	FIoStatus							InitializeInstallCache();
	void								TryEnterTickLoop();
	void								TickLoop();
	bool								Tick();
	FIoStatus							TickMountRequest(FMountRequest& MountRequest);
	void								CompleteMountRequest(FMountRequest& MountRequest, FOnDemandMountResult&& MountResult);
	void								OnEncryptionKeyAdded(const FGuid& Id, const FAES::FAESKey& Key);
	static void							CreateContainersFromToc(
											FStringView MountId,
											FStringView TocPath,
											FOnDemandToc& Toc,
											const TArray<FString>& ExistingContainers,
											TArray<FSharedOnDemandContainer>& Out);

	TUniquePtr<FOnDemandHttpThread>			HttpClient;
	TUniquePtr<FOnDemandContentInstaller>	Installer;
	FSharedInstallCache						InstallCache;
	FSharedPackageStoreBackend				PackageStoreBackend;
	FSharedStreamingBackend					StreamingBackend;
	FDelegateHandle							OnMountPakHandle;
	FDelegateHandle							OnServerPostForkHandle;
	TArray<FSharedOnDemandContainer>		Containers;
	TMap<FString, FIoBuffer>				PendingContainerHeaders;
	mutable UE::FMutex						ContainerMutex;

	TArray<FSharedMountRequest>				MountRequests;
	UE::FMutex								RequestMutex;

	EOnDemandStreamingOptions				StreamingOptions = EOnDemandStreamingOptions::Default;
	bool									bTicking = false;
	bool									bTickRequested = false;
	TFuture<void>							TickFuture;
};

} // namespace UE::IoStore

////////////////////////////////////////////////////////////////////////////////
FStringBuilderBase& operator<<(FStringBuilderBase& Sb, UE::IoStore::EOnDemandContainerFlags Flags);
FString LexToString(UE::IoStore::EOnDemandContainerFlags Flags);

