// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/ChunkDbChunkSource.h"
#include "Misc/Guid.h"
#include "Async/Async.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Algo/Transform.h"
#include "Core/Platform.h"
#include "Common/FileSystem.h"
#include "Common/StatsCollector.h"
#include "Data/ChunkData.h"
#include "Installer/ChunkStore.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/MessagePump.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerSharedContext.h"
#include "Memory/SharedBuffer.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Tasks/Task.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "Misc/Compression.h"
#include "Compression/OodleDataCompressionUtil.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
namespace ChunkDbSourceHelpers
{
	bool DisableOsIntervention(uint32& OutPreviousOsIntervention)
	{
		// This prevents dialogs from popping up if we try to read a chunkdb from a removable drive
		// with no media. 

		// We only call this during startup on a single thread so we can limit this to 
		// just our thread.				
		return ::SetThreadErrorMode(SEM_FAILCRITICALERRORS, (DWORD*)&OutPreviousOsIntervention);		
	}

	void ResetOsIntervention(uint32 Previous)
	{
		::SetThreadErrorMode((DWORD)Previous, NULL);
	}
}
#else
namespace ChunkDbSourceHelpers
{
	bool DisableOsIntervention(uint32& OutPreviousOsIntervention)
	{
		OutPreviousOsIntervention = 0;
		return false;
	}

	void ResetOsIntervention(uint32 Previous)
	{
	}
}
#endif

DEFINE_LOG_CATEGORY_STATIC(LogChunkDbChunkSource, Log, All);

namespace BuildPatchServices
{
	/**
	 * Struct holding variables for accessing a chunkdb file's data.
	 */
	struct FChunkDbDataAccess
	{
		FChunkDatabaseHeader Header;
		TUniquePtr<FArchive> Archive;
		FString ChunkDbFileName;

		// When the reference tracker gets below this watermark, then we know we are done with this file and we can
		// close/retire it.
		int32 RetireAt = 0;

		// If we're retired then any access is invalid and fatal as the file has been closed and could be deleted.
		bool bIsRetired = false;

		void Retire(IFileSystem* FileSystemIfDeleting, bool bDelete)
		{
			bIsRetired = true;
			FString ArchiveName = Archive->GetArchiveName();
			Archive.Reset();

			if (bDelete && FileSystemIfDeleting)
			{
				if (!FileSystemIfDeleting->DeleteFile(*ChunkDbFileName))
				{
					GLog->Logf(TEXT("Failed to delete chunkdb upon retirement: %s"), *ChunkDbFileName);
				}
			}
		}
	};

	// Holds where to get the chunk data from. File + location in file.
	struct FChunkAccessLookup
	{
		FChunkLocation* Location;
		FChunkDbDataAccess* DbFile;
	};

	class FChunkDbChunkSource : public IConstructorChunkDbChunkSource
	{
	public:
		FChunkDbChunkSource(FChunkDbSourceConfig Configuration, IFileSystem* FileSystem, const TArray<FGuid>& ChunkAccessOrderList, IChunkDataSerialization* ChunkDataSerialization, IChunkDbChunkSourceStat* ChunkDbChunkSourceStat);
		~FChunkDbChunkSource()
		{
		}

		virtual void ReportFileCompletion(int32 RemainingChunkCount) override
		{
			//
			// Since we've completed a file we know we won't need to resume/retry it and can delete
			// the source chunkdb that it used.
			//
			for (FChunkDbDataAccess& ChunkDbDataAccess : ChunkDbDataAccesses)
			{
				if (!ChunkDbDataAccess.bIsRetired &&
					ChunkDbDataAccess.RetireAt > RemainingChunkCount)
				{
					ChunkDbDataAccess.Retire(FileSystem, Configuration.bDeleteChunkDBAfterUse);
				}
			}
		}
		virtual int32 GetChunkUnavailableAt(const FGuid& DataId) const override
		{
			// While technically the chunks retire as a result of delete-during-install, we only do this
			// when they aren't needed any more, so we can set this to "never retires"
			return TNumericLimits<int32>::Max();
		}

		virtual FRequestProcessFn CreateRequest(const FGuid& DataId, FMutableMemoryView DestinationBuffer, void* UserPtr, FChunkRequestCompleteDelegate CompleteFn) override;
		virtual const TSet<FGuid>& GetAvailableChunks() const override 
		{
			return AvailableChunks; 
		}

		virtual uint64 GetChunkDbSizesAtIndexes(const TArray<int32>& FileCompletionIndexes, TArray<uint64>& OutChunkDbSizesAtCompletion) const override;

