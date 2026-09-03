// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/FilePackageWriterUtil.h"

#include "HAL/FileManager.h"
#include "Misc/PackagePath.h"
#include "Misc/Paths.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/SavePackage.h"

namespace FilePackageWriterUtil
{
	static void WriteToFile(const FString& Filename, const FCompositeBuffer& Buffer)
	{
		IFileManager& FileManager = IFileManager::Get();

		struct FFailureReason
		{
			uint32 LastErrorCode = 0;
			bool bSizeMatchFailed = false;
			int64 ExpectedSize = 0;
			int64 ActualSize = 0;
			bool bArchiveError = false;
		};
		TOptional<FFailureReason> FailureReason;

		for (int32 Tries = 0; Tries < 3; ++Tries)
		{
			FArchive* Ar = FileManager.CreateFileWriter(*Filename);
			if (!Ar)
			{
				if (!FailureReason)
				{
					FailureReason = FFailureReason{ FPlatformMisc::GetLastError(), false };
				}
				continue;
			}

			int64 DataSize = 0;
			for (const FSharedBuffer& Segment : Buffer.GetSegments())
			{
				int64 SegmentSize = static_cast<int64>(Segment.GetSize());
				Ar->Serialize(const_cast<void*>(Segment.GetData()), SegmentSize);
				DataSize += SegmentSize;
			}
			bool bArchiveError = Ar->IsError();
			delete Ar;

			int64 ActualSize = FileManager.FileSize(*Filename);
			if (ActualSize != DataSize)
			{
				if (!FailureReason)
				{
					FailureReason = FFailureReason{ 0, true, DataSize, ActualSize, bArchiveError };
				}
				FileManager.Delete(*Filename);
				continue;
			}
			return;
		}

		FString ReasonText;
		if (FailureReason && FailureReason->bSizeMatchFailed)
		{
			ReasonText = FString::Printf(TEXT("Unexpected file size. Tried to write %" INT64_FMT " but resultant size was %" INT64_FMT ".%s")
				TEXT(" Another operation is modifying the file, or the write operation failed to write completely."),
				FailureReason->ExpectedSize, FailureReason->ActualSize, FailureReason->bArchiveError ? TEXT(" Ar->Serialize failed.") : TEXT(""));
		}
		else if (FailureReason && FailureReason->LastErrorCode != 0)
		{
			TCHAR LastErrorText[1024];
			FPlatformMisc::GetSystemErrorMessage(LastErrorText, UE_ARRAY_COUNT(LastErrorText), FailureReason->LastErrorCode);
			ReasonText = LastErrorText;
		}
		else
		{
			ReasonText = TEXT("Unknown failure reason.");
		}
		UE_LOG(LogSavePackage, Fatal, TEXT("SavePackage Async write %s failed: %s"), *Filename, *ReasonText);
	}
}

FFilePackageWriterUtil::FWritePackageParameters::FWritePackageParameters(FRecord& InRecord,
	const IPackageWriter::FCommitPackageInfo& InInfo,
	TMap<FName, TRefCountPtr<FPackageHashes>>* InAllPackageHashes,
	FCriticalSection* InPackageHashesLock,
	bool bInProvidePerPackageResult)
	: Record(InRecord)
	, Info(InInfo)
	, AllPackageHashes(InAllPackageHashes)
	, PackageHashesLock(InPackageHashesLock)
	, bProvidePerPackageResult(bInProvidePerPackageResult)
{}

