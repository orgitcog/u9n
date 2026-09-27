// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LiveLinkFramePreProcessor.h"
#include "Engine/DataTable.h"
#include "MetaHumanRealtimeSmoothing.h"

#include "MetaHumanSmoothingPreProcessor.generated.h"

UCLASS(meta=(DisplayName="MetaHuman Smoothing"))
class METAHUMANLIVELINKSOURCE_API UMetaHumanSmoothingPreProcessor : public ULiveLinkFramePreProcessor
{
	GENERATED_BODY()

public:

	class FMetaHumanSmoothingPreProcessorWorker : public ILiveLinkFramePreProcessorWorker, public FMetaHumanRealtimeSmoothing
	{
	public:
		FMetaHumanSmoothingPreProcessorWorker(const TMap<FName, FMetaHumanRealtimeSmoothingParam>& InSmoothingParams);

		// ~Begin ILiveLinkFramePreProcessorWorker
		
		virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
		virtual bool PreProcessFrame(const FLiveLinkStaticDataStruct& InStaticData, FLiveLinkFrameDataStruct& InOutFrame) override;
		
		// ~End ILiveLinkFramePreProcessorWorker

	private:

		double LastTime = 0;
	};

	UMetaHumanSmoothingPreProcessor();
	
	//~ UObject interface
	
    #if WITH_EDITOR
    	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
    #endif //WITH_EDITOR
    
	//~ End UObject interface

	// ~Begin ULiveLinkFramePreProcessor
	
	virtual TSubclassOf<ULiveLinkRole> GetRole() const override;
	virtual FWorkerSharedPtr FetchWorker() override;

	// ~End ULiveLinkFramePreProcessor

	/** Smoothing parameters **/
	UPROPERTY(EditAnywhere, Category="Smoothing")
	TObjectPtr<UMetaHumanRealtimeSmoothingParams> Parameters;

private:

	/** The worker object instance */
	TSharedPtr<FMetaHumanSmoothingPreProcessorWorker, ESPMode::ThreadSafe> Worker;
};
