// Copyright Epic Games, Inc. All Rights Reserved.

#include "BuildPatchFileConstructor.h"
#include "IBuildManifestSet.h"
#include "HAL/UESemaphore.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "HAL/RunnableThread.h"
#include "Hash/xxhash.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Misc/OutputDeviceRedirector.h"
#include "BuildPatchServicesPrivate.h"
#include "Interfaces/IBuildInstaller.h"
#include "Data/ChunkData.h"
#include "Common/StatsCollector.h"
#include "Common/SpeedRecorder.h"
#include "Common/FileSystem.h"
#include "Compression/CompressionUtil.h"
#include "Installer/ChunkSource.h"
#include "Installer/ChunkDbChunkSource.h"
#include "Installer/InstallChunkSource.h"
#include "Installer/CloudChunkSource.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/InstallerError.h"
#include "Installer/InstallerAnalytics.h"
#include "Installer/InstallerSharedContext.h"
#include "Installer/MessagePump.h"
#include "Templates/Greater.h"
#include "BuildPatchUtil.h"
#include "ProfilingDebugging/CountersTrace.h"

using namespace BuildPatchServices;

static int32 SleepTimeWhenFileSystemThrottledSeconds = 1;
static FAutoConsoleVariableRef CVarSleepTimeWhenFileSystemThrottledSeconds(
	TEXT("BuildPatchFileConstructor.SleepTimeWhenFileSystemThrottledSeconds"),
	SleepTimeWhenFileSystemThrottledSeconds,
	TEXT("The amount of time to sleep if the destination filesystem is throttled."),
	ECVF_Default);

// This can be overridden by the installation parameters.
static bool bCVarStallWhenFileSystemThrottled = false;
static FAutoConsoleVariableRef CVarStallWhenFileSystemThrottled(
	TEXT("BuildPatchFileConstructor.bStallWhenFileSystemThrottled"),
	bCVarStallWhenFileSystemThrottled,
	TEXT("Whether to stall if the file system is throttled"),
	ECVF_Default);

static bool bCVarAllowMultipleFilesInFlight = true;
static FAutoConsoleVariableRef CVarAllowMultipleFilesInFlight(
	TEXT("BuildPatchFileConstructor.bCVarAllowMultipleFilesInFlight"),
	bCVarAllowMultipleFilesInFlight,
	TEXT("Whether to allow multiple files to be constructed at the same time, though still sequentially."),
	ECVF_Default);

static int32 CVarDisableResumeBelowMB = 0;
static FAutoConsoleVariableRef CVarRefDisableResumeBelowMB(
	TEXT("BuildPatchFileConstructor.DisableResumeBelowMB"),
	CVarDisableResumeBelowMB,
	TEXT("If nonzero, installs (not patches) below this size will not create or check any resume data."),
	ECVF_Default);

// Helper functions wrapping common code.
namespace FileConstructorHelpers
{
	void WaitWhilePaused(FThreadSafeBool& bIsPaused, FThreadSafeBool& bShouldAbort)
	{
		// Wait while paused
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.5f);
		}
	}

	bool CheckRemainingDiskSpace(const FString& InstallDirectory, uint64 RemainingBytesRequired, uint64& OutAvailableDiskSpace)
	{
		bool bContinueConstruction = true;
		uint64 TotalSize = 0;
		OutAvailableDiskSpace = 0;
		if (FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalSize, OutAvailableDiskSpace))
		{
			if (OutAvailableDiskSpace < RemainingBytesRequired)
			{
				bContinueConstruction = false;
			}
		}
		else
		{
			// If we can't get the disk space free then the most likely reason is the drive is no longer around...
			bContinueConstruction = false;
		}

		return bContinueConstruction;
	}

	uint64 CalculateRequiredDiskSpace(const FBuildPatchAppManifestPtr& CurrentManifest, const FBuildPatchAppManifestRef& BuildManifest, const EInstallMode& InstallMode, const TSet<FString>& InInstallTags)
	{
		// Make tags expected
		TSet<FString> InstallTags = InInstallTags;
		if (InstallTags.Num() == 0)
		{
			BuildManifest->GetFileTagList(InstallTags);
		}
		InstallTags.Add(TEXT(""));
		// Calculate the files that need constructing.
		TSet<FString> TaggedFiles;
		BuildManifest->GetTaggedFileList(InstallTags, TaggedFiles);
		FString DummyString;
		TSet<FString> FilesToConstruct;
		BuildManifest->GetOutdatedFiles(CurrentManifest.Get(), DummyString, TaggedFiles, FilesToConstruct);
		// Count disk space needed by each operation.
		int64 DiskSpaceDeltaPeak = 0;
		if (InstallMode == EInstallMode::DestructiveInstall && CurrentManifest.IsValid())
		{
			// The simplest method will be to run through each high level file operation, tracking peak disk usage delta.
			int64 DiskSpaceDelta = 0;

			// Loop through all files to be made next, in order.
			// This is sorted coming in and needs to stay in that order to pass BPT test suite
			//FilesToConstruct.Sort(TLess<FString>());
			for (const FString& FileToConstruct : FilesToConstruct)
			{
				// First we would need to make the new file.
				DiskSpaceDelta += BuildManifest->GetFileSize(FileToConstruct);
				if (DiskSpaceDeltaPeak < DiskSpaceDelta)
				{
					DiskSpaceDeltaPeak = DiskSpaceDelta;
				}
				// Then we can remove the current existing file.
				DiskSpaceDelta -= CurrentManifest->GetFileSize(FileToConstruct);
			}
		}
		else
		{
			// When not destructive, or no CurrentManifest, we always stage all new and changed files.
			DiskSpaceDeltaPeak = BuildManifest->GetFileSize(FilesToConstruct);
		}
		return FMath::Max<int64>(DiskSpaceDeltaPeak, 0);
	}
}


struct FAdministrationScope
{
	ISpeedRecorder::FRecord ActivityRecord;
	IFileConstructorStat* FileConstructorStat;
	FAdministrationScope(IFileConstructorStat* InFileConstructorStat)
	{
		FileConstructorStat = InFileConstructorStat;
		FileConstructorStat->OnBeforeAdminister();
		ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
	}
	~FAdministrationScope()
	{
		ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
		ActivityRecord.Size = 0;
		FileConstructorStat->OnAfterAdminister(ActivityRecord);
	}
};

struct FReadScope
{
	ISpeedRecorder::FRecord ActivityRecord;
	IFileConstructorStat* FileConstructorStat;
	FReadScope(IFileConstructorStat* InFileConstructorStat, int64 Size)
	{
		FileConstructorStat = InFileConstructorStat;		
		ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
		ActivityRecord.Size = Size;
		FileConstructorStat->OnBeforeRead();
	}
	~FReadScope()
	{
		ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
		FileConstructorStat->OnAfterRead(ActivityRecord);
	}
};

enum class EConstructionError : uint8
{
	None = 0,
	CannotCreateFile,
	OutOfDiskSpace,
	FailedInitialSizeCheck,
	MissingChunk,
	SerializeError,
	TrackingError,
	OutboundDataError,
	InternalConsistencyError,
	Aborted,
	MissingFileInfo,
	CloseError
};


// Since we can have more than one file in flight, store state here.
struct FFileConstructionState
{
	FGuid ErrorContextGuid;
	EConstructionError ConstructionError = EConstructionError::None;
	int32 CreateFilePlatformLastError = 0;

	FSHA1 HashState;

	// If this is true then we didn't actually have to make the file, it was already done or a symlink or something.
	bool bSkippedConstruction = false;
	bool bSuccess = true;
	bool bIsResumedFile = false;

	// We track how far we are in the file when we write into the write buffer so that
	// we advance progress bars smoothly instead of in huge writebuffer sized chunks.
	UE::FMutex ProgressLock;
	uint64 Progress = 0;
	uint64 LastSeenProgress = 0; // not locked.	

	// Where we started constructing the file. Can be non zero due to resume.
	int64 StartPosition = 0;

	int32 OutstandingBatches = 0;

	// Can be nonzero in the first batch due to resume.
	int32 NextChunkPartToRead = 0;

	TUniquePtr<FArchive> NewFile;

	int32 ConstructionIndex = -1;
	const FFileManifest* FileManifest;
	const FString& BuildFilename; // name as it is in the manifest, references the ConstructList in the configuration.
	FString NewFilename; // with output path and such

	FFileConstructionState(const FFileManifest* InFileManifest, const FString& InBuildFilename, FString&& InNewFilename) 
	: FileManifest(InFileManifest)
	, BuildFilename(InBuildFilename)
	, NewFilename(InNewFilename)
	{
		if (!InFileManifest)
		{
			bSuccess = false;
			ConstructionError = EConstructionError::MissingFileInfo;
		}
	}
};


/**
 * This struct handles loading and saving of simple resume information, that will allow us to decide which
 * files should be resumed from. It will also check that we are creating the same version and app as we expect to be.
 */
struct FResumeData
{
public:
	// File system dependency
	const IFileSystem* const FileSystem;

	// The manifests for the app we are installing
	const IBuildManifestSet* const ManifestSet;

	// Save the staging directory
	const FString StagingDir;

	// The filename to the resume data information
	const FString ResumeDataFilename;

	// The resume ids that we loaded from disk
	TSet<FString> LoadedResumeIds;

	// The set of files that were started
	TSet<FString> FilesStarted;

	// The set of files that were completed, determined by expected file size
	TSet<FString> FilesCompleted;

	// The set of files that exist but are not able to assume resumable
	TSet<FString> FilesIncompatible;

	// Whether we have any resume data for this install
	bool bHasResumeData = false;

	// For small installs we may disable resume entirely to mitigate the number of file operations.
	bool bResumeEnabled = false;

public:

	FResumeData(IFileSystem* InFileSystem, IBuildManifestSet* InManifestSet, const FString& InStagingDir, const FString& InResumeDataFilename)
		: FileSystem(InFileSystem)
		, ManifestSet(InManifestSet)
		, StagingDir(InStagingDir)
		, ResumeDataFilename(InResumeDataFilename)
	{
		// Leave resume disabled until initialized;
	}

	void InitResume()
	{
		bResumeEnabled = true;

		// Load data from previous resume file
		bHasResumeData = FileSystem->FileExists(*ResumeDataFilename);
		GLog->Logf(TEXT("BuildPatchResumeData file found: %s"), bHasResumeData ? TEXT("true") : TEXT("false"));
		if (bHasResumeData)
		{
			// Grab existing resume metadata.
			const bool bCullEmptyLines = true;
			FString PrevResumeData;
			TArray<FString> PrevResumeDataLines;
			FileSystem->LoadFileToString(*ResumeDataFilename, PrevResumeData);
			PrevResumeData.ParseIntoArrayLines(PrevResumeDataLines, bCullEmptyLines);
			// Grab current resume ids
			const bool bCheckLegacyIds = true;
			TSet<FString> NewResumeIds;
			ManifestSet->GetInstallResumeIds(NewResumeIds, bCheckLegacyIds);
			LoadedResumeIds.Reserve(PrevResumeDataLines.Num());
			// Check if any builds we are installing are a resume from previous run.
			for (FString& PrevResumeDataLine : PrevResumeDataLines)
			{
				PrevResumeDataLine.TrimStartAndEndInline();
				LoadedResumeIds.Add(PrevResumeDataLine);
				if (NewResumeIds.Contains(PrevResumeDataLine))
				{
					bHasResumeData = true;
					GLog->Logf(TEXT("BuildPatchResumeData version matched %s"), *PrevResumeDataLine);
				}
			}
		}
	}

	/**
	 * Saves out the resume data
	 */
	void SaveOut(const TSet<FString>& ResumeIds)
	{
		// Save out the patch versions
		if (bResumeEnabled)
		{
			FileSystem->SaveStringToFile(*ResumeDataFilename, FString::Join(ResumeIds, TEXT("\n")));
		}
	}

	/**
	 * Checks whether the file was completed during last install attempt and adds it to FilesCompleted if so
	 * @param Filename    The filename to check
	 */
	void CheckFile(const FString& Filename)
	{
		// If we had resume data, check if this file might have been resumable
		if (bHasResumeData)
		{
			int64 DiskFileSize;
			const FString FullFilename = StagingDir / Filename;
			const bool bFileExists = FileSystem->GetFileSize(*FullFilename, DiskFileSize);
			const bool bCheckLegacyIds = true;
			TSet<FString> FileResumeIds;
			ManifestSet->GetInstallResumeIdsForFile(Filename, FileResumeIds, bCheckLegacyIds);
			if (LoadedResumeIds.Intersect(FileResumeIds).Num() > 0)
			{
				const FFileManifest* NewFileManifest = ManifestSet->GetNewFileManifest(Filename);
				if (NewFileManifest && bFileExists)
				{
					const uint64 UnsignedDiskFileSize = DiskFileSize;
					if (UnsignedDiskFileSize > 0 && UnsignedDiskFileSize <= NewFileManifest->FileSize)
					{
						FilesStarted.Add(Filename);
					}
					if (UnsignedDiskFileSize == NewFileManifest->FileSize)
					{
						FilesCompleted.Add(Filename);
					}
					if (UnsignedDiskFileSize > NewFileManifest->FileSize)
					{
						FilesIncompatible.Add(Filename);
					}
				}
			}
			else if (bFileExists)
			{
				FilesIncompatible.Add(Filename);
			}
		}
	}
};

static FString FormatNumber(uint64 Value) { return FText::AsNumber(Value).ToString(); }


// We need a place to put chunks if we know we're going to need them again after
// their source retires. We don't want to route everything through here. We also
// want to be able to optionally overflow to disk, and ideally this would be persistent
// across resumes so that we don't have to re-download a huge amount of harvested chunks
// from install files.
class FChunkBackingStore : IConstructorChunkSource
{
	FBuildPatchFileConstructor* ParentConstructor = nullptr;
	const FString& InstallDirectory;
	FBuildPatchFileConstructor::FBackingStoreStats& Stats;

	struct FStoredChunk
	{
		TArray<uint8> ChunkData;
		uint32 ChunkSize = 0;
		int32 NextUsageIndex;
		int32 LastUsageIndex;

		// Has the data made it into memory yet?
		bool bCommitted = false;

		// Have we been evicted for memory concerns? We are in UsedEntries if so.
		bool bBackedByDisk = false;

		// When something is reading/writing to us we mark that so we don't
		// evict during async operations.
		uint16 LockCount = 0;
	};

	TMap<FGuid, FStoredChunk> StoredChunks;
	uint64 CurrentMemoryLoad = 0;
	uint64 PeakMemoryLoad = 0;

	int32 ReadCount = 0;
	int32 WriteCount = 0;

	static constexpr uint64 ChunkStoreMemoryLimitDisabledSentinel = TNumericLimits<uint64>::Max();
	uint64 MaxMemoryBytes = 1 << 30; // 1GB.


	// 0 disables
	uint64 MaxDiskSpaceBytes = 1 << 30; // 1GB.
	uint64 AdditionalDiskSpaceHeadroomBytes = 0;
	uint64 InstallationFreeSpaceRequired = 0;

	// The backing store allocates in 128kb chunks.
	static constexpr uint32 BitsPerEntry = 17;
	struct FBackingStoreFreeSpan
	{
		uint32 StartEntryIndex = 0;
		uint32 EndEntryIndex = 0;
		uint64 Size() const { return EntryCount() << BitsPerEntry; }
		uint64 Offset() const { return (uint64)StartEntryIndex << BitsPerEntry; }
		uint64 EntryCount() const { return EndEntryIndex - StartEntryIndex; }
	};
	struct FBackingStoreUsedSpan
	{
		uint32 StartEntryIndex = 0;
		uint32 EndEntryIndex = 0;
		uint32 UsedBytes = 0;
		FXxHash64 Hash;
		uint64 ReservedSize() const { return EntryCount() << BitsPerEntry; }
		uint64 Offset() const { return (uint64)StartEntryIndex << BitsPerEntry; }
		uint64 EntryCount() const { return EndEntryIndex - StartEntryIndex; }
	};

	uint32 BackingStoreMaxEntries = 0;
	uint32 BackingStoreEntryCount = 0;
	uint64 BackingStoreUsedSpace = 0;
	uint64 BackingStoreWastedSpace = 0;
	TMap<FGuid, FBackingStoreUsedSpan> UsedDiskSpans;
	TArray<FBackingStoreFreeSpan> FreeDiskSpans;
	uint64 CurrentDiskLoad() const { return (uint64)(BackingStoreEntryCount) << BitsPerEntry; }

	TUniquePtr<IFileHandle> BackingStoreFileHandle;
	FString BackingStoreFileName;