void FFilePackageWriterUtil::FWriteFileData::HashAndWrite(FMD5& AccumulatedHash, const TRefCountPtr<FPackageHashes>& PackageHashes, IPackageWriter::EWriteOptions WriteOptions) const
{
	//@todo: FH: Should we calculate the hash of both output, currently only the main package output hash is calculated
	if (EnumHasAnyFlags(WriteOptions, IPackageWriter::EWriteOptions::ComputeHash) && bContributeToHash)
	{
		for (const FSharedBuffer& Segment : Buffer.GetSegments())
		{
			AccumulatedHash.Update(static_cast<const uint8*>(Segment.GetData()), Segment.GetSize());
		}

		if (ChunkId.IsValid())
		{
			FBlake3 ChunkHash;
			for (const FSharedBuffer& Segment : Buffer.GetSegments())
			{
				ChunkHash.Update(static_cast<const uint8*>(Segment.GetData()), Segment.GetSize());
			}
			FIoHash FinalHash(ChunkHash.Finalize());
			PackageHashes->ChunkHashes.Add(ChunkId, FinalHash);
		}
	}

	if ((bIsSidecar && EnumHasAnyFlags(WriteOptions, IPackageWriter::EWriteOptions::WriteSidecars)) ||
		(!bIsSidecar && EnumHasAnyFlags(WriteOptions, IPackageWriter::EWriteOptions::WritePackage)))
	{
		const FString* WriteFilename = &Filename;
		FString FilenameBuffer;
		if (EnumHasAnyFlags(WriteOptions, IPackageWriter::EWriteOptions::SaveForDiff))
		{
			FilenameBuffer = FPaths::Combine(FPaths::GetPath(Filename),
				FPaths::GetBaseFilename(Filename) + TEXT("_ForDiff") + FPaths::GetExtension(Filename, true));
			WriteFilename = &FilenameBuffer;
		}
		FilePackageWriterUtil::WriteToFile(*WriteFilename, Buffer);

		if (Regions.Num() > 0)
		{
			TArray<uint8> Memory;
			FMemoryWriter Ar(Memory);
			FFileRegion::SerializeFileRegions(Ar, const_cast<TArray<FFileRegion>&>(Regions));

			FilePackageWriterUtil::WriteToFile(*WriteFilename + FFileRegion::RegionsFileExtension,
				FCompositeBuffer(FSharedBuffer::MakeView(Memory.GetData(), Memory.Num())));
		}
	}
}

void FFilePackageWriterUtil::WritePackage(FWritePackageParameters& Parameters)
{
	if (Parameters.Info.Status == IPackageWriter::ECommitStatus::Success)
	{
		AsyncSave(Parameters);
	}
}

void FFilePackageWriterUtil::AsyncSave(FWritePackageParameters& Parameters)
{
	FCommitContext Context{ Parameters.Info };

	// The order of these collection calls is important, both for ExportsBuffers (affects the meaning of offsets
	// to those buffers) and for OutputFiles (affects the calculation of the Hash for the set of PackageData)
	// The order of ExportsBuffers must match CompleteExportsArchiveForDiff.
	CollectForSavePackageData(Parameters.Record, Context);
	CollectForSaveBulkData(Parameters.Record, Context);
	CollectForSaveLinkerAdditionalDataRecords(Parameters.Record, Context);
	CollectForSaveAdditionalFileRecords(Parameters.Record, Context);
	CollectForSaveExportsFooter(Parameters.Record, Context);
	CollectForSaveExportsPackageTrailer(Parameters.Record, Context);
	CollectForSaveExportsBuffers(Parameters.Record, Context);

	AsyncSaveOutputFiles(Context, Parameters.AllPackageHashes, Parameters.PackageHashesLock, Parameters.bProvidePerPackageResult);
}

void FFilePackageWriterUtil::CollectForSavePackageData(FRecord& Record, FCommitContext& Context)
{
	Context.ExportsBuffers.AddDefaulted(Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Package.Buffer, MoveTemp(Package.Regions) });
	}
}

void FFilePackageWriterUtil::CollectForSaveBulkData(FRecord& Record, FCommitContext& Context)
{
	for (FPackageWriterRecords::FBulkData& BulkRecord : Record.BulkDatas)
	{
		if (BulkRecord.Info.BulkDataType == IPackageWriter::FBulkDataInfo::AppendToExports)
		{
			if (Record.bCompletedExportsArchiveForDiff)
			{
				// Already Added in CompleteExportsArchiveForDiff
				continue;
			}
			Context.ExportsBuffers[BulkRecord.Info.MultiOutputIndex].Add(FExportBuffer{ BulkRecord.Buffer, MoveTemp(BulkRecord.Regions) });
		}
		else
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = BulkRecord.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(BulkRecord.Buffer);
			OutputFile.Regions = MoveTemp(BulkRecord.Regions);
			OutputFile.bIsSidecar = true;
			OutputFile.bContributeToHash = BulkRecord.Info.MultiOutputIndex == 0; // Only caculate the main package output hash
			OutputFile.ChunkId = BulkRecord.Info.ChunkId;
		}
	}
}

