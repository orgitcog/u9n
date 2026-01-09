// Copyright Epic Games, Inc. All Rights Reserved.

#include "Trace/ChaosVDTraceProvider.h"

#include "ChaosVDModule.h"
#include "ChaosVDRecording.h"
#include "ChaosVDSettingsManager.h"
#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "Containers/Ticker.h"
#include "DataProcessors/ChaosVDCollisionChannelsInfoDataProcessor.h"
#include "ExtensionsSystem/ChaosVDExtensionsManager.h"
#include "Misc/MessageDialog.h"
#include "Settings/ChaosVDGeneralSettings.h"
#include "Trace/DataProcessors/ChaosVDCharacterGroundConstraintDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDConstraintDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDJointConstraintDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDMidPhaseDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDSceneQueryDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDSceneQueryVisitDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDSerializedNameEntryDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceImplicitObjectProcessor.h"
#include "Trace/DataProcessors/ChaosVDTraceParticleDataProcessor.h"
#include "Trace/DataProcessors/ChaosVDArchiveHeaderProcessor.h"
#include "Trace/DataProcessors/ChaosVDDataProcessorBase.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FName FChaosVDTraceProvider::ProviderName("ChaosVDProvider");

FChaosVDTraceProvider::FChaosVDTraceProvider(TraceServices::IAnalysisSession& InSession): Session(InSession)
{
	using namespace Chaos::VisualDebugger;
	NameTable = MakeShared<FChaosVDSerializableNameTable>();

	// Start with a default header data as a fallback
	HeaderData = FChaosVDArchiveHeader::Current();
	
	if (UChaosVDGeneralSettings* Settings = FChaosVDSettingsManager::Get().GetSettingsObject<UChaosVDGeneralSettings>())
	{
		bShouldTrimOutStartEmptyFrames = Settings->bTrimEmptyFrames;
		MaxGameFramesToQueueNum = Settings->MaxGameThreadFramesToQueueNum;
	}
}

void FChaosVDTraceProvider::CreateRecordingInstanceForSession(const FString& InSessionName)
{
	if (bHasRecordingOverride)
	{
		return;
	}

	DeleteRecordingInstanceForSession();

	InternalRecording = MakeShared<FChaosVDRecording>();
	InternalRecording->SessionName = InSessionName;
}

void FChaosVDTraceProvider::SetExternalRecordingInstanceForSession(const TSharedRef<FChaosVDRecording>& InExternalCVDRecording)
{
	bHasRecordingOverride = true;
	if (InternalRecording)
	{
		ensure(InternalRecording->IsEmpty());
	}
	InExternalCVDRecording->AddAttributes(EChaosVDRecordingAttributes::Merged);

	InternalRecording = InExternalCVDRecording;
}

void FChaosVDTraceProvider::DeleteRecordingInstanceForSession()
{
	InternalRecording.Reset();
}

void FChaosVDTraceProvider::StartSolverFrame(const int32 InSolverID, FChaosVDSolverFrameData&& FrameData)
{
	if (!InternalRecording.IsValid())
	{
		return;
	}

	bool bIsInvalidSolverID = InSolverID == INDEX_NONE && !CurrentSolverFramesByID.IsEmpty();

	if (!ensure(!bIsInvalidSolverID))
	{
		return;
	}

	if (FChaosVDSolverFrameData* SolveFrameData = CurrentSolverFramesByID.Find(InSolverID))
	{
		InternalRecording->AddFrameForSolver(InSolverID, MoveTemp(*SolveFrameData));
		CurrentSolverFramesByID[InSolverID] = FrameData;
	}
	else
	{
		CurrentSolverFramesByID.Add(InSolverID, FrameData);
	}
}

void FChaosVDTraceProvider::GetAvailablePendingSolverIDsAtGameFrame(const TSharedRef<FChaosVDGameFrameData>& InProcessedGameFrameData, TArray<int32, TInlineAllocator<16>>& OutSolverIDs)
{
	for (const TPair<int32, FChaosVDSolverFrameData>& FrameDataWithSolverID : CurrentSolverFramesByID)
	{
		if (FrameDataWithSolverID.Value.FrameCycle < InProcessedGameFrameData->FirstCycle)
		{
			OutSolverIDs.Add(FrameDataWithSolverID.Key);
		}
	}
}

FString FChaosVDTraceProvider::GenerateFormattedStringListFromSet(const TSet<FString>& StringsSet)
{
	FString FormattedString = TEXT("");
	for (const FString& ListEntry : StringsSet)
	{
		FormattedString.Append(TEXT("- "));
		FormattedString.Append(ListEntry);
		FormattedString.Append(TEXT("\n"));
	}

	return MoveTemp(FormattedString);
}

