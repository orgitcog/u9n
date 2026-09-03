// Copyright Epic Games, Inc. All Rights Reserved.

#include "CPSDataStream.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace UE::CaptureManager {

FCPSDataStream::FCPSDataStream(FFileExportFinished InFileExportFinished)
	: FileExportFinished(MoveTemp(InFileExportFinished))
{
}

bool FCPSDataStream::StartFile(const FString& InTakeName, const FString& InFileName)
{
	checkf(Data.IsEmpty(), TEXT("Data buffer must be empty"));
	return true;
}

bool FCPSDataStream::ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData)
{
	Data.Append(InData);
	return true;
}

bool FCPSDataStream::FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash)
{
	using namespace UE::CaptureManager;

	FMD5 MD5;
	MD5.Update(Data.GetData(), Data.Num());

	FMD5Hash Hash;
	Hash.Set(MD5);

	if (FMemory::Memcmp(Hash.GetBytes(), InHash.GetData(), Hash.GetSize()) != 0)
	{
		TMap<FString, UE::CaptureManager::TProtocolResult<FData>>& TakeEntry = ExportResults.FindOrAdd(InTakeName);
		TakeEntry.Add(InFileName, FCaptureProtocolError(TEXT("Invalid file hash")));
		return true;
	}

	TMap<FString, UE::CaptureManager::TProtocolResult<FData>>& TakeEntry = ExportResults.FindOrAdd(InTakeName);
	TakeEntry.Add(InFileName, MoveTemp(Data));

	return true;
}

void FCPSDataStream::Finalize(UE::CaptureManager::TProtocolResult<void> InResult)
{
	FileExportFinished.ExecuteIfBound(MoveTemp(ExportResults));
}

}