	void DumpFreeDiskSpans()
	{
		UE_LOG(LogBuildPatchServices, Display, TEXT("Backing Store (Entries / Max): %d / %d"), BackingStoreEntryCount, BackingStoreMaxEntries);
		UE_LOG(LogBuildPatchServices, Display, TEXT("Dumping Free Disk Spans..."));		

		for (int32 FreeIndex = 0; FreeIndex < FreeDiskSpans.Num(); FreeIndex++)
		{
			UE_LOG(LogBuildPatchServices, Display, TEXT("   %d/%d: %d - %d"), FreeIndex, FreeDiskSpans.Num(), FreeDiskSpans[FreeIndex].StartEntryIndex, FreeDiskSpans[FreeIndex].EndEntryIndex);
		}
	}
	// false means consistency failure
	[[nodiscard]] bool ConsistencyCheck()
	{
		bool bSuccess = true;

		uint64 UsedEntryReservedBytes = 0;
		uint64 UsedEntryUsedBytes = 0;
		for (TPair<FGuid, FBackingStoreUsedSpan>& Pair : UsedDiskSpans)
		{
			FBackingStoreUsedSpan& Span = Pair.Value;
			UsedEntryReservedBytes += Span.ReservedSize();
			UsedEntryUsedBytes += Span.UsedBytes;
		}

		uint64 FreeBytes = 0;

		for (int32 FreeIndex = 0; FreeIndex < FreeDiskSpans.Num(); FreeIndex++)
		{
			FBackingStoreFreeSpan& Span = FreeDiskSpans[FreeIndex];

			FreeBytes += Span.Size();

			if (Span.Size() == 0)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("BackingStore FreeList Merge Fail: empty entry failed to get deleted"));
				bSuccess = false;
			}
			if (Span.EndEntryIndex > BackingStoreEntryCount)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("BackingStore FreeList Merge Fail: entry exceeds backing store size"));
				bSuccess = false;
			}
			if (Span.StartEntryIndex > Span.EndEntryIndex)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("BackingStore FreeList Merge Fail: negative sized entry"));
				bSuccess = false;
			}

			if (FreeIndex)
			{
				if (Span.StartEntryIndex == FreeDiskSpans[FreeIndex-1].EndEntryIndex)
				{
					UE_LOG(LogBuildPatchServices, Error, TEXT("BackingStore FreeList Merge Fail: adjacent entries failed to merge"));
					bSuccess = false;
				}
				if (Span.StartEntryIndex < FreeDiskSpans[FreeIndex-1].EndEntryIndex)
				{
					UE_LOG(LogBuildPatchServices, Error, TEXT("BackingStore FreeList Merge Fail: adjacent entries overlap or are out of order"));
					bSuccess = false;
				}
			}
		}

		if (!bSuccess)
		{
			DumpFreeDiskSpans();
		}

		if (UsedEntryReservedBytes != (BackingStoreUsedSpace + BackingStoreWastedSpace) ||
			UsedEntryUsedBytes != BackingStoreUsedSpace ||
			CurrentDiskLoad() - UsedEntryReservedBytes != FreeBytes)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Disk Backing Store Consistency Fail:"));
			UE_LOG(LogBuildPatchServices, Error, TEXT("Actual:"));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    ReservedBytes: %s"), *FormatNumber(UsedEntryReservedBytes));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    WastedBytes: %s"), *FormatNumber(UsedEntryReservedBytes - UsedEntryUsedBytes));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    FreeBytes: %s"), *FormatNumber(FreeBytes));
			UE_LOG(LogBuildPatchServices, Error, TEXT("Expected:"));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    ReservedBytes: %s"), *FormatNumber(BackingStoreUsedSpace + BackingStoreWastedSpace));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    WastedBytes: %s"), *FormatNumber(BackingStoreWastedSpace));
			UE_LOG(LogBuildPatchServices, Error, TEXT("    FreeBytes: %s"), *FormatNumber(CurrentDiskLoad() - BackingStoreUsedSpace - BackingStoreWastedSpace));
			bSuccess = false;
		}

		return bSuccess;
	}

	// false means consistency or write failure
	// constructor thread
	[[nodiscard]] bool PageOut(const FGuid& InGuid)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BackingStore_PageOut);

		// InChunk must be valid for paging out at this point!
		FStoredChunk& InChunk = StoredChunks[InGuid];

		// It's possible we loaded back into memory but are already backed by disk so we can
		// just free.
		if (InChunk.bBackedByDisk)
		{
			InChunk.ChunkData.Empty();
			CurrentMemoryLoad -= InChunk.ChunkSize;
			ParentConstructor->SetChunkLocation(InGuid, EConstructorChunkLocation::DiskOverflow);
			return true;
		}

		UE_LOG(LogBuildPatchServices, VeryVerbose, TEXT("Paging out: %s"), *WriteToString<40>(InGuid));

		uint32 EntriesRequired = Align(InChunk.ChunkSize, (1 << BitsPerEntry)) >> BitsPerEntry;

		int32 SpanIndex = 0;
		while (SpanIndex < FreeDiskSpans.Num() && 
			FreeDiskSpans[SpanIndex].EntryCount() < EntriesRequired)
		{
			SpanIndex++;
		}

		bool bAppendingToFile = false;
		if (SpanIndex == FreeDiskSpans.Num())
		{
			// Check our disk space limitations.
			uint64 UseMaxDiskSpaceBytes = MaxDiskSpaceBytes;

			// If we have a headroom value, we want to dynamically adjust
			// our max disk space so that we always leave that amount of disk space free.
			// This is expected to almost always be enabled in order to prevent the backing store from
			// eating into space reserved for the actual installation. 
			if (InstallationFreeSpaceRequired || AdditionalDiskSpaceHeadroomBytes)
			{
				uint64 TotalDiskBytes, FreeDiskBytes;
				if (!FPlatformMisc::GetDiskTotalAndFreeSpace(InstallDirectory, TotalDiskBytes, FreeDiskBytes))
				{
					// If we fail to get disk space then disable it since we don't really know what we're doing
					// at that point.
					InstallationFreeSpaceRequired = 0;
					AdditionalDiskSpaceHeadroomBytes = 0;
				}
				else
				{
					// The free space we got is counting any bytes we've already written to disk, so adjust for that.
					uint64 FreeSizeBytesWithoutBackingStore = FreeDiskBytes + CurrentDiskLoad();

					uint64 HeadroomRequiredBytes = InstallationFreeSpaceRequired + AdditionalDiskSpaceHeadroomBytes;

					// By default we aren't allowed any space due to headroom limitations.
					uint64 HeadroomRestrictedMaxDiskSpace = 0;

					// If we have enough space above the headroom, then we can talk.
					if (FreeSizeBytesWithoutBackingStore > HeadroomRequiredBytes)
					{
						HeadroomRestrictedMaxDiskSpace = FreeSizeBytesWithoutBackingStore - HeadroomRequiredBytes;
						if (UseMaxDiskSpaceBytes == 0 || // Note if UseMaxDiskSpaceBytes is 0 then we they have no other limitation.
							HeadroomRestrictedMaxDiskSpace < UseMaxDiskSpaceBytes)
						{
							UseMaxDiskSpaceBytes = HeadroomRestrictedMaxDiskSpace;
						}
					}
				}
			}

			// Need to expand the backing store.
			if (BackingStoreFileHandle == nullptr ||
				(UseMaxDiskSpaceBytes && (((uint64)BackingStoreEntryCount + EntriesRequired) << BitsPerEntry) > UseMaxDiskSpaceBytes))
			{
				// We can't expand - this fails to page out and gets evicted from the backing store.
				Stats.DiskLostChunkCount++;
				ParentConstructor->SetChunkLocation(InGuid, EConstructorChunkLocation::Cloud);
				CurrentMemoryLoad -= InChunk.ChunkSize;
				InChunk.ChunkData.Empty();
				StoredChunks.Remove(InGuid);
				return true;
			}

			FBackingStoreFreeSpan& NewSpan = FreeDiskSpans.AddDefaulted_GetRef();
			NewSpan.StartEntryIndex = BackingStoreEntryCount;
			BackingStoreEntryCount += EntriesRequired;
			if (BackingStoreEntryCount > BackingStoreMaxEntries)
			{
				BackingStoreMaxEntries = BackingStoreEntryCount;
			}
			NewSpan.EndEntryIndex = NewSpan.StartEntryIndex + EntriesRequired;
			bAppendingToFile = true;

			Stats.DiskPeakUsageBytes = CurrentDiskLoad();
		}

		// SpanIndex is the one we just added or found to reuse
		FBackingStoreFreeSpan& FreeSpan = FreeDiskSpans[SpanIndex];
		FBackingStoreUsedSpan& UsedSpan = UsedDiskSpans.FindOrAdd(InGuid);
		if (!UsedSpan.Hash.IsZero())
		{
			// Can't be paging out twice!
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency failure: Backing store used entry already existed for %s"), *WriteToString<40>(InGuid));
			return false;
		}

		UsedSpan.StartEntryIndex = FreeSpan.StartEntryIndex;
		UsedSpan.EndEntryIndex = UsedSpan.StartEntryIndex + EntriesRequired;
		UsedSpan.UsedBytes = InChunk.ChunkSize;

		FreeSpan.StartEntryIndex += EntriesRequired;

		// LONGTERM - we should be using XX has for all chunk consistency checking, so we would have this value already.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BSPO_Hash);
			UsedSpan.Hash = FXxHash64::HashBuffer(InChunk.ChunkData.GetData(), InChunk.ChunkData.Num());
		}

		if (FreeSpan.EndEntryIndex == FreeSpan.StartEntryIndex)
		{
			FreeDiskSpans.RemoveAt(SpanIndex);
		}

		// Write
		{
			WriteCount++;

			TRACE_CPUPROFILER_EVENT_SCOPE(BSPO_Write);
			if (!BackingStoreFileHandle->Seek(UsedSpan.Offset()))
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to seek disk backing store to %llu"), UsedSpan.Offset());
				return false;
			}

			if (!BackingStoreFileHandle->Write(InChunk.ChunkData.GetData(), InChunk.ChunkData.Num()))
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to write %llu bytes to disk backing store at %llu"), InChunk.ChunkData.Num(), UsedSpan.Offset());
				return false;
			}
		}

		uint64 Wastage = UsedSpan.ReservedSize() - InChunk.ChunkData.Num();

		// If we just added to the end of the file, we only wrote the size of the chunk data, not necessarily
		// the size of our reservation, so top off with zeroes.
		if (bAppendingToFile)
		{
			// This is at most a 128kb allocation (1 << BitsPerEntry), but the chunk size currently in use
			// is 3 bytes shy of being perfectly aligned to a multiple of that meaning we only expect to need to write
			// 3 bytes of zeroes. So we make sure we have enough space on the stack for that.
			TArray<uint8, TInlineAllocator<128>> Zeroes;
			Zeroes.AddZeroed(Wastage);
			if (!BackingStoreFileHandle->Write(Zeroes.GetData(), Zeroes.Num()))
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Failed to write %llu bytes of zeroes to disk backing store at %llu"), Zeroes.Num(), UsedSpan.Offset());
				return false;
			}
		}

		// Done.
		CurrentMemoryLoad -= InChunk.ChunkSize;
		BackingStoreUsedSpace += InChunk.ChunkSize;
		BackingStoreWastedSpace += Wastage;
		InChunk.ChunkData.Empty();
		InChunk.bBackedByDisk = true;
		ParentConstructor->SetChunkLocation(InGuid, EConstructorChunkLocation::DiskOverflow);

		return ConsistencyCheck();
	}

	// false means consistency failure
	[[nodiscard]] bool ReleaseBackingStoreEntry(const FGuid& InGuid, FStoredChunk& InChunk)
	{
		// Return the bits back to the free list.
		FBackingStoreFreeSpan NewFreeSpan;
		int32 FreeSpanIndex;
		{
			FBackingStoreUsedSpan& UsedEntry = UsedDiskSpans[InGuid];

			BackingStoreUsedSpace -= InChunk.ChunkSize;
			BackingStoreWastedSpace -= UsedEntry.ReservedSize() - InChunk.ChunkSize;

			FreeSpanIndex = Algo::LowerBoundBy(FreeDiskSpans, UsedEntry.StartEntryIndex, &FBackingStoreFreeSpan::StartEntryIndex);
			
			NewFreeSpan.StartEntryIndex = UsedEntry.StartEntryIndex;
			NewFreeSpan.EndEntryIndex = UsedEntry.EndEntryIndex;
		
			InChunk.bBackedByDisk = false;

			UsedDiskSpans.Remove(InGuid);
		}

		// Merge into an adjacent entry without adding and having to do a linear
		// pass to coalesce. 
		bool bMerged = false;
		if (FreeSpanIndex < FreeDiskSpans.Num())
		{
			if (NewFreeSpan.EndEntryIndex == FreeDiskSpans[FreeSpanIndex].StartEntryIndex)
			{
				// We are right before to the one after us - just extend them lower
				FreeDiskSpans[FreeSpanIndex].StartEntryIndex = NewFreeSpan.StartEntryIndex;
				bMerged = true;

				// OK they got merged down, see if they can connect with the one below
				if (FreeSpanIndex > 0)
				{
					if (FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex == FreeDiskSpans[FreeSpanIndex].StartEntryIndex)
					{
						// We fill a gap, we can connect and remove.
						FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex = FreeDiskSpans[FreeSpanIndex].EndEntryIndex;
						FreeDiskSpans.RemoveAt(FreeSpanIndex);
					}
				}
			}
		}
		
		if (!bMerged && FreeSpanIndex > 0) // if we missed, this will be Num(). If we have nothing yet it'll fail this and be fine.
		{
			if (FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex == NewFreeSpan.StartEntryIndex)
			{
				// We are right after the one before us, extend them farther.
				FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex = NewFreeSpan.EndEntryIndex;
				bMerged = true;

				// They got merged up, see if we filled a gap.
				if (FreeSpanIndex < FreeDiskSpans.Num())
				{
					if (FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex == FreeDiskSpans[FreeSpanIndex].StartEntryIndex)
					{
						FreeDiskSpans[FreeSpanIndex - 1].EndEntryIndex = FreeDiskSpans[FreeSpanIndex].EndEntryIndex;
						FreeDiskSpans.RemoveAt(FreeSpanIndex);
					}
				}
			}
		}

		// If we didn't merge, we need to insert
		if (!bMerged)
		{
			FreeDiskSpans.Insert(NewFreeSpan, FreeSpanIndex);
		}

		// Check and see if the free space is at the end of the file. If so, we can truncate
		// and free up disk space. 
		if (FreeDiskSpans.Top().EndEntryIndex == BackingStoreEntryCount)
		{
			uint64 TruncateToSize = (uint64)FreeDiskSpans.Top().StartEntryIndex << BitsPerEntry;
			if (BackingStoreFileHandle->Truncate(TruncateToSize)) // truncate can fail... don't update structures if we can't!
			{
				BackingStoreEntryCount = FreeDiskSpans.Top().StartEntryIndex;
				FreeDiskSpans.Pop();
			}
		}

		return ConsistencyCheck();
	}

	[[nodiscard]] bool ReleaseEntryInternal(const FGuid& InGuid, FStoredChunk* InStoredChunk)
	{
		if (InStoredChunk->LockCount == 0)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Releasing memory entry that isn't locked! %s"), *WriteToString<40>(InGuid));
			return false;
		}
		else
		{
			InStoredChunk->LockCount--;
			return true;
		}
	}

