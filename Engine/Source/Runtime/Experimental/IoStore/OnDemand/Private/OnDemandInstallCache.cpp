// Copyright Epic Games, Inc. All Rights Reserved.

#include "OnDemandInstallCache.h"
#include "OnDemandHttpClient.h"
#include "OnDemandIoStore.h"
#include "Statistics.h"

#include "Algo/Accumulate.h"
#include "Algo/Find.h"
#include "Async/Mutex.h"
#include "Async/UniqueLock.h"
#include "Async/AsyncFileHandle.h"
#include "Containers/UnrealString.h"
#include "GenericHash.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "IO/IoContainerHeader.h"
#include "IO/IoChunkId.h"
#include "IO/IoChunkEncoding.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "Misc/ScopeExit.h"
#include "Misc/StringBuilder.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Tasks/Task.h"
#include "ProfilingDebugging/IoStoreTrace.h"

#if WITH_IOSTORE_ONDEMAND_TESTS
#include "Algo/Find.h"
#include "TestHarness.h"
#include "TestMacros/Assertions.h"
#include <catch2/generators/catch_generators.hpp>
#endif

#ifndef UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
#define UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE (0)
#endif

#if UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
#include "Tasks/Pipe.h"
#endif // UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE

#ifndef UE_IAD_DEBUG_CONSOLE_CMDS
#define UE_IAD_DEBUG_CONSOLE_CMDS (1 && !NO_CVARS && !UE_BUILD_SHIPPING)
#endif

namespace UE::IoStore
{

///////////////////////////////////////////////////////////////////////////////
namespace CVars
{
	static bool GIoStoreOnDemandEnableDefrag = true;
	static FAutoConsoleVariableRef CVar_IoStoreOnDemandEnableDefrag(
		TEXT("iostore.EnableDefrag"),
		GIoStoreOnDemandEnableDefrag,
		TEXT("Whether to enable defrag when purging")
	);
}

///////////////////////////////////////////////////////////////////////////////
double ToKiB(uint64 Value)
{
	return double(Value) / 1024.0;
}

///////////////////////////////////////////////////////////////////////////////
double ToMiB(uint64 Value)
{
	return double(Value) / 1024.0 / 1024.0;
}

///////////////////////////////////////////////////////////////////////////////
using FUniqueFileHandle				= TUniquePtr<IFileHandle>;
using FSharedFileHandle				= TSharedPtr<IFileHandle>;
using FSharedFileOpenResult			= TValueOrError<FSharedFileHandle, FFileSystemError>;

using FSharedAsyncFileHandle		= TSharedPtr<IAsyncReadFileHandle>;
using FWeakAsyncFileHandle			= TWeakPtr<IAsyncReadFileHandle>;
using FSharedFileOpenAsyncResult	= TValueOrError<FSharedAsyncFileHandle, FFileSystemError>;

using FCasAddr						= FHash96;

///////////////////////////////////////////////////////////////////////////////
struct FCasBlockId
{
	FCasBlockId() = default;
	explicit FCasBlockId(uint32 InId)
		: Id(InId) { }

	bool IsValid() const { return Id != 0; }

	friend inline bool operator==(FCasBlockId LHS, FCasBlockId RHS)
	{
		return LHS.Id == RHS.Id;
	}

	friend inline uint32 GetTypeHash(FCasBlockId BlockId)
	{
		return GetTypeHash(BlockId.Id);
	}

	friend FArchive& operator<<(FArchive& Ar, FCasBlockId& BlockId)
	{
		Ar << BlockId.Id;
		return Ar;
	}

	static const FCasBlockId Invalid;

	uint32 Id = 0;
};

const FCasBlockId FCasBlockId::Invalid = FCasBlockId();

///////////////////////////////////////////////////////////////////////////////
struct FCasLocation
{
	bool IsValid() const { return BlockId.IsValid() && BlockOffset != MAX_uint32; }

	friend inline bool operator==(FCasLocation LHS, FCasLocation RHS)
	{
		return LHS.BlockId == RHS.BlockId && LHS.BlockOffset == RHS.BlockOffset;
	}

	friend inline uint32 GetTypeHash(FCasLocation Loc)
	{
		return HashCombine(GetTypeHash(Loc.BlockId), GetTypeHash(Loc.BlockOffset));
	}

	friend FArchive& operator<<(FArchive& Ar, FCasLocation& Loc)
	{
		Ar << Loc.BlockId; 
		Ar << Loc.BlockOffset;
		return Ar;
	}

	static const FCasLocation Invalid;

	FCasBlockId	BlockId;
	uint32		BlockOffset = MAX_uint32; 
};

const FCasLocation FCasLocation::Invalid = FCasLocation();

///////////////////////////////////////////////////////////////////////////////
struct FCasBlockInfo
{
	uint64	FileSize = 0;
	int64	LastAccess = 0;
	uint64	RefSize = 0;
};

using FCasBlockInfoMap = TMap<FCasBlockId, FCasBlockInfo>;

///////////////////////////////////////////////////////////////////////////////
struct FCas
{
	static constexpr uint32		DeleteBlockMaxWaitTimeMs = 10000;

	using FLookup				= TMap<FCasAddr, FCasLocation>;
	using FReadHandles			= TMap<FCasBlockId, FWeakAsyncFileHandle>;
	using FLastAccess			= TMap<FCasBlockId, int64>;
	using FBlockIdHandleCounts	= TMap<FCasBlockId, int32>;

	FIoStatus					Initialize(FStringView Directory, bool bDeleteExisting = false);
	FCasLocation				FindChunk(const FIoHash& Hash) const;
	FCasBlockId					CreateBlock();
	FIoStatus					DeleteBlock(FCasBlockId BlockId, TArray<FCasAddr>& OutAddrs);
	FString						GetBlockFilename(FCasBlockId BlockId) const;
	FSharedFileOpenResult		OpenRead(FCasBlockId BlockId);
	FSharedFileOpenAsyncResult	OpenAsyncRead(FCasBlockId BlockId);
	void						OnFileHandleDeleted(FCasBlockId BlockId);
	FUniqueFileHandle			OpenWrite(FCasBlockId BlockId) const;
	void						TrackAccess(FCasBlockId BlockId, int64 UtcTicks);
	void						TrackAccess(FCasBlockId BlockId) { TrackAccess(BlockId, FDateTime::UtcNow().GetTicks()); }
	void						TrackAccessIfNewer(FCasBlockId BlockId, int64 UtcTicks);
	uint64						GetBlockInfo(FCasBlockInfoMap& OutBlockInfo);
	void						Compact();
	FIoStatus					Verify(TArray<FCasAddr>& OutAddrs);

	FStringView				RootDirectory;
	FLookup					Lookup;
	FBlockIdHandleCounts	BlockIds;
	FLastAccess				LastAccess;
	FReadHandles			ReadHandles;
	FEventRef				BlockReadsDoneEvent;
	const uint32			MaxBlockSize = 32 << 20; //TODO: Make configurable	
	const uint32			MinBlockSize = MaxBlockSize >> 1; //TODO: Make configurable	
	mutable UE::FMutex		Mutex;
};

///////////////////////////////////////////////////////////////////////////////
FIoStatus FCas::Initialize(FStringView Directory, bool bDeleteExisting)
{
	RootDirectory = Directory;

	Lookup.Empty();
	BlockIds.Empty();
	LastAccess.Empty();

	TStringBuilder<256> Path;
	FPathViews::Append(Path, RootDirectory, TEXT("blocks"));

	IFileManager& Ifm = IFileManager::Get();

	if (bDeleteExisting)
	{
		bool bRequireExists = false;
		const bool bTree	= true;

		if (Ifm.DeleteDirectory(Path.ToString(), bRequireExists, bTree) == false)
		{
			return FIoStatusBuilder(EIoErrorCode::WriteError)
				<< TEXT("Failed to delete CAS blocks directory '")
				<< Path.ToString()
				<< TEXT("'");
		}
	}

	if (Ifm.DirectoryExists(Path.ToString()) == false)
	{
		const bool bTree = true;
		if (Ifm.MakeDirectory(Path.ToString(), bTree) == false)
		{
			FIoStatus Status = FIoStatusBuilder(EIoErrorCode::WriteError)
				<< TEXT("Failed to create directory '")
				<< Path.ToString()
				<< TEXT("'");
			return Status;
		}
	}

	return EIoErrorCode::Ok;
};

FCasLocation FCas::FindChunk(const FIoHash& Hash) const
{
	const FCasAddr* Addr	= reinterpret_cast<const FCasAddr*>(&Hash);
	const uint32 TypeHash	= GetTypeHash(*Addr);
	{
		UE::TUniqueLock Lock(Mutex);
		if (const FCasLocation* Loc = Lookup.FindByHash(TypeHash, *Addr))
		{
			return *Loc;
		}
	}

	return FCasLocation{};
}

FCasBlockId FCas::CreateBlock()
{
	IPlatformFile&	Ipf = FPlatformFileManager::Get().GetPlatformFile();
	FCasBlockId		Out = FCasBlockId::Invalid;

	UE::TUniqueLock Lock(Mutex);

	for (uint32 Id = 1; Id < MAX_uint32 && !Out.IsValid(); Id++)
	{
		const FCasBlockId BlockId(Id);
		if (BlockIds.Contains(BlockId))
		{
			continue;
		}

		const FString Filename = GetBlockFilename(BlockId);
		if (Ipf.FileExists(*Filename))
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Unused CAS block id %u already exists on disk"), BlockId.Id);
			continue;
		}

		BlockIds.Add(BlockId, 0);
		LastAccess.FindOrAdd(BlockId, FDateTime::UtcNow().GetTicks());
		Out = BlockId;
	}

	return Out;
}

FIoStatus FCas::DeleteBlock(FCasBlockId BlockId, TArray<FCasAddr>& OutAddrs)
{
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	const FString	Filename = GetBlockFilename(BlockId);

	UE::TDynamicUniqueLock Lock(Mutex, UE::FDeferLock());

	// Wait for pending reads to flush before deleting block
	uint32			StartTimeCycles	= FPlatformTime::Cycles();
	const uint32	WaitTimeMs		= 1000;

	for (;;)
	{
		Lock.Lock();

		const int32 RequestCount = BlockIds.FindRef(BlockId);
		if (RequestCount)
		{
			Lock.Unlock();

			if (FPlatformTime::ToMilliseconds(FPlatformTime::Cycles() - StartTimeCycles) > DeleteBlockMaxWaitTimeMs)
			{
				return FIoStatusBuilder(EIoErrorCode::Timeout)
					<< TEXT("Timed out waiting for pending read(s) while deleting CAS block '")
					<< Filename
					<< TEXT("'"); 
			}

			BlockReadsDoneEvent->Wait(WaitTimeMs);
		}
		else
		{
			// Leave mutex locked until it goes out of scope
			break;
		}
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deleting CAS block '%s'"), *Filename);
	if (Ipf.DeleteFile(*Filename) == false)
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to delete CAS block '")
			<< Filename
			<< TEXT("'");
	}

	BlockIds.Remove(BlockId);
	ReadHandles.Remove(BlockId);
	for (auto It = Lookup.CreateIterator(); It; ++It)
	{
		if (It->Value.BlockId == BlockId)
		{
			OutAddrs.Add(It->Key);
			It.RemoveCurrent();
		}
	}

	return FIoStatus::Ok;
}

FString FCas::GetBlockFilename(FCasBlockId BlockId) const
{
	check(BlockId.IsValid());
	const uint32 Id = NETWORK_ORDER32(BlockId.Id);
	FString Hex;
	BytesToHexLower(reinterpret_cast<const uint8*>(&Id), sizeof(int32), Hex);
	TStringBuilder<256> Path;
	FPathViews::Append(Path, RootDirectory, TEXT("blocks"), Hex);
	Path << TEXT(".ucas");

	return FString(Path.ToView());
}

FSharedFileOpenResult FCas::OpenRead(FCasBlockId BlockId)
{
	const FString	Filename = GetBlockFilename(BlockId);
	IPlatformFile&	Ipf = FPlatformFileManager::Get().GetPlatformFile();

	UE::TUniqueLock Lock(Mutex);

	FFileOpenResult Result = Ipf.OpenRead(*Filename, IPlatformFile::EOpenReadFlags::AllowWrite);
	if (Result.HasValue())
	{
		BlockIds.FindOrAdd(BlockId, 0)++;

		FSharedFileHandle NewHandle(
			Result.GetValue().Release(),
			[this, BlockId](IFileHandle* RawHandle)
			{
				delete RawHandle;
				OnFileHandleDeleted(BlockId);
			}
		);

		return MakeValue(MoveTemp(NewHandle));
	}

	return MakeError(Result.StealError());
}

FSharedFileOpenAsyncResult FCas::OpenAsyncRead(FCasBlockId BlockId)
{
	UE::TUniqueLock Lock(Mutex);

	if (FWeakAsyncFileHandle* MaybeHandle = ReadHandles.Find(BlockId))
	{
		if (FSharedAsyncFileHandle Handle = MaybeHandle->Pin(); Handle.IsValid())
		{
			return MakeValue(MoveTemp(Handle));
		}
	}

	IPlatformFile&			Ipf = FPlatformFileManager::Get().GetPlatformFile();
	const FString			Filename = GetBlockFilename(BlockId);
	FFileOpenAsyncResult	HandleResult(Ipf.OpenAsyncRead(*Filename, IPlatformFile::EOpenReadFlags::AllowWrite));

	if (HandleResult.HasValue())
	{
		BlockIds.FindOrAdd(BlockId, 0)++;

		FSharedAsyncFileHandle NewHandle(
			HandleResult.GetValue().Release(),
			[this, BlockId](IAsyncReadFileHandle* RawHandle)
			{
				delete RawHandle;
				OnFileHandleDeleted(BlockId);
			}
		);
		ReadHandles.FindOrAdd(BlockId, NewHandle);
		
		return MakeValue(MoveTemp(NewHandle));
	}

	return MakeError(HandleResult.StealError());
}

void FCas::OnFileHandleDeleted(FCasBlockId BlockId)
{
	UE::TUniqueLock Lock(Mutex);
	const int32 Count = --BlockIds.FindChecked(BlockId);
	check(Count >= 0);
	if (Count == 0)
	{
		BlockReadsDoneEvent->Trigger();
	}
}

FUniqueFileHandle FCas::OpenWrite(FCasBlockId BlockId) const
{
	IPlatformFile&	Ipf = FPlatformFileManager::Get().GetPlatformFile();
	const FString	Filename = GetBlockFilename(BlockId);
	const bool		bAppend = true;
	const bool		bAllowRead = true;

	return FUniqueFileHandle(Ipf.OpenWrite(*Filename, bAppend, bAllowRead));
}

void FCas::TrackAccess(FCasBlockId BlockId, int64 UtcTicks)
{
	check(BlockId.IsValid());
	UE::TUniqueLock Lock(Mutex);
	LastAccess.FindOrAdd(BlockId, UtcTicks);
}

void FCas::TrackAccessIfNewer(FCasBlockId BlockId, int64 UtcTicks)
{
	check(BlockId.IsValid());
	UE::TUniqueLock Lock(Mutex);
	int64& FoundTicks = LastAccess.FindOrAdd(BlockId, FDateTime::MinValue().GetTicks());
	if (FoundTicks < UtcTicks)
	{
		FoundTicks = UtcTicks;
	}
}

