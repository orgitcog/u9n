// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/InstallChunkSource.h"

#include "Algo/Transform.h"
#include "Async/Mutex.h"
#include "BuildPatchHash.h"
#include "Common/FileSystem.h"
#include "Common/StatsCollector.h"
#include "Core/BlockStructure.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryReader.h"
#include "Tasks/Task.h"

DEFINE_LOG_CATEGORY_STATIC(LogInstallChunkSource, Log, All);

namespace BuildPatchServices
{
	static FSHAHash GetShaHashForDataSet(const void* ChunkData, const uint32 ChunkSize)
	{
		FSHAHash ShaHashCheck;
		FSHA1::HashBuffer(ChunkData, ChunkSize, ShaHashCheck.Hash);
		return ShaHashCheck;
	}
	
	class FInstallChunkSource : public IConstructorInstallChunkSource
	{
	public:
		FInstallChunkSource(IFileSystem* FileSystem, IInstallChunkSourceStat* InInstallChunkSourceStat, const TMultiMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, 
			const TSet<FGuid>& ChunksThatWillBeNeeded);
		~FInstallChunkSource();
		
		virtual FRequestProcessFn CreateRequest(const FGuid& DataId, FMutableMemoryView DestinationBuffer, void* UserPtr, FChunkRequestCompleteDelegate CompleteFn) override;
		virtual const TSet<FGuid>& GetAvailableChunks() const override
		{
			return AvailableInBuilds;
		}
		virtual void OnBeforeDeleteFile(const FString& FilePath) override
		{
			// Make sure we close our handle before the deletion occurs.
			// With multiple files in flight in the constructor we can be deleting a file at the same time as we are
			// reading chunks for other files, which means we can hit OpenedFileHandles from multiple threads.
			UE::TUniqueLock _(FileHandleLock);
			OpenedFileHandles.Remove(FilePath);
		}
		virtual int32 GetChunkUnavailableAt(const FGuid& DataId) const override;
		virtual void SetFileRetirementPositions(TMap<FString, int32>&& InFileRetirementPositions) override
		{
			FileRetirementPositions = MoveTemp(InFileRetirementPositions);
		}

		virtual void GetChunksForFile(const FString& FilePath, TSet<FGuid>& OutChunks) const override;

		virtual void EnumerateFilesForChunk(const FGuid& DataId, TUniqueFunction<void(const FString& NormalizedInstallDirectory, const FString& NormalizedFileName)>&& Callback) const override
		{
			const FString* FoundInstallDirectory;
			const FBuildPatchAppManifest* FoundInstallManifest;
			FindChunkLocation(DataId, &FoundInstallDirectory, &FoundInstallManifest);

			const TArray<FChunkSourceDetails>* ChunkSource = ChunkSources.Find(DataId);

			if (FoundInstallDirectory == nullptr || FoundInstallManifest == nullptr || ChunkSource == nullptr)
			{
				return;
			}

			// The installation directory starts off normalized by then appends a directory which
			// might be empty, leaving a trailing slash. Rather than chase down all possibilities we
			// just re normalize.
			FString NormalizedInstallDirectory = *FoundInstallDirectory;
			FPaths::NormalizeDirectoryName(NormalizedInstallDirectory);

			for (const FChunkSourceDetails& ChunkDetails : *ChunkSource)
			{
				// afaict the file manifest filename is normalized because in the manifest builder it
				// generates it from file spans, which are created in directorybuildstreamer which makes them
				// relative, and internal to that function they are normalized.
				Callback(NormalizedInstallDirectory, ChunkDetails.FileManifest->Filename);
			}
		}

	private:
		void FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const;

	private:
	
		IFileSystem* FileSystem;
		IInstallChunkSourceStat* InstallChunkSourceStat;

		// Storage of enumerated chunks.
		TSet<FGuid> AvailableInBuilds;
		TArray<TPair<FString, FBuildPatchAppManifestRef>> InstallationSources;