public:

	FChunkBackingStore(FBuildPatchFileConstructor* InParentConstructor, const FString& InInstallDirectory, FBuildPatchFileConstructor::FBackingStoreStats& InStats) 
	: ParentConstructor(InParentConstructor)
	, InstallDirectory(InInstallDirectory)
	, Stats(InStats)
	{
		BackingStoreFileName = InParentConstructor->Configuration.BackingStoreDirectory / TEXT("backingstore");

		bool bUseDiskOverflowStore = true;
		GConfig->GetBool(TEXT("BuildPatchServices"), TEXT("bEnableDiskOverflowStore"), bUseDiskOverflowStore, GEngineIni);
		
		// Is there a hard limit on how much disk space we can use?
		// negative = no
		// 0 = disable disk overflow
		int64 MaxDiskMB = 0;
		GConfig->GetInt64(TEXT("BuildPatchServices"), TEXT("DiskOverflowStoreLimitMB"), MaxDiskMB, GEngineIni);
		if (MaxDiskMB < 0)
		{
			MaxDiskSpaceBytes = TNumericLimits<int64>::Max();
		}
		else
		{
			MaxDiskSpaceBytes = MaxDiskMB << 20;
		}

		// Do we want to always try and keep some disk space available, no matter what our limit is?
		// note that independent of this we try and prevent the disk backing store from eating into space
		// we have reserved for the actual install so they don't compete.
		//
		// this checks the free space after each file and updates the space limit correspondingly.
		int64 AdditionalDiskSpaceHeadroomMB = 0;
		GConfig->GetInt64(TEXT("BuildPatchServices"), TEXT("DiskOverflowStoreAdditionalHeadroomMB"), AdditionalDiskSpaceHeadroomMB, GEngineIni);
		if (AdditionalDiskSpaceHeadroomMB >= 0)
		{
			AdditionalDiskSpaceHeadroomBytes = AdditionalDiskSpaceHeadroomMB << 20;
		}

		if (bUseDiskOverflowStore)
		{
			IFileManager::Get().MakeDirectory(*InParentConstructor->Configuration.BackingStoreDirectory);
			BackingStoreFileHandle.Reset(FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*BackingStoreFileName, false, true));
		}

		if (!BackingStoreFileHandle)
		{
			// Prevent any pageouts.
			UE_LOG(LogBuildPatchServices, Warning, TEXT("Unable to open disk backing store at %s"), *BackingStoreFileName);
			UE_LOG(LogBuildPatchServices, Warning, TEXT("Disk backing store will be disabled"));
			MaxDiskSpaceBytes = 0;
		}

		UE_LOG(LogBuildPatchServices, Display, TEXT("DiskOverflowStore is: %s - MaxSize = %s, Additional Headroom = %s"), 
			BackingStoreFileHandle ? TEXT("Enabled") : TEXT("Disabled"),
			*FormatNumber(MaxDiskSpaceBytes),
			*FormatNumber(AdditionalDiskSpaceHeadroomBytes)
			);

		
		// Now memory limits.
		uint64 ChunkStoreMemoryLimit = 0;

		// Check old values and warn/assume
		{
			int32 ChunkStoreMemorySizeChunks;
			const bool bLoadedStoreSize = GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkStoreMemorySize"), ChunkStoreMemorySizeChunks, GEngineIni);
			if (bLoadedStoreSize)
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("Outdated memory size limitation found: ChunkStoreMemorySize. Assuming chunk size is 1MB, use ChunkStoreMemorySizeMB instead."));
				ChunkStoreMemoryLimit = (uint64)FMath::Max(ChunkStoreMemorySizeChunks, 0) << 20;
			}

			int32 CloudChunkStoreMemorySizeChunks = 0;
			int32 InstallChunkStoreMemorySizeChunks = 0;
			const bool bLoadedCloudSize = GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("CloudChunkStoreMemorySize"), CloudChunkStoreMemorySizeChunks, GEngineIni);
			const bool bLoadedInstallSize = GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("InstallChunkStoreMemorySize"), InstallChunkStoreMemorySizeChunks, GEngineIni);
			if (bLoadedCloudSize || bLoadedInstallSize)
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("Outdated memory size limitations found: CloudChunkStoreMemorySize or InstallChunkStoreMemorySize. Assuming chunk size is 1MB."));
				UE_LOG(LogBuildPatchServices, Warning, TEXT("Use ChunkStoreMemorySizeMB and/or ChunkStoreMemoryHeadRoomMB."));
				uint64 OldMemoryLimit = FMath::Max(CloudChunkStoreMemorySizeChunks + InstallChunkStoreMemorySizeChunks, 0);
				OldMemoryLimit <<= 20;
				ChunkStoreMemoryLimit = OldMemoryLimit;
			}
		}

		

		// check current values. We expect this to override anything from above.
		int32 ChunkStoreMemoryLimitMB = 0;
		if (GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkStoreMemorySizeMB"), ChunkStoreMemoryLimitMB, GEngineIni))
		{
			// To be consistent with other limitations, negative disables it
			// 0 is OK - we require locked data to be in memory so we'll go over the limit but it'll be a minimum.
			if (ChunkStoreMemoryLimitMB < 0)
			{
				ChunkStoreMemoryLimit = ChunkStoreMemoryLimitDisabledSentinel;
			}
			else
			{
				ChunkStoreMemoryLimit = (uint64)ChunkStoreMemoryLimitMB << 20;
			}
		}
		

		// Get headroom. Default to 2GB of headroom. If no config is entered then we expect a 0 chunk limit that
		// gets updated off of this default headroom.
		int32 ChunkStoreMemoryHeadRoomMB = 2000;
		if (GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ChunkStoreMemoryHeadRoomMB"), ChunkStoreMemoryHeadRoomMB, GEngineIni))
		{
			if (ChunkStoreMemoryHeadRoomMB < 0)
			{
				// negative disables
				ChunkStoreMemoryHeadRoomMB = -1;
			}
			else if (ChunkStoreMemoryHeadRoomMB < 500)
			{
				UE_LOG(LogBuildPatchServices, Warning, TEXT("ChunkStoreMemoryHeadRoomMB too low (%d), using min (500)"), ChunkStoreMemoryHeadRoomMB);
				ChunkStoreMemoryHeadRoomMB = 500;
			}
		}		



		FPlatformMemoryStats MemoryStats = FPlatformMemory::GetStats();
		uint64 AvailableMem = MemoryStats.AvailablePhysical;
		if (ChunkStoreMemoryHeadRoomMB >= 0)
		{
			uint64 RequestedHeadRoom = ((uint64)ChunkStoreMemoryHeadRoomMB << 20);

			uint64 ProposedChunkStoreMemoryLimit = ChunkStoreMemoryLimit;

			if (RequestedHeadRoom < AvailableMem)
			{
				uint64 MemoryStoreMem = AvailableMem - RequestedHeadRoom;
				ProposedChunkStoreMemoryLimit = MemoryStoreMem;
			}
			else
			{
				// Cap at available.
				if (ProposedChunkStoreMemoryLimit > AvailableMem)
				{
					ProposedChunkStoreMemoryLimit = AvailableMem;
				}
			}

			// If there's already a limit requested by the inis, we don't want to make it _smaller_. If there's no
			// limit specified, then use the proposed limit.
			if (ChunkStoreMemoryLimit != ChunkStoreMemoryLimitDisabledSentinel)
			{
				ChunkStoreMemoryLimit = FMath::Max(ChunkStoreMemoryLimit, ProposedChunkStoreMemoryLimit);
			}
			else
			{
				ChunkStoreMemoryLimit = ProposedChunkStoreMemoryLimit;
			}
		}

		if (ChunkStoreMemoryLimit == ChunkStoreMemoryLimitDisabledSentinel)
		{
			UE_LOG(LogBuildPatchServices, Display, TEXT("ChunkStoreMemoryLimits are disabled"));
		}
		else
		{
			UE_LOG(LogBuildPatchServices, Display, TEXT("ChunkStoreMemoryLimits: %s using headroom of %s (%s available memory, %s used physical, %s used virtual)"), 
				*FormatNumber(ChunkStoreMemoryLimit),
				ChunkStoreMemoryHeadRoomMB >= 0 ? *FormatNumber((uint64)ChunkStoreMemoryHeadRoomMB << 20) : TEXT("<disabled>"),
				*FormatNumber(AvailableMem),
				*FormatNumber(MemoryStats.UsedPhysical),
				*FormatNumber(MemoryStats.UsedVirtual)
			);
		}

		MaxMemoryBytes = ChunkStoreMemoryLimit;
		Stats.MemoryLimitBytes = MaxMemoryBytes;
	}

	~FChunkBackingStore()
	{
		BackingStoreFileHandle.Reset(); // have to close the file before we try to delete it.
		if (!FPlatformFileManager::Get().GetPlatformFile().DeleteFile(*BackingStoreFileName))
		{
			UE_LOG(LogBuildPatchServices, Warning, TEXT("Unable to delete disk backing store: %s"), *BackingStoreFileName);
		}

		UE_LOG(LogBuildPatchServices, Verbose, TEXT("BackingStore Reads: %d Writes: %d"), ReadCount, WriteCount);
	}

	// Set the amount of disk space the installation needs to we can ensure that we don't 
	// expand into that space no matter what our config disk space limits are.
	void SetDynamicDiskSpaceHeadroom(uint64 InInstallationFreeSpaceRequired)
	{
		InstallationFreeSpaceRequired = InInstallationFreeSpaceRequired;
	}
	void DisableDynamicDiskSpaceHeadroom()
	{
		InstallationFreeSpaceRequired = 0;
	}

	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool DereserveHarvestingEntry(const FGuid& InGuid)
	{
		FStoredChunk* StoredChunk = StoredChunks.Find(InGuid);
		if (StoredChunk)
		{
			if (StoredChunk->bCommitted || StoredChunk->bBackedByDisk)
			{
				UE_LOG(LogBuildPatchServices, Error, 
					TEXT("Consistency Failure: deserve memory entry that's uncommitted or paged out! %s, paged out = %d committed = %d"), 
					*WriteToString<40>(InGuid), StoredChunk->bBackedByDisk, StoredChunk->bCommitted);
				return false;
			}

			CurrentMemoryLoad -= StoredChunk->ChunkData.Num();
			StoredChunks.Remove(InGuid);
		}
		else
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Cleared memory entry that doesn't exist! %s"), *WriteToString<40>(InGuid));
			return false;
		}
		return true;
	}

	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool ReleaseEntry(const FGuid& InGuid)
	{
		FStoredChunk* StoredChunk = StoredChunks.Find(InGuid);
		if (StoredChunk)
		{
			return ReleaseEntryInternal(InGuid, StoredChunk);
		}
		else
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Releasing memory entry that doesn't exist! %s"), *WriteToString<40>(InGuid));
			return false;
		}
	}

	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool CommitAndReleaseEntry(const FGuid& InGuid)
	{
		FStoredChunk* StoredChunk = StoredChunks.Find(InGuid);
		if (StoredChunk)
		{
			if (StoredChunk->bCommitted)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Committing memory entry that is already committed! %s"), *WriteToString<40>(InGuid));
				return false;
			}
			StoredChunk->bCommitted = true;
			return ReleaseEntryInternal(InGuid, StoredChunk);
		}
		else
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Committed memory entry that doesn't exist! %s"), *WriteToString<40>(InGuid));
			return false;
		}
	}

	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool LockEntry(const FGuid& InGuid)
	{
		FStoredChunk* StoredChunk = StoredChunks.Find(InGuid);
		if (!StoredChunk)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: locking memory entry that doesn't exist! %s"), *WriteToString<40>(InGuid));
			return false;
		}
		else
		{
			StoredChunk->LockCount++;
		}
		return true;
	}

	// Returns an empty view on consistency failure
	// Constructor thread.
	[[nodiscard]] FMutableMemoryView ReserveAndLockEntry(const FGuid& InGuid, uint32 InChunkSize, int32 LastUsageIndex)
	{
		// Note that this function can be called to reserve an entry already in the backing store
		// because we need to read from the disk to memory for a sub-chunk (or otherwise). So it needs
		// to be able to handle reserving on top of paged out chunks.

		int32 CurrentUsageIndex = ParentConstructor->ChunkReferenceTracker->GetCurrentUsageIndex();

		if (MaxMemoryBytes != ChunkStoreMemoryLimitDisabledSentinel)
		{
			while (CurrentMemoryLoad + InChunkSize > MaxMemoryBytes)
			{
				// Gotta dump stuff to disk. If we fail to dump to disk we mark the chunk
				// as only available via the cloud source.

				// Evict the one that is the longest until we use it. Chunks are almost always the same size
				// so we expect this to run once.
				int32 FarthestNextUsage = -1;
				FGuid EvictGuid;
				for (TPair<FGuid, FStoredChunk>& Pair : StoredChunks)
				{
					FStoredChunk& Chunk = Pair.Value;

					if (Chunk.LockCount || // Actively in use - don't evict.
						!Chunk.ChunkData.Num()) // Not in memory - can't evict.
					{					
						continue;
					}

					if (Chunk.NextUsageIndex < CurrentUsageIndex)
					{
						Chunk.NextUsageIndex = ParentConstructor->ChunkReferenceTracker->GetNextUsageForChunk(Pair.Key, Chunk.LastUsageIndex);
					}

					if (Chunk.NextUsageIndex > FarthestNextUsage)
					{
						FarthestNextUsage = Pair.Value.NextUsageIndex;
						EvictGuid = Pair.Key;
					}
				}

				if (FarthestNextUsage == -1)
				{
					// This means we can't reserve and also keep our memory requirements. 
					//
					// We currently have a minimum memory requirement for construction: Partial or reused chunks
					// are routed through the backing store for holding. Partial because we need a full allocation to decompress
					// the chunk and reuse so we don't have to re-read from disk.
					//
					// For unpatched installs this is minimal as most chunks are able to write directly to
					// the destination buffer. For optimized delta manifests this is not the case as BPT assembles
					// chunks from all over the place - I've seen 80+ chunk references to assemble 16MB of data,
					// resulting in total buffer usage of 80MB + 16MB > 100MB of memory use per batch. With two
					// batches in flight this pushes 200MB total buffer allocation.
					//
					// This is not likely an issue in real life as 200MB isn't much, but it does mean if we have a 
					// low memory usage limit we can hit this legitimately.
					//
					// So...we let the reservation continue in violation of our memory constraints and hope we don't OOM.
					//
					// Future work could be:
					// 1. Limit batch creation based on inflight chunk in addition to write buffer size
					// 2. Only allow 1 batch in flight when using an optimized delta + low memory constraints.
					break;
				}
				else
				{
					if (!PageOut(EvictGuid))
					{
						return FMutableMemoryView();
					}
				}
			} // end while over limits
		} // end if memory limits enabled
		FStoredChunk& StoredChunk = StoredChunks.FindOrAdd(InGuid);

		//
		// It's possible to request a reservation for a chunk already in the backing store when reading from the
		// disk into memory (i.e. with a non-direct read). However it can't already be in memory.
		//
		if (StoredChunk.ChunkData.Num())
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency failure: Reserving read space for a chunk already in memory: %s"), *WriteToString<40>(InGuid));
			return FMutableMemoryView();
		}
		else
		{
			StoredChunk.LastUsageIndex = LastUsageIndex;
			StoredChunk.bCommitted = false;
			StoredChunk.ChunkData.AddUninitialized(InChunkSize);
			StoredChunk.ChunkSize = InChunkSize;
			StoredChunk.LockCount = 1;
			CurrentMemoryLoad += InChunkSize;
			if (CurrentMemoryLoad > PeakMemoryLoad)
			{
				PeakMemoryLoad = CurrentMemoryLoad;
				Stats.MemoryPeakUsageBytes = PeakMemoryLoad;
			}
		}

		FMutableMemoryView ChunkBuffer(StoredChunk.ChunkData.GetData(), StoredChunk.ChunkData.Num());
		return ChunkBuffer;
	}
	
	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool CheckRetirements(int32 CurrentUsageIndex)
	{
		// 6 just chosen because we wouldn't expect a ton of things at once
		// but we could get several. shrug.
		TArray<FGuid, TInlineAllocator<6>> GuidsToDelete;

		for (TPair<FGuid, FStoredChunk>& Pair : StoredChunks)
		{
			if (Pair.Value.LastUsageIndex < CurrentUsageIndex)
			{
				GuidsToDelete.Add(Pair.Key);
			}
		}

		for (FGuid& ToDelete : GuidsToDelete)
		{
			FStoredChunk* Chunk = StoredChunks.Find(ToDelete);
			if (Chunk->bBackedByDisk)
			{
				if (!ReleaseBackingStoreEntry(ToDelete, *Chunk))
				{
					return false;
				}
			}

			if (Chunk->LockCount)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Retiring memory entry with lock count! %s %d"), *WriteToString<40>(ToDelete), Chunk->LockCount);
				return false;
			}
			
			if (Chunk->ChunkData.Num())
			{
				if (!Chunk->bCommitted)
				{
					UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Retiring memory entry that never got committed! %s"), *WriteToString<40>(ToDelete));
					return false;
				}

				CurrentMemoryLoad -= Chunk->ChunkSize;
			}

			StoredChunks.Remove(ToDelete);

			ParentConstructor->SetChunkLocation(ToDelete, EConstructorChunkLocation::Retired);
		}

		return true;
	}
	
	// This should only be for paged out chunks... in-memory chunks should be handled directly.
	// Consistency failures in this will pass as data read fails, which will end up redirecting to
	// the cloud. However the post-file consistency check will catch it and fail the install.
	// Constructor thread only for this chunk source
	[[nodiscard]] FRequestProcessFn CreateRequest(const FGuid& DataId, FMutableMemoryView DestinationBuffer, void* UserPtr, FChunkRequestCompleteDelegate CompleteFn)
	{
		// LONGTERM - can we make this fully async once we have a pipe we can prevent reentrancy on or ReadAtOffset API?
		TRACE_CPUPROFILER_EVENT_SCOPE(BackingStoreRead);

		bool bSuccess = true;
		// This should go to a generate IO dispatch with completion function I think? idk...
		// This acts as any other IO source - we don't know where it's going, it might be going back into memory.
		FStoredChunk* StoredChunk = StoredChunks.Find(DataId);
		bSuccess = StoredChunk != nullptr;

		if (bSuccess && !StoredChunk->bBackedByDisk)
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Trying to page in a chunk that isn't paged out! %s"), *WriteToString<40>(DataId));
			bSuccess = false;
		}

		FBackingStoreUsedSpan* UsedEntry = nullptr;
		if (bSuccess)
		{
			UsedEntry = UsedDiskSpans.Find(DataId);
			if (!UsedEntry)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Backing store entry not found for paged out chunk! %s"), *WriteToString<40>(DataId));
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			ReadCount++;
			BackingStoreFileHandle->Seek(UsedEntry->Offset());
			if (!BackingStoreFileHandle->Read((uint8*)DestinationBuffer.GetData(), DestinationBuffer.GetSize()))
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Backing store page-in failed read! %s"), *WriteToString<40>(DataId));
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(BSR_Hash);
			if (FXxHash64::HashBuffer(DestinationBuffer) != UsedEntry->Hash)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency Failure: Backing store page-in failed hash check! %s"), *WriteToString<40>(DataId));
				bSuccess = false;
			}
		}

		if (!bSuccess)
		{
			Stats.DiskLoadFailureCount++;
		}

		Stats.DiskChunkLoadCount++;
		CompleteFn.Execute(DataId, false, !bSuccess, UserPtr);
		return [](bool) { return; };
	}

	// false means consistency failure
	// Constructor thread
	[[nodiscard]] bool CheckNoLocks(bool bIsHarvest)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(BackingStore_CheckNoLocks);
		// After a file there should be no locked chunks since there are no reads.
		bool bSuccess = true;
		for (TPair<FGuid, FStoredChunk>& Chunk : StoredChunks)
		{
			if (Chunk.Value.LockCount)
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("Chunk %s locked with count %d after %s!"), *WriteToString<40>(Chunk.Key), Chunk.Value.LockCount, bIsHarvest ? TEXT("harvest") : TEXT("file completion"));
				bSuccess = false;
			}
		}

		if (bSuccess)
		{
			bSuccess = ConsistencyCheck();
		}

		return bSuccess;
	}

	// This is for the disk store - we don't free the page entry until it retires, so it's always available and can
	// be read direct.
	virtual int32 GetChunkUnavailableAt(const FGuid& DataId) const override { return TNumericLimits<int32>::Max(); }

	FMemoryView GetViewForChunk(const FGuid& DataId) const
	{
		FStoredChunk const* StoredChunk = StoredChunks.Find(DataId);
		if (!StoredChunk)
		{
			return FMemoryView();
		}

		return FMemoryView(StoredChunk->ChunkData.GetData(), StoredChunk->ChunkData.Num());
	}
}; // FChunkBackingStore

/* FBuildPatchFileConstructor implementation
 *****************************************************************************/
