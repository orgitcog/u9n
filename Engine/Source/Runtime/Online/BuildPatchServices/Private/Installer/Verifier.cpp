// Copyright Epic Games, Inc. All Rights Reserved.

#include "Installer/Verifier.h"
#include "HAL/ThreadSafeBool.h"
#include "Common/StatsCollector.h"
#include "Common/FileSystem.h"
#include "BuildPatchVerify.h"
#include "BuildPatchUtil.h"
#include "HAL/UESemaphore.h"
#include "IBuildManifestSet.h"
#include "Misc/ConfigCacheIni.h"

DEFINE_LOG_CATEGORY_STATIC(LogVerifier, Log, All);

namespace BuildPatchServices
{
	bool TryConvertToVerifyResult(EVerifyError InVerifyError, EVerifyResult& OutVerifyResult)
	{
		switch (InVerifyError)
		{
			case EVerifyError::FileMissing: OutVerifyResult = EVerifyResult::FileMissing; return true;
			case EVerifyError::OpenFileFailed: OutVerifyResult = EVerifyResult::OpenFileFailed; return true;
			case EVerifyError::HashCheckFailed: OutVerifyResult = EVerifyResult::HashCheckFailed; return true;
			case EVerifyError::FileSizeFailed: OutVerifyResult = EVerifyResult::FileSizeFailed; return true;
		}
		return false;
	}

	bool TryConvertToVerifyError(EVerifyResult InVerifyResult, EVerifyError& OutVerifyError)
	{
		switch (InVerifyResult)
		{
			case EVerifyResult::FileMissing: OutVerifyError = EVerifyError::FileMissing; return true;
			case EVerifyResult::OpenFileFailed: OutVerifyError = EVerifyError::OpenFileFailed; return true;
			case EVerifyResult::HashCheckFailed: OutVerifyError = EVerifyError::HashCheckFailed; return true;
			case EVerifyResult::FileSizeFailed: OutVerifyError = EVerifyError::FileSizeFailed; return true;
		}
		return false;
	}

	class FVerifier : public IVerifier
	{
	public:
		FVerifier(IFileSystem* FileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, IBuildInstallerSharedContextPtr InSharedContext, IBuildManifestSet* InManifestSet, FString InVerifyDirectory, FString InStagedFileDirectory);
		~FVerifier() {}

		// IControllable interface begin.
		virtual void SetPaused(bool bInIsPaused) override;
		virtual void Abort() override;
		// IControllable interface end.

		// IVerifier interface begin.
		virtual EVerifyResult Verify(TArray<FString>& CorruptFiles) override;
		virtual void AddTouchedFiles(const TSet<FString>& TouchedFiles) override;
		// IVerifier interface end.

	private:
		FString SelectFullFilePath(const FString& BuildFile);
		EVerifyResult VerfiyFileSha(TArray<uint8>& ReadBuffer, const FString& BuildFile, const FFileManifest& BuildFileManifest);
		EVerifyResult VerfiyFileSize(const FString& BuildFile, const FFileManifest& BuildFileManifest);

	private:
		const FString VerifyDirectory;
		const FString StagedFileDirectory;
		IFileSystem* const FileSystem;
		IVerifierStat* const VerifierStat;
		IBuildManifestSet* const ManifestSet;
		IBuildInstallerSharedContextPtr SharedContext;

		EVerifyMode VerifyMode;
		TSet<FString> FilesToVerify;
		TSet<FString> FilesPassedVerify;
		FThreadSafeBool bIsPaused;
		FThreadSafeBool bShouldAbort;

		struct FThreadVerifyJob
		{
			const FFileManifest* BuildFileManifest;
			bool bVerifySha;
			FString FileName;
		};

		FEvent* AllThreadsDoneEvent;
		std::atomic_int64_t ThreadProcessedBytes;
		TArray<EVerifyResult> ThreadJobResults;

		UE::FMutex ThreadJobListLock;
		int32 NextThreadJobListIndex = 0;		
		TArray<FThreadVerifyJob> ThreadJobList;

		TArray<IBuildInstallerThread*> SecondaryProcessThreads;

		void ProcessVerifyJobs(FSemaphore* ThreadDoneSem);
	};