int32 FChaosVDTraceProvider::RemapSolverID(int32 SolverID)
{
	int32 RemappedSolverID = SolverID;

	{
		// Lock the recording until we manage to reserve a unique solver ID replacement
		FWriteScopeLock WriteLock(InternalRecording->RecordingDataLock);

		while (InternalRecording->HasSolverID_AssumesLocked(RemappedSolverID))
		{
			constexpr int32 MaxValue = std::numeric_limits<int32>::max() -1;
			check(RemappedSolverID < MaxValue);
	
			RemappedSolverID = InternalRecording->GetAvailableTrackIDForRemapping();
		}

		InternalRecording->ReserveSolverID_AssumesLocked(RemappedSolverID);
	}

	UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%hs] Remapped solver id from [%d] to [%d]."), __func__, SolverID, RemappedSolverID);

	RemappedSolversIDs.Emplace(SolverID, RemappedSolverID);

	return RemappedSolverID;
}

int32 FChaosVDTraceProvider::GetRemappedSolverID(int32 SolverID)
{
	int32 RemappedSolverID = SolverID;

	if (int32* SolverIDPtr = RemappedSolversIDs.Find(SolverID))
	{
		RemappedSolverID = *SolverIDPtr;
	}
	else
	{
		UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%hs] Failed to get remapped solver id [%d]. Data that references the invalid solver id will be ignored."), __func__, SolverID);
		RemappedSolverID = INDEX_NONE; 
	}

	return RemappedSolverID;
}

void FChaosVDTraceProvider::CommitProcessedGameFramesToRecording()
{
	TArray<int32, TInlineAllocator<16>> SolverIDs;

	// The Game Frame events are not generated by CVD trace code, and we don't have control over them.
	// we use them as general timestamps.
	// These are generated even when no solvers are available (specially in PIE), so we need to discard any game frame that will not resolve to a solver frame
	// Physics Frames and GT frames lifetimes might not align with async physics enabled, so to make sure we have all the solver data for that time range, we queue a handful of game frames before processing them.

	if (CurrentGameFrameQueueSize > MaxGameFramesToQueueNum)
	{
		TSharedPtr<FChaosVDGameFrameData> ProcessedGameFrameData;
		DeQueueGameFrameForProcessing(ProcessedGameFrameData);

		if (StartLastCommitedFrameTimeSeconds == 0.0)
		{
			StartLastCommitedFrameTimeSeconds = FPlatformTime::Seconds();
		}

		if (ProcessedGameFrameData.IsValid())
		{
			InternalRecording->GetAvailableSolverIDsAtGameFrame(*ProcessedGameFrameData, SolverIDs);

			// Is it possible that the solver data is not commited to the recording yet as it is being processed.
			// Usually this happens on recordings with Async Physics
			if (SolverIDs.IsEmpty())
			{
				GetAvailablePendingSolverIDsAtGameFrame(ProcessedGameFrameData.ToSharedRef(), SolverIDs);
			}
	
			const bool bHasAnySolverData = !SolverIDs.IsEmpty();
			const bool bHasAnyGameFrame = InternalRecording->GetAvailableGameFramesNumber() > 0;
			
			bool bHasRelevantCVDData = true;
			if (bShouldTrimOutStartEmptyFrames)
			{
				bHasRelevantCVDData = bHasAnyGameFrame ? true : bHasAnySolverData;
			}

			if (bHasRelevantCVDData)
			{
				InternalRecording->AddGameFrameData(*ProcessedGameFrameData);	
			}
		}
	}

	SolverIDs.Reset();
}

void FChaosVDTraceProvider::StartGameFrame(const TSharedPtr<FChaosVDGameFrameData>& InFrameData)
{
	if (!InternalRecording.IsValid() || bHasRecordingOverride)
	{
		return;
	}

	CommitProcessedGameFramesToRecording();

	EnqueueGameFrameForProcessing(InFrameData);
}

FChaosVDSolverFrameData* FChaosVDTraceProvider::GetCurrentSolverFrame(const int32 InSolverID)
{
	// If we didn't remap any ID yet, InSolverID might be INDEX_NONE. This is expected as we can have data that was started being recorded in the
	// middle of a frame and therefore the solver hasn't been open in CVD yet.
	bool bIsInvalidSolverID = InSolverID == INDEX_NONE && (!CurrentSolverFramesByID.IsEmpty() && RemappedGameThreadTrackID != INDEX_NONE);

	if (bIsInvalidSolverID)
	{
		UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%hs] was called with an invalid solver ID. Data that references the invalid solver id will be ignored."), __func__);
		return nullptr;
	}

	if (FChaosVDSolverFrameData* SolveFrameData = CurrentSolverFramesByID.Find(InSolverID))
	{
		return SolveFrameData;
	}

	return nullptr;
}