		static void LoadChunkDbFiles(
			const TArray<FString>& ChunkDbFiles, IFileSystem* FileSystem, const TArray<FGuid>& ChunkAccessOrderedList,
			TArray<FChunkDbDataAccess>& OutChunkDbDataAccesses, TMap<FGuid, FChunkAccessLookup>& OutChunkDbDataAccessLookup, TSet<FGuid>* OutOptionalAvailableStore);

	private:

		// Configuration.
		const FChunkDbSourceConfig Configuration;
		// Dependencies.
		IFileSystem* FileSystem = nullptr;
		IChunkReferenceTracker* ChunkReferenceTracker = nullptr;
		IChunkDataSerialization* ChunkDataSerialization = nullptr;
		IChunkDbChunkSourceStat* ChunkDbChunkSourceStat = nullptr;
		// Storage of our chunkdb and enumerated available chunks lookup.
		TArray<FChunkDbDataAccess> ChunkDbDataAccesses;
		TMap<FGuid, FChunkAccessLookup> ChunkDbDataAccessLookup;
		TSet<FGuid> AvailableChunks;

		// Number of chunks to process in this manifest when we started.
		int32 OriginalChunkCount = 0;
	};

	// Read in the headers, evalutate the list of chunks, and determine when we'll be done with our chunk dbs.
	void FChunkDbChunkSource::LoadChunkDbFiles(
		const TArray<FString>& ChunkDbFiles, IFileSystem* FileSystem, const TArray<FGuid>& ChunkAccessOrderedList, 
		TArray<FChunkDbDataAccess>& OutChunkDbDataAccesses, TMap<FGuid, FChunkAccessLookup>& OutChunkDbDataAccessLookup, TSet<FGuid>* OutOptionalAvailableStore)
	{

		// Allow OS intervention only once.
		bool bResetOsIntervention = false;
		bool bHasPreviousOsIntervention = false;
		uint32 PreviousOsIntervention = 0;
		// Load each chunkdb's TOC to enumerate available chunks.
		for (const FString& ChunkDbFilename : ChunkDbFiles)
		{
			TUniquePtr<FArchive> ChunkDbFile(FileSystem->CreateFileReader(*ChunkDbFilename));
			if (ChunkDbFile.IsValid())
			{
				// Load header.
				FChunkDatabaseHeader Header;
				*ChunkDbFile << Header;
				if (ChunkDbFile->IsError())
				{
					GLog->Logf(TEXT("Failed to load chunkdb header for %s"), *ChunkDbFilename);
				}
				else if (Header.Contents.Num() == 0)
				{
					GLog->Logf(TEXT("Loaded empty chunkdb %s"), *ChunkDbFilename);
				}
				else
				{
					// Hold on to the handle and header info.
					FChunkDbDataAccess DataSource;
					DataSource.Archive = MoveTemp(ChunkDbFile);
					DataSource.ChunkDbFileName = ChunkDbFilename;
					DataSource.Header = MoveTemp(Header);
					OutChunkDbDataAccesses.Add(MoveTemp(DataSource));
				}
			}
			else if (!bResetOsIntervention)
			{
				bResetOsIntervention = true;
				bHasPreviousOsIntervention = ChunkDbSourceHelpers::DisableOsIntervention(PreviousOsIntervention);
			}
		}
		// Reset OS intervention if we disabled it.
		if (bResetOsIntervention && bHasPreviousOsIntervention)
		{
			ChunkDbSourceHelpers::ResetOsIntervention(PreviousOsIntervention);
		}

		// Index all chunks to their location info.
		for (FChunkDbDataAccess& ChunkDbDataAccess : OutChunkDbDataAccesses)
		{
			for (FChunkLocation& ChunkLocation : ChunkDbDataAccess.Header.Contents)
			{
				if (!OutChunkDbDataAccessLookup.Contains(ChunkLocation.ChunkId))
				{
					OutChunkDbDataAccessLookup.Add(ChunkLocation.ChunkId, { &ChunkLocation, &ChunkDbDataAccess });

					if (OutOptionalAvailableStore)
					{
						OutOptionalAvailableStore->Add(ChunkLocation.ChunkId);
					}
				}
			}
		}

		TMap<FString, int32> FileLastSeenAt;

		int32 GuidIndex = 0;
		for (; GuidIndex < ChunkAccessOrderedList.Num(); GuidIndex++)
		{
			const FGuid& Guid = ChunkAccessOrderedList[GuidIndex];

			FChunkAccessLookup* SourceForGuid = OutChunkDbDataAccessLookup.Find(Guid);
			if (!SourceForGuid)
			{
				continue;
			}

			FileLastSeenAt.FindOrAdd(SourceForGuid->DbFile->Archive->GetArchiveName()) = GuidIndex;
		}

		for (FChunkDbDataAccess& ChunkDbDataAccess : OutChunkDbDataAccesses)
		{
			// Default to retired immediately (this is functionally after the first file completes)
			ChunkDbDataAccess.RetireAt = ChunkAccessOrderedList.Num();
			int32* LastAt = FileLastSeenAt.Find(ChunkDbDataAccess.Archive->GetArchiveName());
			if (LastAt)
			{
				// The reference stack gets popped rather than advanced, so we need to reverse the ordering.
				// LastAt is the chunk index that last used the file, which means when we are at
				// LastAt + 1 through the stack we can delete the file.
				ChunkDbDataAccess.RetireAt = ChunkAccessOrderedList.Num() - (*LastAt + 1);
			}
		}
	}
	
