// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportClient/ExportClient.h"

#include "Containers/Map.h"

namespace UE::CaptureManager {

class FCPSDataStream : public UE::CaptureManager::FBaseStream
{
public:
	using FData = TArray<uint8>;
	using FResults = TMap<FString, TMap<FString, UE::CaptureManager::TProtocolResult<FData>>>;

	DECLARE_DELEGATE_OneParam(FFileExportFinished, FResults InExportResults);

	FCPSDataStream(FFileExportFinished InFileExportFinished);

	bool StartFile(const FString& InTakeName, const FString& InFileName) override;
	bool ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData) override;
	bool FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash) override;

	void Finalize(UE::CaptureManager::TProtocolResult<void> InResult) override;

private:
	FData Data;
	FFileExportFinished FileExportFinished;

	FResults ExportResults;
};

}