uint64 FCas::GetBlockInfo(FCasBlockInfoMap& OutBlockInfo)
{
	TStringBuilder<256> Path;
	FPathViews::Append(Path, RootDirectory, TEXT("blocks"));

	struct FDirectoryVisitor final
		: public IPlatformFile::FDirectoryVisitor
	{
		FDirectoryVisitor(IPlatformFile& PlatformFile, FCasBlockInfoMap& InBlockInfo, FLastAccess&& Access)
			: Ipf(PlatformFile)
			, BlockInfo(InBlockInfo)
			, LastAccess(MoveTemp(Access))
		{ }
		
		virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
		{
			if (bIsDirectory)
			{
				return true;
			}

			const FStringView Filename(FilenameOrDirectory);
			if (FPathViews::GetExtension(Filename) == TEXTVIEW("ucas") == false)
			{
				return true;
			}

			const int64			FileSize = Ipf.FileSize(FilenameOrDirectory);
			const FStringView	IndexHex = FPathViews::GetBaseFilename(Filename);
			const FCasBlockId	BlockId(FParse::HexNumber(WriteToString<128>(IndexHex).ToString()));

			if (BlockId.IsValid() == false || FileSize < 0)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Found invalid CAS block '%s', FileSize=%lld"),
					FilenameOrDirectory, FileSize);
				return true;
			}

			if (BlockInfo.Contains(BlockId))
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Found duplicate CAS block '%s'"), FilenameOrDirectory);
				return true;
			}

			const int64* UtcTicks = LastAccess.Find(BlockId);

			BlockInfo.Add(BlockId, FCasBlockInfo
			{
				.FileSize = uint64(FileSize),
				.LastAccess = UtcTicks != nullptr ? *UtcTicks : 0
			});
			TotalSize += uint64(FileSize);

			return true;
		}

		IPlatformFile&		Ipf;
		FCasBlockInfoMap&	BlockInfo;
		FLastAccess			LastAccess;
		uint64				TotalSize = 0;
	};

	FLastAccess Access;
	{
		TUniqueLock Lock(Mutex);
		Access = LastAccess;
	}
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	FDirectoryVisitor Visitor(Ipf, OutBlockInfo, MoveTemp(Access));
	Ipf.IterateDirectory(Path.ToString(), Visitor);

	return Visitor.TotalSize;
}

void FCas::Compact()
{
	UE::TUniqueLock Lock(Mutex);
	Lookup.Compact();
	BlockIds.Compact();
	ReadHandles.Compact();
	LastAccess.Compact();
}

FIoStatus FCas::Verify(TArray<FCasAddr>& OutAddrs)
{
	FCasBlockInfoMap	BlockInfo;
	const uint64		TotalSize = GetBlockInfo(BlockInfo);
	uint64				TotalVerifiedBytes = 0;
	FIoStatus			Status = FIoStatus::Ok;

	for (auto BlockIt = BlockIds.CreateIterator(); BlockIt; ++BlockIt)
	{
		const FCasBlockId BlockId = BlockIt->Key;
		if (const FCasBlockInfo* Info = BlockInfo.Find(BlockId))
		{
			TotalVerifiedBytes += Info->FileSize;
			continue;
		}

		const FString Filename = GetBlockFilename(BlockId);
		UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Missing CAS block '%s'"), *Filename);

		LastAccess.Remove(BlockId);
		BlockIt.RemoveCurrent();
		Status = EIoErrorCode::NotFound;
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Verified %d CAS blocks of total %.2lf MiB"),
		BlockIds.Num(), ToMiB(TotalVerifiedBytes));

	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	for (const TPair<FCasBlockId, FCasBlockInfo>& Kv : BlockInfo)
	{
		const FCasBlockId BlockId = Kv.Key;
		if (BlockIds.Contains(BlockId))
		{
			continue;
		}

		const FString Filename = GetBlockFilename(BlockId);
		if (Ipf.DeleteFile(*Filename))
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Deleted orphaned CAS block '%s'"), *Filename);
		}
	}

	TSet<FString> MissingReferencedBlocks;
	for (auto It = Lookup.CreateIterator(); It; ++It)
	{
		if (!BlockIds.Contains(It->Value.BlockId))
		{
			MissingReferencedBlocks.Add(GetBlockFilename(It->Value.BlockId));
			
			OutAddrs.Add(It->Key);
			It.RemoveCurrent();

			Status = EIoErrorCode::NotFound;
		}
	}

	for (const FString& Filename : MissingReferencedBlocks)
	{
		UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Lookup references missing CAS block '%s'"), *Filename);
	}

	return Status; 
}

///////////////////////////////////////////////////////////////////////////////
struct FCasJournal
{
	enum class EVersion : uint32
	{
		Invalid	= 0,
		Initial,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	enum class EErrorCode : uint32
	{
		None					= 0,
		Simulated				= 1,
		DefragOutOfDiskSpace	= 2,
		DefragHashMismatch		= 3
	};

	struct FHeader
	{
		static const inline uint8 MagicSequence[16] = {'C', 'A', 'S', 'J', 'O', 'U', 'R', 'N', 'A', 'L', 'H', 'E', 'A', 'D', 'E', 'R'};

		bool		IsValid() const;

		uint8		Magic[16] = {0};
		EVersion	Version = EVersion::Invalid;
		uint8		Pad[12] = {0};
	};
	static_assert(sizeof(FHeader) == 32);

	struct FFooter
	{
		static const inline uint8 MagicSequence[16] = {'C', 'A', 'S', 'J', 'O', 'U', 'R', 'N', 'A', 'L', 'F', 'O', 'O', 'T', 'E', 'R'};

		bool IsValid() const;

		uint8 Magic[16] = {0};
	};
	static_assert(sizeof(FFooter) == 16);

	struct FEntry
	{
		enum class EType : uint8
		{
			None = 0,
			ChunkLocation,
			BlockCreated,
			BlockDeleted,
			BlockAccess,
			CriticalError
		};

		struct FChunkLocation
		{
			EType			Type = EType::ChunkLocation;
			uint8			Pad[3]= {0};
			FCasLocation	CasLocation;
			FCasAddr		CasAddr;
		};
		static_assert(sizeof(FChunkLocation) == 24);

		struct FBlockOperation
		{
			EType		Type = EType::None;
			uint8		Pad[3]= {0};
			FCasBlockId	BlockId;
			int64		UtcTicks = 0;
			uint8		Pad1[8]= {0};
		};
		static_assert(sizeof(FBlockOperation) == 24);

		struct FCriticalError
		{
			EType		Type = EType::CriticalError;
			EErrorCode	ErrorCode = EErrorCode::None;
		};
		static_assert(sizeof(FBlockOperation) == 24);

		union
		{
			FChunkLocation	ChunkLocation;
			FBlockOperation	BlockOperation;
			FCriticalError	CriticalError;
		};

		EType Type() const { return *reinterpret_cast<const EType*>(this); }
	};
	static_assert(sizeof(FEntry) == 24);

	struct FTransaction
	{
		void			ChunkLocation(const FCasLocation& Location, const FCasAddr& Addr);
		void			BlockCreated(FCasBlockId BlockId);
		void			BlockDeleted(FCasBlockId BlockId);
		void			BlockAccess(FCasBlockId BlockId, int64 UtcTicks);
		void			CriticalError(FCasJournal::EErrorCode ErrorCode);

		FString			JournalFile;
		TArray<FEntry>	Entries;
	};

	using FEntryHandler		= TFunction<void(const FEntry&)>;

	static FIoStatus		Replay(const FString& JournalFile, FEntryHandler&& Handler);
	static FIoStatus		Create(const FString& JournalFile);
	static FTransaction		Begin(FString&& JournalFile);
	static FTransaction		Begin(const FString& JournalFile) { return Begin(FString(JournalFile)); }
	static FIoStatus		Commit(FTransaction&& Transaction);
};

///////////////////////////////////////////////////////////////////////////////
const TCHAR* GetErrorText(FCasJournal::EErrorCode ErrorCode)
{
	switch (ErrorCode)
	{
		case FCasJournal::EErrorCode::None:
			return TEXT("None");
		case FCasJournal::EErrorCode::Simulated:
			return TEXT("Simulated error");
		case FCasJournal::EErrorCode::DefragOutOfDiskSpace:
			return TEXT("Defrag failed due to out of disk space");
		case FCasJournal::EErrorCode::DefragHashMismatch:
			return TEXT("Found corrupt chunk while defragging");
	}

	return TEXT("Unknown");
}

///////////////////////////////////////////////////////////////////////////////
bool FCasJournal::FHeader::IsValid() const
{
	if (FMemory::Memcmp(&Magic, &FHeader::MagicSequence, sizeof(FHeader::MagicSequence)) != 0)
	{
		return false;
	}

	if (static_cast<uint32>(Version) > static_cast<uint32>(EVersion::Latest))
	{
		return false;
	}

	return true;
}

bool FCasJournal::FFooter::IsValid() const
{
	return FMemory::Memcmp(Magic, FFooter::MagicSequence, sizeof(FFooter::MagicSequence)) == 0;
}

FIoStatus FCasJournal::Replay(const FString& JournalFile, FEntryHandler&& Handler)
{
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();

	if (Ipf.FileExists(*JournalFile) == false)
	{
		return EIoErrorCode::NotFound;
	}

	TUniquePtr<IFileHandle> FileHandle(Ipf.OpenRead(*JournalFile));
	if (FileHandle.IsValid() == false)
	{
		return EIoErrorCode::FileNotOpen;
	}

	FHeader Header;
	if ((FileHandle->Read(reinterpret_cast<uint8*>(&Header), sizeof(FHeader)) == false) || (Header.IsValid() == false))
	{
		return FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to validate journal header '")
			<< JournalFile
			<< TEXT("'");
	}

	const int64 FileSize	= FileHandle->Size();
	const int64 EntryCount	= (FileSize - sizeof(FHeader) - sizeof(FFooter)) / sizeof(FEntry);

	if (EntryCount < 0)
	{
		return EIoErrorCode::ReadError;
	}

	if (EntryCount == 0)
	{
		return EIoErrorCode::Ok;
	}

	const int64 FooterPos = FileSize - sizeof(FFooter);
	if (FooterPos < 0)
	{
		return FIoStatusBuilder(EIoErrorCode::CorruptToc)
			<< TEXT("Invalid journal footer");
	}

	const int64 EntriesPos = FileHandle->Tell();
	if (FileHandle->Seek(FooterPos) == false)
	{
		return EIoErrorCode::ReadError;
	}

	FFooter Footer;
	if ((FileHandle->Read(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter)) == false) || (Footer.IsValid() == false))
	{
		return FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to validate journal footer '")
			<< JournalFile
			<< TEXT("'");
	}

	if (FileHandle->Seek(EntriesPos) == false)
	{
		return EIoErrorCode::ReadError;
	}

	TArray<FEntry> Entries;
	Entries.SetNumZeroed(IntCastChecked<int32>(EntryCount));

	if (FileHandle->Read(reinterpret_cast<uint8*>(Entries.GetData()), sizeof(FEntry) * EntryCount) == false)
	{
		return EIoErrorCode::ReadError;
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Replaying %" INT64_FMT " CAS journal entries of total %.2lf KiB from '%s'"),
		EntryCount, ToKiB(sizeof(FEntry) * EntryCount), *JournalFile);

	for (const FEntry& Entry : Entries)
	{
		if (Entry.Type() == FEntry::EType::CriticalError)
		{
			const FEntry::FCriticalError& Error = Entry.CriticalError;
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Found critical error entry '%s' (%d) in journal '%s'"),
				GetErrorText(Error.ErrorCode), Error.ErrorCode, *JournalFile);

			return FIoStatus(EIoErrorCode::ReadError, FString(GetErrorText(Error.ErrorCode)));
		}

		Handler(Entry);
	}

	return EIoErrorCode::Ok;
}

FIoStatus FCasJournal::Create(const FString& JournalFile)
{
	IPlatformFile& Ipf = FPlatformFileManager::Get().GetPlatformFile();
	Ipf.DeleteFile(*JournalFile);

	TUniquePtr<IFileHandle> FileHandle(Ipf.OpenWrite(*JournalFile));
	if (FileHandle.IsValid() == false)
	{
		return EIoErrorCode::FileNotOpen;
	}

	FHeader Header;
	FMemory::Memcpy(&Header.Magic, &FHeader::MagicSequence, sizeof(FHeader::MagicSequence));
	Header.Version = EVersion::Latest;

	if (FileHandle->Write(reinterpret_cast<uint8*>(&Header), sizeof(FHeader)) == false)
	{
		return EIoErrorCode::WriteError;
	}

	FFooter Footer;
	FMemory::Memcpy(&Footer.Magic, &FFooter::MagicSequence, sizeof(FFooter::MagicSequence));
	if (FileHandle->Write(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter)) == false)
	{
		return EIoErrorCode::WriteError;
	}

	return EIoErrorCode::Ok;
}

FCasJournal::FTransaction FCasJournal::Begin(FString&& JournalFile)
{
	return FTransaction
	{
		.JournalFile = MoveTemp(JournalFile)
	};
}

FIoStatus FCasJournal::Commit(FTransaction&& Transaction)
{
	if (Transaction.Entries.IsEmpty())
	{
		return EIoErrorCode::Ok;
	}

	IPlatformFile&	Ipf = FPlatformFileManager::Get().GetPlatformFile();

	// Validate header and footer
	{
		TUniquePtr<IFileHandle> FileHandle(Ipf.OpenRead(*Transaction.JournalFile));
		const int64				FileSize = FileHandle.IsValid() ? FileHandle->Size() : -1;

		if (FileSize < sizeof(FHeader))
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::ReadError, 0);
			return FIoStatusBuilder(EIoErrorCode::FileOpenFailed)
				<< TEXT("Failed to validate CAS journal file '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		FHeader Header;
		if ((FileHandle->Read(reinterpret_cast<uint8*>(&Header), sizeof(FHeader)) == false) || (Header.IsValid() == false))
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::SignatureError, 0);
			return FIoStatusBuilder(EIoErrorCode::ReadError)
				<< TEXT("Failed to validate CAS journal header '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		const int64 FooterPos = FileSize - sizeof(FFooter);
		if (FileHandle->Seek(FooterPos) == false)
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::SignatureError, 0);
			return FIoStatusBuilder(EIoErrorCode::ReadError)
				<< TEXT("Failed to validate CAS journal footer '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		FFooter Footer;
		if ((FileHandle->Read(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter)) == false) || (Footer.IsValid() == false))
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::SignatureError, 0);
			return FIoStatusBuilder(EIoErrorCode::ReadError)
				<< TEXT("Failed to validate CAS journal footer '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}
	}

	// Append entries
	{
		const bool				bAppend = true;
		TUniquePtr<IFileHandle> FileHandle(Ipf.OpenWrite(*Transaction.JournalFile, bAppend));
		const int64				FileSize	= FileHandle.IsValid() ? FileHandle->Size() : -1;
		const int64				EntriesPos	= FileSize > 0 ? FileSize - sizeof(FFooter) : -1;

		if ((EntriesPos < 0) || (FileHandle->Seek(EntriesPos) == false))
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::WriteError, 0);
			return FIoStatusBuilder(EIoErrorCode::FileOpenFailed)
				<< TEXT("Failed to open CAS journal '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		const int64 TotalEntrySize = Transaction.Entries.Num() * sizeof(FEntry);
		if (FileHandle->Write(
			reinterpret_cast<const uint8*>(Transaction.Entries.GetData()),
			TotalEntrySize) == false)
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::WriteError, 0);
			return FIoStatusBuilder(EIoErrorCode::WriteError)
				<< TEXT("Failed to write CAS journal entries to '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		FFooter Footer;
		FMemory::Memcpy(&Footer.Magic, &FFooter::MagicSequence, sizeof(FFooter::MagicSequence));
		if (FileHandle->Write(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter)) == false)
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::WriteError, 0);
			return FIoStatusBuilder(EIoErrorCode::WriteError)
				<< TEXT("Failed to write CAS journal footer to '")
				<< Transaction.JournalFile
				<< TEXT("'");
		}

		if (FileHandle->Flush() == false)
		{
			FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::WriteError, 0);
			return EIoErrorCode::WriteError;
		}

		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Committed %d CAS journal entries of total %.2lf KiB to '%s'"),
			Transaction.Entries.Num(), ToKiB(TotalEntrySize), *Transaction.JournalFile);

		FOnDemandInstallCacheStats::OnJournalCommit(EIoErrorCode::Ok, TotalEntrySize);

		return EIoErrorCode::Ok;
	}
}

