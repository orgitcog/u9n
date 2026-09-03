// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDataGathererDiscoveryCache.h"

#include "AssetDataGathererPrivate.h"
#include "AssetRegistryPrivate.h"
#include "AssetRegistry/AssetRegistryTelemetry.h"
#include "Compression/CompressedBuffer.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Logging/LogMacros.h"
#include "Memory/SharedBuffer.h"
#include "Misc/Paths.h"
#include "Misc/PathViews.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Serialization/Archive.h"
#include "Serialization/LargeMemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "TelemetryRouter.h"
#include "Templates/UniquePtr.h"

FArchive& operator<<(FArchive& Ar, FFileJournalFileHandle& H)
{
	Ar.Serialize(H.Bytes, sizeof(H.Bytes));
	return Ar;
}

namespace AssetDataGathererConstants
{
const FGuid DiscoveryCacheVersion(TEXT("4F4C364CC08C47B9BF18278136E1CB6E"));
}

namespace UE::AssetDataGather::Private
{

FString FAssetDataDiscoveryCache::GetCacheFileName() const
{
	return FPaths::Combine(GGatherSettings.GetAssetRegistryCacheRootFolder(),
		TEXT("CachedAssetRegistryDiscovery.bin"));
}

void FAssetDataDiscoveryCache::LoadAndUpdateCache()
{
	if (bInitialized)
	{
		return;
	}
	TRACE_CPUPROFILER_EVENT_SCOPE(AssetDataGatherLoadDiscoveryCache);
	bInitialized = true;

	WriteEnabled = GGatherSettings.IsDiscoveryCacheWriteEnabled();
	CachedVolumes.Empty();
	if (!GGatherSettings.IsDiscoveryCacheReadEnabled() && WriteEnabled == EFeatureEnabled::Never)
	{
		return;
	}

	FString TestError;
	FFileJournalId TestJournalId;
	FFileJournalEntryHandle TestLatestJournalEntry;
	FString ProjectDir = FPaths::ProjectDir();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FString TestVolumeName = PlatformFile.FileJournalGetVolumeName(ProjectDir);
	EFileJournalResult TestResult = PlatformFile.FileJournalGetLatestEntry(*TestVolumeName,	TestJournalId,
		TestLatestJournalEntry, &TestError);
	bool bPlatformSupported = TestResult == EFileJournalResult::Success;

	if (bPlatformSupported)
	{
		// Do a quick test if can grab data from the journal file which does a similar call in IterateDirectory
		// If it fails we are very likely going to fail to iterate using journal files, so lets disable the cache
		FFileJournalData TestData = PlatformFile.FileJournalGetFileData(*FPaths::ConvertRelativePathToFull(ProjectDir), &TestError);
		if (!TestData.bIsValid)
		{
			bPlatformSupported = false;

			UE::Telemetry::AssetRegistry::FFileJournalErrorTelemetry Telemetry;
			Telemetry.Directory = FPaths::ConvertRelativePathToFull(ProjectDir);
			Telemetry.ErrorString = TestError;
			FTelemetryRouter::Get().ProvideTelemetry(Telemetry);
		}
	}

	bool bInvalidateEnabled = GGatherSettings.IsDiscoveryCacheInvalidateEnabled();
	bool bReadEnabled = GGatherSettings.IsDiscoveryCacheReadEnabled() && (bPlatformSupported || !bInvalidateEnabled);
	// Precalculate IfPlatformSupported -> Never if we already know the project doesn't support it
	if (WriteEnabled == EFeatureEnabled::IfPlatformSupported && !bPlatformSupported)
	{
		WriteEnabled = EFeatureEnabled::Never;
	}

	if ((!bReadEnabled && GGatherSettings.IsDiscoveryCacheReadEnabled()) ||
		(WriteEnabled == EFeatureEnabled::Never && GGatherSettings.IsDiscoveryCacheWriteEnabled() != EFeatureEnabled::Never))
	{
		const TCHAR* MissingOperation = (!bReadEnabled && WriteEnabled == EFeatureEnabled::Never) ? TEXT("read or written") :
			(!bReadEnabled ? TEXT("read") : TEXT("written"));
		UE_LOG(LogAssetRegistry, Display,
			TEXT("PlatformFileJournal is not available on volume '%s' of project directory '%s', so AssetDiscovery cache will not be %s. Unavailability reason:\n\t%s"),
			*TestVolumeName, *FPaths::ConvertRelativePathToFull(ProjectDir), MissingOperation, *TestError);

		UE::Telemetry::AssetRegistry::FFileJournalErrorTelemetry Telemetry;
		Telemetry.Directory = FPaths::ConvertRelativePathToFull(ProjectDir);
		Telemetry.ErrorString = TestError;
		FTelemetryRouter::Get().ProvideTelemetry(Telemetry);
	}

	if (!bReadEnabled)
	{
		return;
	}

	bool bReadSucceeded = TryReadCacheFile();
	if (!bReadSucceeded)
	{
		return;
	}

	if (!bInvalidateEnabled)
	{
		return;
	}

	for (TPair<FString, FCachedVolumeInfo>& VolumePair : CachedVolumes)
	{
		FString& VolumeName = VolumePair.Key;
		FCachedVolumeInfo& VolumeInfo = VolumePair.Value;

		TMap<FFileJournalFileHandle, FString> KnownDirectories;
		TSet<FString> ModifiedDirectories;
		for (TMap<FString, FCachedDirScanDir>::TIterator DirIter(VolumeInfo.Dirs); DirIter; ++DirIter)
		{
			FString& DirName = DirIter->Key;
			FCachedDirScanDir& DirData = DirIter->Value;
			// If not valid, we cannot remove the DirData yet because we will need it to get the list of
			// its child directories that exist in the tree, at the point when we rescan it
			if (DirData.bCacheValid && DirData.JournalHandle != FileJournalFileHandleInvalid)
			{
				KnownDirectories.Add(DirData.JournalHandle, DirName);
			}
		}

		EFileJournalResult ReadModifiedResult = EFileJournalResult::Success;
		bool bReadModifiedSucceeded = false;
		if (!VolumeInfo.bJournalAvailable)
		{
			UE_LOG(LogAssetRegistry, Warning,
				TEXT("PlatformFileJournal is not available on volume '%s'. AssetRegistry discovery of files on this volume will be uncached. Unavailability reason:\n\t%s"),
				*VolumeName, *VolumeInfo.LastError);

			UE::Telemetry::AssetRegistry::FFileJournalErrorTelemetry MissingJournalTelemetry;
			MissingJournalTelemetry.Directory = VolumeName;
			MissingJournalTelemetry.ErrorString = VolumeInfo.LastError;
			FTelemetryRouter::Get().ProvideTelemetry(MissingJournalTelemetry);
		}
		else
		{
			EFileJournalResult Result = PlatformFile.FileJournalReadModified(*VolumeInfo.VolumeName,
				VolumeInfo.JournalId, VolumeInfo.NextJournalEntryToScan, KnownDirectories, ModifiedDirectories,
				VolumeInfo.NextJournalEntryToScan, &VolumeInfo.LastError);
			switch (Result)
			{
			case EFileJournalResult::Success:
				bReadModifiedSucceeded = true;
				break;
			case EFileJournalResult::JournalWrapped:
			{
				UE_LOG(LogAssetRegistry, Display,
					TEXT("PlatformFileJournal journal has wrapped for volume '%s'. AssetRegistry discovery of files on this volume will be uncached. Notes on wrapping:")
					TEXT("\r\n%s"),
					*VolumeName, *VolumeInfo.LastError);

				UE::Telemetry::AssetRegistry::FFileJournalWrappedTelemetry WrappedJournalTelemetry;
				WrappedJournalTelemetry.VolumeName = VolumeName;
				WrappedJournalTelemetry.JournalMaximumSize = VolumeInfo.JournalMaximumSize;
				FTelemetryRouter::Get().ProvideTelemetry(WrappedJournalTelemetry);
				break;
			}
			default:
			{
				UE_LOG(LogAssetRegistry, Warning,
					TEXT("PlatformFileJournal is not available for volume '%s'. AssetRegistry discovery of files on this volume will be uncached. Unavailability reason:")
					TEXT("\n\t%s"),
					*VolumeName, *VolumeInfo.LastError);

				UE::Telemetry::AssetRegistry::FFileJournalErrorTelemetry MissingJournalTelemetry;
				MissingJournalTelemetry.Directory = VolumeName;
				MissingJournalTelemetry.ErrorString = VolumeInfo.LastError;
				FTelemetryRouter::Get().ProvideTelemetry(MissingJournalTelemetry);
				break;
			}
			}
		}

		if (!bReadModifiedSucceeded)
		{
			VolumeInfo.JournalId = VolumeInfo.JournalIdOnDisk;
			VolumeInfo.NextJournalEntryToScan = VolumeInfo.NextJournalEntryOnDisk;
			VolumeInfo.Dirs.Empty();
		}
		else
		{
			for (const FString& ModifiedDirectory : ModifiedDirectories)
			{
				FCachedDirScanDir* DirData = VolumeInfo.Dirs.Find(ModifiedDirectory);
				if (DirData)
				{
					// We cannot remove the DirData yet because we will need it to get the list of its child
					// directories that exist in the tree, at the point when we rescan it
					DirData->bCacheValid = false;
				}
			}
		}
	}
}

void FAssetDataDiscoveryCache::SaveCache()
{
	if (WriteEnabled == EFeatureEnabled::Never)
	{
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(AssetDataGatherSaveDiscoveryCache);
	for (TPair<FString, FCachedVolumeInfo>& VolumePair : CachedVolumes)
	{
		VolumePair.Value.PreSave();
	}

	FString Filename = GetCacheFileName();

	FLargeMemoryWriter Writer;
	SerializeWriteCacheFile(Writer);
	FCompressedBuffer Compressed = FCompressedBuffer::Compress(FSharedBuffer::MakeView(Writer.GetView()));

	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileWriter(*Filename));
	if (!Ar)
	{
		UE_LOG(LogAssetRegistry, Warning, TEXT("Could not write to DiscoveryCacheFile %s."), *Filename);
		return;
	}
	Compressed.Save(*Ar);
}

bool FAssetDataDiscoveryCache::TryReadCacheFile()
{
	FSharedBuffer RawBuffer;
	bool bCacheValid = true;
	bool bCacheCorrupt = false;
	FString Filename = GetCacheFileName();
	{
		TUniquePtr<FArchive> CompressedFileArchive(IFileManager::Get().CreateFileReader(*Filename));
		if (!CompressedFileArchive || CompressedFileArchive->TotalSize() == 0)
		{
			bCacheValid = false;
		}
		else
		{
			FCompressedBufferReader CompressedBuffer(*CompressedFileArchive);
			if (CompressedBuffer.GetRawSize() == 0)
			{
				bCacheValid = false;
				bCacheCorrupt = true;
			}
			else
			{
				RawBuffer = CompressedBuffer.Decompress();
				if (RawBuffer.GetSize() != CompressedBuffer.GetRawSize())
				{
					bCacheValid = false;
					bCacheCorrupt = true;
				}
			}
		}
	}
	if (bCacheValid)
	{
		FMemoryReaderView Ar(RawBuffer.GetView());
		SerializeReadCacheFile(Ar);
		if (Ar.IsError())
		{
			bCacheValid = false;
			bCacheCorrupt = true;
		}
	}

	if (!bCacheValid)
	{
		if (bCacheCorrupt)
		{
			UE_LOG(LogAssetRegistry, Warning,
				TEXT("Corrupt AssetDiscovery cache %s. AssetRegistry discovery of files will be uncached."),
				*Filename);
		}
		else
		{
			UE_LOG(LogAssetRegistry, Display,
				TEXT("No AssetDiscovery cache present at %s. AssetRegistry discovery of files will be uncached."),
				*Filename);
		}
		CachedVolumes.Empty();
		return false;
	}

	for (TPair<FString, FCachedVolumeInfo>& Pair : CachedVolumes)
	{
		Pair.Value.InitializePlatformData();
	}
	return true;
}

void FAssetDataDiscoveryCache::SerializeReadCacheFile(FArchive& Ar)
{
	FGuid Version;
	Ar << Version;
	if (Version != AssetDataGathererConstants::DiscoveryCacheVersion)
	{
		Ar.SetError();
		return;
	}

	CachedVolumes.Empty();
	Ar << CachedVolumes;
}

void FAssetDataDiscoveryCache::SerializeWriteCacheFile(FArchive& Ar)
{
	FGuid Version = AssetDataGathererConstants::DiscoveryCacheVersion;
	Ar << Version;
	Ar << CachedVolumes;
}

void FAssetDataDiscoveryCache::Shutdown()
{
	WriteEnabled = EFeatureEnabled::Never;

	CachedVolumes.Empty();
	while (!ScanQueueDirFullDatas.IsEmpty())
	{
		TPair<FString, FCachedDirScanDir> Pair;
		ScanQueueDirFullDatas.Dequeue(Pair);
	}
	while (!ScanQueueDirHandles.IsEmpty())
	{
		FDiscoveredSubDirData SubDirData;
		ScanQueueDirHandles.Dequeue(SubDirData);
	}
}

FCachedVolumeInfo& FAssetDataDiscoveryCache::FindOrAddVolume(FStringView PathOrVolumeName)
{
	FStringView VolumeNameView;
	FStringView Remainder;
	FPathViews::SplitVolumeSpecifier(PathOrVolumeName, VolumeNameView, Remainder);
	FString VolumeName(VolumeNameView);
	if (VolumeName.IsEmpty())
	{
		VolumeName = GEmptyVolumeName;
	}
	FCachedVolumeInfo& Volume = CachedVolumes.FindOrAdd(VolumeName);
	Volume.ConditionalConstruct(VolumeName);
	return Volume;
}

FCachedDirScanDir& FAssetDataDiscoveryCache::FindOrAddDir(FStringView Path, FCachedVolumeInfo** OutVolume)
{
	FCachedVolumeInfo& Volume = FindOrAddVolume(Path);
	if (OutVolume)
	{
		*OutVolume = &Volume;
	}
	return Volume.FindOrAddDir(Path);
}

void FAssetDataDiscoveryCache::RemoveDir(FStringView Path)
{
	FStringView VolumeNameView;
	FStringView Remainder;
	FPathViews::SplitVolumeSpecifier(Path, VolumeNameView, Remainder);
	FString VolumeName(VolumeNameView);
	FCachedVolumeInfo* Info = CachedVolumes.Find(VolumeName);
	if (!Info)
	{
		return;
	}
	Info->RemoveDirs({ FString(Path) });
}

FCachedVolumeInfo* FAssetDataDiscoveryCache::FindVolume(FStringView PathOrVolumeName)
{
	FStringView VolumeNameView;
	FStringView Remainder;
	FPathViews::SplitVolumeSpecifier(PathOrVolumeName, VolumeNameView, Remainder);
	FString VolumeName(VolumeNameView);

	return CachedVolumes.Find(VolumeName);
}

FCachedDirScanDir* FAssetDataDiscoveryCache::FindDir(FStringView Path, FCachedVolumeInfo** OutVolume)
{
	FCachedVolumeInfo* Volume = FindVolume(Path);
	if (OutVolume)
	{
		*OutVolume = Volume;
	}
	if (!Volume)
	{
		return nullptr;
	}
	return Volume->FindDir(Path);
}

bool FindOrAddIsReparsePointRecursive(FStringView DirName, FCachedVolumeInfo& Volume, bool& bOutAdded)
{
	bOutAdded = false;
	if (DirName.IsEmpty())
	{
		return false;
	}

	// We must use a pointer here since re-assigning to Existing later as a value will invoke 
	// copy assignment on a potentially invalid type
	FCachedDirScanDir* Existing = &Volume.FindOrAddDir(DirName, bOutAdded);

	if (!Existing->bIsInsideReparsePoint.IsSet())
	{
		// Usually bIsReparsePoint is already set because we calculated it when we encountered this directory in the directory scan of its parent directory.
		// But for top-level directories or directories that were found to be modified by the journal, we did not already encounter it.
		// Calculate it now manually if it is not already set.
		if (!Existing->bIsReparsePoint.IsSet())
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			Existing->bIsReparsePoint = PlatformFile.IsSymlink(DirName.GetData()) == ESymlinkResult::Symlink;
		}

		// Base case for when we have found a reparse point, end the recursion and set true for all our previous calls
		if (Existing->bIsReparsePoint.GetValue())
		{
			Existing->bIsInsideReparsePoint = true;
			return true;
		}

		bool bAddedDuringRecursion = false;
		bool bIsInsideReparsePoint = FindOrAddIsReparsePointRecursive(FPaths::GetPath(DirName.GetData()), Volume, bAddedDuringRecursion);
		// If we added a new entry, then we may have invalidated Existing and need to fetch it again
		if (bAddedDuringRecursion)
		{
			bOutAdded = true;
			Existing = Volume.FindDir(DirName);
		}
		Existing->bIsInsideReparsePoint = bIsInsideReparsePoint;
	}

