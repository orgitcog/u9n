// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Elements/Interfaces/TypedElementDataStorageInterface.h"

#include "TedsSettingsEditorSubsystem.generated.h"

class FTedsSettingsManager;

UCLASS()
class TEDSSETTINGS_API UTedsSettingsEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:

	UTedsSettingsEditorSubsystem();

	const bool IsEnabled() const;

	DECLARE_MULTICAST_DELEGATE(FOnEnabledChanged)
	FOnEnabledChanged& OnEnabledChanged();

	/** Finds an existing row (may be active or inactive) or adds a new inactive settings section row if no existing row is found. */
	UE::Editor::DataStorage::RowHandle FindOrAddSettingsSection(const FName& ContainerName, const FName& CategoryName, const FName& SectionName);

	/**
	 * Gets the settings section details for the given row.
	 *
	 * @param	Row					The row from which to read settings details.
	 * @param	OutContainerName	Reference to the ContainerName for the given row, only set if this method returns true.
	 * @param	OutCategoryName		Reference to the CategoryName for the given row, only set if this method returns true.
	 * @param	OutSectionName		Reference to the SectionName for the given row, only set if this method returns true.
	 *
	 * @return	True if the settings section details are successfully returned in the out parameters.
	 */
	bool GetSettingsSectionFromRow(UE::Editor::DataStorage::RowHandle Row, FName& OutContainerName, FName& OutCategoryName, FName& OutSectionName);

protected: // USubsystem

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:

	TSharedPtr<FTedsSettingsManager> SettingsManager;
	FOnEnabledChanged EnabledChangedDelegate;

};
