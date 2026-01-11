// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "TakePresetSettings.generated.h"

class ULevelSequence;

/** Wraps the class so this can be customized by a type layout. */
USTRUCT()
struct FTakeRecorderTargetRecordClassProperty
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Take Recorder")
	TSoftClassPtr<ULevelSequence> TargetRecordClass;
};

/**
 * Settings for how to assemble UTakePreset.
 * 
 * These settings belong into UTakeRecorderSettings but those are in TakeRecorder, which depends on TakesCore. 
 * DisplayName is important so these settings are displayed in the same category as UTakeRecorderSettings.
 */
UCLASS(config=EditorSettings, BlueprintType, DisplayName = "Take Recorder")
class TAKESCORE_API UTakePresetSettings : public UObject
{
public:
	GENERATED_BODY()

	UTakePresetSettings();

	/** @return The settings object */
	static UTakePresetSettings* Get();

	/** @return The class that recorded sequences should have */
	UClass* GetTargetRecordClass() const;

	DECLARE_MULTICAST_DELEGATE(FOnSettingsChanged);
	/** Invoked when any settings change. */
	FOnSettingsChanged& OnSettingsChanged() { return OnSettingsChangedDelegate; }

	static FName GetTargetRecordClassMemberName() { return GET_MEMBER_NAME_CHECKED(UTakePresetSettings, TargetRecordClass); } 

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	
	/** The class that recorded sequences should have. */
	UPROPERTY(config, EditAnywhere, Category = "Take Recorder")
	FTakeRecorderTargetRecordClassProperty TargetRecordClass;

	/** Invoked when any settings change. */
	FOnSettingsChanged OnSettingsChangedDelegate;
};