	return Existing->bIsInsideReparsePoint.GetValue();
}

bool FAssetDataDiscoveryCache::FindOrAddIsReparsePoint(FStringView DirName, FCachedVolumeInfo& Volume)
{
	bool bOutAdded = false;
	return FindOrAddIsReparsePointRecursive(DirName, Volume, bOutAdded);
}

void FAssetDataDiscoveryCache::QueueConsume()
{
	if (WriteEnabled == EFeatureEnabled::Never)
	{
		return;
	}
	FDiscoveredSubDirData SubDirData;
	while (ScanQueueDirHandles.Dequeue(SubDirData))
	{
		FString& DirName = SubDirData.DirNameAbsPath;
		FCachedVolumeInfo& Volume = FindOrAddVolume(DirName);
		if (!Volume.bJournalAvailable && WriteEnabled != EFeatureEnabled::Always)
		{
			continue;
		}
		FCachedDirScanDir& Existing = Volume.FindOrAddDir(DirName);
		Existing.JournalHandle = MoveTemp(SubDirData.JournalHandle);
		Existing.bIsReparsePoint = SubDirData.bIsReparsePoint;
	}

	TPair<FString, FCachedDirScanDir> ScanDirPair;
	while (ScanQueueDirFullDatas.Dequeue(ScanDirPair))
	{
		FString& DirName = ScanDirPair.Key;
		FCachedDirScanDir& ScanDir = ScanDirPair.Value;
		FCachedVolumeInfo& Volume = FindOrAddVolume(DirName);
		if (FindOrAddIsReparsePoint(DirName, Volume))
		{
			continue;
		}
		if (!Volume.bJournalAvailable && WriteEnabled != EFeatureEnabled::Always)
		{
			continue;
		}

		FCachedDirScanDir& Existing = Volume.FindOrAddDir(DirName);

		// Mark for removal any subpaths in the cache that no longer exist on disk
		if (!Existing.SubDirRelPaths.IsEmpty())
		{
			TSet<FString> StillExisting(ScanDir.SubDirRelPaths);
			for (FString& OldRelPath : Existing.SubDirRelPaths)
			{
				if (!StillExisting.Contains(OldRelPath))
				{
					Volume.DirsToRemove.Add(FPaths::Combine(DirName, OldRelPath));
				}
			}
		}

		// If neither the new entry nor the existing entry have the JournalHandle,
		// initialize it now.
		if (ScanDir.JournalHandle == FileJournalFileHandleInvalid)
		{
			if (Existing.JournalHandle != FileJournalFileHandleInvalid)
			{
				ScanDir.JournalHandle = Existing.JournalHandle;
			}
			else
			{
				FFileJournalData PlatformData =
					FPlatformFileManager::Get().GetPlatformFile().FileJournalGetFileData(*DirName);
				ScanDir.JournalHandle = PlatformData.JournalHandle;
			}
		}

		// These values get set in FindOrAddIsReparsePoint, make sure they dont get lost here
		ScanDir.bIsReparsePoint = Existing.bIsReparsePoint;
		ScanDir.bIsInsideReparsePoint = Existing.bIsInsideReparsePoint;

		Existing = MoveTemp(ScanDir);
		Existing.bCacheValid = true;
	}
}