void FFilePackageWriterUtil::CollectForSaveLinkerAdditionalDataRecords(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FPackageWriterRecords::FLinkerAdditionalData& AdditionalRecord : Record.LinkerAdditionalDatas)
	{
		Context.ExportsBuffers[AdditionalRecord.Info.MultiOutputIndex].Add(FExportBuffer{ AdditionalRecord.Buffer, MoveTemp(AdditionalRecord.Regions) });
	}
}

void FFilePackageWriterUtil::CollectForSaveAdditionalFileRecords(FRecord& Record, FCommitContext& Context)
{
	for (FPackageWriterRecords::FAdditionalFile& AdditionalRecord : Record.AdditionalFiles)
	{
		FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
		OutputFile.Filename = AdditionalRecord.Info.Filename;
		OutputFile.Buffer = FCompositeBuffer(AdditionalRecord.Buffer);
		OutputFile.bIsSidecar = true;
		OutputFile.bContributeToHash = AdditionalRecord.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
		OutputFile.ChunkId = AdditionalRecord.Info.ChunkId;
	}
}

void FFilePackageWriterUtil::CollectForSaveExportsFooter(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	uint32 FooterData = PACKAGE_FILE_TAG;
	FSharedBuffer Buffer = FSharedBuffer::Clone(&FooterData, sizeof(FooterData));
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		Context.ExportsBuffers[Package.Info.MultiOutputIndex].Add(FExportBuffer{ Buffer, TArray<FFileRegion>() });
	}
}

void FFilePackageWriterUtil::CollectForSaveExportsPackageTrailer(FRecord& Record, FCommitContext& Context)
{
	if (Record.bCompletedExportsArchiveForDiff)
	{
		// Already Added in CompleteExportsArchiveForDiff
		return;
	}

	for (FPackageWriterRecords::FPackageTrailer& PackageTrailer : Record.PackageTrailers)
	{
		Context.ExportsBuffers[PackageTrailer.Info.MultiOutputIndex].Add(
			FExportBuffer{ PackageTrailer.Buffer, TArray<FFileRegion>() });
	}
}

void FFilePackageWriterUtil::CollectForSaveExportsBuffers(FRecord& Record, FCommitContext& Context)
{
	check(Context.ExportsBuffers.Num() == Record.Packages.Num());
	for (FPackageWriterRecords::FWritePackage& Package : Record.Packages)
	{
		TArray<FExportBuffer>& ExportsBuffers = Context.ExportsBuffers[Package.Info.MultiOutputIndex];
		check(ExportsBuffers.Num() > 0);

		// Split the ExportsBuffer into (1) Header and (2) Exports + AllAppendedData
		int64 HeaderSize = Package.Info.HeaderSize;
		FExportBuffer& HeaderAndExportsBuffer = ExportsBuffers[0];
		FSharedBuffer& HeaderAndExportsData = HeaderAndExportsBuffer.Buffer;

	
		// Header (.uasset/.umap)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = Package.Info.LooseFilePath;
			OutputFile.Buffer = FCompositeBuffer(
				FSharedBuffer::MakeView(HeaderAndExportsData.GetData(), HeaderSize, HeaderAndExportsData));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only calculate the main package output hash
		}

		// Exports + AllAppendedData (.uexp)
		{
			FWriteFileData& OutputFile = Context.OutputFiles.Emplace_GetRef();
			OutputFile.Filename = FPaths::ChangeExtension(Package.Info.LooseFilePath, LexToString(EPackageExtension::Exports));
			OutputFile.bIsSidecar = false;
			OutputFile.bContributeToHash = Package.Info.MultiOutputIndex == 0; // Only calculate the main package output hash

			int32 NumBuffers = ExportsBuffers.Num();
			TArray<FSharedBuffer> BuffersForComposition;
			BuffersForComposition.Reserve(NumBuffers);

			const uint8* ExportsStart = static_cast<const uint8*>(HeaderAndExportsData.GetData()) + HeaderSize;
			BuffersForComposition.Add(FSharedBuffer::MakeView(ExportsStart, HeaderAndExportsData.GetSize() - HeaderSize,
				HeaderAndExportsData));
			OutputFile.Regions.Append(MoveTemp(HeaderAndExportsBuffer.Regions));

			for (FExportBuffer& ExportsBuffer : TArrayView<FExportBuffer>(ExportsBuffers).Slice(1, NumBuffers - 1))
			{
				BuffersForComposition.Add(ExportsBuffer.Buffer);
				OutputFile.Regions.Append(MoveTemp(ExportsBuffer.Regions));
			}
			OutputFile.Buffer = FCompositeBuffer(BuffersForComposition);

			// Adjust regions so they are relative to the start of the uexp file
			for (FFileRegion& Region : OutputFile.Regions)
			{
				Region.Offset -= HeaderSize;
			}
		}
	}
}