TWeakPtr<FChaosVDGameFrameData> FChaosVDTraceProvider::GetCurrentGameFrame()
{
	if (bHasRecordingOverride)
	{
		if (FChaosVDSolverFrameData* FrameData = GetCurrentSolverFrame(GetCurrentGameThreadTrackID()))
		{
			TSharedPtr<FChaosVDGameFrameDataWrapper> GTFrameDataWrapper = FrameData->GetCustomData().GetData<FChaosVDGameFrameDataWrapper>();
			if (ensure(GTFrameDataWrapper))
			{
				return GTFrameDataWrapper->FrameData;
			}
		}
	}
	else if (TSharedPtr<FChaosVDGameFrameData> GTFrameData = CurrentGameFrame.Pin())
	{
		return GTFrameData;
	}

	return nullptr;
}

FChaosVDTraceProvider::FBinaryDataContainer& FChaosVDTraceProvider::FindOrAddUnprocessedData(const int32 DataID)
{
	if (const TSharedPtr<FBinaryDataContainer>* UnprocessedData = UnprocessedDataByID.Find(DataID))
	{
		check(UnprocessedData->IsValid());
		return *UnprocessedData->Get();
	}
	else
	{
		const TSharedPtr<FBinaryDataContainer> DataContainer = MakeShared<FBinaryDataContainer>(DataID);
		UnprocessedDataByID.Add(DataID, DataContainer);
		return *DataContainer.Get();
	}
}

void FChaosVDTraceProvider::RemoveUnprocessedData(const int32 DataID)
{
	// The removal call should always come before the data is processed
	ensure(UnprocessedDataByID.Remove(DataID) != 0);
}

bool FChaosVDTraceProvider::ProcessBinaryData(const int32 DataID)
{
	RegisterDefaultDataProcessorsIfNeeded();

	if (const TSharedPtr<FBinaryDataContainer>* UnprocessedDataPtr = UnprocessedDataByID.Find(DataID))
	{
		const TSharedPtr<FBinaryDataContainer> UnprocessedData = *UnprocessedDataPtr;
		if (UnprocessedData.IsValid())
		{
			UnprocessedData->bIsReady = true;

			const TArray<uint8>* RawData = nullptr;
			TArray<uint8> UncompressedData;
			if (UnprocessedData->bIsCompressed)
			{
				UncompressedData.Reserve(UnprocessedData->UncompressedSize);
				FOodleCompressedArray::DecompressToTArray(UncompressedData, UnprocessedData->RawData);
				RawData = &UncompressedData;
			}
			else
			{
				RawData = &UnprocessedData->RawData;
			}

			if (TSharedPtr<FChaosVDDataProcessorBase>* DataProcessorPtrPtr = RegisteredDataProcessors.Find(UnprocessedData->TypeName))
			{
				if (TSharedPtr<FChaosVDDataProcessorBase> DataProcessorPtr = *DataProcessorPtrPtr)
				{
					if (ensure(DataProcessorPtr->ProcessRawData(*RawData)))
					{
						UnprocessedDataByID.Remove(DataID);
						DataProcessedSoFarCounter++;
						return true;
					}
					else
					{
						UE_LOG(LogChaosVDEditor, Verbose, TEXT("[%s] Failed to serialize Binary Data with ID [%d] | Type [%s]"), ANSI_TO_TCHAR(__FUNCTION__), DataID, *UnprocessedData->TypeName);
						TypesFailedToSerialize.Add(UnprocessedData->TypeName);
						DataProcessedSoFarCounter++;
					}
				}
			}
			else
			{
				MissingDataProcessors.Add(UnprocessedData->TypeName);	
			}
		}

		UnprocessedDataByID.Remove(DataID);
	}

	return false;
}

TSharedPtr<FChaosVDRecording> FChaosVDTraceProvider::GetRecordingForSession() const
{
	return InternalRecording;
}

void FChaosVDTraceProvider::RegisterDataProcessor(TSharedPtr<FChaosVDDataProcessorBase> InDataProcessor)
{
	RegisteredDataProcessors.Add(InDataProcessor->GetCompatibleTypeName(), InDataProcessor);
}