		UE::FMutex FileHandleLock; // Protects OpenedFileHandles
		TMap<FString, TUniquePtr<FArchive>> OpenedFileHandles;

		// The index (ChunkReferenceTracker->GetCurrentUsageIndex) at which our files will get deleted due to destructive install to make room for
		// the new file. 
		TMap<FString, int32> FileRetirementPositions;

		struct FChunkSourceDetails
		{
			const FFileManifest* FileManifest;
			const FChunkPart* ChunkPart;
			uint64 FileOffset;
		};

		TMap<FGuid, TArray<FChunkSourceDetails>> ChunkSources;
	};

	FInstallChunkSource::FInstallChunkSource(IFileSystem* InFileSystem, IInstallChunkSourceStat* InInstallChunkSourceStat, 
		const TMultiMap<FString, FBuildPatchAppManifestRef>& InInstallationSources, const TSet<FGuid>& ChunksThatWillBeNeeded)
		: FileSystem(InFileSystem)
		, InstallChunkSourceStat(InInstallChunkSourceStat)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(InstallChunkSource_ctor);

		// Cache faster lookup information.		
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InInstallationSources)
		{
			if (Pair.Value->EnumerateProducibleChunks(Pair.Key, ChunksThatWillBeNeeded, AvailableInBuilds) > 0)
			{
				InstallationSources.Add(Pair);
			}
		}
		UE_LOG(LogInstallChunkSource, Log, TEXT("Useful Sources:%d. Available Chunks:%d."), InstallationSources.Num(), AvailableInBuilds.Num());

		// Cache what file we get everything from so we aren't linearly looking over this every request
		for (const TPair<FString, FBuildPatchAppManifestRef>& InstallationSource : InstallationSources)
		{
			FBuildPatchAppManifestRef Manifest = InstallationSource.Value;

			TSet<FString> FilesInManifest;
			Manifest->GetFileList(FilesInManifest);

			for (const FString& FileName : FilesInManifest)
			{
				const FFileManifest* FileManifest = Manifest->GetFileManifest(FileName);

				uint64 FileOffset = 0;
				for (const FChunkPart& ChunkPart : FileManifest->ChunkParts)
				{
					if (AvailableInBuilds.Contains(ChunkPart.Guid))
					{
						TArray<FChunkSourceDetails>& ChunkSource = ChunkSources.FindOrAdd(ChunkPart.Guid);
						ChunkSource.Add({FileManifest, &ChunkPart, FileOffset});
					}

					FileOffset += ChunkPart.Size;
				}
			}
		}
	}

	FInstallChunkSource::~FInstallChunkSource()
	{
		
	}

	void FInstallChunkSource::GetChunksForFile(const FString& FilePath, TSet<FGuid>& OutChunks) const
	{
		const FFileManifest* FileManifest = nullptr;
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InstallationSources)
		{
			if (FilePath.StartsWith(Pair.Key))
			{
				FString BuildRelativeFilePath = FilePath;
				FPaths::MakePathRelativeTo(BuildRelativeFilePath, *(Pair.Key / TEXT("")));
				FileManifest = Pair.Value->GetFileManifest(BuildRelativeFilePath);
				break;
			}
		}
		if (FileManifest != nullptr)
		{
			Algo::Transform(FileManifest->ChunkParts, OutChunks, &FChunkPart::Guid);
		}
	}

	IConstructorChunkSource::FRequestProcessFn FInstallChunkSource::CreateRequest(const FGuid& DataId, FMutableMemoryView DestinationBuffer, void* UserPtr, FChunkRequestCompleteDelegate CompleteFn)
	{
		const FString* FoundInstallDirectory;
		const FBuildPatchAppManifest* FoundInstallManifest;
		FindChunkLocation(DataId, &FoundInstallDirectory, &FoundInstallManifest);
		if (FoundInstallDirectory == nullptr || FoundInstallManifest == nullptr)
		{
			CompleteFn.Execute(DataId, false, true, UserPtr);
			return [](bool) {return;};
		}

		// Must be run sequentially! No threaded protection provided.
		return [this, FoundInstallManifest, FoundInstallDirectory = *FoundInstallDirectory, DataId, DestinationBuffer, UserPtr, CompleteFn](bool bIsAborted)
		{
			ISpeedRecorder::FRecord ActivityRecord;
			ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
			InstallChunkSourceStat->OnLoadStarted(DataId);

			TRACE_CPUPROFILER_EVENT_SCOPE(InstallRead);
			if (bIsAborted)
			{
				ActivityRecord.CyclesEnd = ActivityRecord.CyclesStart;
				InstallChunkSourceStat->OnLoadComplete(DataId, IInstallChunkSourceStat::ELoadResult::Aborted, ActivityRecord);

				CompleteFn.Execute(DataId, true, false, UserPtr);
				return;
			}

			const TArray<FChunkSourceDetails>* ChunkSource = ChunkSources.Find(DataId);
			const FChunkInfo* ChunkInfoPtr = FoundInstallManifest->GetChunkInfo(DataId);
			if (!ChunkInfoPtr || !ChunkSource)
			{
				ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
				InstallChunkSourceStat->OnLoadComplete(DataId, IInstallChunkSourceStat::ELoadResult::MissingPartInfo, ActivityRecord);

				CompleteFn.Execute(DataId, false, true, UserPtr);
				return;
			}

			FBlockStructure ChunkBlocks;

			IInstallChunkSourceStat::ELoadResult Result = IInstallChunkSourceStat::ELoadResult::Success;
	
			bool bLoadedWholeChunk = false;
			for (int32 FileChunkPartsIdx = 0; FileChunkPartsIdx < ChunkSource->Num(); ++FileChunkPartsIdx)
			{
				if (bLoadedWholeChunk)
				{
					// The manifest gave us more chunk parts than we needed to generate the full chunk. This shouldn't happen,
					// and so conceptually is an error, but since we have all the data we can technically proceed.
					// This seems to happen with some regularity - need to understand why.
					//UE_LOG(LogInstallChunkSource, Display, TEXT("Chunk %s had more chunk sources than necessary to re-assemble"), *WriteToString<40>(DataId));
					break;
				}
				const FChunkSourceDetails& FileChunkPart = ChunkSource->operator[](FileChunkPartsIdx);

				// Validate the chunk can load into the destination.
				uint32 ChunkEndLocation = FileChunkPart.ChunkPart->Offset + FileChunkPart.ChunkPart->Size;
				if (ChunkEndLocation > DestinationBuffer.GetSize())
				{
					// The chunk metadata tried to assemble larger than the chunk itself - error
					UE_LOG(LogInstallChunkSource, Error, TEXT("Chunk %s assembled larger than the actual chunk size (chunk wanted end %u vs buffer size %llu"), *WriteToString<40>(DataId), ChunkEndLocation, DestinationBuffer.GetSize());
					Result = IInstallChunkSourceStat::ELoadResult::InvalidChunkParts;
					break;
				}

				FString FullFilename = FoundInstallDirectory / FileChunkPart.FileManifest->Filename;

				// We use the internal FArchive pointer so that we don't have to hold the file handle
				// lock over the read - the uniqueptr might move around, but the managed pointer will not.
				// We do know that our specific file won't get deleted until we are done.
				FArchive* FileArchive = nullptr;
				{
					FileHandleLock.Lock();
					TUniquePtr<FArchive>* FileArchivePtr = OpenedFileHandles.Find(FullFilename);
					if (FileArchivePtr == nullptr)
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(Install_OpenSource);

						// Don't lock over the file open since that could take a while.
						FileHandleLock.Unlock();
						TUniquePtr<FArchive> NewReader = FileSystem->CreateFileReader(*FullFilename);
						FileHandleLock.Lock();

						if (NewReader.IsValid())
						{
							OpenedFileHandles.Add(FullFilename, MoveTemp(NewReader));
							FileArchivePtr = OpenedFileHandles.Find(FullFilename);
						}
					}
					if (FileArchivePtr)
					{
						FileArchive = FileArchivePtr->Get();
					}
					FileHandleLock.Unlock();
				}

				if (FileArchive == nullptr)
				{
					Result = IInstallChunkSourceStat::ELoadResult::OpenFileFail;
					break;
				}

				{
					TRACE_CPUPROFILER_EVENT_SCOPE(Install_Serialize);
					FileArchive->Seek(FileChunkPart.FileOffset);
					FileArchive->Serialize((uint8*)DestinationBuffer.GetData() + FileChunkPart.ChunkPart->Offset, FileChunkPart.ChunkPart->Size);

					ActivityRecord.Size += FileChunkPart.ChunkPart->Size;

					FBlockStructure NewChunk;
					NewChunk.Add(FileChunkPart.ChunkPart->Offset, FileChunkPart.ChunkPart->Size);
					if (NewChunk.Intersect(ChunkBlocks).GetHead() != nullptr)
					{
						// This used to be allowed but in advance of multi threaded reading we want to make sure this
						// doesn't happen anymore (already shouldn't be...)
						UE_LOG(LogInstallChunkSource, Error, TEXT("Chunk %s had overlapping chunk parts"), *WriteToString<40>(DataId));
						Result = IInstallChunkSourceStat::ELoadResult::InvalidChunkParts;
						break;
					}
					ChunkBlocks.Add(FileChunkPart.ChunkPart->Offset, FileChunkPart.ChunkPart->Size);
				}

				// The expectation is that we get the full chunk only once we've assembled all of the parts
				// provided by the manifest, so the last iteration should set this to true. If it isn't the last
				// iteration, then we'll hit the faux-error case at the top of the loop.
				bLoadedWholeChunk = ChunkBlocks.GetHead() && ChunkBlocks.GetHead() == ChunkBlocks.GetTail() && ChunkBlocks.GetHead()->GetSize() == ChunkInfoPtr->WindowSize;
			}
			
			if (!bLoadedWholeChunk)
			{
				if (Result == IInstallChunkSourceStat::ELoadResult::Success)
				{
					// If we failed without hitting a different case, then we just didn't have enough parts.
					Result = IInstallChunkSourceStat::ELoadResult::InvalidChunkParts;
				}

				ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
				InstallChunkSourceStat->OnLoadComplete(DataId, IInstallChunkSourceStat::ELoadResult::MissingPartInfo, ActivityRecord);

				CompleteFn.Execute(DataId, false, !bLoadedWholeChunk, UserPtr);
				return;
			}

			// We set this here because it is used to compute the IO speeds, however we can't call OnLoadComplete because we don't know
			// the hash result yet.
			ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();

			// Check chunk hash. 
			UE::Tasks::Launch(TEXT("Install_Hash"), [DataId, UserPtr, DestinationBuffer, FoundInstallManifest, CompleteFn, ActivityRecord, InstallChunkSourceStat=InstallChunkSourceStat]()
				{
					IInstallChunkSourceStat::ELoadResult Result = IInstallChunkSourceStat::ELoadResult::Success;
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(InstallHash);
						FSHAHash ChunkShaHash;
						uint64 ChunkRollingHash = 0;

						if (FoundInstallManifest->GetChunkShaHash(DataId, ChunkShaHash))
						{
							if (GetShaHashForDataSet(DestinationBuffer.GetData(), DestinationBuffer.GetSize()) != ChunkShaHash)
							{
								Result = IInstallChunkSourceStat::ELoadResult::HashCheckFailed;
							}
						}
						else if (FoundInstallManifest->GetChunkHash(DataId, ChunkRollingHash))
						{
							if (FRollingHash::GetHashForDataSet((const uint8*)DestinationBuffer.GetData(), DestinationBuffer.GetSize()) != ChunkRollingHash)
							{
								Result = IInstallChunkSourceStat::ELoadResult::HashCheckFailed;
							}
						}
						else
						{
							Result = IInstallChunkSourceStat::ELoadResult::MissingHashInfo;
						}
					}

					InstallChunkSourceStat->OnLoadComplete(
						DataId, 
						Result, 
						ActivityRecord);

					CompleteFn.Execute(DataId, false, Result != IInstallChunkSourceStat::ELoadResult::Success, UserPtr);
				}
			);
		};
	}
	
	int32 FInstallChunkSource::GetChunkUnavailableAt(const FGuid& DataId) const
	{
		if (FileRetirementPositions.Num() == 0)
		{
			// If we aren't destructive then it's always available.
			return TNumericLimits<int32>::Max();
		}

		const FString* FoundInstallDirectory;
		const FBuildPatchAppManifest* FoundInstallManifest;
		FindChunkLocation(DataId, &FoundInstallDirectory, &FoundInstallManifest);
		if (FoundInstallDirectory == nullptr || FoundInstallManifest == nullptr)
		{
			return TNumericLimits<int32>::Max();
		}

		// This chunk is no longer available as soon as the first file containing a part is complete (if destructive install)
		int32 ChunkUnavailableAt = TNumericLimits<int32>::Max();

		const TArray<FChunkSourceDetails>* ChunkSource = ChunkSources.Find(DataId);
		if (ChunkSource)
		{
			for (const FChunkSourceDetails& Part : *ChunkSource)
			{
				const int32* FirstIndexAfterFile = FileRetirementPositions.Find(Part.FileManifest->Filename);
				if (FirstIndexAfterFile &&
					*FirstIndexAfterFile < ChunkUnavailableAt)
				{
					ChunkUnavailableAt = *FirstIndexAfterFile;
				}
			}
		}

		return ChunkUnavailableAt;
	}
	
	void FInstallChunkSource::FindChunkLocation(const FGuid& DataId, const FString** FoundInstallDirectory, const FBuildPatchAppManifest** FoundInstallManifest) const
	{
		uint64 ChunkHash;
		*FoundInstallDirectory = nullptr;
		*FoundInstallManifest = nullptr;
		for (const TPair<FString, FBuildPatchAppManifestRef>& Pair : InstallationSources)
		{
			// GetChunkHash can be used as a check for whether this manifest references this chunk.
			if (Pair.Value->GetChunkHash(DataId, ChunkHash))
			{
				*FoundInstallDirectory = &Pair.Key;
				*FoundInstallManifest = &Pair.Value.Get();
				return;
			}
		}
	}

	IConstructorInstallChunkSource* IConstructorInstallChunkSource::CreateInstallSource(IFileSystem* FileSystem, IInstallChunkSourceStat* InstallChunkSourceStat, 
		const TMultiMap<FString, FBuildPatchAppManifestRef>& InstallationSources, const TSet<FGuid>& ChunksThatWillBeNeeded)
	{
		return new FInstallChunkSource(FileSystem, InstallChunkSourceStat, InstallationSources, ChunksThatWillBeNeeded);
	}

	const TCHAR* ToString(const IInstallChunkSourceStat::ELoadResult& LoadResult)
	{
		switch(LoadResult)
		{
			case IInstallChunkSourceStat::ELoadResult::Success:
				return TEXT("Success");
			case IInstallChunkSourceStat::ELoadResult::MissingHashInfo:
				return TEXT("MissingHashInfo");
			case IInstallChunkSourceStat::ELoadResult::MissingPartInfo:
				return TEXT("MissingPartInfo");
			case IInstallChunkSourceStat::ELoadResult::OpenFileFail:
				return TEXT("OpenFileFail");
			case IInstallChunkSourceStat::ELoadResult::HashCheckFailed:
				return TEXT("HashCheckFailed");
			case IInstallChunkSourceStat::ELoadResult::Aborted:
				return TEXT("Aborted");
			case IInstallChunkSourceStat::ELoadResult::InvalidChunkParts:
				return TEXT("InvalidChunkParts");
			default:
				return TEXT("Unknown");
		}
	}
}