	// Get how many bytes of chunkdbs will still exist on disk after the given indexes (i.e. will not have been retired)
	// Return is the total size of given chunkdbs.
	static uint64 GetChunkDbSizesAtIndexesInternal(const TArray<FChunkDbDataAccess>& InOpenedChunkDbs, int32 InOriginalChunkCount, const TArray<int32>& InFileCompletionIndexes, TArray<uint64>& OutChunkDbSizesAtCompletion)
	{
		uint64 AllChunkDbSize = 0;
		for (const FChunkDbDataAccess& ChunkFile : InOpenedChunkDbs)
		{
			if (!ChunkFile.bIsRetired)
			{
				AllChunkDbSize += ChunkFile.Archive->TotalSize();
			}
		}
		
		// Go over the list of completions and evalute how many chunk dbs are left over.
		for (int32 FileCompletionIndex : InFileCompletionIndexes)
		{
			// retiring happens as the list is _popped_, so everything is backwards.
			int32 RetireAtEquivalent = InOriginalChunkCount - FileCompletionIndex;

			uint64 TotalSizeAtIndex = 0;
			for (const FChunkDbDataAccess& ChunkFile : InOpenedChunkDbs)
			{
				if (!ChunkFile.bIsRetired &&
					ChunkFile.RetireAt < RetireAtEquivalent)
				{
					TotalSizeAtIndex += ChunkFile.Archive->TotalSize();
				}
			}

			OutChunkDbSizesAtCompletion.Add(TotalSizeAtIndex);
		}
		return AllChunkDbSize;
	}


	uint64 FChunkDbChunkSource::GetChunkDbSizesAtIndexes(const TArray<int32>& FileCompletionIndexes, TArray<uint64>& OutChunkDbSizesAtCompletion) const
	{
		return GetChunkDbSizesAtIndexesInternal(ChunkDbDataAccesses, OriginalChunkCount, FileCompletionIndexes, OutChunkDbSizesAtCompletion);
	}

	uint64 IConstructorChunkDbChunkSource::GetChunkDbSizesAtIndexes(const TArray<FString>& ChunkDbFiles, IFileSystem* FileSystem, const TArray<FGuid>& ChunkAccessOrderedList, const TArray<int32>& FileCompletionIndexes, TArray<uint64>& OutChunkDbSizesAtCompletion)
	{
		TArray<FChunkDbDataAccess> ChunkFiles;
		TMap<FGuid, FChunkAccessLookup> ChunkGuidLookup;

		FChunkDbChunkSource::LoadChunkDbFiles(ChunkDbFiles, FileSystem, ChunkAccessOrderedList, ChunkFiles, ChunkGuidLookup, nullptr);

		return GetChunkDbSizesAtIndexesInternal(ChunkFiles, ChunkAccessOrderedList.Num(), FileCompletionIndexes, OutChunkDbSizesAtCompletion);
	}

