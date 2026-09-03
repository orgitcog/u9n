// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"

#include "MetaHumanEditorSettings.generated.h"

DECLARE_MULTICAST_DELEGATE(FMetaHumanEditorSettingsChanged);


UCLASS(Config = EditorPerProjectUserSettings)
class METAHUMANCOREEDITOR_API UMetaHumanEditorSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanEditorSettings();

	//~ UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangeEvent) override;

public:

	// Number of samples when using A/B split window - higher value gives better quality but uses more memory
	UPROPERTY(Config, EditAnywhere, Category = "A/B split", meta = (ClampMin = "1", ClampMax = "8", UIMin = "1", UIMax = "8"))
	int32 SampleCount;

	// Maximum effective resolution of A/B split window
	UPROPERTY(Config, EditAnywhere, Category = "A/B split", meta = (ClampMin = "128", ClampMax = "65536", UIMin = "128", UIMax = "65536"))
	int32 MaximumResolution;

	// If true will force the ingestion process to run sequentially
	UPROPERTY(Config, EditAnywhere, Category = "Capture Source")
	bool bForceSerialIngestion;

	// If true, capture sources from the developers content folder will be shown in the Capture Manager
	UPROPERTY(Config, EditAnywhere, Category = "Capture Source")
	bool bShowDevelopersContent;

	// If true, capture sources from the developers content folder of other users will be shown in the Capture Manager
	UPROPERTY(Config, EditAnywhere, Category = "Capture Source")
	bool bShowOtherDevelopersContent;

	// If true trackers will be loaded when opening identity
	UPROPERTY(Config, EditAnywhere, Category = "Tracking")
	bool bLoadTrackersOnStartup;

	// If true identities will be prepared for performance using a fast, but memory intensive, method. Only applicable to machines with <64Gb of memory
	UPROPERTY(Config)
	bool bTrainSolversFastLowMemory_DEPRECATED;

	// Slots for storing the Performance editor's view setup. A means of saving and recalling A/B modes, display options, open widgets etc 
	UPROPERTY(Config, EditAnywhere, Category = "Performance view setup", DisplayName = "Slot 1")
	TMap<FString, FString> PerformanceViewSetupSlot1;

	UPROPERTY(Config, EditAnywhere, Category = "Performance view setup", DisplayName = "Slot 2")
	TMap<FString, FString> PerformanceViewSetupSlot2;

	UPROPERTY(Config, EditAnywhere, Category = "Performance view setup", DisplayName = "Slot 3")
	TMap<FString, FString> PerformanceViewSetupSlot3;

	UPROPERTY(Config, EditAnywhere, Category = "Performance view setup", DisplayName = "Slot 4")
	TMap<FString, FString> PerformanceViewSetupSlot4;

	// Delegate called when a property changes
	FMetaHumanEditorSettingsChanged OnSettingsChanged;
};