	FVerifier::FVerifier(IFileSystem* InFileSystem, IVerifierStat* InVerificationStat, EVerifyMode InVerifyMode, IBuildInstallerSharedContextPtr InSharedContext, IBuildManifestSet* InManifestSet, FString InVerifyDirectory, FString InStagedFileDirectory)
		: VerifyDirectory(MoveTemp(InVerifyDirectory))
		, StagedFileDirectory(MoveTemp(InStagedFileDirectory))
		, FileSystem(InFileSystem)
		, VerifierStat(InVerificationStat)
		, ManifestSet(InManifestSet)
		, SharedContext(InSharedContext)
		, VerifyMode(InVerifyMode)
		, bIsPaused(false)
		, bShouldAbort(false)
	{
		ManifestSet->GetFilesTaggedForRepair(FilesToVerify);

	}

	void FVerifier::ProcessVerifyJobs(FSemaphore* ThreadDoneSem)
	{
		TArray<uint8> FileReadBuffer;

		for (;;)
		{
			if (bShouldAbort)
			{
				break;
			}

			// Get a thread index.
			int32 OurJobIndex = -1;
			{
				UE::TUniqueLock _(ThreadJobListLock);
				if (NextThreadJobListIndex < ThreadJobList.Num())
				{
					OurJobIndex = NextThreadJobListIndex;
					NextThreadJobListIndex++;
				}
			}

			if (OurJobIndex == -1)
			{
				// No work to do.
				break;
			}

			// Only allocate our buffer when we get a job
			FileReadBuffer.SetNumUninitialized(4 << 20); // 4MB chunked reads.

			FThreadVerifyJob& OurJob = ThreadJobList[OurJobIndex];

			VerifierStat->OnFileStarted(OurJob.FileName, OurJob.BuildFileManifest->FileSize);

			EVerifyResult FileVerifyResult = OurJob.bVerifySha ? VerfiyFileSha(FileReadBuffer, OurJob.FileName, *OurJob.BuildFileManifest) : VerfiyFileSize(OurJob.FileName, *OurJob.BuildFileManifest);

			VerifierStat->OnFileCompleted(OurJob.FileName, FileVerifyResult);

			ThreadJobResults[OurJobIndex] = FileVerifyResult;
		}

		if (ThreadDoneSem)
		{
			ThreadDoneSem->Release(1);
		}
		
		// We can't touch _anything_ after here because the outer class holding the lambda could
		// be destroyed.
	}

	void FVerifier::SetPaused(bool bInIsPaused)
	{
		bIsPaused = bInIsPaused;
	}

	void FVerifier::Abort()
	{
		bShouldAbort = true;
	}

	EVerifyResult FVerifier::Verify(TArray<FString>& CorruptFiles)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(Verify);
		bShouldAbort = false;
		EVerifyResult VerifyResult = EVerifyResult::Success;
		CorruptFiles.Empty();

		// If we check all files, grab them all now.
		if (VerifyMode == EVerifyMode::FileSizeCheckAllFiles || VerifyMode == EVerifyMode::ShaVerifyAllFiles)
		{
			ManifestSet->GetExpectedFiles(FilesToVerify);
		}

		// Setup progress tracking.
		TSet<FString> VerifyList = FilesToVerify.Difference(FilesPassedVerify);
		VerifierStat->OnProcessedDataUpdated(0);
		VerifierStat->OnTotalRequiredUpdated(ManifestSet->GetTotalNewFileSize(VerifyList));

		// Select verify function.
		const bool bVerifyShaMode = VerifyMode == EVerifyMode::ShaVerifyAllFiles || VerifyMode == EVerifyMode::ShaVerifyTouchedFiles;


		ThreadJobList.Reset();
		ThreadJobResults.Reset();
		ThreadProcessedBytes = 0;
		NextThreadJobListIndex = 0;

		for (const FString& BuildFile : VerifyList)
		{
			FThreadVerifyJob Job;
			Job.FileName = BuildFile;
			Job.BuildFileManifest = ManifestSet->GetNewFileManifest(BuildFile);
			Job.bVerifySha = bVerifyShaMode || ManifestSet->IsFileRepairAction(BuildFile);

			ThreadJobList.Add(MoveTemp(Job));
			ThreadJobResults.Add(EVerifyResult::Aborted);
		}

		AllThreadsDoneEvent = FPlatformProcess::GetSynchEventFromPool();

