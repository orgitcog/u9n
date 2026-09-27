// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ExportClient/ExportClient.h"

class FFileStream : public FBaseStream
{
public:

	DECLARE_DELEGATE_TwoParams(FReportProgress, const FString& InTakeName, float InProgress);
	DECLARE_DELEGATE_TwoParams(FExportFinished, const FString& InTakeName, TProtocolResult<void> InResult);

	FFileStream(FString InBaseDir, FString InTakeName, uint64 InSize);

	virtual bool StartFile(const FString& InTakeName, const FString& InFileName) override;

	virtual bool ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData) override;
	virtual bool FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash) override;

	virtual void Done(TProtocolResult<void> InResult) override;

	void SetExportFinished(FExportFinished InExportFinished);
	void SetProgressHandler(FReportProgress InReportProgress);

private:

	void ReportProgressStep(const FString& InFileName, const uint32 InArrivedSize);

	FString BaseDir;
	FString TakeName;
	TUniquePtr<FArchive> Writer;
	TUniquePtr<FMD5> MD5Generator;

	uint64 TotalExportExpectedSize;
	uint64 TotalExportArrivedSize;

	FExportFinished OnExportFinished;
	FReportProgress OnReportProgress;
};

class FDataStream : public FBaseStream
{
public:
	using FData = TArray<uint8>;

	DECLARE_DELEGATE_TwoParams(FFileExportFinished, const FString& InTakeName, TProtocolResult<FData> InDataResult);

	FDataStream();

	bool StartFile(const FString& InTakeName, const FString& InFileName) override;
	bool ProcessData(const FString& InTakeName, const FString& InFileName, const TConstArrayView<uint8>& InData) override;
	bool FinishFile(const FString& InTakeName, const FString& InFileName, const TStaticArray<uint8, 16>& InHash) override;

	void Done(TProtocolResult<void> InResult) override;

	void SetExportFinished(FFileExportFinished InFileExportFinished);

private:

	FData Data;
	FFileExportFinished FileExportFinished;
};