void FCasJournal::FTransaction::ChunkLocation(const FCasLocation& Location, const FCasAddr& Addr)
{
	Entries.AddZeroed_GetRef().ChunkLocation = FEntry::FChunkLocation
	{
		.CasLocation	= Location,
		.CasAddr		= Addr
	};
}

void FCasJournal::FTransaction::BlockCreated(FCasBlockId BlockId)
{
	Entries.AddZeroed_GetRef().BlockOperation = FEntry::FBlockOperation
	{
		.Type		= FEntry::EType::BlockCreated,
		.BlockId	= BlockId,
		.UtcTicks	= FDateTime::UtcNow().GetTicks()
	};
}

void FCasJournal::FTransaction::BlockDeleted(FCasBlockId BlockId)
{
	Entries.AddZeroed_GetRef().BlockOperation = FEntry::FBlockOperation
	{
		.Type		= FEntry::EType::BlockDeleted,
		.BlockId	= BlockId,
		.UtcTicks	= FDateTime::UtcNow().GetTicks()
	};
}

void FCasJournal::FTransaction::BlockAccess(FCasBlockId BlockId, int64 UtcTicks)
{
	Entries.AddZeroed_GetRef().BlockOperation = FEntry::FBlockOperation
	{
		.Type		= FEntry::EType::BlockAccess,
		.BlockId	= BlockId,
		.UtcTicks	= UtcTicks
	};
}

void FCasJournal::FTransaction::CriticalError(FCasJournal::EErrorCode ErrorCode)
{
	Entries.AddZeroed_GetRef().CriticalError = FEntry::FCriticalError
	{
		.Type		= FEntry::EType::CriticalError,
		.ErrorCode	= ErrorCode
	};
}

///////////////////////////////////////////////////////////////////////////////
struct FCasSnapshot
{
	enum class EVersion : uint32
	{
		Invalid = 0,
		Initial,

		LatestPlusOne,
		Latest = LatestPlusOne - 1
	};

	struct FHeader
	{
		static const inline uint8 MagicSequence[16] = {'+', 'S', 'N', 'A', 'P', 'S', 'H', 'O', 'T', 'H', 'E', 'A', 'D', 'E', 'R', '+'};

		bool		IsValid() const;

		uint8		Magic[16] = {0};
		EVersion	Version = EVersion::Invalid;
		uint8		Pad[12] = {0};
	};
	static_assert(sizeof(FHeader) == 32);

	struct FFooter
	{
		static const inline uint8 MagicSequence[16] = {'+', 'S', 'N', 'A', 'P', 'S', 'H', 'O', 'T', 'F', 'O', 'O', 'T', 'E', 'R', '+'};

		bool IsValid() const;

		uint8 Magic[16] = {0};
	};
	static_assert(sizeof(FFooter) == 16);

	struct FBlock
	{
		friend FArchive& operator<<(FArchive& Ar, FBlock& Block)
		{
			Ar << Block.BlockId;
			Ar << Block.LastAccess;
			return Ar;
		}

		FCasBlockId BlockId;
		int64		LastAccess;
	};

	using FChunkLocation = TPair<FCasAddr, FCasLocation>;

	static TIoStatusOr<FCasSnapshot>	FromJournal(const FString& JournalFile);
	static TIoStatusOr<FCasSnapshot>	Load(const FString& SnapshotFile, int64* OutFileSize = nullptr);
	static TIoStatusOr<int64>			Save(const FCasSnapshot& Snapshot, const FString& SnapshotFile);
	static TIoStatusOr<int64>			TryCreateAndResetJournal(const FString& SnapshotFile, const FString& JournalFile); 

	TArray<FBlock>						Blocks;
	TArray<FChunkLocation>				ChunkLocations;
	FCasBlockId							CurrentBlockId;
};

///////////////////////////////////////////////////////////////////////////////
bool FCasSnapshot::FHeader::IsValid() const
{
	if (FMemory::Memcmp(&Magic, &FHeader::MagicSequence, sizeof(FHeader::MagicSequence)) != 0)
	{
		return false;
	}

	if (static_cast<uint32>(Version) > static_cast<uint32>(EVersion::Latest))
	{
		return false;
	}

	return true;
}

bool FCasSnapshot::FFooter::IsValid() const
{
	return FMemory::Memcmp(Magic, FFooter::MagicSequence, sizeof(FFooter::MagicSequence)) == 0;
}

TIoStatusOr<FCasSnapshot> FCasSnapshot::FromJournal(const FString& JournalFile)
{
	TMap<FCasAddr, FCasLocation>	CasLookup;
	TMap<FCasBlockId, int64>		LastAccess;
	TSet<FCasBlockId>				BlockIds;
	FCasBlockId						CurrentBlockId;

	FIoStatus ReplayStatus = FCasJournal::Replay(
		JournalFile,
		[&CasLookup, &LastAccess, &BlockIds, &CurrentBlockId](const FCasJournal::FEntry& JournalEntry)
		{
			switch(JournalEntry.Type())
			{
			case FCasJournal::FEntry::EType::ChunkLocation:
			{
				const FCasJournal::FEntry::FChunkLocation& ChunkLocation = JournalEntry.ChunkLocation;
				if (ChunkLocation.CasLocation.IsValid())
				{
					FCasLocation& Loc = CasLookup.FindOrAdd(ChunkLocation.CasAddr);
					Loc = ChunkLocation.CasLocation;
				}
				else
				{
					CasLookup.Remove(ChunkLocation.CasAddr);
				}
				break;
			}
			case FCasJournal::FEntry::EType::BlockCreated:
			{
				const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
				CurrentBlockId = Op.BlockId;
				BlockIds.Add(Op.BlockId);
				break;
			}
			case FCasJournal::FEntry::EType::BlockDeleted:
			{
				const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
				BlockIds.Remove(Op.BlockId);
				if (CurrentBlockId == Op.BlockId)
				{
					CurrentBlockId = FCasBlockId::Invalid; 
				}
				break;
			}
			case FCasJournal::FEntry::EType::BlockAccess:
			{
				const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
				LastAccess.Add(Op.BlockId, Op.UtcTicks);
				break;
			}
			};
		});

	if (ReplayStatus.IsOk() == false)
	{
		return ReplayStatus;
	}

	FCasSnapshot Snapshot;
	Snapshot.Blocks.Reserve(BlockIds.Num());
	for (FCasBlockId BlockId : BlockIds)
	{
		const int64* AccessTime = LastAccess.Find(BlockId);
		Snapshot.Blocks.Add(FBlock
		{
			.BlockId	= BlockId,
			.LastAccess = AccessTime != nullptr ? *AccessTime : FDateTime::UtcNow().GetTicks()
		});
	}

	Snapshot.ChunkLocations = CasLookup.Array();
	Snapshot.CurrentBlockId = CurrentBlockId;

	return Snapshot;
}

TIoStatusOr<int64> FCasSnapshot::Save(const FCasSnapshot& Snapshot, const FString& SnapshotFile)
{
	IFileManager& Ifm = IFileManager::Get();

	const FString TmpSnapshotFile = FPaths::ChangeExtension(SnapshotFile, TEXT(".snptmp"));

	TUniquePtr<FArchive> Ar(Ifm.CreateFileWriter(*TmpSnapshotFile));
	if (Ar.IsValid() == false)
	{
		return FIoStatus(EIoErrorCode::FileNotOpen);
	}

	FHeader Header;
	FMemory::Memcpy(&Header.Magic, &FHeader::MagicSequence, sizeof(FHeader::MagicSequence));
	Header.Version = EVersion::Latest;

	Ar->Serialize(reinterpret_cast<uint8*>(&Header), sizeof(FHeader));
	if (Ar->IsError())
	{
		Ar.Reset();
		Ifm.Delete(*TmpSnapshotFile);
		return FIoStatus(EIoErrorCode::WriteError);
	}

	FCasSnapshot& NonConst = *const_cast<FCasSnapshot*>(&Snapshot);
	*Ar << NonConst.Blocks;
	*Ar << NonConst.ChunkLocations;
	*Ar << NonConst.CurrentBlockId;

	if (Ar->IsError())
	{
		Ar.Reset();
		Ifm.Delete(*TmpSnapshotFile);
		return FIoStatus(EIoErrorCode::WriteError);
	}

	FFooter Footer;
	FMemory::Memcpy(&Footer.Magic, &FFooter::MagicSequence, sizeof(FFooter::MagicSequence));
	Ar->Serialize(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter));
	if (Ar->IsError())
	{
		Ar.Reset();
		Ifm.Delete(*TmpSnapshotFile);
		return FIoStatus(EIoErrorCode::WriteError);
	}

	const int64 FileSize = Ar->TotalSize();
	if (Ar->Close() == false)
	{
		Ar.Reset();
		Ifm.Delete(*TmpSnapshotFile);
		return FIoStatus(EIoErrorCode::WriteError);
	}

	if (Ifm.Move(*SnapshotFile, *TmpSnapshotFile) == false)
	{
		Ifm.Delete(*TmpSnapshotFile);
		return FIoStatus(EIoErrorCode::WriteError);
	}

	return FileSize; 
}

TIoStatusOr<FCasSnapshot> FCasSnapshot::Load(const FString& SnapshotFile, int64* OutFileSize)
{
	IFileManager& Ifm = IFileManager::Get();

	TUniquePtr<FArchive> Ar(Ifm.CreateFileReader(*SnapshotFile));
	if (Ar.IsValid() == false)
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}

	FHeader Header;
	Ar->Serialize(reinterpret_cast<uint8*>(&Header), sizeof(FHeader));
	if (Ar->IsError() || Header.IsValid() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to validate snapshot header '")
			<< SnapshotFile 
			<< TEXT("'");
		return Status;
	}

	FCasSnapshot Snapshot;
	*Ar << Snapshot.Blocks;
	*Ar << Snapshot.ChunkLocations;
	*Ar << Snapshot.CurrentBlockId;

	FFooter Footer;
	Ar->Serialize(reinterpret_cast<uint8*>(&Footer), sizeof(FFooter));
	if (Ar->IsError() || Footer.IsValid() == false)
	{
		FIoStatus Status = FIoStatusBuilder(EIoErrorCode::ReadError)
			<< TEXT("Failed to validate snapshot footer '")
			<< SnapshotFile 
			<< TEXT("'");
		return Status;
	}

	if (OutFileSize != nullptr)
	{
		*OutFileSize = Ar->Tell();
	}
	return Snapshot;
}

TIoStatusOr<int64> FCasSnapshot::TryCreateAndResetJournal(const FString& SnapshotFile, const FString& JournalFile)
{
	IFileManager& Ifm = IFileManager::Get();

	const int64 JournalFileSize = Ifm.FileSize(*JournalFile);
	if (JournalFileSize < 0)
	{
		return FIoStatus(EIoErrorCode::NotFound);
	}

	// Load the snapshot from the journal
	TIoStatusOr<FCasSnapshot> SnapshotStatus = FCasSnapshot::FromJournal(JournalFile);
	if (SnapshotStatus.IsOk() == false)
	{
		return SnapshotStatus.Status();
	}

	// Save the snapshot
	int64 SnapshotSize		= -1;
	FCasSnapshot Snapshot	= SnapshotStatus.ConsumeValueOrDie();
	if (TIoStatusOr<int64> Status = FCasSnapshot::Save(Snapshot, SnapshotFile); Status.IsOk())
	{
		SnapshotSize = Status.ConsumeValueOrDie();
	}
	else
	{
		return Status.Status();
	}

	// Try create a new empty journal 
	const FString TmpJournalFile = FPaths::ChangeExtension(JournalFile, TEXT(".jrntmp"));
	if (FIoStatus Status = FCasJournal::Create(TmpJournalFile); Status.IsOk() == false)
	{
		if (Ifm.Delete(*SnapshotFile) == false)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to delete CAS snapshot '%s'"), *SnapshotFile); 
		}

		return Status;
	}

	if (Ifm.Move(*JournalFile , *TmpJournalFile) == false)
	{
		if (Ifm.Delete(*SnapshotFile) == false)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to delete CAS snapshot '%s'"), *SnapshotFile); 
		}

		return FIoStatus(EIoErrorCode::WriteError);
	}

	return SnapshotSize;
}

///////////////////////////////////////////////////////////////////////////////
class FOnDemandInstallCache final
	: public IOnDemandInstallCache 
{
	using FSharedBackendContextRef	= TSharedRef<const FIoDispatcherBackendContext>;
	using FSharedBackendContext		= TSharedPtr<const FIoDispatcherBackendContext>;

	struct FChunkRequest
	{
		explicit FChunkRequest(
			FSharedAsyncFileHandle FileHandle,
			FIoRequestImpl* Request,
			FOnDemandChunkInfo&& Info,
			FIoOffsetAndLength Range,
			uint64 RequestedRawSize)
				: SharedFileHandle(FileHandle)
				, DispatcherRequest(Request)
				, ChunkInfo(MoveTemp(Info))
				, ChunkRange(Range)
				, EncodedChunk(ChunkRange.GetLength())
				, RawSize(RequestedRawSize)
		{
			check(DispatcherRequest != nullptr);
			check(ChunkInfo.IsValid());
			check(Request->NextRequest == nullptr);
			check(Request->BackendData == nullptr);
		}

		static FChunkRequest* Get(FIoRequestImpl& Request)
		{
			return reinterpret_cast<FChunkRequest*>(Request.BackendData);
		}

		static FChunkRequest& GetRef(FIoRequestImpl& Request)
		{
			check(Request.BackendData);
			return *reinterpret_cast<FChunkRequest*>(Request.BackendData);
		}

		static FChunkRequest& Attach(FIoRequestImpl& Request, FChunkRequest* ChunkRequest)
		{
			check(Request.BackendData == nullptr);
			check(ChunkRequest != nullptr);
			Request.BackendData = ChunkRequest;
			return *ChunkRequest;
		}

		static TUniquePtr<FChunkRequest> Detach(FIoRequestImpl& Request)
		{
			void* ChunkRequest = nullptr;
			Swap(ChunkRequest, Request.BackendData);
			return TUniquePtr<FChunkRequest>(reinterpret_cast<FChunkRequest*>(ChunkRequest));
		}

		FSharedAsyncFileHandle			SharedFileHandle;
		TUniquePtr<IAsyncReadRequest>	FileReadRequest;
		FIoRequestImpl*					DispatcherRequest;
		FOnDemandChunkInfo				ChunkInfo;
		FIoOffsetAndLength				ChunkRange;
		FIoBuffer						EncodedChunk;
		uint64							RawSize;
	};

	struct FPendingChunks
	{
		static constexpr uint64 MaxPendingBytes = 4ull << 20;

		bool IsEmpty() const
		{
			check(Chunks.Num() == ChunkHashes.Num());
			return TotalSize == 0 && Chunks.IsEmpty() && ChunkHashes.IsEmpty();
		}

		void Append(FIoBuffer&& Chunk, const FIoHash& ChunkHash)
		{
			check(Chunks.Num() == ChunkHashes.Num());
			TotalSize += Chunk.GetSize();
			ChunkHashes.Add(ChunkHash);
			Chunks.Add(MoveTemp(Chunk));
		}

		FIoBuffer Pop(FIoHash& OutChunkHash)
		{
			check(Chunks.Num() == ChunkHashes.Num());
			check(Chunks.IsEmpty() == false);
			FIoBuffer Chunk = Chunks.Pop(EAllowShrinking::No);
			TotalSize		= TotalSize - Chunk.GetSize();
			OutChunkHash	= ChunkHashes.Pop(EAllowShrinking::No);
			return Chunk;
		}

		void Reset()
		{
			Chunks.Reset();
			ChunkHashes.Reset();
			TotalSize = 0;
		}

		TArray<FIoBuffer>	Chunks;
		TArray<FIoHash>		ChunkHashes;
		uint64				TotalSize = 0;
	};

	using FUniquePendingChunks = TUniquePtr<FPendingChunks>;

public:
	FOnDemandInstallCache(const FOnDemandInstallCacheConfig& Config, FOnDemandIoStore& IoStore);
	virtual ~FOnDemandInstallCache();

	// IIoDispatcherBackend
	virtual void								Initialize(FSharedBackendContextRef Context) override;
	virtual void								Shutdown() override;
	virtual void								ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved) override;
	virtual FIoRequestImpl*						GetCompletedIoRequests() override;
	virtual void								CancelIoRequest(FIoRequestImpl* Request) override;
	virtual void								UpdatePriorityForIoRequest(FIoRequestImpl* Request) override;
	virtual bool								DoesChunkExist(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<uint64>					GetSizeForChunk(const FIoChunkId& ChunkId) const override;
	virtual TIoStatusOr<FIoMappedRegion>		OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options) override;
	virtual const TCHAR*						GetName() const override;

	// IOnDemandInstallCache
	virtual bool								IsChunkCached(const FIoHash& ChunkHash) override;
	virtual FIoStatus							PutChunk(FIoBuffer&& Chunk, const FIoHash& ChunkHash) override;
	virtual FIoStatus							Purge(TSet<FIoHash>&& ChunksToInstall) override;
	virtual FIoStatus							PurgeAllUnreferenced(bool bDefrag, const uint64* BytesToPurge = nullptr) override;
	virtual FIoStatus							DefragAll(const uint64* BytesToFree = nullptr) override;
	virtual FIoStatus							Verify() override;
	virtual FIoStatus							Flush() override;
	virtual FOnDemandInstallCacheUsage			GetCacheUsage() override;