void FAssetDataDiscoveryCache::QueueAdd(FString DirName, FCachedDirScanDir DirData)
{
	if (WriteEnabled == EFeatureEnabled::Never)
	{
		return;
	}
	ScanQueueDirFullDatas.Enqueue(TPair<FString, FCachedDirScanDir>(MoveTemp(DirName), MoveTemp(DirData)));
}

void FAssetDataDiscoveryCache::QueueAdd(FString DirName, FFileJournalFileHandle JournalHandle, bool bIsReparsePoint)
{
	if (WriteEnabled == EFeatureEnabled::Never)
	{
		return;
	}
	ScanQueueDirHandles.Enqueue(FDiscoveredSubDirData{MoveTemp(DirName), MoveTemp(JournalHandle), bIsReparsePoint});
}

void FCachedVolumeInfo::ConditionalConstruct(const FString& InVolumeName)
{
	if (!VolumeName.IsEmpty())
	{
		return;
	}
	VolumeName = InVolumeName;
	InitializePlatformData();
}

void FCachedVolumeInfo::PreSave()
{
	RemoveDirs(MoveTemp(DirsToRemove));
}

void FCachedVolumeInfo::InitializePlatformData()
{
	if (VolumeName.IsEmpty() || VolumeName == GEmptyVolumeName)
	{
		bJournalAvailable = false;
		JournalMaximumSize = 0;
		JournalIdOnDisk = FileJournalIdInvalid;
		NextJournalEntryOnDisk = FileJournalEntryHandleInvalid;
		JournalId = FileJournalIdInvalid;
		NextJournalEntryToScan = FileJournalEntryHandleInvalid;
	}
	else
	{
		EFileJournalResult Result = FPlatformFileManager::Get().GetPlatformFile().FileJournalGetLatestEntry(
			*VolumeName, JournalIdOnDisk, NextJournalEntryOnDisk, &LastError);
		bJournalAvailable = Result == EFileJournalResult::Success;
		JournalMaximumSize = bJournalAvailable ? FPlatformFileManager::Get().GetPlatformFile().FileJournalGetMaximumSize(*VolumeName) : 0;
		if (NextJournalEntryToScan == FileJournalEntryHandleInvalid)
		{
			JournalId = JournalIdOnDisk;
			NextJournalEntryToScan = NextJournalEntryOnDisk;
		}
	}
}

