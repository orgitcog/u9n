// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassSettings.h"
#include "MassProcessingPhaseManager.h"
#include "MassProcessor.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "StructUtils/InstancedStruct.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "MassEntitySettings.generated.h"


#define GET_MASS_CONFIG_VALUE(a) (GetMutableDefault<UMassEntitySettings>()->a)

struct FPropertyChangedEvent;


/**
 * Implements the settings for MassEntity plugin
 */
UCLASS(MinimalAPI, config = Mass, defaultconfig, DisplayName = "Mass Entity")
class UMassEntitySettings : public UMassModuleSettings
{
	GENERATED_BODY()
public:
#if WITH_EDITORONLY_DATA
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnSettingsChange, const FPropertyChangedEvent& /*PropertyChangedEvent*/);
#endif // WITH_EDITORONLY_DATA
	DECLARE_MULTICAST_DELEGATE(FOnInitialized);

	MASSENTITY_API UMassEntitySettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	MASSENTITY_API void BuildProcessorListAndPhases();
	MASSENTITY_API void AddToActiveProcessorsList(TSubclassOf<UMassProcessor> ProcessorClass);

	MASSENTITY_API TConstArrayView<FMassProcessingPhaseConfig> GetProcessingPhasesConfig();
	const FMassProcessingPhaseConfig& GetProcessingPhaseConfig(const EMassProcessingPhase ProcessingPhase) const { check(ProcessingPhase != EMassProcessingPhase::MAX); return ProcessingPhasesConfig[int(ProcessingPhase)]; }

	static FOnInitialized& GetOnInitializedEvent() { return GET_MASS_CONFIG_VALUE(OnInitializedEvent); }
#if WITH_EDITOR
	FOnSettingsChange& GetOnSettingsChange() { return OnSettingsChange; }

	static bool IsInitialized() { return GET_MASS_CONFIG_VALUE(bInitialized); }

protected:
	MASSENTITY_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	MASSENTITY_API virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	MASSENTITY_API virtual void PostInitProperties() override;
	MASSENTITY_API virtual void BeginDestroy() override;

	MASSENTITY_API void OnPostEngineInit();
	MASSENTITY_API void BuildPhases();
	MASSENTITY_API void BuildProcessorList();

public:
	UPROPERTY(EditDefaultsOnly, Category = Mass, config, AdvancedDisplay)
	int32 ChunkMemorySize = 128 * 1024;

	/**
	 * The name of the file to dump the processor dependency graph. T
	 * The dot file will be put in the project log folder.
	 * To generate a svg out of that file, simply run dot executable with following parameters: -Tsvg -O filename.dot 
	 */
	UPROPERTY(EditDefaultsOnly, Category = Mass, Transient)
	FString DumpDependencyGraphFileName;

	/** Lets users configure processing phases including the composite processor class to be used as a container for the phases' processors. */
	UPROPERTY(EditDefaultsOnly, Category = Mass, config)
	FMassProcessingPhaseConfig ProcessingPhasesConfig[(uint8)EMassProcessingPhase::MAX];

	/** This list contains all the processors available in the given binary (including plugins). The contents are sorted by display name.*/
	UPROPERTY(VisibleAnywhere, Category = Mass, Transient, Instanced, EditFixedSize)
	TArray<TObjectPtr<UMassProcessor>> ProcessorCDOs;

#if WITH_EDITORONLY_DATA
protected:
	FOnSettingsChange OnSettingsChange;
#endif // WITH_EDITORONLY_DATA
	bool bInitialized = false;
	bool bEngineInitialized = false;

	FOnInitialized OnInitializedEvent;
};