FBuildPatchFileConstructor::FBuildPatchFileConstructor(
	FFileConstructorConfig InConfiguration, IFileSystem* InFileSystem, 
	IConstructorChunkDbChunkSource* InChunkDbChunkSource, IConstructorCloudChunkSource* InCloudChunkSource, IConstructorInstallChunkSource* InInstallChunkSource, 
	IChunkReferenceTracker* InChunkReferenceTracker, IInstallerError* InInstallerError, IInstallerAnalytics* InInstallerAnalytics, IMessagePump* InMessagePump,
	IFileConstructorStat* InFileConstructorStat, TMap<FGuid, EConstructorChunkLocation>&& InChunkLocations)
	: Configuration(MoveTemp(InConfiguration))
	, bIsDownloadStarted(false)
	, bInitialDiskSizeCheck(false)
	, bIsPaused(false)
	, bShouldAbort(false)
	, FileSystem(InFileSystem)
	, ChunkDbSource(InChunkDbChunkSource)
	, InstallSource(InInstallChunkSource)
	, CloudSource(InCloudChunkSource)
	, ChunkLocations(MoveTemp(InChunkLocations))
	, ChunkReferenceTracker(InChunkReferenceTracker)
	, InstallerError(InInstallerError)
	, InstallerAnalytics(InInstallerAnalytics)
	, MessagePump(InMessagePump)
	, FileConstructorStat(InFileConstructorStat)
	, TotalJobSize(0)
	, ByteProcessed(0)
	, RequiredDiskSpace(0)
	, AvailableDiskSpace(0)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FileConstructor_ctor);

	bStallWhenFileSystemThrottled = bCVarStallWhenFileSystemThrottled;
	if (Configuration.StallWhenFileSystemThrottled.IsSet())
	{
		UE_LOG(LogBuildPatchServices, Display, TEXT("Overridding StallWhenFileSystemThrottled to: %d, cvar was %d"), Configuration.StallWhenFileSystemThrottled.GetValue(), bStallWhenFileSystemThrottled);
		bStallWhenFileSystemThrottled = Configuration.StallWhenFileSystemThrottled.GetValue();
	}

	bAllowMultipleFilesInFlight = bCVarAllowMultipleFilesInFlight;

	BackingStore.Reset(new FChunkBackingStore(this, Configuration.InstallDirectory, BackingStoreStats));

	// Count initial job size
	ConstructionList.Reserve(Configuration.ConstructList.Num());
	
	// Track when we will complete files in the reference chain.
	int32 CurrentPosition = 0;
	FileCompletionPositions.Reserve(Configuration.ConstructList.Num());

	// The first index after the file is complete.
	// \todo with the construction list now stable across the install, we could
	// key this off a StringView and save the allocs. For another CL...
	TMap<FString, int32> FileRetirementPositions;

	for (const FString& FileToConstructName : Configuration.ConstructList)
	{
		const FFileManifest* FileManifest = Configuration.ManifestSet->GetNewFileManifest(FileToConstructName);

		FFileToConstruct FileToConstruct;
		FileToConstruct.FileManifest = FileManifest;

		// If we are missing the file manifest, we will fail to install when we get to the file. However,
		// we guarantee a 1:1 mapping with the arrays we are filling here so we use invalid data for those
		// slots (which won't get used).
		// Maybe we should fail immediately? Need to review whether we can fail in the constructor or we need
		// to delay until Run().		
		if (FileManifest)
		{
			TotalJobSize += FileManifest->FileSize;
		
			// We will be advancing the chunk reference tracker by this many chunks.
			int32 AdvanceCount = FileManifest->ChunkParts.Num();
			CurrentPosition += AdvanceCount;
		}

		FileCompletionPositions.Add(CurrentPosition);
		FileRetirementPositions.Add(FileToConstructName, CurrentPosition);
		ConstructionList.Add(MoveTemp(FileToConstruct));
	}

	// Let the install source know when we're going to be deleting their sources.
	bool bHasInstallSource = false;
	if (InstallSource)
	{
		InstallSource->SetFileRetirementPositions(MoveTemp(FileRetirementPositions));	

		if (InstallSource->GetAvailableChunks().Num() != 0)
		{
			bHasInstallSource = true;

			// We need to set up a dependency chain so that files can know when they can start.			
			TMap<FStringView, int32> FileToIndexMap;
			FileToIndexMap.Reserve(Configuration.ConstructList.Num());
			for (int32 FileConstructIndex = 0; FileConstructIndex < Configuration.ConstructList.Num(); FileConstructIndex++)
			{
				// The construct list has filenames that match the manifest, and afaict manifest filenames are already normalized.
				FileToIndexMap.Add(Configuration.ConstructList[FileConstructIndex], FileConstructIndex);
			}

			for (int32 FileConstructIndex = 0; FileConstructIndex < Configuration.ConstructList.Num(); FileConstructIndex++)
			{
				FFileToConstruct& FileToConstruct = ConstructionList[FileConstructIndex];				
				if (!FileToConstruct.FileManifest)
				{
					continue;
				}

				for (const FChunkPart& ChunkPart : FileToConstruct.FileManifest->ChunkParts)
				{
					EConstructorChunkLocation ChunkLocation = ChunkLocations[ChunkPart.Guid];
					if (ChunkLocation == EConstructorChunkLocation::Install)
					{
						InstallSource->EnumerateFilesForChunk(ChunkPart.Guid, [FileConstructIndex, &FileToIndexMap, &FileToConstruct, NormalizedInstallDirectory = &Configuration.InstallDirectory](const FString& ChunkNormalizedInstallDirectory, const FString& NormalizedFilenameContainingChunk)
							{
								if (*NormalizedInstallDirectory != ChunkNormalizedInstallDirectory)
								{
									// We aren't affecting that install source so it's not a dependency.
									return;
								}

								const int32* DependentFileIndexPtr = FileToIndexMap.Find(NormalizedFilenameContainingChunk);
								if (DependentFileIndexPtr)
								{
									int32 DependentFileIndex = *DependentFileIndexPtr;

									// If the file is constructed before us then we can't start until it's ready.
									// We only care about the latest file.
									if (DependentFileIndex < FileConstructIndex)
									{
										FileToConstruct.LatestDependentInstallSource = FMath::Max(FileToConstruct.LatestDependentInstallSource, DependentFileIndex);
									}
								}
							}
						);
					}
				}
			}			
		}
	}

	//
	// Create the threads we are allowed to.
	//
	bHasChunkDbSource = ChunkDbSource->GetAvailableChunks().Num() != 0;
	
	// Default everything to running synchronously.
	for (int8& ThreadAssignment : ThreadAssignments)
	{
		ThreadAssignment = -1;
	}
	WriteThreadIndex = -1;

	bool bSpawnAdditionalIOThreads = Configuration.bDefaultSpawnAdditionalIOThreads;
	if (GConfig->GetBool(TEXT("Portal.BuildPatch"), TEXT("ConstructorSpawnAdditionalIOThreads"), bSpawnAdditionalIOThreads, GEngineIni))
	{
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got INI ConstructorSpawnAdditionalIOThreads = %d"), bSpawnAdditionalIOThreads);
	}

	if (Configuration.SpawnAdditionalIOThreads.IsSet())
	{
		bSpawnAdditionalIOThreads = Configuration.SpawnAdditionalIOThreads.Get(bSpawnAdditionalIOThreads);
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got override ConstructorSpawnAdditionalIOThreads = %d"), bSpawnAdditionalIOThreads);
	}

	// For now we have to strictly assign jobs to threads so that we don't accidentally
	// hit the same file handle on multiple threads. Once we have proper ReadAtOffset support
	// we can go nuts (and just use UE::Tasks)
	// LONGTERM try using UE::Pipe and just blasting everything on tasks?
	int32 ThreadCount = 0;
	if (bSpawnAdditionalIOThreads)
	{
		WriteThreadIndex = 0;
		ThreadCount++;

		if (bHasInstallSource)
		{
			ThreadAssignments[EConstructorChunkLocation::Install] = ThreadCount;
			ThreadCount++;
		}
		if (bHasChunkDbSource)
		{
			ThreadAssignments[EConstructorChunkLocation::ChunkDb] = ThreadCount;
			ThreadCount++;
		}
	}
	
	WakeUpDispatchThreadEvent = FPlatformProcess::GetSynchEventFromPool();
	CloudSource->SetWakeupFunction([WakeUpDispatchThreadEvent = WakeUpDispatchThreadEvent]()
		{
			WakeUpDispatchThreadEvent->Trigger();
		}
	);

	// Preallocate the arrays so we don't get any movement.
	Threads.SetNum(ThreadCount);
	ThreadWakeups.SetNum(ThreadCount);
	ThreadCompleteEvents.SetNum(ThreadCount);
	ThreadJobPostingLocks.SetNum(ThreadCount); // Default init is fine for these.
	ThreadJobPostings.SetNum(ThreadCount);  // Default init is fine for these.

	for (int32 ThreadIndex = 0; ThreadIndex < ThreadCount; ThreadIndex++)
	{
		Threads[ThreadIndex] = Configuration.SharedContext->CreateThread();
		ThreadWakeups[ThreadIndex] = FPlatformProcess::GetSynchEventFromPool();
		ThreadCompleteEvents[ThreadIndex] = FPlatformProcess::GetSynchEventFromPool();

		Threads[ThreadIndex]->RunTask([this, ThreadIndex]() { GenericThreadFn(ThreadIndex); });
	}

}

FBuildPatchFileConstructor::~FBuildPatchFileConstructor()
{
	// Wait for threads to shut down.
	Abort();
	
	
	for (int32 ThreadIndex = 0; ThreadIndex < Threads.Num(); ThreadIndex++)
	{
		ThreadCompleteEvents[ThreadIndex]->Wait();
	}

	for (int32 ThreadIndex = 0; ThreadIndex < Threads.Num(); ThreadIndex++)
	{
		FPlatformProcess::ReturnSynchEventToPool(ThreadWakeups[ThreadIndex]);
		FPlatformProcess::ReturnSynchEventToPool(ThreadCompleteEvents[ThreadIndex]);
		Configuration.SharedContext->ReleaseThread(Threads[ThreadIndex]);
	}

	FPlatformProcess::ReturnSynchEventToPool(WakeUpDispatchThreadEvent);
	WakeUpDispatchThreadEvent = nullptr;
}

void FBuildPatchFileConstructor::SetPaused(bool bInIsPaused)
{
	bool bWasPaused = bIsPaused.AtomicSet(bInIsPaused);
	if (bWasPaused && !bInIsPaused)
	{
		// If we unpaused, the dispatch thread might be waiting in an event for us
		// to tell it to unpark.
		WakeUpDispatchThreadEvent->Trigger();
	}
}

void FBuildPatchFileConstructor::Abort()
{
	bool bAlreadyAborted = bShouldAbort.AtomicSet(true);

	UE_LOG(LogBuildPatchServices, Verbose, TEXT("Issuing abort (previously aborted: %d)"), bAlreadyAborted)

	if (bAlreadyAborted)
	{
		return;
	}

	// Make sure to wake up any threads that might be parked so they can bail.
	WakeUpDispatchThreadEvent->Trigger();
	
	for (int32 ThreadIndex = 0; ThreadIndex < Threads.Num(); ThreadIndex++)
	{
		ThreadWakeups[ThreadIndex]->Trigger();
	}

}

void FBuildPatchFileConstructor::SetChunkLocation(const FGuid& InGuid, EConstructorChunkLocation InNewLocation)
{
	// NOTE - reentrant. We assume a) that ChunkLocations is filled before threading
	// and that b) no guids are used across concurrent actions.
	FRWScopeLock ChunkLock(ChunkLocationsLock, SLT_Write);
	EConstructorChunkLocation* Location = ChunkLocations.Find(InGuid);
	if (!Location)
	{
		UE_LOG(LogBuildPatchServices, Error, TEXT("Consistency failure: setting chunk location for non existent chunk %s"), *WriteToString<40>(InGuid));
	}
	else
	{
		if (*Location != EConstructorChunkLocation::Cloud &&
			InNewLocation == EConstructorChunkLocation::Cloud)
		{
			uint64 ChunkSize = Configuration.ManifestSet->GetDownloadSize(InGuid);

			UE_LOG(LogBuildPatchServices, VeryVerbose, TEXT("Migrating chunk to cloud: %s, %llu bytes"), *WriteToString<40>(InGuid), ChunkSize);

			DownloadRequirement += ChunkSize;
			CloudSource->PostRequiredByteCount(DownloadRequirement);
		}

		*Location = InNewLocation;
	}
}

void FBuildPatchFileConstructor::QueueGenericThreadTask(int32 ThreadIndex, IConstructorChunkSource::FRequestProcessFn&& Task)
{
	// No thread for this task - run synchronously
	if (ThreadIndex == -1 || ThreadIndex >= Threads.Num())
	{
		Task(false);
		WakeUpDispatchThreadEvent->Trigger();
		return;
	}

	bool bPosted = false;
	ThreadJobPostingLocks[ThreadIndex].Lock();
	if (!bShouldAbort)
	{
		ThreadJobPostings[ThreadIndex].Add(MoveTemp(Task));
		bPosted = true;
	}
	ThreadJobPostingLocks[ThreadIndex].Unlock();
	ThreadWakeups[ThreadIndex]->Trigger();

	if (!bPosted)
	{
		// This means we aborted during the queue - make sure to run
		Task(true);
	}
}


void FBuildPatchFileConstructor::GenericThreadFn(int32 ThreadIndex)
{
	for (;;)
	{
		ThreadWakeups[ThreadIndex]->Wait();

		TArray<IConstructorChunkSource::FRequestProcessFn> GrabbedJobs;
		
		{
			ThreadJobPostingLocks[ThreadIndex].Lock();
			GrabbedJobs = MoveTemp(ThreadJobPostings[ThreadIndex]);
			ThreadJobPostingLocks[ThreadIndex].Unlock();
		}

		if (bShouldAbort)
		{
			for (IConstructorChunkSource::FRequestProcessFn& AbortJob : GrabbedJobs)
			{
				AbortJob(true);
			}
			WakeUpDispatchThreadEvent->Trigger();
			break;
		}

		for (IConstructorChunkSource::FRequestProcessFn& Job : GrabbedJobs)
		{
			Job(false);
		}

		WakeUpDispatchThreadEvent->Trigger();
	}

	ThreadCompleteEvents[ThreadIndex]->Trigger();
}

bool FBuildPatchFileConstructor::HarvestChunksForCompletedFile(const FString& CompletedFullPathFileName)
{
	UE_LOG(LogBuildPatchServices, Verbose, TEXT("Harvesting source: %s"), *CompletedFullPathFileName);

	TRACE_CPUPROFILER_EVENT_SCOPE(Harvest);
	// We need to grab any chunks from install sources that are no longer available.
	// Anything that's already been loaded is already placed into the memory store appropriately,
	// but anything that _hasn't_ needs to be pulled out.	
	TSet<FGuid> FileChunks;
	InstallSource->GetChunksForFile(CompletedFullPathFileName, FileChunks);

	struct FNeededChunk
	{
		FGuid Id;
		int32 LastUsageIndex;
		int32 NextUsageIndex;
		int32 ChunkSize;
	};

	TArray<FNeededChunk> ChunksFromFileWeNeed;

	{
		FRWScopeLock ChunkLock(ChunkLocationsLock, SLT_ReadOnly);

		int32 CurrentUsageIndex = ChunkReferenceTracker->GetCurrentUsageIndex();
		for (const FGuid& FileChunk : FileChunks)
		{
			EConstructorChunkLocation* Location = ChunkLocations.Find(FileChunk);
			if (Location && *Location == EConstructorChunkLocation::Install)
			{
				int32 LastUsageIndex = 0;
				int32 NextUsageIndex = ChunkReferenceTracker->GetNextUsageForChunk(FileChunk, LastUsageIndex);

				if (NextUsageIndex == -1 ||
					LastUsageIndex < CurrentUsageIndex)
				{
					// The chunk is no longer needed
					*Location = EConstructorChunkLocation::Retired;
					continue;
				}

				int32 ChunkSize = Configuration.ManifestSet->GetChunkInfo(FileChunk)->WindowSize;

				ChunksFromFileWeNeed.Add({FileChunk, LastUsageIndex, NextUsageIndex, ChunkSize});
			}
		}
	}

	if (ChunksFromFileWeNeed.Num() == 0)
	{
		return true;
	}

	// Try to load all the chunks that are about to go away. If it fails we don't particularly
	// care since we would have fallen back to cloud anyway.

	// There's some care here - if we just kick off a ton of reads, all those backing store
	// entries are locked during the reads so we have to allocate space and can't page anything
	// out. This is fine if we can load the whole file, but under constrained memory we want to
	// only keep stuff that's going to get used soon - so we load the stuff we _aren't_ going to use
	// soon so it can get paged out. Then we load in batches so we can release the locks and let them
	// page out. 
	// LONGTERM - detect this condition and write directly to the disk backing store? Ideally this
	// would be something we can retain across restarts as right now any harvested chunks get lost
	// on abort and cause a download from the cloud source (not chunkdb!)
	Algo::SortBy(ChunksFromFileWeNeed, &FNeededChunk::NextUsageIndex, TGreater<int32>());

	constexpr int32 HarvestBatchSize = 16 << 20; // 16 MB

	bool bHarvestSuccess = true;
	for (int32 ChunkIndex = 0; ChunkIndex < ChunksFromFileWeNeed.Num();)
	{
		int32 BatchSize = 0;
		int32 ChunkEndIndex = ChunkIndex + 1;
		for (; ChunkEndIndex < ChunksFromFileWeNeed.Num(); ChunkEndIndex++)
		{
			if (BatchSize + ChunksFromFileWeNeed[ChunkEndIndex].ChunkSize > HarvestBatchSize)
			{
				break;
			}
			BatchSize += ChunksFromFileWeNeed[ChunkEndIndex].ChunkSize;
		}

		PendingHarvestRequests.store(ChunkEndIndex - ChunkIndex, std::memory_order_release);

		for (int32 DispatchIndex = ChunkIndex; DispatchIndex < ChunkEndIndex; DispatchIndex++)
		{
			FNeededChunk& Chunk = ChunksFromFileWeNeed[DispatchIndex];

			SetChunkLocation(Chunk.Id, EConstructorChunkLocation::Memory);

			FMutableMemoryView Destination = BackingStore->ReserveAndLockEntry(Chunk.Id, Chunk.ChunkSize, Chunk.LastUsageIndex);
			if (Destination.GetSize() == 0)
			{
				// Call the completion function so we decrement the request count,
				// but this is a consistency failure so we can't use the results.
				ChunkHarvestCompletedFn(Chunk.Id, false, false, 0);
				bHarvestSuccess = false;
			}

			IConstructorChunkSource::FRequestProcessFn HarvestFn = InstallSource->CreateRequest(Chunk.Id, Destination, 0, IConstructorChunkSource::FChunkRequestCompleteDelegate::CreateRaw(this, &FBuildPatchFileConstructor::ChunkHarvestCompletedFn));

			HarvestFn(false);
		}

		// The read is synchronous but the verification is not, so we still need to do the wait.
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(HarvestWait);
			WakeUpDispatchThreadEvent->Wait();
		}

		if (bHarvestSuccess)
		{
			// Unlock any memory store entries
			FRWScopeLock ChunkLock(ChunkLocationsLock, SLT_ReadOnly);

			for (int32 DispatchIndex = ChunkIndex; DispatchIndex < ChunkEndIndex; DispatchIndex++)
			{
				// The read could have failed - in which case the location was switched to cloud from Memory.
				// We handle this here so we don't have to deal with synchronization in the completion function.
				if (ChunkLocations[ChunksFromFileWeNeed[DispatchIndex].Id] == EConstructorChunkLocation::Cloud)
				{
					if (!BackingStore->DereserveHarvestingEntry(ChunksFromFileWeNeed[DispatchIndex].Id))
					{
						bHarvestSuccess = false;
					}
				}
				else
				{
					if (!BackingStore->CommitAndReleaseEntry(ChunksFromFileWeNeed[DispatchIndex].Id))
					{
						bHarvestSuccess = false;
					}
				}
			}
		}

		if (bHarvestSuccess && !bAllowMultipleFilesInFlight)
		{
			bHarvestSuccess = BackingStore->CheckNoLocks(true);
		}

		ChunkIndex = ChunkEndIndex;

		if (bShouldAbort || !bHarvestSuccess)
		{
			break;
		}
	}

	return bHarvestSuccess;
}

