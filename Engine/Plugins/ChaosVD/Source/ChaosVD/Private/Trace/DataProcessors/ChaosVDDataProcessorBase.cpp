// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDDataProcessorBase.h"

FChaosVDDataProcessorBase::~FChaosVDDataProcessorBase()
{
}

FStringView FChaosVDDataProcessorBase::GetCompatibleTypeName() const
{
	return CompatibleType;
}

bool FChaosVDDataProcessorBase::ProcessRawData(const TArray<uint8>& InData)
{
	ProcessedBytes += InData.Num();
	return true;
}

uint64 FChaosVDDataProcessorBase::GetProcessedBytes() const
{
	return ProcessedBytes;
}

void FChaosVDDataProcessorBase::SetTraceProvider(const TSharedPtr<FChaosVDTraceProvider>& InProvider)
{
	TraceProvider = InProvider;
}

FChaosVDGenericDataProcessor::FChaosVDGenericDataProcessor(FStringView InCompatibleType, const TFunction<bool(const TArray<uint8>&)>& InProcessDataCallback) : FChaosVDDataProcessorBase(InCompatibleType), ProcessDataCallback(InProcessDataCallback)
{
}

bool FChaosVDGenericDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	return FChaosVDDataProcessorBase::ProcessRawData(InData) && ProcessDataCallback && ProcessDataCallback(InData);
}
