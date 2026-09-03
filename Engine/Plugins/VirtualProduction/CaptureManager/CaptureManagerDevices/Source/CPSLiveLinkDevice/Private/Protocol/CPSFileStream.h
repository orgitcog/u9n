// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportClient/ExportClient.h"

namespace UE::CaptureManager
{

class FCPSFileStream : public UE::CaptureManager::FBaseStream
{
public:

	DECLARE_DELEGATE_OneParam(FReportProgress, float InProgress);
	DECLARE_DELEGATE_OneParam(FExportFinished, UE::CaptureManager::TProtocolResult<void> InResult);

	FCPSFileStream(FString InBaseDir, uint64 InSize);

	virtual bool StartFile(const FString& InTakeName, const FString& InFileName) override;

	virtual bool ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData) override;
	virtual bool FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash) override;

	virtual void Finalize(UE::CaptureManager::TProtocolResult<void> InResult) override;

	void SetExportFinished(FExportFinished InExportFinished);
	void SetProgressHandler(FReportProgress InReportProgress);

private:

	void ReportProgressStep(const FString& InFileName, const uint32 InArrivedSize);

	FString BaseDir;
	TUniquePtr<FArchive> Writer;
	TUniquePtr<FMD5> MD5Generator;

	uint64 TotalExportExpectedSize;
	uint64 TotalExportArrivedSize;

	FExportFinished OnExportFinished;
	FReportProgress OnReportProgress;
};

}

