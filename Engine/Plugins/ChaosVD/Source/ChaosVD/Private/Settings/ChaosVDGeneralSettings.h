// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Settings/ChaosVDCoreSettings.h"
#include "ChaosVDGeneralSettings.generated.h"

/**
 * General settings that controls how CVD behaves
 */
UCLASS(config=ChaosVD, PerObjectConfig)
class UChaosVDGeneralSettings : public UChaosVDSettingsObjectBase
{
	GENERATED_BODY()

public:
	/** If true, CVD will only load frames that have solver data in them - Only takes effect before loading a file */
	UPROPERTY(Config, EditAnywhere, Category = "Session Data Loading")
	bool bTrimEmptyFrames = true;

	/** How many Game thread frames CVD should queue internally before making them available in the visualization and timeline controls - Only takes effect before loading a file */
	UPROPERTY(Config, EditAnywhere, Category = "Session Data Loading")
	int32 MaxGameThreadFramesToQueueNum = 10;

	/** If true, CVD will only load collision geometry that is visible */
	UPROPERTY(Config, EditAnywhere, Category = "Performance | Streaming")
	bool bStreamingSystemEnabled = true;

	/** Extent size of the box used for calculate what should be streamed in */
	UPROPERTY(Config, EditAnywhere, Category = "Performance | Streaming")
	float StreamingBoxExtentSize = 10000.0f;

	/** If set to true CVD will process any updates to the streaming accel structure in worker threads, in between streaming updates */
	UPROPERTY(Config, EditAnywhere, Category = "Performance | Streaming")
	bool bProcessPendingOperationsQueueInWorkerThread = true;

	/** If set to true CVD will keep the scene outliner up to date as the recording is played. If during the recording a
	 * significant amount of objects are loaded/unloaded, the performance impact will be significant enough to degrade the playback stability. */
	UPROPERTY(Config, EditAnywhere, Category = "Performance | Scene Outliner")
	bool bUpdateSceneOutlinerDuringPlayback = false;

	/** How many times CVD will attempt to connect to a live trace or load from file session if the first attempt failed */
	UPROPERTY(Config, EditAnywhere, Category = Connection)
	int32 MaxConnectionRetries = 14;
};