private:
	void										RegisterConsoleCommands();
	FIoStatus									Reset();
	FIoStatus									InitialVerify();
	uint64										AddReferencesToBlocks(
													const TArray<FSharedOnDemandContainer>& Containers, 
													const TArray<TBitArray<>>& ChunkEntryIndices,
													const TSet<FIoHash>& ChunksToInstall,
													FCasBlockInfoMap& BlockInfoMap,
													uint64& OutTotalReferencedBytes) const;
	FIoStatus									Purge(FCasBlockInfoMap& BlockInfo, uint64 TotalBytesToPurge, uint64& OutTotalPurgedBytes);
	FIoStatus									Defrag(
													const TArray<FSharedOnDemandContainer>& Containers,
													const TArray<TBitArray<>>& ChunkEntryIndices,
													FCasBlockInfoMap& BlockInfo, 
													const uint64* TotalBytesToFree = nullptr);
	bool										Resolve(FIoRequestImpl* Request);
	void										CompleteRequest(FIoRequestImpl* Request, EIoErrorCode Status);
	FIoStatus									FlushPendingChunks(FPendingChunks& Chunks, int64 UtcAccessTicks = 0);
	FIoStatus									FlushPendingChunksImpl(FPendingChunks& Chunks, int64 UtcAccessTicks = 0);
	FString										GetJournalFilename() const { return CacheDirectory / TEXT("cas.jrn"); }
	FString										GetSnapshotFilename() const { return CacheDirectory / TEXT("cas.snp"); }

	FOnDemandIoStore&			IoStore;
	FString						CacheDirectory;
	FCas						Cas;
	std::atomic<FCasBlockId>	CurrentBlock{ FCasBlockId::Invalid };
	FUniquePendingChunks		PendingChunks;
	FSharedBackendContext		BackendContext;
	FIoRequestList				CompletedRequests;
	UE::FMutex					Mutex;
	uint64						MaxCacheSize{ 0 };
	uint64						MaxJournalSize;

#if UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
	UE::Tasks::FPipe			ExclusivePipe{ UE_SOURCE_LOCATION };
#endif // UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE

#if UE_IAD_DEBUG_CONSOLE_CMDS
	TArray<IConsoleCommand*> ConsoleCommands;
#endif // UE_IAS_DEBUG_CONSOLE_CMDS
};

///////////////////////////////////////////////////////////////////////////////
FOnDemandInstallCache::FOnDemandInstallCache(const FOnDemandInstallCacheConfig& Config, FOnDemandIoStore& InIoStore)
	: IoStore(InIoStore)
	, CacheDirectory(Config.RootDirectory)
	, MaxCacheSize(Config.DiskQuota)
	, MaxJournalSize(Config.JournalMaxSize)
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Initializing install cache, MaxCacheSize=%.2lf MiB, MaxJournalSize=%.2lf KiB"),
		ToMiB(MaxCacheSize), ToKiB(MaxJournalSize));

	const uint64 MinDiskQuota = 2 * Cas.MaxBlockSize;
	if (MaxCacheSize < MinDiskQuota)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to initialize install cache - disk quota must be at least %.2lf MiB"), ToMiB(MinDiskQuota));
		return;
	}

	// Reserve one block of space for defragmentation overhead
	MaxCacheSize -= Cas.MaxBlockSize;
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Effective MaxCacheSize without defragmentation space is MaxCacheSize=%.2lf MiB"), ToMiB(MaxCacheSize));

	FIoStatus Status = Cas.Initialize(CacheDirectory);
	if (Status.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to initialize install cache, reason '%s'"), *Status.ToString());
		return;
	}

	// Try read the journal snapshot
	{
		const FString SnapshotFile = GetSnapshotFilename();
		int64 SnapshotSize = -1;
		TIoStatusOr<FCasSnapshot> SnapshotStatus = FCasSnapshot::Load(SnapshotFile, &SnapshotSize);
		if (SnapshotStatus.IsOk())
		{
			FCasSnapshot Snapshot = SnapshotStatus.ConsumeValueOrDie();
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Loaded CAS snapshot '%s' %.2lf KiB with %d blocks and %d chunk locations"),
					*SnapshotFile, ToKiB(SnapshotSize), Snapshot.Blocks.Num(), Snapshot.ChunkLocations.Num());

			Cas.Lookup.Reserve(Snapshot.ChunkLocations.Num());
			for (TPair<FCasAddr, FCasLocation>& Kv : Snapshot.ChunkLocations)
			{
				Cas.Lookup.Add(MoveTemp(Kv));
			}

			Cas.BlockIds.Reserve(Snapshot.Blocks.Num());
			Cas.LastAccess.Reserve(Snapshot.Blocks.Num());
			for (const FCasSnapshot::FBlock& Block : Snapshot.Blocks)
			{
				Cas.BlockIds.Add(Block.BlockId, 0);
				Cas.LastAccess.Add(Block.BlockId, Block.LastAccess);
			}
		}
	}

	// Replay the journal 
	const FString JournalFile = GetJournalFilename();
	Status = FCasJournal::Replay(JournalFile, [this](const FCasJournal::FEntry& JournalEntry)
	{
		switch(JournalEntry.Type())
		{
		case FCasJournal::FEntry::EType::ChunkLocation:
		{
			const FCasJournal::FEntry::FChunkLocation& ChunkLocation = JournalEntry.ChunkLocation;
			if (ChunkLocation.CasLocation.IsValid())
			{
				FCasLocation& Loc = Cas.Lookup.FindOrAdd(ChunkLocation.CasAddr);
				Loc = ChunkLocation.CasLocation;
			}
			else
			{
				Cas.Lookup.Remove(ChunkLocation.CasAddr);
			}
			break;
		}
		case FCasJournal::FEntry::EType::BlockCreated:
		{
			const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
			CurrentBlock = Op.BlockId;
			Cas.BlockIds.Add(Op.BlockId, 0);
			break;
		}
		case FCasJournal::FEntry::EType::BlockDeleted:
		{
			const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
			Cas.BlockIds.Remove(Op.BlockId);
			FCasBlockId MaybeCurrentBlock = Op.BlockId;
			CurrentBlock.compare_exchange_strong(MaybeCurrentBlock, FCasBlockId::Invalid);
			break;
		}
		case FCasJournal::FEntry::EType::BlockAccess:
		{
			const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
			Cas.TrackAccess(Op.BlockId, Op.UtcTicks);
			break;
		}
		};
	});

	// Initializing the cache for the first time
	if (Status.GetErrorCode() == EIoErrorCode::NotFound)
	{
		if (Status = FCasJournal::Create(JournalFile); Status.IsOk())
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Created CAS journal '%s'"), *JournalFile);

			// Make sure that there are no existing blocks when starting from an empty cache
			const bool bDeleteExisting = true;
			Status = Cas.Initialize(CacheDirectory, bDeleteExisting);
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to create CAS journal '%s'"), *JournalFile);
		}
	}

	// Verify the current state of the cache
	if (Status.IsOk())
	{
		Status = InitialVerify();
	}

	// Try to reset the cache if something has gone wrong 
	if (Status.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Resetting install cash, reason '%s'"),
			*Status.ToString());

		FOnDemandInstallCacheStats::OnStartupError(Status.GetErrorCode());
		Status = Reset();
	}

	if (Status.IsOk())
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Install cache Ok!"));
		RegisterConsoleCommands();
		Cas.Compact();
	}
	else
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to initialize install cache, reason '%s'"),
			*Status.ToString());
	}
}

FOnDemandInstallCache::~FOnDemandInstallCache()
{
}

void FOnDemandInstallCache::Initialize(FSharedBackendContextRef Context)
{
	BackendContext = Context;
}

void FOnDemandInstallCache::Shutdown()
{
	FCas::FLastAccess LastAccess;
	{
		TUniqueLock Lock(Cas.Mutex);
		LastAccess = MoveTemp(Cas.LastAccess);
	}

	const FString JournalFile = GetJournalFilename();
	FCasJournal::FTransaction Transaction = FCasJournal::Begin(JournalFile);
	for (const TPair<FCasBlockId, int64>& Kv : LastAccess)
	{
		Transaction.BlockAccess(Kv.Key, Kv.Value);
	}

	if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); !Status.IsOk())
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to update CAS journal '%s' with block timestamp(s), reason '%s'"),
			*JournalFile, *Status.ToString());
	}

	IFileManager& Ifm = IFileManager::Get();
	const FString JournalFilename = GetJournalFilename();
	if (Ifm.FileSize(*JournalFile) > int64(MaxJournalSize))
	{
		const FString SnapshotFilename = GetSnapshotFilename();
		TIoStatusOr<int64> SnapshotStatus = FCasSnapshot::TryCreateAndResetJournal(SnapshotFilename, JournalFilename);
		if (SnapshotStatus.IsOk())
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Saved CAS snapshot '%s' %.2lf KiB"), *SnapshotFilename, ToKiB(SnapshotStatus.ValueOrDie()));
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to create CAS snapshot from journal '%s', reason '%s'"),
				*JournalFile, *SnapshotStatus.Status().ToString());
		}
	}

#if UE_IAD_DEBUG_CONSOLE_CMDS
	for (IConsoleCommand* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
#endif // UE_IAD_DEBUG_CONSOLE_CMDS
}

void FOnDemandInstallCache::ResolveIoRequests(FIoRequestList Requests, FIoRequestList& OutUnresolved)
{
	while (FIoRequestImpl* Request = Requests.PopHead())
	{
		if (Resolve(Request) == false)
		{
			OutUnresolved.AddTail(Request);
		}
	}
}

FIoRequestImpl* FOnDemandInstallCache::GetCompletedIoRequests()
{
	FIoRequestImpl* FirstCompleted = nullptr;
	{
		UE::TUniqueLock Lock(Mutex);
		for (FIoRequestImpl& Completed : CompletedRequests)
		{
			TUniquePtr<FChunkRequest> Detached = FChunkRequest::Detach(Completed);
		}
		FirstCompleted = CompletedRequests.GetHead();
		CompletedRequests = FIoRequestList();
	}

	return FirstCompleted;
}

void FOnDemandInstallCache::CancelIoRequest(FIoRequestImpl* Request)
{
	check(Request != nullptr);
	UE::TUniqueLock Lock(Mutex);
	if (FChunkRequest* ChunkRequest = FChunkRequest::Get(*Request))
	{
		if (ChunkRequest->FileReadRequest.IsValid())
		{
			ChunkRequest->FileReadRequest->Cancel();
		}
	}
}

void FOnDemandInstallCache::UpdatePriorityForIoRequest(FIoRequestImpl* Request)
{
}

bool FOnDemandInstallCache::DoesChunkExist(const FIoChunkId& ChunkId) const
{
	EIoErrorCode ErrorCode = EIoErrorCode::UnknownChunkID;
	if (FOnDemandChunkInfo ChunkInfo = IoStore.GetInstalledChunkInfo(ChunkId, ErrorCode))
	{
		const FCasLocation CasLoc = Cas.FindChunk(ChunkInfo.Hash());
		return CasLoc.IsValid();
	}

	return false;
}

TIoStatusOr<uint64> FOnDemandInstallCache::GetSizeForChunk(const FIoChunkId& ChunkId) const
{
	EIoErrorCode ErrorCode = EIoErrorCode::UnknownChunkID;
	if (FOnDemandChunkInfo ChunkInfo = IoStore.GetInstalledChunkInfo(ChunkId, ErrorCode))
	{
		return ChunkInfo.RawSize();
	}

	return FIoStatus(ErrorCode);
}

TIoStatusOr<FIoMappedRegion> FOnDemandInstallCache::OpenMapped(const FIoChunkId& ChunkId, const FIoReadOptions& Options)
{
	return FIoStatus(EIoErrorCode::FileOpenFailed);
}

const TCHAR* FOnDemandInstallCache::GetName() const
{
	return TEXT("OnDemandInstallCache");
}


bool FOnDemandInstallCache::Resolve(FIoRequestImpl* Request)
{
	EIoErrorCode ErrorCode = EIoErrorCode::UnknownChunkID;
	FOnDemandChunkInfo ChunkInfo = IoStore.GetInstalledChunkInfo(Request->ChunkId, ErrorCode);
	if (ChunkInfo.IsValid() == false)
	{
		if (ErrorCode == EIoErrorCode::NotInstalled)
		{
			CompleteRequest(Request, EIoErrorCode::NotInstalled);
			return true;
		}
		return false;
	}

	const FCasLocation CasLoc = Cas.FindChunk(ChunkInfo.Hash());
	if (CasLoc.IsValid() == false)
	{
		CompleteRequest(Request, EIoErrorCode::NotInstalled);
		return true;
	}

	const uint64 RequestSize = FMath::Min<uint64>(
		Request->Options.GetSize(),
		ChunkInfo.RawSize() - Request->Options.GetOffset());

	TIoStatusOr<FIoOffsetAndLength> ChunkRange = FIoChunkEncoding::GetChunkRange(
		ChunkInfo.RawSize(),
		ChunkInfo.BlockSize(),
		ChunkInfo.Blocks(),
		Request->Options.GetOffset(),
		RequestSize);

	if (ChunkRange.IsOk() == false)
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to get chunk range"));
		CompleteRequest(Request, ChunkRange.Status().GetErrorCode());
		return true;
	}

	TRACE_IOSTORE_BACKEND_REQUEST_STARTED(Request, this);
	Cas.TrackAccess(CasLoc.BlockId);