void FFilePackageWriterUtil::AsyncSaveOutputFiles(FCommitContext& Context, TMap<FName, TRefCountPtr<FPackageHashes>>* AllPackageHashes, FCriticalSection* PackageHashesLock, bool bProvidePerPackageResult)
{
	if (bProvidePerPackageResult && !AllPackageHashes)
	{
		UE_LOG(LogSavePackage, Error, TEXT("FFilePackageWriterUtil::AsyncSaveOutputFiles : if bProvidePerPackageResult is true then AllPackageHashes can't be null."));
		return;
	}

	if (AllPackageHashes && !PackageHashesLock)
	{
		UE_LOG(LogSavePackage, Error, TEXT("FFilePackageWriterUtil::AsyncSaveOutputFiles : if AllPackageHashes is provided, then PackageHashesLock can't be null."));
		return;
	}

	if (!EnumHasAnyFlags(Context.Info.WriteOptions, IPackageWriter::EWriteOptions::Write | IPackageWriter::EWriteOptions::ComputeHash))
	{
		return;
	}

	UE::SavePackageUtilities::IncrementOutstandingAsyncWrites();

	TRefCountPtr<FPackageHashes> ThisPackageHashes;
	TUniquePtr<TPromise<int>> PackageHashesCompletionPromise;

	if (EnumHasAnyFlags(Context.Info.WriteOptions, IPackageWriter::EWriteOptions::ComputeHash))
	{
		ThisPackageHashes = new FPackageHashes();
		
		if (bProvidePerPackageResult)
		{
			PackageHashesCompletionPromise.Reset(new TPromise<int>());
			ThisPackageHashes->CompletionFuture = PackageHashesCompletionPromise->GetFuture();
		}

		bool bAlreadyExisted = false;
		if(AllPackageHashes)
		{
			FScopeLock PackageHashesScopeLock(PackageHashesLock);
			TRefCountPtr<FPackageHashes>& ExistingPackageHashes = AllPackageHashes->FindOrAdd(Context.Info.PackageName);
			// This calculation of bAlreadyExisted looks weird but we're finding the _refcount_, not the hashes. So if it gets
			// constructed, it's not actually assigned a pointer.
			bAlreadyExisted = ExistingPackageHashes.IsValid();
			ExistingPackageHashes = ThisPackageHashes;
		}
		if (bAlreadyExisted)
		{
			UE_LOG(LogSavePackage, Error, TEXT("FCookedFilePackageWriter encountered the same package twice in a cook! (%s)"), *Context.Info.PackageName.ToString());
		}
	}

	UE::Tasks::Launch(TEXT("HashAndWriteCookedFile"),
		[OutputFiles = MoveTemp(Context.OutputFiles), WriteOptions = Context.Info.WriteOptions,
		ThisPackageHashes = MoveTemp(ThisPackageHashes), PackageHashesCompletionPromise = MoveTemp(PackageHashesCompletionPromise)]
		() mutable
		{
			FMD5 AccumulatedHash;
			for (const FWriteFileData& OutputFile : OutputFiles)
			{
				OutputFile.HashAndWrite(AccumulatedHash, ThisPackageHashes, WriteOptions);
			}

			if (EnumHasAnyFlags(WriteOptions, IPackageWriter::EWriteOptions::ComputeHash))
			{
				ThisPackageHashes->PackageHash.Set(AccumulatedHash);
			}

			if (PackageHashesCompletionPromise)
			{
				// Note that setting this Promise might call arbitrary code from anything that subscribed
				// to ThisPackageHashes->CompletionFuture.Then(). So don't call it inside a lock.
				PackageHashesCompletionPromise->SetValue(0);
			}

			// This is used to release the game thread to access the hashes
			UE::SavePackageUtilities::DecrementOutstandingAsyncWrites();
		},
		UE::Tasks::ETaskPriority::BackgroundNormal
	);
}