		// Events get reset when they are returned to the pool but if there are any outstanding
		// triggers due to races in the previous user we would have a triggered event, so we Reset here. 
		// This doesn't really remove the race but it prevents it from sitting latent for long periods of time.
		AllThreadsDoneEvent->Reset();

		if (SharedContext)
		{
			// 3 cores should saturate any modern drive, and on anything that isn't we just sit in the wait
			// state anyway. 
			int32 ThreadsToCreate = 1;
			GConfig->GetInt(TEXT("BuildPatchTool"), TEXT("VerificationThreadCount"), ThreadsToCreate, GEngineIni);

			int32 ThreadsRemaining = ThreadsToCreate;

			FSemaphore ThreadDoneSem(0, ThreadsToCreate);
			
			// We wait to create our threads so that the shared threads from file construction are returned.
			for (int i = 0; i < ThreadsToCreate; i++)
			{
				SecondaryProcessThreads.Add(SharedContext->CreateThread());
				SecondaryProcessThreads[i]->RunTask([this, &ThreadDoneSem]() { ProcessVerifyJobs(&ThreadDoneSem); });
			}

			for (;;)
			{
				if (ThreadDoneSem.TryAcquire(100))
				{
					ThreadsRemaining--;
					if (ThreadsRemaining == 0)
					{
						// We acquired all the counts so we're done.
						break;
					}
				}

				// Update our overall progress tracker.
				VerifierStat->OnProcessedDataUpdated(ThreadProcessedBytes.load(std::memory_order_acquire));
			}

			// If our event is triggered then something has gone very wrong since we could not have triggered it.
			// This is still here to try and figure out what's been going wrong with it before we switched to a semaphore.
			bool bEventIsTriggered = AllThreadsDoneEvent->Wait(0);
			if (!ensure(!bEventIsTriggered))
			{
				UE_LOG(LogVerifier, Error, TEXT("Verifier has a triggered event!"));
			}

			for (int i = 0; i < ThreadsToCreate; i++)
			{
				SharedContext->ReleaseThread(SecondaryProcessThreads[i]);
			}
			SecondaryProcessThreads.Empty();
		}
		else
		{
			// Can't create threads, just do the work here.
			ProcessVerifyJobs(nullptr);
		}

		FPlatformProcess::ReturnSynchEventToPool(AllThreadsDoneEvent);
		AllThreadsDoneEvent = nullptr;

		// move results over.
		for (int32 i=0; i<ThreadJobList.Num(); i++)
		{
			EVerifyResult FileVerifyResult = ThreadJobResults[i];

			if (FileVerifyResult != EVerifyResult::Success)
			{
				CorruptFiles.Add(ThreadJobList[i].FileName);
				UE_LOG(LogVerifier, Warning, TEXT("File verification failed on: %s (cause = %d)"), *ThreadJobList[i].FileName, FileVerifyResult);
				if (VerifyResult == EVerifyResult::Success)
				{
					VerifyResult = FileVerifyResult;
				}
			}
			// If success, and it was an SHA verify, cache the result so we don't repeat an SHA verify.
			else if (ThreadJobList[i].bVerifySha)
			{
				FilesPassedVerify.Add(ThreadJobList[i].FileName);
			}
		}