#if UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
	const bool bIsLocationInCurrentBlock = CasLoc.BlockId == CurrentBlock;
	if (bIsLocationInCurrentBlock)
	{
		// The current block may have open writes which may cause async reads to fail
		// on some platforms. Schedule the reads to happen on the same pipe as writes

		// The internal request parameters are attached/owned by the I/O request via
		// the backend data parameter. The chunk request is deleted in GetCompletedRequests
		FChunkRequest::Attach(*Request, new FChunkRequest(
			FSharedAsyncFileHandle(),
			Request,
			MoveTemp(ChunkInfo),
			ChunkRange.ConsumeValueOrDie(),
			RequestSize));

		ExclusivePipe.Launch(UE_SOURCE_LOCATION, [this, Request, CasLoc]
		{
			FChunkRequest& ChunkRequest = FChunkRequest::GetRef(*Request);
			EIoErrorCode Status = EIoErrorCode::FileOpenFailed;

			const FString Filename = Cas.GetBlockFilename(CasLoc.BlockId);

			FSharedFileOpenResult FileOpenResult = Cas.OpenRead(CasLoc.BlockId);
			if (FileOpenResult.IsValid())
			{
				Status = EIoErrorCode::ReadError;

				TSharedPtr<IFileHandle> FileHandle = FileOpenResult.StealValue();
				const int64 CasBlockOffset = CasLoc.BlockOffset + ChunkRequest.ChunkRange.GetOffset();
				if (Request->IsCancelled())
				{
					UE_LOG(LogIoStoreOnDemand, Verbose, TEXT("Cancelled request - skipped seek to offset %lld in CAS block '%s'"), CasBlockOffset, *Filename);
				}
				else if (FileHandle->Seek(CasBlockOffset))
				{
					const bool bOk = FileHandle->Read(ChunkRequest.EncodedChunk.GetData(), ChunkRequest.EncodedChunk.GetSize());
					if (bOk)
					{
						Status = EIoErrorCode::Ok;
					}
					else
					{
						UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to read %llu bytes at offset %lld in CAS block '%s'"),
							ChunkRequest.EncodedChunk.GetSize(),
							CasBlockOffset,
							*Filename);
					}
				}
				else
				{
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to seek to offset %lld in CAS block '%s'"), CasBlockOffset, *Filename);
				}
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to open CAS block '%s' for reading, reason '%s'"),
					*Filename, *FileOpenResult.GetError().GetMessage());
			}

			UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Request, Status]
			{
				CompleteRequest(Request, Status);
			});
		}, UE::Tasks::ETaskPriority::BackgroundHigh);

		return true;
	}
#endif // UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE

	FSharedFileOpenAsyncResult FileOpenResult = Cas.OpenAsyncRead(CasLoc.BlockId);
	if (FileOpenResult.HasError())
	{
		const FString Filename = Cas.GetBlockFilename(CasLoc.BlockId);
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to open CAS block '%s' for async reading, reason '%s'"), *Filename, *FileOpenResult.GetError().GetMessage());
		TUniquePtr<FChunkRequest> Detached = FChunkRequest::Detach(*Request);
		FOnDemandInstallCacheStats::OnReadCompleted(EIoErrorCode::FileOpenFailed);
		CompleteRequest(Request, EIoErrorCode::FileOpenFailed);
		return true;
	}
	
	FSharedAsyncFileHandle FileHandle(FileOpenResult.StealValue());

	// The internal request parameters are attached/owned by the I/O request via
	// the backend data parameter. The chunk request is deleted in GetCompletedRequests
	FChunkRequest& ChunkRequest = FChunkRequest::Attach(*Request, new FChunkRequest(
		FileHandle,
		Request,
		MoveTemp(ChunkInfo),
		ChunkRange.ConsumeValueOrDie(),
		RequestSize));

	FAsyncFileCallBack Callback = [this, Request](bool bWasCancelled, IAsyncReadRequest* ReadRequest)
	{
		UE::Tasks::Launch(UE_SOURCE_LOCATION, [this, Request, bWasCancelled]
		{
			const EIoErrorCode Status = bWasCancelled ? EIoErrorCode::ReadError : EIoErrorCode::Ok;
			CompleteRequest(Request, Status);
		});
	};

	ChunkRequest.FileReadRequest.Reset(FileHandle->ReadRequest(
		CasLoc.BlockOffset + ChunkRequest.ChunkRange.GetOffset(),
		ChunkRequest.ChunkRange.GetLength(),
		EAsyncIOPriorityAndFlags::AIOP_BelowNormal,
		&Callback,
		ChunkRequest.EncodedChunk.GetData()));

	if (ChunkRequest.FileReadRequest.IsValid() == false)
	{
		TRACE_IOSTORE_BACKEND_REQUEST_FAILED(Request);
		TUniquePtr<FChunkRequest> Detached = FChunkRequest::Detach(*Request);
		CompleteRequest(Request, EIoErrorCode::ReadError);
		return true;
	}

	return true;
}

bool FOnDemandInstallCache::IsChunkCached(const FIoHash& ChunkHash)
{
	const FCasLocation Loc = Cas.FindChunk(ChunkHash);
	return Loc.IsValid();
}

FIoStatus FOnDemandInstallCache::PutChunk(FIoBuffer&& Chunk, const FIoHash& ChunkHash)
{
	if (PendingChunks.IsValid() == false)
	{
		PendingChunks = MakeUnique<FPendingChunks>();
	}

	if (PendingChunks->TotalSize > FPendingChunks::MaxPendingBytes)
	{
		if (FIoStatus Status = FlushPendingChunks(*PendingChunks); Status.IsOk() == false)
		{
			return Status;
		}
		check(PendingChunks->IsEmpty());
	}

	PendingChunks->Append(MoveTemp(Chunk), ChunkHash);
	return FIoStatus::Ok;
}

FIoStatus FOnDemandInstallCache::Purge(TSet<FIoHash>&& ChunksToInstall)
{
	FCasBlockInfoMap	BlockInfo;
	const uint64		TotalCachedBytes = Cas.GetBlockInfo(BlockInfo);

	TArray<FSharedOnDemandContainer>	Containers;
	TArray<TBitArray<>>					ChunkEntryIndices;

	IoStore.GetReferencedContent(Containers, ChunkEntryIndices);
	check(Containers.Num() == ChunkEntryIndices.Num());

	uint64 ReferencedBytes = 0;
	uint64 FragmentedBytes = 0;
	uint64 TotalReferencedBlockBytes = 0;
	int64 OldestBlockAccess = FDateTime::MaxValue().GetTicks();

	const uint64 TotalUncachedBytes = AddReferencesToBlocks(Containers, ChunkEntryIndices, ChunksToInstall, BlockInfo, ReferencedBytes);
	for (const TPair<FCasBlockId, FCasBlockInfo>& Kv : BlockInfo)
	{
		const FCasBlockInfo& Info = Kv.Value;
		if (Info.RefSize < Info.FileSize)
		{
			FragmentedBytes += (Info.FileSize - Info.RefSize);
		}
		if (Info.RefSize > 0)
		{
			TotalReferencedBlockBytes += Info.FileSize;
		}
		if (Info.LastAccess < OldestBlockAccess)
		{
			OldestBlockAccess = Info.LastAccess;
		}
	}

	FOnDemandInstallCacheStats::OnCacheUsage(
		MaxCacheSize, TotalCachedBytes, TotalReferencedBlockBytes, ReferencedBytes, FragmentedBytes, OldestBlockAccess);

	const uint64 TotalRequiredBytes = TotalCachedBytes + TotalUncachedBytes;
	if (TotalRequiredBytes <= MaxCacheSize)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Skipping cache purge, MaxCacheSize=%.2lf MiB, CacheSize=%.2lf MiB, ReferencedBlockSize=%.2lf MiB, ReferencedSize=%.2lf MiB, FragmentedBytes=%.2lf MiB, UncachedSize=%.2lf MiB"),
			ToMiB(MaxCacheSize), ToMiB(TotalCachedBytes), ToMiB(TotalReferencedBlockBytes), ToMiB(ReferencedBytes), ToMiB(FragmentedBytes), ToMiB(TotalUncachedBytes));
		return FIoStatus::Ok;
	}

	//TODO: Compute fragmentation metric and redownload chunks when this number gets too high

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Purging install cache, MaxCacheSize=%.2lf MiB, CacheSize=%.2lf MiB, ReferencedBlockSize=%.2lf MiB, ReferencedSize=%.2lf MiB, FragmentedBytes=%.2lf MiB, UncachedSize=%.2lf MiB"),
		ToMiB(MaxCacheSize), ToMiB(TotalCachedBytes), ToMiB(TotalReferencedBlockBytes), ToMiB(ReferencedBytes), ToMiB(FragmentedBytes), ToMiB(TotalUncachedBytes));

	const uint64	TotalBytesToPurge	= TotalRequiredBytes - MaxCacheSize;
	uint64			TotalPurgedBytes	= 0;

	FIoStatus Status = Purge(BlockInfo, TotalBytesToPurge, TotalPurgedBytes);

	if (TotalPurgedBytes > 0)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Purged %.2lf MiB (%.2lf%%) from install cache"),
			ToMiB(TotalPurgedBytes), 100.0 * (double(TotalPurgedBytes) / double(TotalCachedBytes)));
	}

	const uint64 NewCachedBytes = TotalCachedBytes - TotalPurgedBytes;
	UE_CLOG(NewCachedBytes > MaxCacheSize,
		LogIoStoreOnDemand, Warning, TEXT("Max install cache size exceeded by %.2lf MiB (%.2lf%%)"),
			ToMiB(NewCachedBytes - MaxCacheSize), 100.0 * (double(NewCachedBytes - MaxCacheSize) / double(MaxCacheSize)));

	FOnDemandInstallCacheStats::OnPurge(Status.GetErrorCode(), MaxCacheSize, NewCachedBytes, TotalBytesToPurge, TotalPurgedBytes);

	if (TotalPurgedBytes < TotalBytesToPurge)
	{
		if (UE::IoStore::CVars::GIoStoreOnDemandEnableDefrag)
		{
			// Attempt to defrag
			const uint64 DefragBytesToPurge = TotalBytesToPurge - TotalPurgedBytes;
			FIoStatus DefragStatus = Defrag(Containers, ChunkEntryIndices, BlockInfo, &DefragBytesToPurge);
			if (DefragStatus.IsOk() == false)
			{
				return FIoStatusBuilder(EIoErrorCode::WriteError) 
					<< FString::Printf(TEXT("Failed to purge %" UINT64_FMT " from install cache after defrag (%s)"), TotalBytesToPurge, *DefragStatus.ToString());
			}
		}
		else
		{
			return FIoStatusBuilder(EIoErrorCode::WriteError) << FString::Printf(TEXT("Failed to purge %" UINT64_FMT " from install cache"), TotalBytesToPurge);
		}
	}

	return Status;
}

FIoStatus FOnDemandInstallCache::PurgeAllUnreferenced(bool bDefrag, const uint64* BytesToPurge /*= nullptr*/)
{
	FCasBlockInfoMap	BlockInfo;
	const uint64		TotalCachedBytes = Cas.GetBlockInfo(BlockInfo);

	TArray<FSharedOnDemandContainer>	Containers;
	TArray<TBitArray<>>					ChunkEntryIndices;

	IoStore.GetReferencedContent(Containers, ChunkEntryIndices);
	check(Containers.Num() == ChunkEntryIndices.Num());

	uint64 ReferencedBytes = 0;
	AddReferencesToBlocks(Containers, ChunkEntryIndices, {}, BlockInfo, ReferencedBytes);

	const uint64 TotalReferencedBytes = Algo::TransformAccumulate(BlockInfo,
		[](const TPair<FCasBlockId, FCasBlockInfo>& Kv) { return (Kv.Value.RefSize > 0) ? Kv.Value.FileSize : uint64(0); },
		uint64(0));

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Purging install cache, MaxCacheSize=%.2lf MiB, CacheSize=%.2lf MiB, ReferencedBytes=%.2lf MiB"),
		ToMiB(MaxCacheSize), ToMiB(TotalCachedBytes), ToMiB(TotalReferencedBytes));

	const uint64	TotalBytesToPurge	= BytesToPurge ? *BytesToPurge : MaxCacheSize;
	uint64			TotalPurgedBytes	= 0;
	FIoStatus Status = Purge(BlockInfo, TotalBytesToPurge, TotalPurgedBytes);

	if (TotalPurgedBytes > 0)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Purged %.2lf MiB (%.2lf%%) from install cache"),
			ToMiB(TotalPurgedBytes), 100.0 * (double(TotalPurgedBytes) / double(TotalCachedBytes)));
	}

	const uint64 NewCachedBytes = TotalCachedBytes - TotalPurgedBytes;
	UE_CLOG(NewCachedBytes > MaxCacheSize,
		LogIoStoreOnDemand, Warning, TEXT("Max install cache size exceeded by %.2lf MiB (%.2lf%%)"),
			ToMiB(NewCachedBytes - MaxCacheSize), 100.0 * (double(NewCachedBytes - MaxCacheSize) / double(MaxCacheSize)));

	if (BytesToPurge)
	{
		if (bDefrag)
		{
			// Attempt to defrag
			const uint64 DefragBytesToPurge = TotalBytesToPurge - TotalPurgedBytes;
			FIoStatus DefragStatus = Defrag(Containers, ChunkEntryIndices, BlockInfo, &DefragBytesToPurge);
			if (DefragStatus.IsOk() == false)
			{
				return FIoStatusBuilder(EIoErrorCode::WriteError)
					<< FString::Printf(TEXT("Failed to purge %" UINT64_FMT " from install cache after defrag (%s)"), TotalBytesToPurge, *DefragStatus.ToString());
			}
		}
		else
		{
			return FIoStatusBuilder(EIoErrorCode::WriteError) << FString::Printf(TEXT("Failed to purge %" UINT64_FMT " from install cache"), TotalBytesToPurge);
		}
	}
	else if (bDefrag)
	{
		// Just do Full Defrag
		FIoStatus DefragStatus = Defrag(Containers, ChunkEntryIndices, BlockInfo);
		if (DefragStatus.IsOk() == false)
		{
			return DefragStatus;
		}
	}

	return Status;
}

FIoStatus FOnDemandInstallCache::DefragAll(const uint64* BytesToFree /*= nullptr*/)
{
	FCasBlockInfoMap	BlockInfo;
	const uint64		TotalCachedBytes = Cas.GetBlockInfo(BlockInfo);

	TArray<FSharedOnDemandContainer>	Containers;
	TArray<TBitArray<>>					ChunkEntryIndices;

	IoStore.GetReferencedContent(Containers, ChunkEntryIndices);
	check(Containers.Num() == ChunkEntryIndices.Num());

	uint64 ReferencedBytes = 0;
	AddReferencesToBlocks(Containers, ChunkEntryIndices, {}, BlockInfo, ReferencedBytes);

	const uint64 TotalReferencedBlockBytes = Algo::TransformAccumulate(BlockInfo,
		[](const TPair<FCasBlockId, FCasBlockInfo>& Kv) { return (Kv.Value.RefSize > 0) ? Kv.Value.FileSize : uint64(0); },
		uint64(0));

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Defragmenting install cache, MaxCacheSize=%.2lf MiB, CacheSize=%.2lf MiB, ReferencedBlockSize=%.2lf MiB, ReferencedSize=%.2lf MiB"),
		ToMiB(MaxCacheSize), ToMiB(TotalCachedBytes), ToMiB(TotalReferencedBlockBytes), ToMiB(ReferencedBytes));

	return Defrag(Containers, ChunkEntryIndices, BlockInfo, BytesToFree);
}

