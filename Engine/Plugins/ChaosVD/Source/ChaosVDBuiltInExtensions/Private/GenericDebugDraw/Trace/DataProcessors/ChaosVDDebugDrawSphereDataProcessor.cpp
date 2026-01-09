// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDDebugDrawSphereDataProcessor.h"

#include "ChaosVDDebugDrawDataProcessorUtils.h"
#include "DataWrappers/ChaosVDDebugShapeDataWrapper.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"


FChaosVDDebugDrawSphereDataProcessor::FChaosVDDebugDrawSphereDataProcessor() : FChaosVDDataProcessorBase(FChaosVDDebugDrawSphereDataWrapper::WrapperTypeName)
{
}

bool FChaosVDDebugDrawSphereDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	FChaosVDDataProcessorBase::ProcessRawData(InData);

	const TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	const TSharedPtr<FChaosVDDebugDrawSphereDataWrapper> DebugDrawData = MakeShared<FChaosVDDebugDrawSphereDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *DebugDrawData, ProviderSharedPtr.ToSharedRef());
	
	if (bSuccess)
	{
		using namespace Chaos::VisualDebugger::Utils;

		TSharedRef<FChaosVDTraceProvider> ProviderAsSharedRef = ProviderSharedPtr.ToSharedRef();
		TSharedRef<FChaosVDDebugDrawSphereDataWrapper> DebugDrawDataAsSharedRef = DebugDrawData.ToSharedRef();
		RemapDebugDrawShapeDataSolverID(DebugDrawDataAsSharedRef, ProviderAsSharedRef);
	
		if (TSharedPtr<FChaosVDDebugShapeDataContainer> DebugDrawShapeData = GetShapeDataContainer(DebugDrawDataAsSharedRef, ProviderAsSharedRef, EShapeDataContainerAccessorFlags::MarkFrameDirty))
		{
			DebugDrawShapeData->RecordedDebugDrawSpheres.Add(DebugDrawData);
		}
	}

	return bSuccess;
}
