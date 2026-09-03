// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceConnectionExportStreams.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "LiveLinkFaceFootageIngest"

FFileStream::FFileStream(FString InBaseDir, FString InTakeName, uint64 InSize)
	: BaseDir(MoveTemp(InBaseDir))
	, TakeName(MoveTemp(InTakeName))
	, TotalExportExpectedSize(InSize)
	, TotalExportArrivedSize(0)
{
}

bool FFileStream::StartFile(const FString& InTakeName, const FString& InFileName)
{
	check(TakeName == InTakeName);

	FString FilePathToBeSaved = FPaths::Combine(BaseDir, InTakeName, InFileName);

	checkf(!Writer, TEXT("Writer must be nullptr when new file arrives"));
	Writer = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*FilePathToBeSaved, EFileWrite::FILEWRITE_None));

	checkf(!MD5Generator, TEXT("MD5 generator must be nullptr when new file arrives"));
	MD5Generator = MakeUnique<FMD5>();

	return true;
}

bool FFileStream::ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData)
{
	check(TakeName == InTakeName);
	Writer->Serialize(const_cast<uint8*>(InData.GetData()), InData.Num());
	ReportProgressStep(InFileName, InData.Num());

	MD5Generator->Update(InData.GetData(), InData.Num());

	return true;
}

bool FFileStream::FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash)
{
	check(TakeName == InTakeName);

	checkf(Writer, TEXT("Writer must be valid when file has arrived"));
	Writer->Close();
	Writer = nullptr;

	checkf(MD5Generator, TEXT("MD5 Generator must be valid when file has arrived"));
	TStaticArray<uint8, 16> Hash;
	MD5Generator->Final(Hash.GetData());
	MD5Generator = nullptr;

	if (FMemory::Memcmp(Hash.GetData(), InHash.GetData(), Hash.Num()) != 0)
	{
		Done(FCaptureProtocolError(TEXT("Invalid hash file")));

		return false;
	}

	return true;
}

void FFileStream::Done(TProtocolResult<void> InResult)
{
	// Close the writer if not closed
	if (Writer)
	{
		Writer->Close();
		Writer = nullptr;
	}

	OnExportFinished.ExecuteIfBound(TakeName, MoveTemp(InResult));
}

void FFileStream::SetExportFinished(FExportFinished InExportFinished)
{
	OnExportFinished = MoveTemp(InExportFinished);
}

void FFileStream::SetProgressHandler(FReportProgress InReportProgress)
{
	OnReportProgress = MoveTemp(InReportProgress);
}

void FFileStream::ReportProgressStep(const FString& InFileName, const uint32 InArrivedSize)
{
	TotalExportArrivedSize += InArrivedSize;
	OnReportProgress.ExecuteIfBound(TakeName, (float) TotalExportArrivedSize / TotalExportExpectedSize);
}

FDataStream::FDataStream()
{
}

bool FDataStream::StartFile(const FString& InTakeName, const FString& InFileName)
{
	checkf(Data.IsEmpty(), TEXT("Data buffer must be empty"));
	return true;
}

bool FDataStream::ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData)
{
	Data.Append(InData);
	return true;
}

bool FDataStream::FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash)
{
	FMD5 MD5;
	MD5.Update(Data.GetData(), Data.Num());

	FMD5Hash Hash;
	Hash.Set(MD5);

	if (FMemory::Memcmp(Hash.GetBytes(), InHash.GetData(), Hash.GetSize()) != 0)
	{
		FileExportFinished.ExecuteIfBound(InTakeName, FCaptureProtocolError(TEXT("Invalid file hash")));
	}

	FileExportFinished.ExecuteIfBound(InTakeName, MoveTemp(Data));

	return true;
}

void FDataStream::Done(TProtocolResult<void> InResult)
{
}

void FDataStream::SetExportFinished(FFileExportFinished InFileExportFinished)
{
	FileExportFinished = MoveTemp(InFileExportFinished);
}

#undef LOCTEXT_NAMESPACE