FIoStatus FOnDemandInstallCache::Verify()
{
	struct FChunkLookup
	{
		TMap<FCasAddr, int32> AddrToIndex;
	};

	struct FCasAddrLocation
	{
		FCasAddr		Addr;
		FCasLocation	Location;

		bool operator<(const FCasAddrLocation& Other) const
		{
			if (Location.BlockId == Other.Location.BlockId)
			{
				return Location.BlockOffset < Other.Location.BlockOffset;
			}
			return Location.BlockId.Id < Other.Location.BlockId.Id;
		}
	};

	TArray<FSharedOnDemandContainer>	Containers = IoStore.GetContainers(EOnDemandContainerFlags::InstallOnDemand);
	TArray<FCasAddrLocation>			ChunkLocations;
	TArray<FChunkLookup>				ChunkLookups;

	{
		TUniqueLock Lock(Cas.Mutex);
		ChunkLocations.Reserve(Cas.Lookup.Num());
		for (const TPair<FCasAddr, FCasLocation>& Kv : Cas.Lookup)
		{
			ChunkLocations.Add(FCasAddrLocation
			{
				.Addr		= Kv.Key,
				.Location	= Kv.Value
			});
		}
	}
	ChunkLocations.Sort();

	ChunkLookups.Reserve(Containers.Num());
	for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
	{
		FSharedOnDemandContainer& Container = Containers[Idx];
		FChunkLookup& Lookup				= ChunkLookups.AddDefaulted_GetRef();

		Lookup.AddrToIndex.Reserve(Container->ChunkEntries.Num());
		for (int32 EntryIndex = 0; const FOnDemandChunkEntry& Entry : Container->ChunkEntries)
		{
			const FCasAddr& Addr = *reinterpret_cast<const FCasAddr*>(&Entry.Hash);
			Lookup.AddrToIndex.Add(Addr, EntryIndex++);
		}
	}

	auto FindChunkEntry = [&Containers, &ChunkLookups](const FCasAddr& Addr, int32& OutContainerIndex) -> int32
	{
		OutContainerIndex = INDEX_NONE;
		for (int32 Idx = 0; Idx < Containers.Num(); ++Idx)
		{
			FChunkLookup& Lookup = ChunkLookups[Idx];
			if (const int32* EntryIndex = Lookup.AddrToIndex.Find(Addr))
			{
				OutContainerIndex = Idx;
				return *EntryIndex;
			}
		}

		return INDEX_NONE;
	};

	const int32	ChunkCount			= ChunkLocations.Num();
	uint32		CorruptChunkCount	= 0;
	uint32		MissingChunkCount	= 0;
	uint32		ReadErrorCount		= 0;
	uint64		TotalVerifiedBytes	= 0;
	FIoBuffer	Chunk(1 << 20);

	if (ChunkCount == 0)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Verify skipped, install cache is empty"));
		return FIoStatus::Ok;
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Verifying %d installed chunks..."), ChunkCount);
	for (int32 ChunkIndex = 0; const FCasAddrLocation& ChunkLocation : ChunkLocations)
	{
		FSharedFileOpenResult OpenResult = Cas.OpenRead(ChunkLocation.Location.BlockId);
		if (OpenResult.HasError())
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to open block %u for reading"), ChunkLocation.Location.BlockId.Id);

			ReadErrorCount++;
			ChunkIndex++;
			continue;
		}

		int32 ContainerIndex	= INDEX_NONE;
		int32 EntryIndex		= FindChunkEntry(ChunkLocation.Addr, ContainerIndex);

		if (EntryIndex == INDEX_NONE)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to find chunk entry for CAS adress '%s'"), *LexToString(ChunkLocation.Addr));

			MissingChunkCount++;
			ChunkIndex++;
			continue;
		}

		const FSharedOnDemandContainer& Container	= Containers[ContainerIndex];
		const FIoChunkId& ChunkId					= Container->ChunkIds[EntryIndex];
		const FOnDemandChunkEntry& ChunkEntry		= Container->ChunkEntries[EntryIndex];
		FSharedFileHandle FileHandle				= OpenResult.GetValue();
		const int64 ChunkSize						= Align(int64(ChunkEntry.EncodedSize), FAES::AESBlockSize);
		TotalVerifiedBytes							+= ChunkSize;

		if (int64(Chunk.GetSize()) < ChunkSize)
		{
			Chunk = FIoBuffer(ChunkSize);
		}

		if (FileHandle->Seek(int64(ChunkLocation.Location.BlockOffset)) == false)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Chunk %d/%d SEEK FAILED, Container='%s', ChunkId='%s', ChunkSize=%lld, Hash='%s', Block=%u, BlockOffset=%u"),
				ChunkIndex + 1, ChunkCount, *Container->Name, *LexToString(ChunkId), ChunkSize, *LexToString(ChunkEntry.Hash),
				ChunkLocation.Location.BlockId.Id, ChunkLocation.Location.BlockOffset);

			ReadErrorCount++;
			ChunkIndex++;
			continue;
		}

		if (FileHandle->Read(reinterpret_cast<uint8*>(Chunk.GetData()), ChunkSize) == false)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Chunk %d/%d READ FAILED, Container='%s', ChunkId='%s', ChunkSize=%lld, Hash='%s', Block=%u, BlockOffset=%u"),
				ChunkIndex + 1, ChunkCount, *Container->Name, *LexToString(ChunkId), ChunkSize, *LexToString(ChunkEntry.Hash),
				ChunkLocation.Location.BlockId.Id, ChunkLocation.Location.BlockOffset);

			ReadErrorCount++;
			ChunkIndex++;
			continue;
		}

		const FIoHash ChunkHash = FIoHash::HashBuffer(Chunk.GetView().Left(ChunkSize));

		if (ChunkHash == ChunkEntry.Hash)
		{
			UE_LOG(LogIoStoreOnDemand, VeryVerbose, TEXT("Chunk %d/%d OK, Container='%s', ChunkId='%s', ChunkSize=%lld, Hash='%s', Block=%u, BlockOffset=%u"),
				ChunkIndex + 1, ChunkCount, *Container->Name, *LexToString(ChunkId), ChunkSize, *LexToString(ChunkEntry.Hash),
				ChunkLocation.Location.BlockId.Id, ChunkLocation.Location.BlockOffset);
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Chunk %d/%d CORRUPT, Container='%s', ChunkId='%s', ChunkSize=%lld, Hash='%s', ActualHash='%s', Block=%u, BlockOffset=%u"),
				ChunkIndex + 1, ChunkCount, *Container->Name, *LexToString(ChunkId), ChunkSize, *LexToString(ChunkEntry.Hash), *LexToString(ChunkHash),
				ChunkLocation.Location.BlockId.Id, ChunkLocation.Location.BlockOffset);

			CorruptChunkCount++;
		}

		ChunkIndex++;
	}

	if (CorruptChunkCount > 0 || MissingChunkCount > 0 || ReadErrorCount > 0)
	{
		const FString Reason = FString::Printf(TEXT("Verify install cache failed, Corrupt=%u, Missing=%u, ReadErrors=%u"),
			CorruptChunkCount, MissingChunkCount, ReadErrorCount);

		if (CorruptChunkCount > 0 || ReadErrorCount > 0)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("%s"), *Reason);
		}
		else
		{
			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("%s"), *Reason);
		}

		if (CorruptChunkCount > 0)
		{
			return FIoStatus(EIoErrorCode::SignatureError);
		}

		if (ReadErrorCount > 0)
		{
			return FIoStatus(EIoErrorCode::ReadError);
		}

		return FIoStatus(EIoErrorCode::NotFound);
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Successfully verified %d chunk(s) of total %.2lf MiB"),
		ChunkCount, ToMiB(TotalVerifiedBytes));

	return FIoStatus::Ok;
}

void FOnDemandInstallCache::RegisterConsoleCommands()
{
#if UE_IAD_DEBUG_CONSOLE_CMDS
	ConsoleCommands.Emplace(
	IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("iostore.SimulateCriticalInstallCacheError"),
		TEXT(""),
		FConsoleCommandDelegate::CreateLambda([this]()
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Simulating critical install cache error"));

			FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());
			Transaction.CriticalError(FCasJournal::EErrorCode::Simulated);
			if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); Status.IsOk() == false)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Failed to append critical error to journal, reason '%s'"),
					*Status.ToString());
			}
		}),
		ECVF_Default)
	);
#endif // UE_IAD_DEBUG_CONSOLE_CMDS
}

FIoStatus FOnDemandInstallCache::Reset()
{
	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Resetting install cache in directory '%s'"), *CacheDirectory);

	IFileManager& Ifm	= IFileManager::Get();
	const bool bTree	= true;

	if (Ifm.DeleteDirectory(*CacheDirectory, false, bTree) == false)
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to delete directory '")
			<< CacheDirectory
			<< TEXT("'");
	}

	if (Ifm.MakeDirectory(*CacheDirectory, bTree) == false)
	{
		return FIoStatusBuilder(EIoErrorCode::WriteError)
			<< TEXT("Failed to create directory '")
			<< CacheDirectory
			<< TEXT("'");
	}

	if (FIoStatus Status = Cas.Initialize(CacheDirectory); Status.IsOk() == false)
	{
		return Status;
	}

	const FString JournalFile = GetJournalFilename();
	if (FIoStatus Status = FCasJournal::Create(JournalFile); Status.IsOk() == false)
	{
		return Status;
	}

	UE_LOG(LogIoStoreOnDemand, Log, TEXT("Created CAS journal '%s'"), *JournalFile);
	return FIoStatus::Ok;
}

FIoStatus FOnDemandInstallCache::InitialVerify()
{
	// Verify the blocks on disk with the current state of the CAS
	{
		TArray<FCasAddr> RemovedChunks;
		if (FIoStatus Verify = Cas.Verify(RemovedChunks); Verify.IsOk() == false)
		{
			FOnDemandInstallCacheStats::OnCasVerificationError(RemovedChunks.Num());

			// Remove all entries that doesn't have a valid cache block 
			FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());
			for (const FCasAddr& Addr : RemovedChunks)
			{
				Transaction.ChunkLocation(FCasLocation::Invalid, Addr);
			}

			if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); Status.IsOk() == false)
			{
				return Status;
			}
		}
	}

	// Check if the cache is over budget
	{
		FCasBlockInfoMap	BlockInfo;
		uint64				CacheSize = Cas.GetBlockInfo(BlockInfo);

		if (CacheSize > MaxCacheSize)
		{
			const uint64	TotalBytesToPurge = CacheSize - MaxCacheSize;
			uint64			TotalPurgedBytes = 0;

			UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Cache size is greater than disk quota - Purging install cache, MaxCacheSize=%.2lf MiB, TotalSize=%.2lf MiB, TotalBytesToPurge=%.2lf MiB"),
				ToMiB(MaxCacheSize), ToMiB(CacheSize), ToMiB(TotalBytesToPurge));

			FIoStatus PurgeStatus = Purge(BlockInfo, TotalBytesToPurge, TotalPurgedBytes);
			if (PurgeStatus.IsOk() && TotalPurgedBytes >= TotalBytesToPurge)
			{
				UE_LOG(LogIoStoreOnDemand, Warning, TEXT("Successfully purged %.2lf MiB from install cache"), ToMiB(TotalPurgedBytes));
			}
			else
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to purge %.2lf MiB from install cache. Actually purged %.2lf MiB from install cache"),
					ToMiB(TotalBytesToPurge), ToMiB(TotalPurgedBytes));

				return FIoStatusBuilder(EIoErrorCode::WriteError) << FString::Printf(TEXT("Failed to purge overbudget cache (%s)"), *PurgeStatus.ToString());
			}
		}
	}

	return FIoStatus::Ok;
}

uint64 FOnDemandInstallCache::AddReferencesToBlocks(
	const TArray<FSharedOnDemandContainer>& Containers,
	const TArray<TBitArray<>>& ChunkEntryIndices,
	const TSet<FIoHash>& ChunksToInstall,
	FCasBlockInfoMap& BlockInfoMap,
	uint64& OutTotalReferencedBytes) const
{
	uint64 TotalUncachedBytes = 0;
	OutTotalReferencedBytes	= 0;

	for (int32 Index = 0; FSharedOnDemandContainer Container : Containers)
	{
		const TBitArray<>& IsReferenced = ChunkEntryIndices[Index++];
		for (int32 EntryIndex = 0; const FOnDemandChunkEntry& Entry : Container->ChunkEntries)
		{
			const bool bToInstall = ChunksToInstall.Contains(Entry.Hash);
			const bool bIsReferenced = IsReferenced[EntryIndex];

			uint64 ChunkDiskSize = Align(int64(Entry.EncodedSize), FAES::AESBlockSize);

			if (bIsReferenced)
			{
				OutTotalReferencedBytes += ChunkDiskSize;
			}

			FCasBlockInfo* BlockInfo = nullptr;
			if (bToInstall || bIsReferenced)
			{
				if (FCasLocation Loc = Cas.FindChunk(Entry.Hash); Loc.IsValid())
				{
					BlockInfo = BlockInfoMap.Find(Loc.BlockId);
					if (!BlockInfo)
					{
						UE_CLOG(bIsReferenced, LogIoStoreOnDemand, Error, TEXT("Failed to find CAS block info for referenced chunk, ChunkId='%s', Container='%s'"),
							*LexToString(Container->ChunkIds[EntryIndex]), *Container->Name);
					}
				}
				else
				{
					UE_CLOG(bIsReferenced, LogIoStoreOnDemand, Error, TEXT("Failed to find CAS location for referenced chunk, ChunkId='%s', Container='%s'"),
						*LexToString(Container->ChunkIds[EntryIndex]), *Container->Name);
				}
			}

			if (BlockInfo)
			{
				BlockInfo->RefSize += ChunkDiskSize;
			}
			else if (bToInstall)
			{
				TotalUncachedBytes += ChunkDiskSize;
			}

			EntryIndex++;
		}
	}

	return TotalUncachedBytes;
}

FIoStatus FOnDemandInstallCache::Purge(FCasBlockInfoMap& BlockInfo, const uint64 TotalBytesToPurge, uint64& OutTotalPurgedBytes)
{
	BlockInfo.ValueSort([](const FCasBlockInfo& LHS, const FCasBlockInfo& RHS)
	{
		return LHS.LastAccess < RHS.LastAccess;
	});

	OutTotalPurgedBytes = 0;

	for (auto It = BlockInfo.CreateIterator(); It; ++It)
	{
		const FCasBlockId BlockId = It->Key;
		const FCasBlockInfo& Info = It->Value;
		if (Info.RefSize > 0)
		{
			continue;
		}

		FCasJournal::FTransaction	Transaction = FCasJournal::Begin(GetJournalFilename());
		TArray<FCasAddr>			RemovedChunks;

		if (FIoStatus Status = Cas.DeleteBlock(BlockId, RemovedChunks); !Status.IsOk())
		{
			return Status;
		}

		// This should be the only thread writing to CurrentBlock
		FCasBlockId MaybeCurrentBlock = BlockId;
		CurrentBlock.compare_exchange_strong(MaybeCurrentBlock, FCasBlockId::Invalid);

		OutTotalPurgedBytes += Info.FileSize;

		It.RemoveCurrent();

		for (const FCasAddr& Addr : RemovedChunks)
		{
			Transaction.ChunkLocation(FCasLocation::Invalid, Addr);
		}
		Transaction.BlockDeleted(BlockId);

		if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); !Status.IsOk())
		{
			return Status;
		}
		
		if (OutTotalPurgedBytes >= TotalBytesToPurge)
		{
			break;
		}
	}

	return FIoStatus::Ok;
}