void FChaosVDTraceProvider::HandleAnalysisComplete()
{
	if (!MissingDataProcessors.IsEmpty())
	{
		FString MissingProcessorNameList = GenerateFormattedStringListFromSet(MissingDataProcessors);
	
		FText MissingDataProcessorsMessage = FText::FormatOrdered(LOCTEXT("MissingDataProcessorMessage", "This recording was made with CVD extensions that are not supported in this version. \n\nAs a result, the following data types could not be read and will be ignored : \n\n {0}"), FText::FromString(MissingProcessorNameList));

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([MissingDataProcessorsMessage](float DeltaTime)
		{
			FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::Ok, MissingDataProcessorsMessage, LOCTEXT("MissingDataProcessorMessageTitle", "Partially unsupported CVD Recording"));
			return false;
		}));
	}

	if (!TypesFailedToSerialize.IsEmpty())
	{
		FString FailedTypeList = GenerateFormattedStringListFromSet(TypesFailedToSerialize);
	
		FText MissingDataProcessorsMessage = FText::FormatOrdered(LOCTEXT("FailedSerializationMessage", "The following data types were part of the recording, but they couldn't be read : \n\n {0} \n\n Visualization related to that data will not be shown."), FText::FromString(FailedTypeList));

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([MissingDataProcessorsMessage](float DeltaTime)
		{
			FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, MissingDataProcessorsMessage, LOCTEXT("FailedSerializationMessageTitle", "Failed to read data"));
			return false;
		}));
	}

	UnprocessedDataByID.Reset();

	UE_LOG(LogChaosVDEditor, Log, TEXT("Trace Analysis complete for session [%s] | Calculating data loaded stats..."), Session.GetName());

	static const FNumberFormattingOptions SizeFormattingOptions = FNumberFormattingOptions().SetMinimumFractionalDigits(2).SetMaximumFractionalDigits(2);

	uint64 TotalBytes = 0;
	for (const TPair<FStringView, TSharedPtr<FChaosVDDataProcessorBase>>& DataProcessor : RegisteredDataProcessors)
	{
		if (DataProcessor.Value)
		{
			uint64 ProcessedBytes = DataProcessor.Value->GetProcessedBytes();
			TotalBytes += ProcessedBytes;
			UE_LOG(LogChaosVDEditor, Log, TEXT("Data loaded for type [%s]  => [%s] "), DataProcessor.Key.IsEmpty() ? TEXT("Invalid") : DataProcessor.Key.GetData(), *FText::AsMemory(ProcessedBytes, &SizeFormattingOptions,nullptr, EMemoryUnitStandard::IEC).ToString());
		}
	}

	if (TSharedPtr<FChaosVDRecording> Recording = GetRecordingForSession())
	{
		double TotalTimeProcessingFrames = FPlatformTime::Seconds() - StartLastCommitedFrameTimeSeconds;

		int32 NumOfGameFramesProcessed = Recording->GetAvailableGameFramesNumber();
		double AvgTimePerFrameSeconds = TotalTimeProcessingFrames / NumOfGameFramesProcessed;
		
		UE_LOG(LogChaosVDEditor, Log, TEXT(" [%d] Game frames Processed at [%f] ms per frame on average"), NumOfGameFramesProcessed, AvgTimePerFrameSeconds * 1000.0);
	}

	UE_LOG(LogChaosVDEditor, Log, TEXT("Total size of loaded data => [%s]"), *FText::AsMemory(TotalBytes, &SizeFormattingOptions,nullptr, EMemoryUnitStandard::IEC).ToString());
}

FChaosVDFrameStageData* FChaosVDTraceProvider::GetCurrentSolverStageDataForCurrentFrame(int32 SolverID, EChaosVDSolverStageAccessorFlags Flags)
{
	auto CreateInBetweenSolverStage = [](FChaosVDSolverFrameData& InFrameData)
	{
		// Add an empty step. It will be filled out by the particle (and later on other objects/elements) events
		FChaosVDFrameStageData& SolverStageData = InFrameData.SolverSteps.AddDefaulted_GetRef();
		SolverStageData.StepName = TEXT("Between Stage Data");
		EnumAddFlags(SolverStageData.StageFlags, EChaosVDSolverStageFlags::Open);

		return &SolverStageData;
	};

	if (FChaosVDSolverFrameData* FrameData = GetCurrentSolverFrame(SolverID))
	{
		if (FrameData->SolverSteps.Num() == 0)
		{
			if (EnumHasAnyFlags(Flags, EChaosVDSolverStageAccessorFlags::CreateNewIfEmpty))
			{
				return CreateInBetweenSolverStage(*FrameData);
			}
		}

		FChaosVDFrameStageData& CurrentSolverStage = FrameData->SolverSteps.Last();
		if (EnumHasAnyFlags(CurrentSolverStage.StageFlags, EChaosVDSolverStageFlags::Open))
		{
			return &CurrentSolverStage;
		}

		if (EnumHasAnyFlags(Flags, EChaosVDSolverStageAccessorFlags::CreateNewIfClosed))
		{
			return CreateInBetweenSolverStage(*FrameData);
		}
	}

	return nullptr;
}