void FBuildPatchFileConstructor::Run()
{
	FileConstructorStat->OnTotalRequiredUpdated(TotalJobSize);

	
	// We'd really like to have a sense of what each chunk looks like, size-wise, so that
	// we know things like how many downloads we expect per batch.
	// Map of window sizes to counts of that size.
	TMap<uint32, int32> WindowSizes;

	// lock not requried - no threads yet.
	DownloadRequirement = 0;
	for (const TPair<FGuid, EConstructorChunkLocation>& ChunkLocation : ChunkLocations)
	{
		FChunkInfo const* ChunkInfo = Configuration.ManifestSet->GetChunkInfo(ChunkLocation.Key);
		WindowSizes.FindOrAdd(ChunkInfo->WindowSize)++;

		if (ChunkLocation.Value == EConstructorChunkLocation::Cloud)
		{
			DownloadRequirement += ChunkInfo->FileSize;
		}
	}
	CloudSource->PostRequiredByteCount(DownloadRequirement);

	{
		ExpectedChunkSize = 0;
		int32 LargestWindowCount = 0;
		for (TPair<uint32, int32>& Entry : WindowSizes)
		{
			if (Entry.Value > LargestWindowCount)
			{
				LargestWindowCount = Entry.Value;
				ExpectedChunkSize = Entry.Key;
			}
		}

		if (ExpectedChunkSize)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Expected chunk size: %d count %d"), ExpectedChunkSize, LargestWindowCount);
		}
		else
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Can't find largest chunk size, using 1MB"));
			ExpectedChunkSize = 1 << 20;
		}
	}

	// We disable resume if we are a fresh install that's below a certain threshold in order to minimize
	// io operations.
	bool bResumeEnabled = true;
	if (!Configuration.ManifestSet->ContainsUpdate())
	{
		int64 DisableResumeBelowBytes = ((int64)Configuration.DisableResumeBelowMB.Get(CVarDisableResumeBelowMB)) << 20;
		if (DisableResumeBelowBytes > TotalJobSize)
		{
			UE_LOG(LogBuildPatchServices, Log, TEXT("Disabling resume: JobSize = %s, Disable = %s, from %s"), *FormatNumber(TotalJobSize), *FormatNumber(DisableResumeBelowBytes), Configuration.DisableResumeBelowMB.IsSet() ? TEXT("config") : TEXT("cvar"));
			bResumeEnabled = false;
		}
	}

	const FString ResumeDataFilename = Configuration.MetaDirectory / TEXT("$resumeData");
	FResumeData ResumeData(FileSystem, Configuration.ManifestSet, Configuration.StagingDirectory, ResumeDataFilename);

	if (bResumeEnabled)
	{
		IFileManager::Get().MakeDirectory(*Configuration.MetaDirectory);

		// Check for resume data, we need to also look for a legacy resume file to use instead in case we are resuming from an install of previous code version.
		const FString LegacyResumeDataFilename = Configuration.StagingDirectory / TEXT("$resumeData");		
		const bool bHasLegacyResumeData = FileSystem->FileExists(*LegacyResumeDataFilename);
		// If we find a legacy resume data file, lets move it first.
		if (bHasLegacyResumeData)
		{
			FileSystem->MoveFile(*ResumeDataFilename, *LegacyResumeDataFilename);
		}

		ResumeData.InitResume();

		// Remove incompatible files
		if (ResumeData.bHasResumeData)
		{
			for (const FString& FileToConstruct : Configuration.ConstructList)
			{
				ResumeData.CheckFile(FileToConstruct);
				const bool bFileIncompatible = ResumeData.FilesIncompatible.Contains(FileToConstruct);
				if (bFileIncompatible)
				{
					GLog->Logf(TEXT("FBuildPatchFileConstructor: Deleting incompatible stage file %s"), *FileToConstruct);
					FileSystem->DeleteFile(*(Configuration.StagingDirectory / FileToConstruct));
				}
			}
		}

		// Save for started versions
		TSet<FString> ResumeIds;
		const bool bCheckLegacyIds = false;

		Configuration.ManifestSet->GetInstallResumeIds(ResumeIds, bCheckLegacyIds);
		ResumeData.SaveOut(ResumeIds);
	}

	// Start resume progress at zero or one.
	FileConstructorStat->OnResumeStarted();

	// While we have files to construct, run.
	ConstructFiles(ResumeData);

	// Mark resume complete if we didn't have work to do.
	if (!bIsDownloadStarted)
	{
		FileConstructorStat->OnResumeCompleted();
	}
	FileConstructorStat->OnConstructionCompleted();
}

uint64 FBuildPatchFileConstructor::GetRequiredDiskSpace()
{
	return RequiredDiskSpace.load(std::memory_order_relaxed);
}

uint64 FBuildPatchFileConstructor::GetAvailableDiskSpace()
{
	return AvailableDiskSpace.load(std::memory_order_relaxed);
}

FBuildPatchFileConstructor::FOnBeforeDeleteFile& FBuildPatchFileConstructor::OnBeforeDeleteFile()
{
	return BeforeDeleteFileEvent;
}

void FBuildPatchFileConstructor::CountBytesProcessed( const int64& ByteCount )
{
	ByteProcessed += ByteCount;
	FileConstructorStat->OnProcessedDataUpdated(ByteProcessed);
}

int64 FBuildPatchFileConstructor::GetRemainingBytes()
{
	// Need the sum of the output sizes of files not yet started.
	// Since this gets called from any thread, construction will continue
	// as we calculate this, but all the structures are stable as long as
	// the constructor is still valid memory.

	int32 LocalNextIndexToConstruct = NextIndexToConstruct.load(std::memory_order_acquire);

	uint64 RemainingBytes = 0;
	for (int32 ConstructIndex = LocalNextIndexToConstruct; ConstructIndex < ConstructionList.Num(); ConstructIndex++)
	{
		if (ConstructionList[ConstructIndex].FileManifest)
		{
			RemainingBytes += ConstructionList[ConstructIndex].FileManifest->FileSize;
		}
	}

	return RemainingBytes;
}

uint64 FBuildPatchFileConstructor::CalculateInProgressDiskSpaceRequired(const FFileManifest& InProgressFileManifest, uint64 InProgressFileAmountWritten)
{
	if (Configuration.InstallMode == EInstallMode::DestructiveInstall)
	{
		// The simplest method will be to run through each high level file operation, tracking peak disk usage delta.

		// We know we need enough space to finish writing this file
		uint64 RemainingThisFileSpace = InProgressFileManifest.FileSize - InProgressFileAmountWritten;
		
		int64 DiskSpaceDeltaPeak = RemainingThisFileSpace;
		int64 DiskSpaceDelta = RemainingThisFileSpace;

		// Then we move this file over.
		{
			const FFileManifest* OldFileManifest = Configuration.ManifestSet->GetCurrentFileManifest(InProgressFileManifest.Filename);
			if (OldFileManifest)
			{
				DiskSpaceDelta -= OldFileManifest->FileSize;
			}

			// We've already accounted for the new file above, so we could be pretty negative if we resumed the file
			// almost at the end and had an existing file we're deleting.
		}

		// Loop through all files to be made next, in order.
		int32 LocalNextIndexToConstruct = NextIndexToConstruct.load(std::memory_order_acquire);
		for (int32 ConstructionIndex = LocalNextIndexToConstruct; ConstructionIndex < ConstructionList.Num(); ConstructionIndex++)
		{
			const FFileManifest* NewFileManifest = ConstructionList[ConstructionIndex].FileManifest;
			if (!NewFileManifest)
			{
				continue;
			}

			const FFileManifest* OldFileManifest = Configuration.ManifestSet->GetCurrentFileManifest(Configuration.ConstructList[ConstructionIndex]);

			// First we would need to make the new file.
			DiskSpaceDelta += NewFileManifest->FileSize;
			if (DiskSpaceDeltaPeak < DiskSpaceDelta)
			{
				DiskSpaceDeltaPeak = DiskSpaceDelta;
			}
			// Then we can remove the current existing file.
			if (OldFileManifest)
			{
				DiskSpaceDelta -= OldFileManifest->FileSize;
			}
		}
		return DiskSpaceDeltaPeak;
	}
	else
	{
		// When not destructive, we always stage all new and changed files.
		uint64 RemainingFilesSpace = GetRemainingBytes();
		uint64 RemainingThisFileSpace = InProgressFileManifest.FileSize - InProgressFileAmountWritten;
		return RemainingFilesSpace + RemainingThisFileSpace;
	}
}

uint64 FBuildPatchFileConstructor::CalculateDiskSpaceRequirementsWithDeleteDuringInstall()
{
	if (ChunkDbSource == nullptr)
	{
		// invalid use.
		return 0;
	}

	// These are the sizes at after each file that we _started_ with. This is the size after retirement for the
	// file at those positions.
	TArray<uint64> ChunkDbSizesAtPosition;
	uint64 TotalChunkDbSize = ChunkDbSource->GetChunkDbSizesAtIndexes(FileCompletionPositions, ChunkDbSizesAtPosition);

	// Strip off the files we've completed.
	int32 CompletedFileCount = NextIndexToConstruct.load(std::memory_order_acquire);

	// Since we are called after the first file is popped (but before it's actually done), we have one less completed.
	check(CompletedFileCount > 0); // should ALWAYS be at least 1!
	if (CompletedFileCount > 0)
	{
		CompletedFileCount--;
	}

	uint64 MaxDiskSize = FBuildPatchUtils::CalculateDiskSpaceRequirementsWithDeleteDuringInstall(
		Configuration.ConstructList, CompletedFileCount, Configuration.ManifestSet, ChunkDbSizesAtPosition, TotalChunkDbSize);

	// Strip off the data we already have on disk.
	uint64 PostDlSize = 0;
	if (MaxDiskSize > TotalChunkDbSize)
	{
		PostDlSize = MaxDiskSize - TotalChunkDbSize;
	}

	return PostDlSize;
}

struct FBatchState;
struct FRequestInfo
{
	struct FRequestSplat
	{
		int32 DestinationOffset;
		int32 OffsetInChunk;
		int32 BytesToCopy;
	};

	FGuid Guid;

	// We can do a lot of shortcuts if we are working with the entire chunk
	int32 ChunkSize = 0;

	// We could request the same guid multiple times
	// for the same buffer... in this case we want one
	// request but we need to remember to splat it
	// afterwards.
	TArray<FRequestSplat, TInlineAllocator<1>> Splats;
	
	// The read goes here - this is usually directly into the write buffer,
	// but we might need to duplicate out of this (and this might be memory
	// store owned if we don't use the whole chunk)
	FMutableMemoryView ReadBuffer;

	FBatchState* Batch = nullptr;

	// Splats offset into this.
	FMutableMemoryView DestinationBuffer;

	FFileConstructionState* File = nullptr;

	// We can only read direct in some cases.
	bool bReadIntoMemoryStore = false;

	// Never save back to memory store if it came from it.
	bool bSourceIsMemoryStore = false;
	bool bAborted = false;
	bool bFailedToRead = false;
	bool bLaunchedFallback = false;

	int32 ChunkUnavailableAt = TNumericLimits<int32>::Max();
};


struct FBatchState
{
	int32_t BatchId = 0;

	TMap<FGuid, FRequestInfo> Requests;
	std::atomic_int32_t PendingRequestCount = 0;
	std::atomic_int32_t FailedRequestCount = 0;

	// Reads for the batch end up here, and will be written to the target
	// file in order.
	FMutableMemoryView BatchBuffer;

	EConstructionError Error = EConstructionError::None;
	FGuid ErrorContextGuid;

	FFileConstructionState* OwningFile = nullptr;

	int32 StartChunkPartIndex = 0;
	int32 ChunkCount = 0;

	// If true, this batch never reads or writes, it exists to complete the empty file in order.
	bool bIsEmptyFileSentinel = false;
	bool bNeedsWrite = false;
	bool bIsReading = false;
	bool bIsWriting = false;	

	// Set by the completing threads when the batch is done.
	std::atomic<bool> bIsFinished = true;
	
	FBatchState()
	{
		static std::atomic_int32_t UniqueBatchId = 1;
		BatchId = UniqueBatchId.fetch_add(1);
	}
};

// Called from basically any thread.
void FBuildPatchFileConstructor::ChunkHarvestCompletedFn(const FGuid& Guid, bool bAborted, bool bFailedToRead, void* UserPtr)
{
	if (bFailedToRead)
	{
		// We tell the main thread this failed by setting the location since that's thread
		// safe.
		SetChunkLocation(Guid, EConstructorChunkLocation::Cloud);
	}

	if (PendingHarvestRequests.fetch_add(-1, std::memory_order_relaxed) == 1)
	{
		WakeUpDispatchThreadEvent->Trigger();
	}
}

// Called from basically any thread.
void FBuildPatchFileConstructor::RequestCompletedFn(const FGuid& Guid, bool bAborted, bool bFailedToRead, void* UserPtr)
{
	FRequestInfo* Request = (FRequestInfo*)UserPtr;
	Request->bAborted = bAborted;
	Request->bFailedToRead = bFailedToRead;

	// If failed but didn't abort and we haven't already kicked the fallback, kick the fallback
	if (!bAborted && bFailedToRead && !Request->bLaunchedFallback)
	{		
		Request->bLaunchedFallback = true;
		Request->bFailedToRead = false;

		// We couldn't read the expected source. This means we need to fall back to the cloud source.

		// This should be safe because the values already exist in the map and we only
		// ever have 1 request for a Guid active at one time. However if we've already read into the
		// memory store then it's already updated to memory which is where the cloud source will read
		// it to. If it's not reading into the memory store, then we need to remember to grab it
		// from the cloud next time.
		if (!Request->bReadIntoMemoryStore)
		{
			SetChunkLocation(Request->Guid, EConstructorChunkLocation::Cloud);
		}
		else
		{
			// Normally SetChunkLocation will update the download amount, but we aren't actually
			// changing the chunk's location since it's going into the memory store. We do still
			// need to tell the user about the download requirement though:
			uint64 ChunkSize = Configuration.ManifestSet->GetDownloadSize(Guid);

			ChunkLocationsLock.WriteLock();
			DownloadRequirement += ChunkSize;
			CloudSource->PostRequiredByteCount(DownloadRequirement);
			ChunkLocationsLock.WriteUnlock();

		}

		if (bHasChunkDbSource)
		{
			// Only send this message if we have chunk dbs. The theory is if they don't have chunkdbs then they are expecting
			// chunks to download from the cloud. If they provide chunkdbs then they are surprised when chunks come from the cloud.
			MessagePump->SendMessage({FGenericMessage::EType::CloudSourceUsed, Guid});
		}

		QueueGenericThreadTask(ThreadAssignments[EConstructorChunkLocation::Cloud], CloudSource->CreateRequest(
			Request->Guid, 
			Request->ReadBuffer, 
			Request,
			IConstructorChunkSource::FChunkRequestCompleteDelegate::CreateRaw(this, &FBuildPatchFileConstructor::RequestCompletedFn)));
	}
	else
	{
		if (bFailedToRead)
		{
			Request->Batch->ErrorContextGuid = Guid;
			Request->Batch->FailedRequestCount.fetch_add(1, std::memory_order_relaxed);
		}
		else if (!bAborted)
		{
			uint64 TotalToDestinationBuffer = 0;

			if (Request->bReadIntoMemoryStore)
			{
				// If the read went to memory, we need to copy splats. Otherwise it was a single
				// direct read so do nothing.
				TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_Splats);
				for (const FRequestInfo::FRequestSplat& Splat : Request->Splats)
				{
					FMemory::Memcpy(
						(uint8*)Request->DestinationBuffer.GetData() + Splat.DestinationOffset,
						(uint8*)Request->ReadBuffer.GetData() + Splat.OffsetInChunk,
						Splat.BytesToCopy);
					TotalToDestinationBuffer += Splat.BytesToCopy;
				}
			}
			else
			{
				// Direct means 1 splat
				TotalToDestinationBuffer += Request->Splats[0].BytesToCopy;
			}

			Request->File->ProgressLock.Lock();
			Request->File->Progress += TotalToDestinationBuffer;
			Request->File->ProgressLock.Unlock();
		}
		
		if (Request->Batch->PendingRequestCount.fetch_add(-1, std::memory_order_relaxed) == 1)
		{
			Request->Batch->bIsFinished.store(true, std::memory_order_release);
			WakeUpDispatchThreadEvent->Trigger();
		}
	}
}

// Return a function that writes the data for the batch to the given file.
IConstructorChunkSource::FRequestProcessFn FBuildPatchFileConstructor::CreateWriteRequest(FArchive* File, FBatchState& Batch)
{
	return [this, File, Batch = &Batch](bool bIsAbort)
	{
		if (bIsAbort)
		{
			return;
		}

		// Has to be mutable because of the serialize call.
		FMutableMemoryView WriteBuffer = Batch->BatchBuffer;

		// Manage write limits.
		if (bStallWhenFileSystemThrottled)
		{
			uint64 AvailableBytes = FileSystem->GetAllowedBytesToWriteThrottledStorage(*File->GetArchiveName());
			while (WriteBuffer.GetSize() > AvailableBytes)
			{
				UE_LOG(LogBuildPatchServices, Display, TEXT("Avaliable write bytes to write throttled storage exhausted (%s).  Sleeping %ds.  Bytes needed: %llu, bytes available: %llu")
					, *File->GetArchiveName(), SleepTimeWhenFileSystemThrottledSeconds, WriteBuffer.GetSize(), AvailableBytes);
				FPlatformProcess::Sleep(SleepTimeWhenFileSystemThrottledSeconds);
				AvailableBytes = FileSystem->GetAllowedBytesToWriteThrottledStorage(*File->GetArchiveName());

				if (bShouldAbort)
				{
					return;
				}
			}
		}

		FileConstructorStat->OnBeforeWrite();
		ISpeedRecorder::FRecord ActivityRecord;
		ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
		
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(WriteThread_Serialize)
			File->Serialize(WriteBuffer.GetData(), WriteBuffer.GetSize());
		}

		ActivityRecord.Size = WriteBuffer.GetSize();
		ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
		FileConstructorStat->OnAfterWrite(ActivityRecord);
		
		Batch->bIsFinished.store(true, std::memory_order_release);
	};
}
					

// Always called on Constructor thread.
void FBuildPatchFileConstructor::CompleteReadBatch(const FFileManifest& FileManifest, FBatchState& Batch)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_CompleteRead);

	if (Batch.Error == EConstructionError::None && Batch.FailedRequestCount.load(std::memory_order_acquire))
	{
		Batch.Error = EConstructionError::MissingChunk;
	}

	UE_LOG(LogBuildPatchServices, Verbose, TEXT("Completing ReadBatch: %d"), Batch.BatchId);

	// We have to copy the memory source chunks after the reads are done
	// because if we have two buffer's reads queued, the first one could be
	// filling the memory source. If we copy these after we are done, we guarantee
	// that the previous buffer has completed its reads so we know we are
	// working with valid memory.
	if (Batch.Error == EConstructionError::None)
	{
		for (TPair<FGuid, FRequestInfo>& RequestPair : Batch.Requests)
		{					
			FRequestInfo& Request = RequestPair.Value;

			if (Request.bSourceIsMemoryStore)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_DirectCopy);
				// Just copy what we need directly.
				FMemoryView ChunkData = BackingStore->GetViewForChunk(Request.Guid);
				bool bFailedToGetChunk = ChunkData.GetSize() == 0;

				if (!bFailedToGetChunk)
				{
					uint64 TotalToDestinationBuffer = 0;
					for (const FRequestInfo::FRequestSplat& Splat : Request.Splats)
					{
						FMemory::Memcpy(
							(uint8*)Request.DestinationBuffer.GetData() + Splat.DestinationOffset,
							(uint8*)ChunkData.GetData() + Splat.OffsetInChunk,
							Splat.BytesToCopy);
						TotalToDestinationBuffer += Splat.BytesToCopy;
					}

					Request.File->ProgressLock.Lock();
					Request.File->Progress += TotalToDestinationBuffer;
					Request.File->ProgressLock.Unlock();
				}
				else
				{
					Batch.Error = EConstructionError::MissingChunk;
					Batch.ErrorContextGuid = Request.Guid;
				}

				// Mark that we're done with the memory so it can get evicted if necessary.
				if (!BackingStore->ReleaseEntry(Request.Guid))
				{
					Batch.Error = EConstructionError::InternalConsistencyError;
					Batch.ErrorContextGuid = Request.Guid;
				}
			}
			else if (Request.bReadIntoMemoryStore)
			{
				// Commit the read so the memory store knows its safe to evict if necessary.
				if (!BackingStore->CommitAndReleaseEntry(Request.Guid))
				{
					Batch.Error = EConstructionError::InternalConsistencyError;
					Batch.ErrorContextGuid = Request.Guid;
				}
			}
		}
	}

	if (Batch.Error == EConstructionError::None)
	{
		// Retire the chunks we've used. This has to be in order 
		for (int32 i = 0; i < Batch.ChunkCount; i++)
		{
			if (!ChunkReferenceTracker->PopReference(FileManifest.ChunkParts[Batch.StartChunkPartIndex + i].Guid))
			{
				Batch.Error = EConstructionError::TrackingError;
				Batch.ErrorContextGuid = FileManifest.ChunkParts[Batch.StartChunkPartIndex + i].Guid;
			}
		}
	}

	if (Batch.Error == EConstructionError::None)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_MemRetire);
		// Has to be after the splats because this might free the memory we need!				
		if (!BackingStore->CheckRetirements(ChunkReferenceTracker->GetCurrentUsageIndex()))
		{
			Batch.Error = EConstructionError::InternalConsistencyError;
		}
	}
}