FIoStatus FOnDemandInstallCache::Defrag(
	const TArray<FSharedOnDemandContainer>& Containers,
	const TArray<TBitArray<>>& ChunkEntryIndices,
	FCasBlockInfoMap& BlockInfo, 
	const uint64* TotalBytesToFree /*= nullptr*/)
{
	if (TotalBytesToFree && *TotalBytesToFree == 0)
	{
		return FIoStatus::Ok;
	}

	const uint64 TotalCachedBytes = Algo::TransformAccumulate(BlockInfo,
		[](const TPair<FCasBlockId, FCasBlockInfo>& Kv) { return Kv.Value.FileSize; },
		uint64(0));

	if (TotalCachedBytes > MaxCacheSize)
	{
		// Ruh-Roh! There's not enough of the disk quota left to run a defrag!
		const FString ErrorMsg = FString::Printf(TEXT("Cache size is greater than disk quota - Cannot Defragment!, MaxCacheSize=%.2lf MiB, TotalCachedBytes=%.2lf MiB"), 
			ToMiB(MaxCacheSize), ToMiB(TotalCachedBytes));
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("%s"), *ErrorMsg);
		FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::WriteError, 0);

		// Append a critical error entry to clear the cache at next startup
		FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());
		Transaction.CriticalError(FCasJournal::EErrorCode::DefragOutOfDiskSpace);
		if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); Status.IsOk() == false)
		{
			UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to append critical error to journal, reason '%s'"),
				*Status.ToString());
		}

		return FIoStatusBuilder(EIoErrorCode::WriteError) << ErrorMsg;
	}

	struct FDefragBlockReferencedChunk
	{
		uint32 BlockOffset = 0;
		uint32 EncodedSize = 0;
		FIoHash Hash;
	};

	struct FDefragBlock
	{
		FCasBlockId BlockId;
		int64 LastAccess = 0;
		TArray<FDefragBlockReferencedChunk> ReferencedChunks;
	};

	// Build the list of blocks to defrag and determine if its possible to free enough data through defragging
	TArray<FDefragBlock> BlocksToDefrag;
	
	// Start with the least referenced blocks
	BlockInfo.ValueSort([](const FCasBlockInfo& LHS, const FCasBlockInfo& RHS)
	{
		return LHS.RefSize < RHS.RefSize;
	});

	uint64 FragmentedBytes = 0;
	uint64 TotalBlockSize = 0;

	if (TotalBytesToFree)
	{
		// Partial defrag
		bool bPossibleToFreeBytes = false;

		uint64 FreedBlockBytes = 0;
		uint64 NewBlockBytes = 0;

		for (const TPair<FCasBlockId, FCasBlockInfo>& Kv : BlockInfo)
		{
			const FCasBlockId BlockId = Kv.Key;
			const FCasBlockInfo& Info = Kv.Value;

			if (!bPossibleToFreeBytes && Info.RefSize < Info.FileSize)
			{
				// Block is fragmented
				FragmentedBytes += (Info.FileSize - Info.RefSize);
				TotalBlockSize += Info.FileSize;

				FreedBlockBytes += Info.FileSize;
				NewBlockBytes += Info.RefSize; // For now, assume that nothing will be moved to the current block

				BlocksToDefrag.Add(FDefragBlock{ .BlockId = BlockId, .LastAccess = Info.LastAccess });

				if (FreedBlockBytes >= NewBlockBytes && FreedBlockBytes - NewBlockBytes >= *TotalBytesToFree)
				{
					bPossibleToFreeBytes = true;
				}
			}
			else if (Info.FileSize < Cas.MinBlockSize)
			{
				// Block is too small whether or not its fragmented
				if (ensure(Info.RefSize <= Info.FileSize))
				{
					FragmentedBytes += (Info.FileSize - Info.RefSize);
				}
				
				TotalBlockSize += Info.FileSize;

				BlocksToDefrag.Add(FDefragBlock{ .BlockId = BlockId, .LastAccess = Info.LastAccess });
			}
		}

		if (!bPossibleToFreeBytes)
		{
			FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::WriteError, 0);
			return FIoStatusBuilder(EIoErrorCode::WriteError) << FString::Printf(TEXT("Defrag failed - cannot free %" UINT64_FMT), *TotalBytesToFree);
		}
	}
	else
	{
		// Full defrag
		for (const TPair<FCasBlockId, FCasBlockInfo>& Kv : BlockInfo)
		{
			const FCasBlockId BlockId = Kv.Key;
			const FCasBlockInfo& Info = Kv.Value;

			if (Info.RefSize < Info.FileSize)
			{
				// Block is fragmented
				FragmentedBytes += (Info.FileSize - Info.RefSize);
				TotalBlockSize += Info.FileSize;

				BlocksToDefrag.Add(FDefragBlock{ .BlockId = BlockId, .LastAccess = Info.LastAccess });
			}
			else if (Info.FileSize < Cas.MinBlockSize)
			{
				// Block is too small whether or not its fragmented
				if (ensure(Info.RefSize <= Info.FileSize))
				{
					FragmentedBytes += (Info.FileSize - Info.RefSize);
				}

				TotalBlockSize += Info.FileSize;

				BlocksToDefrag.Add(FDefragBlock{ .BlockId = BlockId, .LastAccess = Info.LastAccess });
			}
		}

		if (BlocksToDefrag.IsEmpty())
		{
			// Already defragged
			UE_LOG(LogIoStoreOnDemand, Display, TEXT("Cache not fragmented."));
			return FIoStatus::Ok;
		}
	}

	UE_LOG(LogIoStoreOnDemand, Display, TEXT("Defrag found %" UINT64_FMT " fragmented bytes of %" UINT64_FMT " total bytes in %i blocks."), FragmentedBytes, TotalBlockSize, BlocksToDefrag.Num());

	// Right now, don't allow moving chunks to the current block for defrag. Its somewhat dangerous and hard to reason about.
	// - Currently, the slack in the current block cannot be determined without opening a write handle to the block.
	// - If we defrag the current block itself, then we would need additional tracking so we don't lose any chunks moved into it.
	// - Additionally, this would also depend on the order blocks are defragged.
	// This should be the only thread writing to CurrentBlock.
	CurrentBlock = FCasBlockId::Invalid;

	// Determine chunks that need to be moved for each defrag block
	for (int32 Index = 0; FSharedOnDemandContainer Container : Containers)
	{
		const TBitArray<>& IsReferenced = ChunkEntryIndices[Index++];
		for (int32 EntryIndex = 0; const FOnDemandChunkEntry& Entry : Container->ChunkEntries)
		{
			if (bool bIsReferenced = IsReferenced[EntryIndex++]; bIsReferenced == false)
			{
				continue;
			}

			if (FCasLocation Loc = Cas.FindChunk(Entry.Hash); Loc.IsValid())
			{
				if (FDefragBlock* DefragBlock = Algo::FindBy(BlocksToDefrag, Loc.BlockId, &FDefragBlock::BlockId))
				{
					DefragBlock->ReferencedChunks.Add(FDefragBlockReferencedChunk
					{
						.BlockOffset = Loc.BlockOffset,
						.EncodedSize = Entry.EncodedSize,
						.Hash = Entry.Hash,
					});
				}
			}
		}
	}

	// Move chunks to new blocks and delete old blocks
	FPendingChunks DefragPendingChunks;
	for (const FDefragBlock& DefragBlock : BlocksToDefrag)
	{
		if (DefragBlock.ReferencedChunks.IsEmpty() == false)
		{
			FSharedFileOpenResult FileOpenResult = Cas.OpenRead(DefragBlock.BlockId);
			if (!FileOpenResult.IsValid())
			{
				const FString Filename = Cas.GetBlockFilename(DefragBlock.BlockId);
				const FString ErrorMsg = FileOpenResult.GetError().GetMessage();
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to open CAS block '%s' for reading, reason '%s'"), *Filename, *ErrorMsg);
				FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::FileOpenFailed, 0);
				return FIoStatusBuilder(EIoErrorCode::FileOpenFailed) << ErrorMsg;
			}

			FSharedFileHandle FileHandle = FileOpenResult.StealValue();

			Algo::SortBy(DefragBlock.ReferencedChunks, &FDefragBlockReferencedChunk::BlockOffset);

			for (const FDefragBlockReferencedChunk& ReffedChunk : DefragBlock.ReferencedChunks)
			{
				FileHandle->Seek(ReffedChunk.BlockOffset);

				const int64 ChunkDiskSize = Align(int64(ReffedChunk.EncodedSize), FAES::AESBlockSize);
				FIoBuffer Buffer(ChunkDiskSize);
				const bool bOk = FileHandle->Read(Buffer.GetData(), Buffer.GetSize());
				if (!bOk)
				{
					FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::ReadError, 0);
					return EIoErrorCode::ReadError;
				}

				const FIoHash ChunkHash = FIoHash::HashBuffer(Buffer.GetView());
				if (ChunkHash != ReffedChunk.Hash)
				{
					UE_LOG(LogIoStoreOnDemand, Error, TEXT("Found chunk with invalid hash while defragging block, BlockId=%u, BlockOffset=%u"),
						DefragBlock.BlockId.Id, ReffedChunk.BlockOffset);

					// Append a critical error entry to clear the cache at next startup
					FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());
					Transaction.CriticalError(FCasJournal::EErrorCode::DefragHashMismatch);
					if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); Status.IsOk() == false)
					{
						UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to append critical error to journal, reason '%s'"),
							*Status.ToString());
					}

					if (FIoStatus Status = FlushPendingChunks(DefragPendingChunks); Status.IsOk() == false)
					{
						FOnDemandInstallCacheStats::OnDefrag(Status.GetErrorCode(), 0);
						return Status;
					}
	
					FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::SignatureError, 0);
					return EIoErrorCode::SignatureError;
				}

				if (DefragPendingChunks.TotalSize > FPendingChunks::MaxPendingBytes)
				{
					if (FIoStatus Status = FlushPendingChunks(DefragPendingChunks, DefragBlock.LastAccess); Status.IsOk() == false)
					{
						FOnDemandInstallCacheStats::OnDefrag(Status.GetErrorCode(), 0);
						return Status;
					}
					check(DefragPendingChunks.IsEmpty());
				}

				DefragPendingChunks.Append(MoveTemp(Buffer), ReffedChunk.Hash);
			}

			FileHandle.Reset();

			if (FIoStatus Status = FlushPendingChunks(DefragPendingChunks); Status.IsOk() == false)
			{
				FOnDemandInstallCacheStats::OnDefrag(Status.GetErrorCode(), 0);
				return Status;
			}
			check(DefragPendingChunks.IsEmpty());
		}

		FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());

		// Flushing should overwrite the lookup info for the cas addr to point at the new block.
		// Can now remove the old block
		TArray<FCasAddr> DeletedChunkAddresses;
		Cas.DeleteBlock(DefragBlock.BlockId, DeletedChunkAddresses);

		for (const FCasAddr& Addr : DeletedChunkAddresses)
		{
			Transaction.ChunkLocation(FCasLocation::Invalid, Addr);
		}
		Transaction.BlockDeleted(DefragBlock.BlockId);

		if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); !Status.IsOk())
		{
			FOnDemandInstallCacheStats::OnDefrag(Status.GetErrorCode(), 0);
			return Status;
		}
	}

	UE_LOG(LogIoStoreOnDemand, Display, TEXT("Defrag removed %" UINT64_FMT " fragmented bytes of %" UINT64_FMT " total bytes in %i blocks."), FragmentedBytes, TotalBlockSize, BlocksToDefrag.Num());

	FOnDemandInstallCacheStats::OnDefrag(EIoErrorCode::Ok, FragmentedBytes);

	return FIoStatus::Ok;
}

FIoStatus FOnDemandInstallCache::Flush()
{
	if (PendingChunks.IsValid())
	{
		FUniquePendingChunks Chunks = MoveTemp(PendingChunks);
		return FlushPendingChunks(*Chunks);
	}

	Cas.Compact();
	return FIoStatus::Ok;
}

FOnDemandInstallCacheUsage FOnDemandInstallCache::GetCacheUsage()
{
	// If this is called from a thread other than the OnDemandIoStore tick thread
	// then its possible the block info and containers may not be in sync with each other
	// or the current state of the tick thread.
	// This should only be used for debugging and telemetry purposes.

	FCasBlockInfoMap	BlockInfo;
	const uint64		TotalCachedBytes = Cas.GetBlockInfo(BlockInfo);

	TArray<FSharedOnDemandContainer>	Containers;
	TArray<TBitArray<>>					ChunkEntryIndices;
	IoStore.GetReferencedContent(Containers, ChunkEntryIndices);
	check(Containers.Num() == ChunkEntryIndices.Num());

	uint64 ReferencedBytes = 0;
	AddReferencesToBlocks(Containers, ChunkEntryIndices, {}, BlockInfo, ReferencedBytes);

	uint64 FragmentedBytes = 0;
	uint64 ReferencedBlockBytes = 0;
	for (const TPair<FCasBlockId, FCasBlockInfo>& Kv : BlockInfo)
	{
		const FCasBlockId BlockId = Kv.Key;
		const FCasBlockInfo& Info = Kv.Value;

		if (Info.RefSize < Info.FileSize)
		{
			FragmentedBytes += (Info.FileSize - Info.RefSize);
		}

		if (Info.RefSize > 0)
		{
			ReferencedBlockBytes += Info.FileSize;
		}
	}

	return FOnDemandInstallCacheUsage
	{
		.MaxSize = MaxCacheSize,
		.TotalSize = TotalCachedBytes,
		.ReferencedBlockSize = ReferencedBlockBytes,
		.ReferencedSize = ReferencedBytes,
		.FragmentedChunksSize = FragmentedBytes,
	};
}

FIoStatus FOnDemandInstallCache::FlushPendingChunks(FPendingChunks& Chunks, int64 UtcAccessTicks)
{
	if (Chunks.IsEmpty())
	{
		return FIoStatus::Ok;
	}

#if UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
	UE::Tasks::TTask<FIoStatus> Task = ExclusivePipe.Launch(UE_SOURCE_LOCATION, [this, &Chunks, UtcAccessTicks]
	{
		return FlushPendingChunksImpl(Chunks, UtcAccessTicks);
	}, UE::Tasks::ETaskPriority::BackgroundHigh);

	Task.Wait();

	return Task.GetResult();

#else
	return FlushPendingChunksImpl(Chunks, UtcAccessTicks);
#endif // UE_ONDEMANDINSTALLCACHE_EXCLUSIVE_WRITE
}

FIoStatus FOnDemandInstallCache::FlushPendingChunksImpl(FPendingChunks& Chunks, int64 UtcAccessTicks)
{
	ON_SCOPE_EXIT { Chunks.Reset(); };

	// This should be the only thread writing to CurrentBlock
	FCasBlockId CurrentBlockId = CurrentBlock;

	while (Chunks.IsEmpty() == false)
	{
		FCasJournal::FTransaction Transaction = FCasJournal::Begin(GetJournalFilename());
		
		if (CurrentBlockId.IsValid() == false)
		{
			CurrentBlockId = Cas.CreateBlock();
			ensure(CurrentBlockId.IsValid());
			CurrentBlock = CurrentBlockId;
			Transaction.BlockCreated(CurrentBlockId);
		}

		TUniquePtr<IFileHandle>	CasFileHandle = Cas.OpenWrite(CurrentBlockId);
		if (CasFileHandle.IsValid() == false)
		{
			FOnDemandInstallCacheStats::OnFlush(EIoErrorCode::WriteError, 0);
			return FIoStatusBuilder(EIoErrorCode::FileOpenFailed)
				<< TEXT("Failed to open cache block file '")
				<< Cas.GetBlockFilename(CurrentBlockId)
				<< TEXT("'");
		}

		const int64 CasBlockOffset = CasFileHandle->Tell();

		FLargeMemoryWriter	Ar(Chunks.TotalSize);
		TArray<FIoHash>		ChunkHashes;
		TArray<int64>		Offsets;

		while (Chunks.IsEmpty() == false)
		{
			if (CasBlockOffset > 0 && CasBlockOffset + Ar.Tell() + Chunks.Chunks[0].GetSize() > Cas.MaxBlockSize)
			{
				break;
			}
			FIoBuffer Chunk = Chunks.Pop(ChunkHashes.AddDefaulted_GetRef());
			Offsets.Add(CasBlockOffset + Ar.Tell());
			Ar.Serialize(Chunk.GetData(), Chunk.GetSize());
		}

		if (Ar.Tell() > 0)
		{
			UE_LOG(LogIoStoreOnDemand, Log, TEXT("Writing %.2lf MiB to CAS block %u"),
				ToMiB(Ar.Tell()), CurrentBlockId.Id);

			if (CasFileHandle->Write(Ar.GetData(), Ar.Tell()) == false)
			{
				return FIoStatusBuilder(EIoErrorCode::WriteError)
					<< TEXT("Failed to serialize chunks to cache block");
			}

			if (UtcAccessTicks)
			{
				Cas.TrackAccessIfNewer(CurrentBlockId, UtcAccessTicks);
			}
			else
			{
				Cas.TrackAccess(CurrentBlockId);
			}

			if (CasFileHandle->Flush() == false)
			{
				FOnDemandInstallCacheStats::OnFlush(EIoErrorCode::WriteError, Ar.Tell());
				return FIoStatusBuilder(EIoErrorCode::WriteError)
					<< TEXT("Failed to flush cache block to disk");
			}

			FOnDemandInstallCacheStats::OnFlush(EIoErrorCode::Ok, Ar.Tell());

			check(ChunkHashes.Num() == Offsets.Num());
			check(CurrentBlockId.IsValid());
			for (int32 Idx = 0, Count = Offsets.Num(); Idx < Count; ++Idx)
			{
				const FCasAddr	CasAddr = FCasAddr::From(ChunkHashes[Idx]);
				const uint32	ChunkOffset = IntCastChecked<uint32>(Offsets[Idx]);

				FCasLocation& Loc = Cas.Lookup.FindOrAdd(CasAddr);
				Loc.BlockId	= CurrentBlockId;
				Loc.BlockOffset	= ChunkOffset;
				Transaction.ChunkLocation(Loc, CasAddr);
			}
		}

		if (FIoStatus Status = FCasJournal::Commit(MoveTemp(Transaction)); Status.IsOk() == false)
		{
			return Status;
		}

		if (Chunks.IsEmpty() == false)
		{
			CurrentBlockId = FCasBlockId::Invalid;
		}
	}

	return FIoStatus::Ok;
}

