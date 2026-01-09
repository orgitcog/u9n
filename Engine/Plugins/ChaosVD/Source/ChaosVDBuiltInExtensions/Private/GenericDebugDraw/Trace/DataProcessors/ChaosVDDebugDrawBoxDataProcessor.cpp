// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDDebugDrawBoxDataProcessor.h"

#include "ChaosVDDebugDrawDataProcessorUtils.h"
#include "DataWrappers/ChaosVDDebugShapeDataWrapper.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"


FChaosVDDebugDrawBoxDataProcessor::FChaosVDDebugDrawBoxDataProcessor() : FChaosVDDataProcessorBase(FChaosVDDebugDrawBoxDataWrapper::WrapperTypeName)
{
}

bool FChaosVDDebugDrawBoxDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	FChaosVDDataProcessorBase::ProcessRawData(InData);

	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FChaosVDDebugDrawBoxDataWrapper> DebugDrawData = MakeShared<FChaosVDDebugDrawBoxDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *DebugDrawData, ProviderSharedPtr.ToSharedRef());
	
	if (bSuccess)
	{
		using namespace Chaos::VisualDebugger::Utils;

		TSharedRef<FChaosVDTraceProvider> ProviderAsSharedRef = ProviderSharedPtr.ToSharedRef();
		TSharedRef<FChaosVDDebugDrawBoxDataWrapper> DebugDrawDataAsSharedRef = DebugDrawData.ToSharedRef();
		RemapDebugDrawShapeDataSolverID(DebugDrawDataAsSharedRef, ProviderAsSharedRef);
	
		if (TSharedPtr<FChaosVDDebugShapeDataContainer> DebugDrawShapeData = GetShapeDataContainer(DebugDrawDataAsSharedRef, ProviderAsSharedRef, EShapeDataContainerAccessorFlags::MarkFrameDirty))
		{
			DebugDrawShapeData->RecordedDebugDrawBoxes.Add(DebugDrawData);
		}
	}

	return bSuccess;
}
