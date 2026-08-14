// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSmoothingPreProcessor.h"

#include "Roles/LiveLinkBasicRole.h"

#include "UObject/Package.h"

UMetaHumanSmoothingPreProcessor::FMetaHumanSmoothingPreProcessorWorker::FMetaHumanSmoothingPreProcessorWorker(const TMap<FName, FMetaHumanRealtimeSmoothingParam>& InSmoothingParams)
	: FMetaHumanRealtimeSmoothing(InSmoothingParams)
{
}

TSubclassOf<ULiveLinkRole> UMetaHumanSmoothingPreProcessor::FMetaHumanSmoothingPreProcessorWorker::GetRole() const
{
	return ULiveLinkBasicRole::StaticClass();
}

bool UMetaHumanSmoothingPreProcessor::FMetaHumanSmoothingPreProcessorWorker::PreProcessFrame(const FLiveLinkStaticDataStruct& InStaticData, FLiveLinkFrameDataStruct& InOutFrame)
{
	const FLiveLinkBaseStaticData* LiveLinkSourceStaticData = InStaticData.Cast<FLiveLinkBaseStaticData>();
	if (!ensureMsgf(LiveLinkSourceStaticData, TEXT("Unexpected static data type when applying MetaHuman smoothing pre-processor")))
	{
		return false;
	}

	FLiveLinkBaseFrameData* LiveLinkSourceFrameData = InOutFrame.Cast<FLiveLinkBaseFrameData>();
	if (!ensureMsgf(LiveLinkSourceFrameData, TEXT("Unexpected frame data type when applying MetaHuman smoothing pre-processor")))
	{
		return false;
	}
	
	const double Now = FPlatformTime::Seconds();
	const double DeltaTime = Now - LastTime;
	LastTime = Now;

	return ProcessFrame(LiveLinkSourceStaticData->PropertyNames, LiveLinkSourceFrameData->PropertyValues, DeltaTime);
}

UMetaHumanSmoothingPreProcessor::UMetaHumanSmoothingPreProcessor()
{
	static constexpr const TCHAR* SmoothingPath = TEXT("/MetaHumanCoreTech/RealtimeMono/DefaultSmoothing.DefaultSmoothing");
	Parameters = LoadObject<UMetaHumanRealtimeSmoothingParams>(GetTransientPackage(), SmoothingPath);
}

#if WITH_EDITOR

void UMetaHumanSmoothingPreProcessor::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Worker.Reset();
	Super::PostEditChangeChainProperty(PropertyChangedEvent);
}

#endif //WITH_EDITOR

TSubclassOf<ULiveLinkRole> UMetaHumanSmoothingPreProcessor::GetRole() const
{
	return ULiveLinkBasicRole::StaticClass();
}

ULiveLinkFramePreProcessor::FWorkerSharedPtr UMetaHumanSmoothingPreProcessor::FetchWorker()
{
	if (!Worker.IsValid() && Parameters)
	{
		Worker = MakeShared<FMetaHumanSmoothingPreProcessorWorker>(Parameters->Parameters);
	}

	return Worker;
}