		return VerifyResult;
	}

	void FVerifier::AddTouchedFiles(const TSet<FString>& TouchedFiles)
	{
		FilesToVerify.Append(TouchedFiles);
		FilesPassedVerify = FilesPassedVerify.Difference(TouchedFiles);
	}

	FString FVerifier::SelectFullFilePath(const FString& BuildFile)
	{
		FString FullFilePath;
		if (StagedFileDirectory.IsEmpty() == false)
		{
			FullFilePath = StagedFileDirectory / BuildFile;
			int64 FileSize;
			if (FileSystem->GetFileSize(*FullFilePath, FileSize))
			{
				return FullFilePath;
			}
		}
		FullFilePath = VerifyDirectory / BuildFile;
		return FullFilePath;
	}

	EVerifyResult FVerifier::VerfiyFileSha(TArray<uint8>& ReadBuffer, const FString& BuildFile, const FFileManifest& BuildFileManifest)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(VerifyFileSha);
		ISpeedRecorder::FRecord ActivityRecord;
		EVerifyResult VerifyResult;
		const FString FileToVerify = SelectFullFilePath(BuildFile);
		uint8 ReturnValue = 0;
		TUniquePtr<FArchive> FileReader = FileSystem->CreateFileReader(*FileToVerify);
		VerifierStat->OnFileProgress(BuildFile, 0);
		if (FileReader.IsValid())
		{
			const int64 FileSize = FileReader->TotalSize();
			if (FileSize != BuildFileManifest.FileSize)
			{
				VerifyResult = EVerifyResult::FileSizeFailed;
			}
			else
			{
				FSHA1 HashState;
				FSHAHash HashValue;
				while (!FileReader->AtEnd() && !bShouldAbort)
				{
					// Pause if necessary
					while (bIsPaused && !bShouldAbort)
					{
						FPlatformProcess::Sleep(0.1f);
					}
					ActivityRecord.CyclesStart = FStatsCollector::GetCycles();
					// Read file and update hash state
					const int64 SizeLeft = FileSize - FileReader->Tell();
					ActivityRecord.Size = FMath::Min<int64>(ReadBuffer.Num(), SizeLeft);

					{
						TRACE_CPUPROFILER_EVENT_SCOPE(VerifyFileSha_Serialize);
						FileReader->Serialize(ReadBuffer.GetData(), ActivityRecord.Size);
					}
					{
						TRACE_CPUPROFILER_EVENT_SCOPE(VerifyFileSha_Hash);
						
						HashState.Update(ReadBuffer.GetData(), ActivityRecord.Size);
					}

					ActivityRecord.CyclesEnd = FStatsCollector::GetCycles();
					VerifierStat->OnFileRead(ActivityRecord);
					VerifierStat->OnFileProgress(BuildFile, FileReader->Tell());

					ThreadProcessedBytes.fetch_add(ActivityRecord.Size, std::memory_order_release);
				}
				HashState.Final();
				HashState.GetHash(HashValue.Hash);
				if (HashValue == BuildFileManifest.FileHash)
				{
					VerifyResult = EVerifyResult::Success;
				}
				else if (!bShouldAbort)
				{
					VerifyResult = EVerifyResult::HashCheckFailed;
				}
				else
				{
					VerifyResult = EVerifyResult::Aborted;
				}
			}
			FileReader->Close();
		}
		else if (FileSystem->FileExists(*FileToVerify))
		{
			VerifyResult = EVerifyResult::OpenFileFailed;
		}
		else
		{
			VerifyResult = EVerifyResult::FileMissing;
		}
		if (VerifyResult != EVerifyResult::Success)
		{
			VerifierStat->OnFileProgress(BuildFile, BuildFileManifest.FileSize);
		}

		return VerifyResult;
	}

	EVerifyResult FVerifier::VerfiyFileSize(const FString& BuildFile, const FFileManifest& BuildFileManifest)
	{
		// Pause if necessary.
		while (bIsPaused && !bShouldAbort)
		{
			FPlatformProcess::Sleep(0.1f);
		}

		VerifierStat->OnFileProgress(BuildFile, 0);
		int64 FileSize;
		EVerifyResult VerifyResult;
		if (FileSystem->GetFileSize(*SelectFullFilePath(BuildFile), FileSize))
		{
			if (FileSize == BuildFileManifest.FileSize)
			{
				VerifyResult = EVerifyResult::Success;
			}
			else
			{
				VerifyResult = EVerifyResult::FileSizeFailed;
			}
		}
		else
		{
			VerifyResult = EVerifyResult::FileMissing;
		}
		VerifierStat->OnFileProgress(BuildFile, BuildFileManifest.FileSize);

		ThreadProcessedBytes.fetch_add(BuildFileManifest.FileSize, std::memory_order_release);
		return VerifyResult;
	}

	IVerifier* FVerifierFactory::Create(IFileSystem* FileSystem, IVerifierStat* VerifierStat, EVerifyMode VerifyMode, IBuildInstallerSharedContextPtr SharedContext, IBuildManifestSet* ManifestSet, FString VerifyDirectory, FString StagedFileDirectory)
	{
		check(FileSystem != nullptr);
		check(VerifierStat != nullptr);
		return new FVerifier(FileSystem, VerifierStat, VerifyMode, SharedContext, ManifestSet, MoveTemp(VerifyDirectory), MoveTemp(StagedFileDirectory));
	}
}