	FChunkDbChunkSource::FChunkDbChunkSource(FChunkDbSourceConfig InConfiguration, IFileSystem* InFileSystem, const TArray<FGuid>& InChunkAccessOrderList, 
		IChunkDataSerialization* InChunkDataSerialization, IChunkDbChunkSourceStat* InChunkDbChunkSourceStat)
		: Configuration(MoveTemp(InConfiguration))
		, FileSystem(InFileSystem)
		, ChunkDataSerialization(InChunkDataSerialization)
		, ChunkDbChunkSourceStat(InChunkDbChunkSourceStat)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FChunkDbChunkSource_ctor);

		OriginalChunkCount = InChunkAccessOrderList.Num();

		LoadChunkDbFiles(Configuration.ChunkDbFiles, FileSystem, InChunkAccessOrderList, ChunkDbDataAccesses, ChunkDbDataAccessLookup, &AvailableChunks);

		// Immediately retire any chunkdbs we don't need so they don't eat disk space during the first file.
		FChunkDbChunkSource::ReportFileCompletion(OriginalChunkCount);
	}


	IConstructorChunkSource::FRequestProcessFn FChunkDbChunkSource::CreateRequest(const FGuid& DataId, FMutableMemoryView DestinationBuffer, void* UserPtr, FChunkRequestCompleteDelegate CompleteFn)
	{
		FChunkAccessLookup* ChunkInfo = ChunkDbDataAccessLookup.Find(DataId);
		if (!ChunkInfo)
		{
			CompleteFn.Execute(DataId, false, true, UserPtr);
			return [](bool) {return;};
		}

		return [ChunkInfo, DataId, DestinationBuffer, UserPtr, CompleteFn, ChunkDataSerialization = ChunkDataSerialization, ChunkDbChunkSourceStat = ChunkDbChunkSourceStat](bool bIsAborted)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(ChunkDbRead);
			if (bIsAborted)
			{
				CompleteFn.Execute(DataId, true, false, UserPtr);
				return;
			}

			FChunkLocation& ChunkLocation = *ChunkInfo->Location;
			FChunkDbDataAccess& ChunkDbDataAccess = *ChunkInfo->DbFile;
			FArchive* ChunkDbFile = ChunkDbDataAccess.Archive.Get();
			if (ChunkDbFile->IsError())
			{
				CompleteFn.Execute(DataId, false, true, UserPtr);
				return;
			}

			IChunkDbChunkSourceStat::ELoadResult LoadResult = IChunkDbChunkSourceStat::ELoadResult::Success;
			ISpeedRecorder::FRecord ActivityRecord;
			ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
			ChunkDbChunkSourceStat->OnLoadStarted(DataId);

			// We'd love to read direct in to the destination if we don't have
			// any compression. However we don't know if it's compressed until we read the header which
			// is tiny and dependent - we don't know how big it is until we read part of it.
			ChunkDbFile->Seek(ChunkLocation.ByteStart);
			FChunkHeader Header;

			// If it's uncompressed, we can read direct to the destination.
			FUniqueBuffer CompressedBuffer;
			bool Result = ChunkDataSerialization->ValidateAndRead(*ChunkDbFile, DestinationBuffer, Header, CompressedBuffer);

			// Save this here so we only include the IO time and not the hash/decompress time.
			ActivityRecord.Size = ChunkDbFile->Tell() - ChunkLocation.ByteStart;
			ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
			ChunkDbChunkSourceStat->OnReadComplete(ActivityRecord);

			if (!Result)
			{
				LoadResult = IChunkDbChunkSourceStat::ELoadResult::SerializationError;
				ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
				ChunkDbChunkSourceStat->OnLoadComplete(DataId, LoadResult);

				// The header or chunk data was bad.
				CompleteFn.Execute(DataId, false, true, UserPtr);
				return;
			}

			// We either need to hash the chunk for validation or decompress it into the destination buffer - don't
			// block IO for this.
			UE::Tasks::Launch(TEXT("ChunkDbDecompressionAndHash"), 
				[DataId, CompleteFn, UserPtr, Header, DestinationBuffer, ChunkDataSerialization = ChunkDataSerialization, 
				CompressedBuffer = MoveTemp(CompressedBuffer), ChunkDbChunkSourceStat = ChunkDbChunkSourceStat]()
				{
					bool bDecompressSucceeded = ChunkDataSerialization->DecompressValidatedRead(Header, DestinationBuffer, CompressedBuffer);

					ChunkDbChunkSourceStat->OnLoadComplete(DataId, 
						bDecompressSucceeded ? IChunkDbChunkSourceStat::ELoadResult::Success : IChunkDbChunkSourceStat::ELoadResult::CorruptedData);

					CompleteFn.Execute(DataId, false, !bDecompressSucceeded, UserPtr);
				}
			);
		};
	}

	IConstructorChunkDbChunkSource* IConstructorChunkDbChunkSource::CreateChunkDbSource(FChunkDbSourceConfig&& Configuration, IFileSystem* FileSystem, 
		const TArray<FGuid>& ChunkAccessOrderList, IChunkDataSerialization* ChunkDataSerialization, IChunkDbChunkSourceStat* ChunkDbChunkSourceStat)
	{
		return new FChunkDbChunkSource(MoveTemp(Configuration), FileSystem, ChunkAccessOrderList, ChunkDataSerialization, ChunkDbChunkSourceStat);
	}

	const TCHAR* ToString(const IChunkDbChunkSourceStat::ELoadResult& LoadResult)
	{
		switch(LoadResult)
		{
			case IChunkDbChunkSourceStat::ELoadResult::Success:
				return TEXT("Success");
			case IChunkDbChunkSourceStat::ELoadResult::CorruptedData:
				return TEXT("CorruptedData");
			case IChunkDbChunkSourceStat::ELoadResult::SerializationError:
				return TEXT("SerializationError");
			default:
				return TEXT("Unknown");
		}
	}
}