FCachedDirScanDir& FCachedVolumeInfo::FindOrAddDir(FStringView InPath)
{
	bool bAdded = false;
	return FindOrAddDir(InPath, bAdded);
}

FCachedDirScanDir& FCachedVolumeInfo::FindOrAddDir(FStringView InPath, bool& bOutAdded)
{
	uint32 PathHash = GetTypeHash(InPath);
	FCachedDirScanDir* ScanDir = Dirs.FindByHash(PathHash, InPath);
	bOutAdded = ScanDir == nullptr;
	if (bOutAdded)
	{
		ScanDir = &Dirs.AddByHash(PathHash, FString(InPath));
	}
	return *ScanDir;
}

void FCachedVolumeInfo::RemoveDirs(TArray<FString>&& InPaths)
{
	// Recursively remove RemoveDir directories; iterate by popping InPaths and pushing child dirs back on
	while (!InPaths.IsEmpty())
	{
		FString RemoveDir = InPaths.Pop(EAllowShrinking::No);
		FCachedDirScanDir DirData;
		if (Dirs.RemoveAndCopyValue(RemoveDir, DirData))
		{
			for (FString& RelPath : DirData.SubDirRelPaths)
			{
				InPaths.Add(FPaths::Combine(RemoveDir, RelPath));
			}
		}
	}
	InPaths.Empty(); // Free allocated memory
}

FCachedDirScanDir* FCachedVolumeInfo::FindDir(FStringView Path)
{
	uint32 PathHash = GetTypeHash(Path);
	return Dirs.FindByHash(PathHash, Path);
}

FArchive& operator<<(FArchive& Ar, FCachedVolumeInfo& Data)
{
	Ar << Data.Dirs;
	Ar << Data.VolumeName;
	Ar << Data.JournalId;
	Ar << Data.NextJournalEntryToScan;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FCachedDirScanDir& Data)
{
	Ar << Data.JournalHandle;
	Ar << Data.SubDirRelPaths;
	Ar << Data.Files;
	Ar << Data.bCacheValid;
	return Ar;
}

FArchive& operator<<(FArchive& Ar, FCachedDirScanFile& Data)
{
	Ar << Data.RelPath;
	Ar << Data.ModificationTime;
	return Ar;
}


} // namespace UE::AssetDataGather::Private