// Always called from Constructor thread
// This return -1 when we run into an error that requires stopping the installation.
// Note that there could be outstanding reads and we can no longer rely on the completion functions
// to wake up the dispatch thread (which we must run on), so we can't ever wait if this returns -1.
// Check Batch.Error for the error on -1 return.
void FBuildPatchFileConstructor::StartReadBatch(FFileConstructionState& CurrentFile, FBatchState& Batch)
{
	CurrentFile.OutstandingBatches++;
	Batch.Requests.Reset();
	Batch.StartChunkPartIndex = CurrentFile.NextChunkPartToRead;

	int32 EndChunkIdx = Batch.StartChunkPartIndex;
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_Count);
		uint32 BufferFillLevel = 0;
		for (; EndChunkIdx < CurrentFile.FileManifest->ChunkParts.Num(); ++EndChunkIdx)
		{
			const FChunkPart& ChunkPart = CurrentFile.FileManifest->ChunkParts[EndChunkIdx];
			if (ChunkPart.Size + BufferFillLevel > Batch.BatchBuffer.GetSize())
			{
				break;
			}

			FRequestInfo& Request = Batch.Requests.FindOrAdd(ChunkPart.Guid);
			Request.Guid = ChunkPart.Guid;
			Request.Batch = &Batch;
			Request.ChunkSize = Configuration.ManifestSet->GetChunkInfo(ChunkPart.Guid)->WindowSize;
			Request.File = &CurrentFile;
			
			FRequestInfo::FRequestSplat& Splat = Request.Splats.AddDefaulted_GetRef();
			Splat.DestinationOffset = BufferFillLevel;
			Splat.OffsetInChunk = ChunkPart.Offset;
			Splat.BytesToCopy = ChunkPart.Size;
				
			BufferFillLevel += ChunkPart.Size;
		}

		// Trim our view to the amount we actually used. The calling code will reclaim the rest.
		Batch.BatchBuffer.LeftInline(BufferFillLevel);
	}

	Batch.ChunkCount = EndChunkIdx - Batch.StartChunkPartIndex;
	Batch.PendingRequestCount = Batch.Requests.Num();
	Batch.FailedRequestCount = 0;
	
	//
	// IMPORTANT!!!
	//
	// We MUST call the completion routine for each request here so that we can know
	// when all outstanding requests are done. If we bail early then we can't know when
	// all requests are done during cancelation/abort. If we hit a consistency error during then
	// we just need to call it with a failure.
	//
	for (TPair<FGuid, FRequestInfo>& RequestPair : Batch.Requests)
	{
		// Can come from:
		// -- memory. This chunk had already been loaded and we needed it again _after it's source expired_.
		// -- chunkdb. async IO + decompress
		// -- cloud. async download + decompress
		// -- disk. we ran out of memory store and had to dump to disk. async IO + maybe decompress
		// -- install. async IO

		// Note -- most chunks will be used more than once.

		// sources that expire: note that expired chunks can always be redownloaded via the cloud source
		//  -- cloud (they expire immediately, but can be redownloaded)
		//  -- install

		// We aren't necessarily using the whole chunk - e.g. if we are a small file, we'll only
		// be a tiny part of the chunk and the rest will need to be used by the next file. In this case
		// we have to read into a memory store destination so that it can be carried over into the
		// next file.


		FRequestInfo& Request = RequestPair.Value;
		Request.DestinationBuffer = Batch.BatchBuffer;

		
		const EConstructorChunkLocation* ChunkLocationPtr = ChunkLocations.Find(Request.Guid);
		if (ChunkLocationPtr == nullptr)
		{
			Batch.Error = EConstructionError::InternalConsistencyError;
			RequestCompletedFn(Request.Guid, true, false, &Request);
			continue;
		}
		EConstructorChunkLocation ChunkLocation = *ChunkLocationPtr;

		Request.bSourceIsMemoryStore = ChunkLocation == EConstructorChunkLocation::Memory;
		if (Request.bSourceIsMemoryStore)
		{
			// We copy after the reads are done since the memory might not be ready.
			// Make sure we don't evict it in the meantime
			if (!BackingStore->LockEntry(Request.Guid))
			{
				Batch.Error = EConstructionError::InternalConsistencyError;
				RequestCompletedFn(Request.Guid, true, false, &Request);
				continue;
			}
			RequestCompletedFn(Request.Guid, false, false, &Request);
		}
		else
		{
			IConstructorChunkSource* ThisChunkSource = nullptr;
			switch (ChunkLocation)
			{
				case EConstructorChunkLocation::Install: ThisChunkSource = InstallSource; break;
				case EConstructorChunkLocation::Cloud: ThisChunkSource = CloudSource; break;
				case EConstructorChunkLocation::ChunkDb: ThisChunkSource = ChunkDbSource; break;
				case EConstructorChunkLocation::Memory: /* already handled above */ break;
				case EConstructorChunkLocation::DiskOverflow: ThisChunkSource = (IConstructorChunkSource*)BackingStore.Get(); break;
			}

			if (ThisChunkSource == CloudSource)
			{
				// If we are already downloading from the cloud, then failures shouldn't try to 
				// fall back to the cloud.
				Request.bLaunchedFallback = true;

				if (bHasChunkDbSource)
				{
					// Only send this message if we have chunk dbs. The theory is if they don't have chunkdbs then they are expecting
					// chunks to download from the cloud. If they provide chunkdbs then they are surprised when chunks come from the cloud.
					MessagePump->SendMessage({FGenericMessage::EType::CloudSourceUsed, RequestPair.Value.Guid});
				}
			}

			// We need to kick a request. The question is whether we can request direct
			// or need to route through the memory store.
			int32 LastUsageIndex = 0;
			ChunkReferenceTracker->GetNextUsageForChunk(Request.Guid, LastUsageIndex);
			Request.ChunkUnavailableAt = ThisChunkSource->GetChunkUnavailableAt(Request.Guid);
				
			const bool bNeedsEntireChunk = Request.Splats.Num() == 1 && Request.Splats[0].BytesToCopy == Request.ChunkSize && Request.Splats[0].OffsetInChunk == 0;
			const bool bNeededAfterRetirement = LastUsageIndex >= Request.ChunkUnavailableAt;

			if (bNeedsEntireChunk && !bNeededAfterRetirement)
			{
				// Read direct.
				Request.ReadBuffer = MakeMemoryView((uint8*)Request.DestinationBuffer.GetData() + Request.Splats[0].DestinationOffset, Request.Splats[0].BytesToCopy);
			}
			else
			{
				// Route through memory store.
				Request.ReadBuffer = BackingStore->ReserveAndLockEntry(Request.Guid, Request.ChunkSize, LastUsageIndex);
				if (Request.ReadBuffer.GetSize() == 0)
				{
					Batch.Error = EConstructionError::InternalConsistencyError;
					RequestCompletedFn(Request.Guid, true, false, &Request);
					continue;
				}
				Request.bReadIntoMemoryStore = true;

				// Note that when we set this, the next batch read could want this chunk before its read is done.
				// Hence reads for memory sources are done _after_ reads are done, because we retire reads in order,
				// we then know this memory is populated.
				SetChunkLocation(RequestPair.Value.Guid, EConstructorChunkLocation::Memory);
			}
			

			IConstructorChunkSource::FRequestProcessFn RequestProcess = ThisChunkSource->CreateRequest(
				Request.Guid, 
				Request.ReadBuffer,
				(void*)&Request,
				IConstructorChunkSource::FChunkRequestCompleteDelegate::CreateRaw(this, &FBuildPatchFileConstructor::RequestCompletedFn));
						
			QueueGenericThreadTask(ThreadAssignments[ChunkLocation], MoveTemp(RequestProcess));
		}
	}

	CurrentFile.NextChunkPartToRead = EndChunkIdx;
}

void FBuildPatchFileConstructor::WakeUpDispatch()
{
	WakeUpDispatchThreadEvent->Trigger();
}

void FBuildPatchFileConstructor::InitFile(FFileConstructionState& CurrentFile, const FResumeData& ResumeData)
{
	if (!CurrentFile.bSuccess)
	{
		return;
	}

	const int64 FileSize = CurrentFile.FileManifest->FileSize;

	// Check resume status for this file.
	const bool bFilePreviouslyComplete = ResumeData.FilesCompleted.Contains(CurrentFile.BuildFilename);	

	// Construct or skip the file.
	if (bFilePreviouslyComplete)
	{
		CountBytesProcessed(FileSize);

		UE_LOG(LogBuildPatchServices, Display, TEXT("Skipping completed file %s"), *CurrentFile.BuildFilename);
		// Go through each chunk part, and dereference it from the reference tracker.
		for (const FChunkPart& ChunkPart : CurrentFile.FileManifest->ChunkParts)
		{
			if (!ChunkReferenceTracker->PopReference(ChunkPart.Guid))
			{
				CurrentFile.bSuccess = false;
				CurrentFile.ConstructionError = EConstructionError::TrackingError;
				break;
			}
		}

		CurrentFile.bSkippedConstruction = true;
		return;
	}

	if (!CurrentFile.bSuccess &&
		!CurrentFile.FileManifest->SymlinkTarget.IsEmpty())
	{
#if PLATFORM_MAC
		CurrentFile.bSuccess = symlink(TCHAR_TO_UTF8(*CurrentFile.FileManifest->SymlinkTarget), TCHAR_TO_UTF8(*CurrentFile.NewFilename)) == 0;
		CurrentFile.bSkippedConstruction = true;
#else
		const bool bSymlinkNotImplemented = false;
		check(bSymlinkNotImplemented);
		CurrentFile.bSuccess = false;
#endif
	}
}

bool FBuildPatchFileConstructor::HandleInitialDiskSizeCheck(const FFileManifest& FileManifest, int64 StartPosition)
{
	// Returns false if we failed the disk space check.
	if (!bInitialDiskSizeCheck)
	{
		bInitialDiskSizeCheck = true;

		// Normal operation can just use the classic calculation
		uint64 LocalDiskSpaceRequired = CalculateInProgressDiskSpaceRequired(FileManifest, StartPosition);

		// If we are delete-during-install this gets more complicated because we'll be freeing up
		// space as we add.
		if (Configuration.bDeleteChunkDBFilesAfterUse)
		{
			LocalDiskSpaceRequired = CalculateDiskSpaceRequirementsWithDeleteDuringInstall();
		}

		uint64 LocalDiskSpaceAvailable = 0;
		{
			uint64 TotalSize = 0;
			uint64 AvailableSpace = 0;
			if (FPlatformMisc::GetDiskTotalAndFreeSpace(Configuration.InstallDirectory, TotalSize, AvailableSpace))
			{
				LocalDiskSpaceAvailable = AvailableSpace;
				BackingStore->SetDynamicDiskSpaceHeadroom(LocalDiskSpaceRequired);
			}
			else
			{
				BackingStore->DisableDynamicDiskSpaceHeadroom();
			}
		}

		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Initial Disk Sizes: Required: %s Available: %s"), *FormatNumber(LocalDiskSpaceRequired), *FormatNumber(LocalDiskSpaceAvailable));

		AvailableDiskSpace.store(LocalDiskSpaceAvailable, std::memory_order_release);
		RequiredDiskSpace.store(LocalDiskSpaceRequired, std::memory_order_release);	

		if (!FileConstructorHelpers::CheckRemainingDiskSpace(Configuration.InstallDirectory, LocalDiskSpaceRequired, LocalDiskSpaceAvailable))
		{
			UE_LOG(LogBuildPatchServices, Error, TEXT("Out of HDD space. Needs %llu bytes, Free %llu bytes"), LocalDiskSpaceRequired, LocalDiskSpaceAvailable);
			InstallerError->SetError(
				EBuildPatchInstallError::OutOfDiskSpace,
				DiskSpaceErrorCodes::InitialSpaceCheck,
				0,
				BuildPatchServices::GetDiskSpaceMessage(Configuration.InstallDirectory, LocalDiskSpaceRequired, LocalDiskSpaceAvailable));
			return false;
		}
	}

	return true;
}

void FBuildPatchFileConstructor::ResumeFile(FFileConstructionState& FileToResume)
{
	if (!FileToResume.bSuccess || FileToResume.bSkippedConstruction)
	{
		return;
	}

	// We have to read in the existing file so that the hash check can still be done.
	TUniquePtr<FArchive> NewFileReader(IFileManager::Get().CreateFileReader(*FileToResume.NewFilename));
	if (!NewFileReader.IsValid())
	{
		// We don't fail if we can't read in the previous file - we try and rebuild it from scratch.
		// (Note that the likely outcome here is we can't open the file for write either and fail to
		// install - we're only here if we were supposed to be resuming!)
		return;
	}

	const int32 ReadBufferSize = 4 * 1024 * 1024;
	// Read buffer
	TArray<uint8> ReadBuffer;
	ReadBuffer.SetNumUninitialized(ReadBufferSize);

	// We don't allow resuming mid-chunk for simplicity, so eat entire chunks until
	// we can't anymore.
	int64 OnDiskSize = NewFileReader->TotalSize();
	int64 ByteCounter = 0;
	for (int32 ChunkPartIdx = 0; ChunkPartIdx < FileToResume.FileManifest->ChunkParts.Num() && !bShouldAbort; ++ChunkPartIdx)
	{
		const FChunkPart& ChunkPart = FileToResume.FileManifest->ChunkParts[ChunkPartIdx];
		const int64 NextBytePosition = ByteCounter + ChunkPart.Size;
		ByteCounter = NextBytePosition;
		if (NextBytePosition <= OnDiskSize)
		{
			// Ensure buffer is large enough
			ReadBuffer.SetNumUninitialized(ChunkPart.Size, EAllowShrinking::No);

			{
				FReadScope _(FileConstructorStat, ChunkPart.Size);
				NewFileReader->Serialize(ReadBuffer.GetData(), ChunkPart.Size);
			}

			FileToResume.HashState.Update(ReadBuffer.GetData(), ChunkPart.Size);

			// Update resume start position
			FileToResume.StartPosition = NextBytePosition;
			FileToResume.NextChunkPartToRead = ChunkPartIdx + 1;

			// Inform the reference tracker of the chunk part skip
			if (!ChunkReferenceTracker->PopReference(ChunkPart.Guid))
			{
				FileToResume.bSuccess = false;
				FileToResume.ConstructionError = EConstructionError::TrackingError;
				FileToResume.ErrorContextGuid = ChunkPart.Guid;
				break;
			}

			CountBytesProcessed(ChunkPart.Size);
			FileConstructorStat->OnFileProgress(FileToResume.BuildFilename, NewFileReader->Tell());
			// Wait if paused
			FileConstructorHelpers::WaitWhilePaused(bIsPaused, bShouldAbort);
		}
		else
		{
			// We can't consume any more full chunks from the part list, bail.
			break;
		}
	}

	NewFileReader->Close();
	FileToResume.bIsResumedFile = true;
}

void FBuildPatchFileConstructor::OpenFileToConstruct(FFileConstructionState& CurrentFile)
{
	if (!CurrentFile.bSuccess || CurrentFile.bSkippedConstruction)
	{
		return;
	}

	// Attempt to create the file
	{
		FAdministrationScope _(FileConstructorStat);
		CurrentFile.NewFile = FileSystem->CreateFileWriter(*CurrentFile.NewFilename, CurrentFile.bIsResumedFile ? EWriteFlags::Append : EWriteFlags::None);
		CurrentFile.CreateFilePlatformLastError = FPlatformMisc::GetLastError();
	}

	CurrentFile.bSuccess = CurrentFile.NewFile != nullptr;
	if (!CurrentFile.bSuccess)
	{
		CurrentFile.ConstructionError = EConstructionError::CannotCreateFile;
		return;
	}

	// Seek to file write position
	if (CurrentFile.NewFile->Tell() != CurrentFile.StartPosition)
	{
		FAdministrationScope _(FileConstructorStat);

		// Currently no way of checking if the seek succeeded. If it didn't and further reads succeed, then
		// we can end up with a bad file on disk and not know it as the hash is assuming this worked - requires
		// full load-and-hash verification to find.
		CurrentFile.NewFile->Seek(CurrentFile.StartPosition);
	}

	CurrentFile.Progress = CurrentFile.StartPosition;
	CurrentFile.LastSeenProgress = CurrentFile.Progress;
}


