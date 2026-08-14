// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"

#include "ChaosVDModule.h"
#include "ChaosVDRecording.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Trace/ChaosVDTraceProvider.h"

FChaosVDTraceParticleDataProcessor::FChaosVDTraceParticleDataProcessor(): FChaosVDDataProcessorBase(FChaosVDParticleDataWrapper::WrapperTypeName)
{
}

bool FChaosVDTraceParticleDataProcessor::ProcessRawData(const TArray<uint8>& InData)
{
	FChaosVDDataProcessorBase::ProcessRawData(InData);

	TSharedPtr<FChaosVDTraceProvider> ProviderSharedPtr = TraceProvider.Pin();
	if (!ensure(ProviderSharedPtr.IsValid()))
	{
		return false;
	}

	TSharedPtr<FChaosVDParticleDataWrapper> ParticleData = MakeShared<FChaosVDParticleDataWrapper>();
	const bool bSuccess = Chaos::VisualDebugger::ReadDataFromBuffer(InData, *ParticleData, ProviderSharedPtr.ToSharedRef());

	if (bSuccess)
	{
		EChaosVDSolverStageAccessorFlags StageAccessorFlags = EChaosVDSolverStageAccessorFlags::CreateNewIfEmpty | EChaosVDSolverStageAccessorFlags::CreateNewIfClosed;

		ParticleData->SolverID = ProviderSharedPtr->GetRemappedSolverID(ParticleData->SolverID);

		if (FChaosVDFrameStageData* CurrentSolverStage = ProviderSharedPtr->GetCurrentSolverStageDataForCurrentFrame(ParticleData->SolverID, StageAccessorFlags))
		{
			int32 NewParticleDataIndex = INDEX_NONE;

			FString& DebugName = ParticleData->DebugName;
			ParticleData->DebugNameHash = CityHash64(reinterpret_cast<const char*>(*DebugName), DebugName.Len() * sizeof(TCHAR));

			// Non-Explicit stages might have duplicated data if a particle was traced twice. As CVD only shows the last state per stage, we need to just keep the last seen state
			// We do this on trace analysis to not do the de-duplication process during playback
			if (!EnumHasAnyFlags(CurrentSolverStage->StageFlags, EChaosVDSolverStageFlags::ExplicitStage))
			{
				if (int32* ExistingParticleDataIndex = CurrentSolverStage->CurrentRecordedParticlesIndexes.Find(ParticleData->ParticleIndex))
				{
					CurrentSolverStage->RecordedParticlesData[*ExistingParticleDataIndex] = ParticleData;
				}
				else
				{
					NewParticleDataIndex = CurrentSolverStage->RecordedParticlesData.Add(ParticleData);
				}
			}
			else
			{
				NewParticleDataIndex = CurrentSolverStage->RecordedParticlesData.Add(ParticleData);
			}

			if (NewParticleDataIndex != INDEX_NONE)
			{
				CurrentSolverStage->CurrentRecordedParticlesIndexes.Add(ParticleData->ParticleIndex, NewParticleDataIndex);
			}
			
		}
	}

	return bSuccess;
}