void FChaosVDTraceProvider::RegisterDefaultDataProcessorsIfNeeded()
{
	if (bDefaultDataProcessorsRegistered)
	{
		return;
	}
	
	TSharedPtr<FChaosVDTraceImplicitObjectProcessor> ImplicitObjectProcessor = MakeShared<FChaosVDTraceImplicitObjectProcessor>();
	ImplicitObjectProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ImplicitObjectProcessor);

	TSharedPtr<FChaosVDTraceParticleDataProcessor> ParticleDataProcessor = MakeShared<FChaosVDTraceParticleDataProcessor>();
	ParticleDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ParticleDataProcessor);

	TSharedPtr<FChaosVDMidPhaseDataProcessor> MidPhaseDataProcessor = MakeShared<FChaosVDMidPhaseDataProcessor>();
	MidPhaseDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(MidPhaseDataProcessor);

	TSharedPtr<FChaosVDConstraintDataProcessor> ConstraintDataProcessor = MakeShared<FChaosVDConstraintDataProcessor>();
	ConstraintDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ConstraintDataProcessor);

	TSharedPtr<FChaosVDSceneQueryDataProcessor> SceneQueryDataProcessor = MakeShared<FChaosVDSceneQueryDataProcessor>();
	SceneQueryDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(SceneQueryDataProcessor);

	TSharedPtr<FChaosVDSceneQueryVisitDataProcessor> SceneQueryVisitDataProcessor = MakeShared<FChaosVDSceneQueryVisitDataProcessor>();
	SceneQueryVisitDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(SceneQueryVisitDataProcessor);

	TSharedPtr<FChaosVDSerializedNameEntryDataProcessor> NameEntryDataProcessor = MakeShared<FChaosVDSerializedNameEntryDataProcessor>();
	NameEntryDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(NameEntryDataProcessor);
	
	TSharedPtr<FChaosVDJointConstraintDataProcessor> JointConstraintDataProcessor = MakeShared<FChaosVDJointConstraintDataProcessor>();
	JointConstraintDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(JointConstraintDataProcessor);

	TSharedPtr<FChaosVDCharacterGroundConstraintDataProcessor> CharacterGroundConstraintDataProcessor = MakeShared<FChaosVDCharacterGroundConstraintDataProcessor>();
	CharacterGroundConstraintDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(CharacterGroundConstraintDataProcessor);

	TSharedPtr<FChaosVDArchiveHeaderProcessor> ArchiveHeaderDataProcessor = MakeShared<FChaosVDArchiveHeaderProcessor>();
	ArchiveHeaderDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(ArchiveHeaderDataProcessor);
	
	TSharedPtr<FChaosVDCollisionChannelsInfoDataProcessor> CollisionChannelsInfoDataProcessor = MakeShared<FChaosVDCollisionChannelsInfoDataProcessor>();
	CollisionChannelsInfoDataProcessor->SetTraceProvider(AsShared());
	RegisterDataProcessor(CollisionChannelsInfoDataProcessor);

	FChaosVDExtensionsManager::Get().EnumerateExtensions([this](const TSharedRef<FChaosVDExtension>& Extension)
	{
		Extension->RegisterDataProcessorsInstancesForProvider(StaticCastSharedRef<FChaosVDTraceProvider>(AsShared()));
		return true;
	});

	bDefaultDataProcessorsRegistered = true;
}

void FChaosVDTraceProvider::EnqueueGameFrameForProcessing(const TSharedPtr<FChaosVDGameFrameData>& FrameData)
{
	CurrentGameFrame = FrameData;
	CurrentGameFrameQueue.Enqueue(FrameData);
	CurrentGameFrameQueueSize++;
}

void FChaosVDTraceProvider::DeQueueGameFrameForProcessing(TSharedPtr<FChaosVDGameFrameData>& OutFrameData)
{
	CurrentGameFrameQueue.Dequeue(OutFrameData);
	CurrentGameFrameQueueSize--;
}

#undef LOCTEXT_NAMESPACE