void FBuildPatchFileConstructor::CompleteConstructedFile(FFileConstructionState& CurrentFile)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Constructor_CompleteFile);
	if (!CurrentFile.bSkippedConstruction)
	{
		if (CurrentFile.bSuccess && CurrentFile.NewFile->IsError())
		{
			// This should aleady have been caught during the write!
			UE_LOG(LogBuildPatchServices, Warning, TEXT("Got serialize error during CompleteConstructedFile! Should have already been caught."));
			CurrentFile.ConstructionError = EConstructionError::SerializeError;
			CurrentFile.bSuccess = false;
		}

		// Close the file writer
		bool bArchiveSuccess = false;
		if (CurrentFile.NewFile)
		{
			FAdministrationScope _(FileConstructorStat);
			bArchiveSuccess = CurrentFile.NewFile->Close();
			CurrentFile.NewFile.Reset();
		}
		else
		{
			bArchiveSuccess = true;
		}

		// Check for final success
		if (CurrentFile.bSuccess && !bArchiveSuccess)
		{
			CurrentFile.ConstructionError = EConstructionError::CloseError;
			CurrentFile.bSuccess = false;
		}

		// We can't check for zero locks if we have multiple files in flight because the other
		// files hold locks.
		if (CurrentFile.bSuccess && !bAllowMultipleFilesInFlight)
		{
			if (!BackingStore->CheckNoLocks(false))
			{
				CurrentFile.bSuccess = false;
				CurrentFile.ConstructionError = EConstructionError::InternalConsistencyError;
			}
		}
	
		// Verify the hash for the file that we created
		if (CurrentFile.bSuccess)
		{
			CurrentFile.HashState.Final();

			FSHAHash HashValue;
			CurrentFile.HashState.GetHash(HashValue.Hash);
			CurrentFile.bSuccess = HashValue == CurrentFile.FileManifest->FileHash;
			if (!CurrentFile.bSuccess)
			{
				CurrentFile.ConstructionError = EConstructionError::OutboundDataError;
			}
		}

#if PLATFORM_MAC
		if (CurrentFile.bSuccess && 
			EnumHasAllFlags(CurrentFile.FileManifest->FileMetaFlags, EFileMetaFlags::UnixExecutable))
		{
			// Enable executable permission bit
			struct stat FileInfo;
			if (stat(TCHAR_TO_UTF8(*CurrentFile.NewFilename), &FileInfo) == 0)
			{
				CurrentFile.bSuccess = chmod(TCHAR_TO_UTF8(*CurrentFile.NewFilename), FileInfo.st_mode | S_IXUSR | S_IXGRP | S_IXOTH) == 0;
				if (!CurrentFile.bSuccess)
				{
					UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed to set exec bit %s"), *CurrentFile.BuildFilename);
				}
			}
		}
#endif

#if PLATFORM_ANDROID
		if (CurrentFile.bSuccess)
		{
			IFileManager::Get().SetTimeStamp(*CurrentFile.NewFilename, FDateTime::UtcNow());
		}
#endif
	} // end if we did actual construction work for this file.

	if (CurrentFile.bSuccess)
	{
		ChunkDbSource->ReportFileCompletion(ChunkReferenceTracker->GetRemainingChunkCount());
	}

	// If we are destructive, remove the old file.
	if (CurrentFile.bSuccess &&
		Configuration.InstallMode == EInstallMode::DestructiveInstall)
	{
		FString FileToDelete = Configuration.InstallDirectory / CurrentFile.BuildFilename;
		FPaths::NormalizeFilename(FileToDelete);
		FPaths::CollapseRelativeDirectories(FileToDelete);
		if (FileSystem->FileExists(*FileToDelete))
		{
			if (InstallSource && !HarvestChunksForCompletedFile(FileToDelete))
			{
				CurrentFile.bSuccess = false;
				CurrentFile.ConstructionError = EConstructionError::InternalConsistencyError;
			} // end if install source exists.

			OnBeforeDeleteFile().Broadcast(FileToDelete);
			{
				// This can take forever due to file system filters. If we throw this on an async
				// job then we can go over our calculated disk space.
				TRACE_CPUPROFILER_EVENT_SCOPE(Delete);
				const bool bRequireExists = false;
				const bool bEvenReadOnly = true;
				IFileManager::Get().Delete(*FileToDelete, bRequireExists, bEvenReadOnly);
			}
		}
	}

	FileConstructorStat->OnFileCompleted(CurrentFile.BuildFilename, CurrentFile.bSuccess);
	
	// Report errors.
	if (!CurrentFile.bSuccess)
	{
		const bool bReportAnalytic = InstallerError->HasError() == false;
		switch (CurrentFile.ConstructionError)
		{
		case EConstructionError::OutboundDataError:
			{
				// Only report if the first error
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Serialised Verify Fail"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Verify failed after constructing %s"), *CurrentFile.BuildFilename);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::OutboundCorrupt);
				break;
			}
		case EConstructionError::OutOfDiskSpace:
			{
				uint64 LocalAvailableDiskSpace = AvailableDiskSpace.load(std::memory_order_acquire);
				uint64 LocalRequiredDiskSpace = RequiredDiskSpace.load(std::memory_order_acquire);
				UE_LOG(LogBuildPatchServices, Error, TEXT("Out of HDD space. Needs %llu bytes, Free %llu bytes"), LocalRequiredDiskSpace, LocalAvailableDiskSpace);
				InstallerError->SetError(
					EBuildPatchInstallError::OutOfDiskSpace,
					DiskSpaceErrorCodes::DuringInstallation,
					0,
					BuildPatchServices::GetDiskSpaceMessage(Configuration.InstallDirectory, LocalRequiredDiskSpace, LocalAvailableDiskSpace));
				break;
			}
		case EConstructionError::CannotCreateFile:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, CurrentFile.CreateFilePlatformLastError, TEXT("Could Not Create File"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Could not create %s (LastError=%d)"), *CurrentFile.BuildFilename, CurrentFile.CreateFilePlatformLastError);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::FileCreateFail, CurrentFile.CreateFilePlatformLastError);
				break;
			}
		case EConstructionError::CloseError:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Could Not Close File"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Could not close %s"), *CurrentFile.BuildFilename);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::CloseFileError);
				break;
			}
		case EConstructionError::MissingChunk:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Missing Chunk"));
				}

				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to missing chunk %s (can be 0000 if unknown)"), *CurrentFile.BuildFilename, *WriteToString<64>(CurrentFile.ErrorContextGuid));
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingChunkData);
				break;
			}
		case EConstructionError::SerializeError:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Serialization Error"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to serialization error"), *CurrentFile.BuildFilename);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::SerializationError);
				break;
			}
		case EConstructionError::TrackingError:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Tracking Error"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to untracked chunk %s (can be 0000 if unknown)"), *CurrentFile.BuildFilename, *WriteToString<64>(CurrentFile.ErrorContextGuid));
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::TrackingError);
				break;
			}
		case EConstructionError::InternalConsistencyError:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Internal Consistency Error"));
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Failed %s due to internal consistency checking failure"), *CurrentFile.BuildFilename);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::InternalConsistencyFailure);
				break;
			}
		case EConstructionError::MissingFileInfo:
			{
				if (bReportAnalytic)
				{
					InstallerAnalytics->RecordConstructionError(CurrentFile.BuildFilename, INDEX_NONE, TEXT("Missing File Manifest"));					
				}
				UE_LOG(LogBuildPatchServices, Error, TEXT("FBuildPatchFileConstructor: Missing file manifest for %s"), *CurrentFile.BuildFilename);
				InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
				break;
			}
		case EConstructionError::FailedInitialSizeCheck:
			{
				// Error already set back when we had the info.
				break;
			}
		case EConstructionError::Aborted:
			{
				// We don't set errors on abort.
				break;
			}
		}

		// Delete the staging file if unsuccessful by means of any failure that could leave the file in unknown state.
		switch (CurrentFile.ConstructionError)
		{
			// Errors we expect that we can conceptually resume:
			case EConstructionError::OutOfDiskSpace:
			case EConstructionError::MissingChunk:
			case EConstructionError::Aborted:
			{
				break;
			}

			// Errors we expect there to be issues with the outbound file:
			case EConstructionError::CannotCreateFile:
			case EConstructionError::CloseError:
			case EConstructionError::SerializeError:
			case EConstructionError::TrackingError:
			case EConstructionError::OutboundDataError:
			case EConstructionError::InternalConsistencyError:
			{
				if (!FileSystem->DeleteFile(*CurrentFile.NewFilename))
				{
					UE_LOG(LogBuildPatchServices, Warning, TEXT("FBuildPatchFileConstructor: Error deleting file: %s (Error Code %i)"), *CurrentFile.NewFilename, FPlatformMisc::GetLastError());
				}
				break;
			}
		}

		// Stop trying to install further files.
		Abort();
	}
}

void FBuildPatchFileConstructor::StartFile(FFileConstructionState& CurrentFile, const FResumeData& ResumeData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(Constructor_StartFile);

	InitFile(CurrentFile, ResumeData);

	if (CurrentFile.bSkippedConstruction)
	{
		return;
	}

	if (ResumeData.FilesStarted.Contains(CurrentFile.BuildFilename))
	{
		ResumeFile(CurrentFile);

		// Generally speaking we only expect there to be one file to resume (as of this writing there
		// is no way for the FilesStarted set to have more than one file), so we update the download requirements
		// after the first one.

		// Need to sum up the size of all remaining chunks that we need. We can't just look at the
		// chunk locations because we don't know if it's still needed or not.
		TSet<FGuid> RemainingNeededChunks = ChunkReferenceTracker->GetReferencedChunks();

		uint64 NewDownloadRequirement = 0;
		FRWScopeLock ChunkLock(ChunkLocationsLock, SLT_Write); // write because we're updating the download requirement
		for (FGuid& NeededChunk : RemainingNeededChunks)
		{
			EConstructorChunkLocation* LocationPtr = ChunkLocations.Find(NeededChunk);
			if (LocationPtr && *LocationPtr == EConstructorChunkLocation::Cloud)
			{
				NewDownloadRequirement += Configuration.ManifestSet->GetChunkInfo(NeededChunk)->FileSize;
			}
		}

		DownloadRequirement = NewDownloadRequirement;
		CloudSource->PostRequiredByteCount(DownloadRequirement);
	}

	// If we haven't done so yet, make the initial disk space check. We do this after resume
	// so that we know how much to discount from our current file size.
	if (!HandleInitialDiskSizeCheck(*CurrentFile.FileManifest, CurrentFile.StartPosition))
	{
		CurrentFile.bSuccess = false;
		CurrentFile.ConstructionError = EConstructionError::FailedInitialSizeCheck;
	}

	if (!bIsDownloadStarted)
	{
		bIsDownloadStarted = true;
		FileConstructorStat->OnResumeCompleted();
	}

	OpenFileToConstruct(CurrentFile);

}

