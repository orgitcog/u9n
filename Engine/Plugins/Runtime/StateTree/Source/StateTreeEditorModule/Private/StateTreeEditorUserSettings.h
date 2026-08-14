// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h" //DeveloperSettings
#include "StateTreeEditorUserSettings.generated.h"

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EStateTreeEditorUserSettingsNodeType : uint8
{
	Condition = 1 << 0,
	Task = 1 << 1,
	Transition = 1 << 2,
	Flag = 1 << 3,
	All = (Condition | Task | Transition | Flag) UMETA(Hidden),
};
ENUM_CLASS_FLAGS(EStateTreeEditorUserSettingsNodeType);

/** User settings for the StateTree editor */
UCLASS(MinimalAPI, config=EditorPerProjectUserSettings, DisplayName="StateTree User Settings")
class UStateTreeEditorUserSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UStateTreeEditorUserSettings() = default;

	EStateTreeEditorUserSettingsNodeType GetStatesViewDisplayNodeType() const
	{
		return StatesViewDisplayNodeType;
	}
	void SetStatesViewDisplayNodeType(EStateTreeEditorUserSettingsNodeType Value);

	float GetStatesViewStateRowHeight() const
	{
		return StatesViewStateRowHeight;
	}
	
	float GetStatesViewNodeRowHeight() const
	{
		return StatesViewNodeRowHeight;
	}

	/** Broadcast when a setting changes. */
	FSimpleMulticastDelegate OnSettingsChanged;

public:
	virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

private:
	/** Which additional node type to display in the States View. */
	UPROPERTY(config, EditAnywhere, Category = "State View", meta = (DisplayName = "Display Node"))
	EStateTreeEditorUserSettingsNodeType StatesViewDisplayNodeType = EStateTreeEditorUserSettingsNodeType::All;
	
	/** Height of a state in the States View. */
	UPROPERTY(config, EditAnywhere, Category = "State View", meta = (DisplayName = "State Height", ClampMin = 8.0f))
	float StatesViewStateRowHeight = 32.0f;
	
	/** Height of a node in the States View. */
	UPROPERTY(config, EditAnywhere, Category = "State View", meta = (DisplayName = "Node Height", ClampMin = 8.0f))
	float StatesViewNodeRowHeight = 16.0f;
};