void FOnDemandInstallCache::CompleteRequest(FIoRequestImpl* Request, EIoErrorCode Status)
{
	if (Status == EIoErrorCode::Ok && !Request->IsCancelled())
	{
		FChunkRequest& ChunkRequest = FChunkRequest::GetRef(*Request);
		const FOnDemandChunkInfo& ChunkInfo = ChunkRequest.ChunkInfo;
		FIoBuffer EncodedChunk = MoveTemp(ChunkRequest.EncodedChunk);

		if (EncodedChunk.GetSize() > 0)
		{
			FIoChunkDecodingParams Params;
			Params.CompressionFormat = ChunkInfo.CompressionFormat();
			Params.EncryptionKey = ChunkInfo.EncryptionKey();
			Params.BlockSize = ChunkInfo.BlockSize();
			Params.TotalRawSize = ChunkInfo.RawSize();
			Params.RawOffset = Request->Options.GetOffset();
			Params.EncodedOffset = ChunkRequest.ChunkRange.GetOffset();
			Params.EncodedBlockSize = ChunkInfo.Blocks();
			Params.BlockHash = ChunkInfo.BlockHashes();

			Request->CreateBuffer(ChunkRequest.RawSize);
			FMutableMemoryView RawChunk = Request->GetBuffer().GetMutableView();

			if (FIoChunkEncoding::Decode(Params, EncodedChunk.GetView(), RawChunk) == false)
			{
				UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to decode chunk, ChunkId='%s'"), *LexToString(Request->ChunkId));
				Status = EIoErrorCode::CompressionError;
			}
		}
	}

	if (Status != EIoErrorCode::Ok)
	{
		Request->SetLastBackendError(Status);
		Request->SetResult(FIoBuffer());
		TRACE_IOSTORE_BACKEND_REQUEST_FAILED(Request);
	}
	else
	{
		TRACE_IOSTORE_BACKEND_REQUEST_COMPLETED(Request, Request->GetBuffer().GetSize());
	}

	{
		UE::TUniqueLock Lock(Mutex);
		CompletedRequests.AddTail(Request);
		FOnDemandInstallCacheStats::OnReadCompleted(Status);
	}

	BackendContext->WakeUpDispatcherThreadDelegate.Execute();
}

///////////////////////////////////////////////////////////////////////////////
TSharedPtr<IOnDemandInstallCache> MakeOnDemandInstallCache(
	FOnDemandIoStore& IoStore,
	const FOnDemandInstallCacheConfig& Config)
{
	IFileManager& Ifm = IFileManager::Get();
	if (Config.bDropCache)
	{
		UE_LOG(LogIoStoreOnDemand, Log, TEXT("Deleting install cache directory '%s'"), *Config.RootDirectory);
		Ifm.DeleteDirectory(*Config.RootDirectory, false, true);
	}

	const bool bTree = true;
	if (!Ifm.MakeDirectory(*Config.RootDirectory, bTree))
	{
		UE_LOG(LogIoStoreOnDemand, Error, TEXT("Failed to create directory '%s'"), *Config.RootDirectory);
		return TSharedPtr<IOnDemandInstallCache>();
	}

	return MakeShared<FOnDemandInstallCache>(Config, IoStore);
}

///////////////////////////////////////////////////////////////////////////////
#if WITH_IOSTORE_ONDEMAND_TESTS

class FTmpDirectoryScope
{
public:
	explicit FTmpDirectoryScope(const FString& InDir)
		: Ifm(IFileManager::Get())
		, Dir(InDir)
	{
		const bool bTree			= true;
		const bool bRequireExists	= false;
		Ifm.DeleteDirectory(*Dir, bRequireExists, bTree);
		Ifm.MakeDirectory(*Dir, bTree);
	}

	~FTmpDirectoryScope()
	{
		const bool bTree			= true;
		const bool bRequireExists	= false;
		Ifm.DeleteDirectory(*Dir, bRequireExists, bTree);
	}
private:
	IFileManager& Ifm;
	FString Dir;
};

FCasAddr CreateCasTestAddr(uint64 Value)
{
	return FCasAddr::From(reinterpret_cast<const uint8*>(&Value), sizeof(uint64));
}

TEST_CASE("IoStore::OnDemand::InstallCache::Journal", "[IoStoreOnDemand][InstallCache]")
{
	const FString TestBaseDir = TEXT("TestTmpDir");

	SECTION("CreateJournalFile")
	{
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");
		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());
	}

	SECTION("SimpleTransaction")
	{
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");
		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());

		FCasJournal::FTransaction Transaction = FCasJournal::Begin(JournalFile);
		Transaction.BlockCreated(FCasBlockId(1));
		Status = FCasJournal::Commit(MoveTemp(Transaction));
		CHECK(Status.IsOk());
	}

	SECTION("ReplayChunkLocations")
	{
		//Arrange
		TArray<FCasAddr>	ExpectedAddresses;
		TArray<uint32>		ExpectedBlockOffsets;
		const FCasBlockId	ExpectedBlockId(42);
		
		for (int32 Idx = 1; Idx < 33; ++Idx)
		{
			ExpectedAddresses.Add(FCasAddr::From(reinterpret_cast<const uint8*>(&Idx), sizeof(uint32)));
			ExpectedBlockOffsets.Add(Idx);
		}

		// Act
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");
		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());

		FCasJournal::FTransaction Transaction = FCasJournal::Begin(JournalFile);
		for (int32 Idx = 0; const FCasAddr& Addr : ExpectedAddresses)
		{
			Transaction.ChunkLocation(
				FCasLocation
				{
					.BlockId = ExpectedBlockId,
					.BlockOffset = ExpectedBlockOffsets[Idx]
				},
				Addr);
		}

		Status = FCasJournal::Commit(MoveTemp(Transaction));
		CHECK(Status.IsOk());

		// Assert
		TArray<FCasJournal::FEntry::FChunkLocation> Locs;
		Status = FCasJournal::Replay(
			JournalFile,
			[&Locs](const FCasJournal::FEntry& JournalEntry)
			{
				switch(JournalEntry.Type())
				{
				case FCasJournal::FEntry::EType::ChunkLocation:
				{
					Locs.Add(JournalEntry.ChunkLocation);
					break;
				}
				default:
					CHECK(false);
					break;
				};
			});
		CHECK(Status.IsOk());
		CHECK(Locs.Num() == ExpectedAddresses.Num());
		for (int32 Idx = 0; const FCasJournal::FEntry::FChunkLocation& Loc : Locs)
		{
			const FCasLocation ExpectedLoc = FCasLocation
			{
				.BlockId = ExpectedBlockId,
				.BlockOffset = uint32(Idx + 1)
			};
			CHECK(Loc.CasLocation.BlockId == ExpectedLoc.BlockId);
			CHECK(Loc.CasLocation.BlockOffset == ExpectedLoc.BlockOffset);
		}
	}

	SECTION("ReplayBlockCreatedAndDeleted")
	{
		// Arrange
		const FCasBlockId ExpectedBlockId(42);

		// Act
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");

		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());

		FCasJournal::FTransaction Tx = FCasJournal::Begin(JournalFile);
		Tx.BlockCreated(ExpectedBlockId);
		Tx.BlockDeleted(ExpectedBlockId);

		Status = FCasJournal::Commit(MoveTemp(Tx));
		CHECK(Status.IsOk());

		// Assert
		FCasBlockId CreatedBlockId;
		FCasBlockId DeletedBlockId;

		Status = FCasJournal::Replay(
			JournalFile,
			[&CreatedBlockId, &DeletedBlockId](const FCasJournal::FEntry& JournalEntry)
			{
				switch(JournalEntry.Type())
				{
				case FCasJournal::FEntry::EType::BlockCreated:
				{
					CreatedBlockId = JournalEntry.BlockOperation.BlockId;
					break;
				}
				case FCasJournal::FEntry::EType::BlockDeleted:
				{
					DeletedBlockId = JournalEntry.BlockOperation.BlockId;
					break;
				}
				default:
					CHECK(false);
					break;
				};
			});

		CHECK(Status.IsOk());
		CHECK(CreatedBlockId == ExpectedBlockId);
		CHECK(DeletedBlockId == ExpectedBlockId);
	}

	SECTION("ReplayBlockAccess")
	{
		// Arrange
		const FCasBlockId ExpectedBlockId(462);
		const uint64 ExpectedTicks = FDateTime::UtcNow().GetTicks();

		// Act
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");

		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());

		FCasJournal::FTransaction Tx = FCasJournal::Begin(JournalFile);
		Tx.BlockAccess(ExpectedBlockId, ExpectedTicks);

		Status = FCasJournal::Commit(MoveTemp(Tx));
		CHECK(Status.IsOk());

		// Assert
		FCasBlockId BlockId;
		uint64 Ticks = 0;

		Status = FCasJournal::Replay(
			JournalFile,
			[&BlockId, &Ticks](const FCasJournal::FEntry& JournalEntry)
			{
				switch(JournalEntry.Type())
				{
				case FCasJournal::FEntry::EType::BlockAccess:
				{
					const FCasJournal::FEntry::FBlockOperation& Op = JournalEntry.BlockOperation;
					BlockId	= Op.BlockId;
					Ticks	= Op.UtcTicks;
					break;
				}
				default:
					CHECK(false);
					break;
				};
			});

		CHECK(Status.IsOk());
		CHECK(BlockId == ExpectedBlockId);
		CHECK(Ticks == ExpectedTicks);
	}
}

TEST_CASE("IoStore::OnDemand::InstallCache::Snapshot", "[IoStoreOnDemand][InstallCache]")
{
	const FString TestBaseDir = TEXT("TestTmpDir");

	SECTION("SaveLoadRoundtrip")
	{
		// Arrange
		FCasSnapshot ExpectedSnapshot;

		for (uint32 Id = 1; Id <= 10; ++Id)
		{
			ExpectedSnapshot.Blocks.Add(FCasSnapshot::FBlock
			{
				.BlockId	= FCasBlockId(Id),
				.LastAccess = FDateTime::UtcNow().GetTicks()
			});

			for (uint32 Idx = 1; Idx <= 10; ++Idx)
			{
				FCasAddr CasAddr = CreateCasTestAddr(Idx);
				FCasLocation Loc = FCasLocation
				{
					.BlockId		= FCasBlockId(Id),
					.BlockOffset	= Idx * 256
				};
				ExpectedSnapshot.ChunkLocations.Emplace(CasAddr, Loc);
			}
		}
		ExpectedSnapshot.CurrentBlockId = FCasBlockId(1);

		// Act
		FTmpDirectoryScope _(TestBaseDir);
		const FString SnapshotFile = TestBaseDir / TEXT("test.snp");
		TIoStatusOr<int64> Status = FCasSnapshot::Save(ExpectedSnapshot, SnapshotFile);
		CHECK(Status.IsOk());
		const FCasSnapshot Snapshot = FCasSnapshot::Load(SnapshotFile).ConsumeValueOrDie();

		// Assert
		CHECK(Snapshot.Blocks.Num() == ExpectedSnapshot.Blocks.Num());
		for (int32 Idx = 0; Idx < Snapshot.Blocks.Num(); ++Idx)
		{
			CHECK(Snapshot.Blocks[Idx].BlockId == ExpectedSnapshot.Blocks[Idx].BlockId);
			CHECK(Snapshot.Blocks[Idx].LastAccess == ExpectedSnapshot.Blocks[Idx].LastAccess);
		}
		CHECK(Snapshot.ChunkLocations.Num() == ExpectedSnapshot.ChunkLocations.Num());
		for (int32 Idx = 0; Idx < Snapshot.ChunkLocations.Num(); ++Idx)
		{
			CHECK(Snapshot.ChunkLocations[Idx].Get<0>() == ExpectedSnapshot.ChunkLocations[Idx].Get<0>());
			CHECK(Snapshot.ChunkLocations[Idx].Get<1>() == ExpectedSnapshot.ChunkLocations[Idx].Get<1>());
		}
		CHECK(Snapshot.CurrentBlockId == ExpectedSnapshot.CurrentBlockId);
	}

	SECTION("CreateFromJournal")
	{
		// Arrange
		FTmpDirectoryScope _(TestBaseDir);
		const FString JournalFile = TestBaseDir / TEXT("test.jrn");
		const FCasBlockId ExpectedCurrentBlockId(2);

		FIoStatus Status = FCasJournal::Create(JournalFile);
		CHECK(Status.IsOk());

		FCasJournal::FTransaction Tx = FCasJournal::Begin(JournalFile);

		// Add a block and some chunk locations
		Tx.BlockCreated(FCasBlockId(1));
		for (int32 Idx = 1; Idx <= 10; ++Idx)
		{
			Tx.ChunkLocation(FCasLocation
			{
				.BlockId		= FCasBlockId(1),
				.BlockOffset	= 256
			},
			CreateCasTestAddr(uint64(Idx) << 32 | 1ull));
		}

		// Remove the block and the corresponding chunk locations
		for (int32 Idx = 1; Idx <= 10; ++Idx)
		{
			Tx.ChunkLocation(FCasLocation::Invalid, CreateCasTestAddr(uint64(Idx) << 32 | 1ull));
		}
		Tx.BlockDeleted(FCasBlockId(1));

		// Add a second block and some chunk locations
		Tx.BlockCreated(ExpectedCurrentBlockId);
		for (int32 Idx = 1; Idx <= 10; ++Idx)
		{
			Tx.ChunkLocation(FCasLocation
			{
				.BlockId		= ExpectedCurrentBlockId,
				.BlockOffset	= uint32(Idx) * 256
			},
			CreateCasTestAddr(Idx));
		}

		Status = FCasJournal::Commit(MoveTemp(Tx));
		CHECK(Status.IsOk());

		// Act
		const FCasSnapshot Snapshot = FCasSnapshot::FromJournal(JournalFile).ConsumeValueOrDie();

		// Assert
		CHECK(Snapshot.CurrentBlockId == ExpectedCurrentBlockId);
		CHECK(Snapshot.Blocks.Num() == 1); 
		CHECK(Snapshot.ChunkLocations.Num() == 10); 
		for (int32 Idx = 1; Idx < Snapshot.ChunkLocations.Num(); ++Idx)
		{
			const FCasAddr Addr = CreateCasTestAddr(Idx);
			const FCasSnapshot::FChunkLocation* Loc =
				Algo::FindByPredicate(
					Snapshot.ChunkLocations,
					[&Addr](const FCasSnapshot::FChunkLocation& L) { return L.Get<0>() == Addr; });
			CHECK(Loc != nullptr);
			if (Loc != nullptr)
			{
				CHECK(Loc->Get<1>().BlockId == ExpectedCurrentBlockId);
				CHECK(Loc->Get<1>().BlockOffset == uint32(Idx) * 256); 
			}
		}
	}
}

#endif // WITH_IOSTORE_ONDEMAND_TESTS

} // namespace UE::IoStore