void FBuildPatchFileConstructor::ConstructFiles(const FResumeData& ResumeData)
{
	MaxWriteBatchSize = FFileConstructorConfig::DefaultIOBatchSizeMB;
	if (GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ConstructorIOBatchSizeMB"), (int32&)MaxWriteBatchSize, GEngineIni))
	{
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got INI ConstructorIOBatchSizeMB = %d"), MaxWriteBatchSize);
	}
	if (Configuration.IOBatchSizeMB.IsSet())
	{
		MaxWriteBatchSize = Configuration.IOBatchSizeMB.Get(MaxWriteBatchSize);
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got override ConstructorIOBatchSizeMB = %d"), MaxWriteBatchSize);
	}
	MaxWriteBatchSize <<= 20; // to MB;

	IOBufferSize = FFileConstructorConfig::DefaultIOBufferSizeMB;
	if (GConfig->GetInt(TEXT("Portal.BuildPatch"), TEXT("ConstructorIOBufferSizeMB"), (int32&)IOBufferSize, GEngineIni))
	{
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got INI ConstructorIOBufferSizeMB = %d"), IOBufferSize);
	}
	if (Configuration.IOBufferSizeMB.IsSet())
	{
		IOBufferSize = Configuration.IOBufferSizeMB.Get(IOBufferSize);
		UE_LOG(LogBuildPatchServices, Verbose, TEXT("Got override ConstructorIOBufferSizeMB = %d"), IOBufferSize);
	}
	IOBufferSize <<= 20; // to MB;

	// Ensure that our batch size can always make progress (all chunks can fit)
	uint32 LargestChunkSize = 0;
	{
		for (const FFileToConstruct& FileToConstruct : ConstructionList)
		{
			if (FileToConstruct.FileManifest)
			{
				for (const FChunkPart& ChunkPart : FileToConstruct.FileManifest->ChunkParts)
				{
					if (ChunkPart.Size > LargestChunkSize)
					{
						LargestChunkSize = ChunkPart.Size;
					}
				}
			}
		}

		if (LargestChunkSize > MaxWriteBatchSize)
		{
			MaxWriteBatchSize = LargestChunkSize;
			UE_LOG(LogBuildPatchServices, Display, TEXT("Increasing batch size to fit any chunk size: %u bytes"), MaxWriteBatchSize);

			if (MaxWriteBatchSize > IOBufferSize)
			{
				IOBufferSize = MaxWriteBatchSize;
				UE_LOG(LogBuildPatchServices, Display, TEXT("Increasing IO buffer size to fit batch size: %u bytes"), IOBufferSize);
			}
		}
	}

	uint64 ConstructStartCycles = FPlatformTime::Cycles64();
	uint64 WriteDoneCycles = 0;
	uint64 ReadDoneCycles = 0;
	uint64 ReadCheckCycles = 0;
	uint64 CloudTickCycles = 0;
	uint64 HashCycles = 0;
	uint64 WaitCycles = 0;
	uint64 WriteStartCycles = 0;

	// List of files that are currently opened. The last one is the one we are starting reads on,
	// the first one is the one we are currently writing.
	TArray<TUniquePtr<FFileConstructionState>> ActiveFiles;

	// List of batches in flight. These must be dispatched in order.
	TArray<TUniquePtr<FBatchState>> Batches;

	// This is the buffer we issue reads into and writes out of. We segment it up
	// based on batch sizing.
	TArray<uint8> IOBUffer;
	IOBUffer.SetNumUninitialized(IOBufferSize);

	struct Range
	{
		uint64 Start=0, End=0;
	};
	TArray<Range, TInlineAllocator<3>> IOBufferFreeBlockList;
	IOBufferFreeBlockList.Add({0, IOBufferSize});

	auto SortAndCoalesceFreeList = [](TArray<Range, TInlineAllocator<3>>& FreeList)
	{
		// Sort the list on Start.
		Algo::SortBy(FreeList, &Range::Start, TLess<uint64>());

		// go over everything and coalesce anything that's adjacent.
		for (int32 i=0; i<FreeList.Num() - 1; i++)
		{
			if (FreeList[i].End == FreeList[i+1].Start)
			{
				// Extend us.
				FreeList[i].End = FreeList[i+1].End;

				// Remove them.
				FreeList.RemoveAt(i+1);

				// Recheck us.
				i--;
			}
		}
	};

	//
	// The abort handling with this loop is:
	// During the loop, if we see the abort signal, we mark all active files as failed.
	// That prevents any new work from starting, but we have to have the outstanding work
	// complete in order to be thread safe. Once that work completes, we then break out of the loop
	// (bDoneWithFiles will still be true).
	//
	bool bStillProcessingFiles = true;
	for (;bStillProcessingFiles;)
	{		
		

		bool bHasAnyFileFailed = false;
		// Check state. We can continue to run this after a failure has been encountered in order
		// to drain any async tasks so we know we can shut down safely.
		{
			// Is our next write done?
			if (Batches.Num() &&
				((Batches[0]->bIsWriting && Batches[0]->bIsFinished.load(std::memory_order_acquire)) || Batches[0]->bIsEmptyFileSentinel))
			{
				uint64 WriteDoneStartCycles = FPlatformTime::Cycles64();

				// Could be empty buffer if sentinel
				if (Batches[0]->BatchBuffer.GetSize())
				{
					uint64 BufferBase = (uint64)((uint8*)Batches[0]->BatchBuffer.GetData() - (uint8*)IOBUffer.GetData());

					IOBufferFreeBlockList.Add({BufferBase, BufferBase + Batches[0]->BatchBuffer.GetSize()});
					SortAndCoalesceFreeList(IOBufferFreeBlockList);
				}

				FFileConstructionState* File = Batches[0]->OwningFile;
				File->OutstandingBatches--;

				Batches.RemoveAt(0);
				
				if (File->NewFile->IsError())
				{
					File->bSuccess = false;

					// This is not great, but right now the FArchive can't report what the error was. Without any
					// way to tease it apart, we run with the most likely issue: we ran out of disk space. Unfortunately
					// it's unreliable to ask the file system what is available and check if it's low and use that for the 
					// conversion.
					File->ConstructionError = EConstructionError::OutOfDiskSpace;
				}
					
				// If we have nothing further to read and all the file's reads are done,
				// since we only ever have 1 write batch we know it must be done.
				if (File->NextChunkPartToRead == File->FileManifest->ChunkParts.Num() &&
					File->OutstandingBatches == 0)
				{
					CompleteConstructedFile(*File);

					UE_LOG(LogBuildPatchServices, Verbose, TEXT("Completed file: %s, New ActiveFileCount = %d"), *File->BuildFilename, ActiveFiles.Num() - 1);

					// Since we write in order, we must be the first in the queue.
					check (ActiveFiles[0].Get() == File);

					// This frees the file
					ActiveFiles.RemoveAt(0);

					// We completed the previous file, if there's another active file we need to tell
					// the stats that it has started. If there isn't, we'll fire it off when we start it.
					if (ActiveFiles.Num())
					{
						FileConstructorStat->OnFileStarted(ActiveFiles[0]->BuildFilename, ActiveFiles[0]->FileManifest->FileSize);
					}
				}

				WriteDoneCycles += FPlatformTime::Cycles64() - WriteDoneStartCycles;
			}

			// Check for completed reads.
			if (Batches.Num())
			{
				uint64 ReadDoneStartCycles = FPlatformTime::Cycles64();

				// We have to retire reads in order because later reads could be waiting for data to
				// get placed correctly by the earlier one. e.g. read 1 could be placing in the memory store
				// and read 2 could be wanting to read from that when it retires.
				// Since we queue the batches in order, we can look at the first reading batch and know it's the next one.
				for (TUniquePtr<FBatchState>& Batch : Batches)
				{
					if (Batch->bIsReading)
					{
						if (Batch->bIsFinished.load(std::memory_order_acquire))
						{
							FFileConstructionState* File = Batch->OwningFile;

							CompleteReadBatch(*File->FileManifest, *Batch.Get());
							Batch->bIsReading = false;
							Batch->bNeedsWrite = true;

							// Only update the file construction error if it's the first one
							// since errors can cascade and make it not clear what the original problem was.
							if (File->ConstructionError == EConstructionError::None &&
								Batch->Error != EConstructionError::None)
							{
								File->ErrorContextGuid = Batch->ErrorContextGuid;
								File->ConstructionError = Batch->Error;
								File->bSuccess = false;
							}
						}
						else
						{
							// We have to retire reads in order, so once we hit one that isn't finished we stop.
							break;
						}
					}
					// else - might be a writing batch before the first read.
				}

				ReadDoneCycles += FPlatformTime::Cycles64() - ReadDoneStartCycles;
			}

			// If we've aborted, fail the files.
			for (TUniquePtr<FFileConstructionState>& File : ActiveFiles)
			{
				if (File->bSuccess &&
					bShouldAbort)
				{
					File->ConstructionError = EConstructionError::Aborted;
					File->bSuccess = false;
				}

				bHasAnyFileFailed |= !File->bSuccess;
			}

			// If we are in a failure case, we want to clear out anything happening with the cloud source.
			// We'll hit this over and over as we "drain", but we need to do that anyway because further failures
			// might try to queue more cloud reads.
			if (bHasAnyFileFailed)
			{
				CloudSource->Abort();
			}

			if (!bIsPaused && !bHasAnyFileFailed)
			{
				// Can we start another read?
				// LONGTERM - if we are memory constrained and have an install source we can consider only dispatching
				// single reads. This dramatically lowers our memory requirements as install sources often use a lot
				// of small pieces of chunks - and we have to load the entire chunk into memory.
				bool bCheckForAnotherRead = true;
				while (bCheckForAnotherRead)
				{
					double ReadCheckStartCycles = FPlatformTime::Cycles64();

					TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_ReadCheck);

					// We only check for another if we got one queued so we fill up the buffer space asap.
					bCheckForAnotherRead = false;

					int32 BiggestFreeBlockSize = 0;
					int32 BiggestFreeBlockSlotIndex = 0;
					for (int32 Slot = 0; Slot < IOBufferFreeBlockList.Num(); Slot++)
					{
						if (IOBufferFreeBlockList[Slot].End - IOBufferFreeBlockList[Slot].Start > BiggestFreeBlockSize)
						{
							BiggestFreeBlockSize = IOBufferFreeBlockList[Slot].End - IOBufferFreeBlockList[Slot].Start;
							BiggestFreeBlockSlotIndex = Slot;
						}
					}

					if (BiggestFreeBlockSize > 0)
					{
						// Default to continue to work on the last file.
						FFileConstructionState* FileToStart = nullptr;
						int32 ActiveFileCount = ActiveFiles.Num();
						if (ActiveFileCount && ActiveFiles[ActiveFileCount-1]->NextChunkPartToRead < ActiveFiles[ActiveFileCount-1]->FileManifest->ChunkParts.Num())
						{
							FileToStart = ActiveFiles[ActiveFileCount-1].Get();
						}

						// No more work do to on active files - is there another to start?
						if (!FileToStart)
						{
							bool bAllowAnotherFile = bAllowMultipleFilesInFlight || (ActiveFiles.Num() == 0);

							// This is not a race because the only place we ever increment is this thread, this function right
							// below.
							int32 IndexToConstruct = NextIndexToConstruct.load(std::memory_order_acquire);
							bool bAnotherFileExists = IndexToConstruct < ConstructionList.Num();
							
							if (bAllowAnotherFile && bAnotherFileExists)
							{
								// Even though another file exists, we might not be able to start it if its dependent
								// files aren't done.
								bool bDelayForDependencies = false;
								if (ConstructionList[IndexToConstruct].LatestDependentInstallSource != -1)
								{
									// Files are in construct order - if the first one is after our last dependency then we know we
									// are safe.
									if (ActiveFileCount && ActiveFiles[0]->ConstructionIndex <= ConstructionList[IndexToConstruct].LatestDependentInstallSource)
									{
										bDelayForDependencies = true;
									}
								}

								if (!bDelayForDependencies)
								{
									// Now commit this file since we are starting it.
									NextIndexToConstruct.fetch_add(1, std::memory_order_acq_rel);

									FString StagingFileName = Configuration.StagingDirectory / Configuration.ConstructList[IndexToConstruct];
									TUniquePtr<FFileConstructionState> AnotherFile = MakeUnique<FFileConstructionState>(
										ConstructionList[IndexToConstruct].FileManifest, 
										Configuration.ConstructList[IndexToConstruct], 
										MoveTemp(StagingFileName));

									AnotherFile->ConstructionIndex = IndexToConstruct;

									TRACE_BOOKMARK(TEXT("Starting File: %s [%u]"), *AnotherFile->BuildFilename, AnotherFile->FileManifest->ChunkParts.Num());

									UE_LOG(LogBuildPatchServices, Verbose, TEXT("Starting File: %s [%s bytes, %d chunks], New ActiveFileCount = %d"), 
										   *AnotherFile->BuildFilename, 
										   *FormatNumber(AnotherFile->FileManifest->FileSize),
										   AnotherFile->FileManifest->ChunkParts.Num(),
										   ActiveFiles.Num() + 1);

									StartFile(*AnotherFile.Get(), ResumeData);

									if (AnotherFile->bSkippedConstruction)
									{
										// Nothing else needs to happen with this file - we'll loop around to try for another.
										bCheckForAnotherRead = true;
									}
									else
									{
										// Only start the file if there's not currently an active file - otherwise we
										// do it when the current one finishes.
										if (ActiveFileCount == 0)
										{
											FileConstructorStat->OnFileStarted(AnotherFile->BuildFilename, AnotherFile->FileManifest->FileSize);
										}

										FileToStart = AnotherFile.Get();
										ActiveFiles.Push(MoveTemp(AnotherFile));
									}
								} // end if not delaying for dependencies
								else
								{
									UE_LOG(LogBuildPatchServices, Verbose, TEXT("Delaying %s due to incomplete dependencies"), *Configuration.ConstructList[IndexToConstruct]);
								}

								// If we are delaying for dependencies we fall through here with FileToStart = nullptr
								// and nothing happens until we check again when the construct thread is woken up again.
							}
							else
							{
								// If we don't have any files to construct, then we can't start any more reads at all.
								// If there are no active files, then we are done.
								if (ActiveFiles.Num() == 0)
								{
									bStillProcessingFiles = false;
								}
							}
						} // end if we are starting a new file.

						// It's possible the file failed during creation and we need to start the failure process. We need to
						// set this since we did the scan before here.
						if (FileToStart && !FileToStart->bSuccess)
						{
							bHasAnyFileFailed = true;
						}

						if (FileToStart && FileToStart->bSuccess)
						{
							if (FileToStart->FileManifest->ChunkParts.Num() == 0)
							{
								// We have a file that will never launch any batches, which means it'll never hit the finalization
								// logic. We can't complete it here because then we are out of order. So we need to inject a placeholder
								// batch that will auto pass the write check and also prevent us from sleeping on an event.
								TUniquePtr<FBatchState> Batch = MakeUnique<FBatchState>();
								Batch->bNeedsWrite = false;
								Batch->bIsReading = false;
								Batch->OwningFile = FileToStart;
								Batch->bIsEmptyFileSentinel = true;
								Batch->bIsFinished.store(false, std::memory_order_release);
								FileToStart->OutstandingBatches++;

								Batches.Add(MoveTemp(Batch));

								bCheckForAnotherRead = true;
							}
							else
							{

								uint32 MaxBufferSize = BiggestFreeBlockSize;
								if (MaxBufferSize > MaxWriteBatchSize)
								{
									MaxBufferSize = MaxWriteBatchSize;
								}

								// We want to do big batches as much as possible. If we only have space for a single chunk,
								// that's fine if it's a single chunk file, but generally we want to try and favor large batches.
								// If we're at the point where we are worried about this, we have enough outstanding work to keep
								// the pipelines full so we can afford to wait for room.
								uint32 MaxFileBatchSize = 0;
								const TArray<FChunkPart>& FileChunkParts = FileToStart->FileManifest->ChunkParts;
								uint32 NextChunkSize = FileChunkParts[FileToStart->NextChunkPartToRead].Size;

								for (int32 ChunkPartIndex = FileToStart->NextChunkPartToRead;
									ChunkPartIndex < FileChunkParts.Num();
									ChunkPartIndex++)
								{
									MaxFileBatchSize += FileChunkParts[ChunkPartIndex].Size;
									if (MaxFileBatchSize >= MaxWriteBatchSize)
									{
										// If it's big enough for the max batch size we no longer care.
										MaxFileBatchSize = MaxWriteBatchSize;
										break;
									}
								}

								// If the file can support a large batch, we want to wait until we have room for a reasonable size.
								// We know we can eventually read the next chunk because during init we sized the buffers such that
								// we could.
								if (NextChunkSize > MaxBufferSize &&
									Batches.Num() == 0)
								{
									FileToStart->bSuccess = false;
									FileToStart->ConstructionError = EConstructionError::InternalConsistencyError;
									UE_LOG(LogBuildPatchServices, Error, TEXT("Chunk size encountered larger than batch buffer size! %u vs %u"), NextChunkSize, MaxBufferSize);
									bHasAnyFileFailed = true;

									// We'll fail the next conditional below and then start the failure process on the next loop.
								}

								if (MaxBufferSize >= MaxFileBatchSize)
								{
									TUniquePtr<FBatchState> Batch = MakeUnique<FBatchState>();
									Batch->bNeedsWrite = true;
									Batch->bIsReading = true;
									Batch->OwningFile = FileToStart;
									Batch->BatchBuffer = FMutableMemoryView(IOBUffer.GetData() + IOBufferFreeBlockList[BiggestFreeBlockSlotIndex].Start, MaxBufferSize);
									Batch->bIsFinished.store(false, std::memory_order_release);
									StartReadBatch(*FileToStart, *Batch.Get());

									// The read might not have used the whole thing, so only consume as much
									// as it needed.
									IOBufferFreeBlockList[BiggestFreeBlockSlotIndex].Start += Batch->BatchBuffer.GetSize();
									if (IOBufferFreeBlockList[BiggestFreeBlockSlotIndex].Start == IOBufferFreeBlockList[BiggestFreeBlockSlotIndex].End)
									{
										// Ate whole thing
										IOBufferFreeBlockList.RemoveAt(BiggestFreeBlockSlotIndex);
										SortAndCoalesceFreeList(IOBufferFreeBlockList);
									}

									bCheckForAnotherRead = true;
								
									UE_LOG(LogBuildPatchServices, VeryVerbose, TEXT("Starting ReadBatch: %d, Chunks=%d, Bytes=%s, Batches=%d"), 
										Batch->BatchId, 
										Batch->ChunkCount, 
										*FormatNumber(Batch->BatchBuffer.GetSize()),
										Batches.Num() + 1
									);

									Batches.Push(MoveTemp(Batch));
								}
							} // end if file has parts
						} // end if we have a file to read from.
					} // end if free block exists.

					ReadCheckCycles += FPlatformTime::Cycles64() - ReadCheckStartCycles;
				} // end looping on whether we should start a read.

				// Can we start a write? 
				// We always have to issue writes in order and there can only be one, so it must be the
				// first active file, and the first batch. 
				if (Batches.Num() && Batches[0]->bNeedsWrite && !Batches[0]->bIsReading)
				{
					uint64 WriteStartStartCycles = FPlatformTime::Cycles64();

					FFileConstructionState* FirstFile = ActiveFiles[0].Get();

					Batches[0]->bNeedsWrite = false;
					Batches[0]->bIsWriting = true;
					Batches[0]->bIsFinished.store(false, std::memory_order_relaxed);

					// Launch the write and hash the buffer on this thread.
					UE_LOG(LogBuildPatchServices, VeryVerbose, TEXT("Writing Batch: %d, file %s [%d - %d]"),
						Batches[0]->BatchId, 
						*FirstFile->BuildFilename,
						Batches[0]->StartChunkPartIndex,
						Batches[0]->StartChunkPartIndex + Batches[0]->ChunkCount
						);

					IConstructorChunkSource::FRequestProcessFn WriteFn = CreateWriteRequest(FirstFile->NewFile.Get(), *Batches[0].Get());
					QueueGenericThreadTask(WriteThreadIndex, MoveTemp(WriteFn));
					
					uint64 HashStartCycles = FPlatformTime::Cycles64();
					WriteStartCycles += HashStartCycles - WriteStartStartCycles;

					{
						TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_Hash);
						FirstFile->HashState.Update((const uint8*)Batches[0]->BatchBuffer.GetData(), Batches[0]->BatchBuffer.GetSize());
					}
					HashCycles += FPlatformTime::Cycles64() - HashStartCycles;

				} // end if checking for write
			} // end not paused
		} // end state check

		// If the file progress changed since we last saw it, post the update.
		// Note that we want this to update reasonably often but we're about to wait
		// potentially until all reads complete - however the only time things actually
		// take a long time wall-clock wise is when we are downloading, and the cloud
		// source will then prevent us from sleeping too long, so we actually catch these
		// updates.
		// We do this from here to ensure we always increase rather than risk multi thread
		// races.
		if (ActiveFiles.Num())
		{
			// We only post the progress for the first file in the active list - this means
			// that when we finish that file we'll likely jump to the middle progress for the next
			// file, but we don't have a way to post the progress per file.
			FFileConstructionState* File = ActiveFiles[0].Get();

			uint64 CurrentFileProgress;
			File->ProgressLock.Lock();
			CurrentFileProgress = File->Progress;
			File->ProgressLock.Unlock();
					
			if (CurrentFileProgress != File->LastSeenProgress)
			{
				// this updates the overall install progress.
				CountBytesProcessed(CurrentFileProgress - File->LastSeenProgress);
				File->LastSeenProgress = CurrentFileProgress;
				FileConstructorStat->OnFileProgress(File->BuildFilename, CurrentFileProgress);
			}
		}

		uint32 WaitTimeMs = TNumericLimits<uint32>::Max();
		{
			uint64 CloudTickStartCycles = FPlatformTime::Cycles64();

			TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_TickCloud);

			// Max downloads is tricky - the internet makes no guarantees about which of our downloads finishes first. So while
			// we want to have as many outstanding as possible to cover up connection overhead / resends and all that, if we enqueue 
			// downloads from several batches, we can end up where the first batch can't complete because it's waiting on a download 
			// that isn't finishing due to congestion from the next batch's download. Then we counterintuitively end up with FEWER 
			// outstanding downloads because we can't launch more batches due to waiting on the front of this long chain. This is easily reproducible
			// where Insights will show highly out of order completion. This ordering unfortunately scales with the number of
			// outstanding downloads to a certain extent: if you allow 16 downloads then you'll end up waiting on a download
			// 16 issues old - 32 downloads and you'll wait on one 32 issues old. We try to bound this by capping the issued
			// downloads here, and in the cloud source we prevent queues if we get too far ahead of the last download.
			// Note this also gets adjusted by the connection health stuff internal to the cloud source.
			const uint32 MaxDownloads = FMath::Max(1U, FMath::DivideAndRoundUp(MaxWriteBatchSize, ExpectedChunkSize));
			
			// WaitTimeMs is an OUT param
			CloudSource->Tick(!bIsPaused && !bHasAnyFileFailed, WaitTimeMs, MaxDownloads);

			CloudTickCycles += FPlatformTime::Cycles64() - CloudTickStartCycles;
		}

		int32 ActiveReadBatches = 0;
		int32 ActiveWriteBatches = 0;
		int32 EmptyFileBatches = 0;
		for (const TUniquePtr<FBatchState>& _Batch : Batches)
		{
			EmptyFileBatches += _Batch->bIsEmptyFileSentinel ? 1 : 0;
			ActiveReadBatches += _Batch->bIsReading ? 1 : 0;
			ActiveWriteBatches += _Batch->bIsWriting ? 1 : 0;
		}

		// Empty files don't have async jobs.
		bool bAsyncJobExists = ActiveReadBatches || ActiveWriteBatches;

		if (bHasAnyFileFailed)
		{
			// We can only bail when all our async jobs have completed.
			if (!bAsyncJobExists)
			{
				break;
			}
		}

		TRACE_INT_VALUE(TEXT("BPS.FC.ActiveFiles"), ActiveFiles.Num());
		TRACE_INT_VALUE(TEXT("BPS.FC.ActiveReadBatches"), ActiveReadBatches);

		if (bStillProcessingFiles && 
			bAsyncJobExists)
		{
			if (WaitTimeMs == TNumericLimits<uint32>::Max())
			{
				WaitTimeMs = 15*1000;
			}

			uint64 WaitStartCycles = FPlatformTime::Cycles64();
			TRACE_CPUPROFILER_EVENT_SCOPE(CFFC_Waiting);
			// We have a bunch of stuff outstanding that will wake us up if something happens.
			WakeUpDispatchThreadEvent->Wait(WaitTimeMs);

			WaitCycles += FPlatformTime::Cycles64() - WaitStartCycles;
		}
	} // end loop until we complete all the files.

	// Any remaining active files (due to abort/failure) need to be failed and completed
	// so that errors get reported. We want to report the non-abort failures first, because
	// anything else that was in flight gets reported as an abort and the first error is the
	// one we actually care about.
	if (ActiveFiles.Num())
	{
		for (TUniquePtr<FFileConstructionState>& File : ActiveFiles)
		{
			if (!File->bSuccess && File->ConstructionError != EConstructionError::Aborted)
			{
				CompleteConstructedFile(*File.Get());
			}
		}

		// Now handle everything else.
		for (TUniquePtr<FFileConstructionState>& File : ActiveFiles)
		{
			if (!File->bSuccess && File->ConstructionError != EConstructionError::Aborted)
			{
				continue; // handled in previous loop.
			}

			if (File->bSuccess)
			{
				File->bSuccess = false;
				File->ConstructionError = EConstructionError::Aborted;
			}
			CompleteConstructedFile(*File.Get());
		}
	}

	uint64 ConstructCycles = FPlatformTime::Cycles64() - ConstructStartCycles;
	uint64 UnaccountedForCycles = ConstructCycles - HashCycles - WaitCycles - ReadCheckCycles - ReadDoneCycles - WriteStartCycles - WriteDoneCycles - CloudTickCycles;

	double ConstructSec = FPlatformTime::ToSeconds64(ConstructCycles);
	UE_LOG(LogBuildPatchServices, Display, TEXT("Construction done: %.2f sec. Hash %.1f%% Wait %.1f%% ReadCheck %.1f%% WriteStart %.1f%% ReadDone %.1f%% WriteDone %.1f%% CloudTick %.1f%% Unaccounted %.1f%%"),
		ConstructSec,
		100.0 * HashCycles / ConstructCycles,
		100.0 * WaitCycles / ConstructCycles,
		100.0 * ReadCheckCycles / ConstructCycles,
		100.0 * WriteStartCycles / ConstructCycles,
		100.0 * ReadDoneCycles / ConstructCycles,
		100.0 * WriteDoneCycles / ConstructCycles,
		100.0 * CloudTickCycles / ConstructCycles,
		100.0 * UnaccountedForCycles / ConstructCycles
		);
}

void FBuildPatchFileConstructor::DeleteDirectoryContents(const FString& RootDirectory)
{
	TArray<FString> SubDirNames;
	IFileManager::Get().FindFiles(SubDirNames, *(RootDirectory / TEXT("*")), false, true);
	for (const FString& DirName : SubDirNames)
	{
		IFileManager::Get().DeleteDirectory(*(RootDirectory / DirName), false, true);
	}

	TArray<FString> SubFileNames;
	IFileManager::Get().FindFiles(SubFileNames, *(RootDirectory / TEXT("*")), true, false);
	for (const FString& FileName : SubFileNames)
	{
		IFileManager::Get().Delete(*(RootDirectory / FileName), false, true);
	}
}
