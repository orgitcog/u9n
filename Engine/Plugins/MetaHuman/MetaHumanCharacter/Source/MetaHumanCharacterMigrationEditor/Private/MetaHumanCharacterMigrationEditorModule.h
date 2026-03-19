// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Misc/NotNull.h"

enum class EMetaHumanCharacterMigrationAction : uint8;
enum class EMetaHumanMigrationDataAssetType : uint8;

namespace UE::MetaHuman
{
	class FSourceMetaHuman;
	class FInstalledMetaHuman;
}

class FMetaHumanCharacterMigrationEditorModule final : public IModuleInterface
{
public:

	//~Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface interface

private:

	bool ShouldMigrate() const;
	bool ShouldImport() const;

	/** Called when an import operation is started from Bridge. If UMetaHumanCharacterEditorSettings::MigrateAction is set to Prompt, asks the users which action to perform */
	bool OnMetaHumanImportStarted(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman);

	/** Called for each asset of file to be imported. If only migrating the legacy MetaHuman do not import anything */
	bool OnShouldImportMetaHumanAssetOrFile(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman, const class FString& InDestPath, bool bInIsFile);

	/** Create a MetaHuman Character from a bridge import */
	void MigrateMetaHuman(const UE::MetaHuman::FSourceMetaHuman& InSourceMetaHuman);

	/** Sets the character skin parameters from the migration info struct and commits the changes using the MetaHumanCharacter subsystem */
	void SetSkin(TNotNull<class UMetaHumanCharacter*> InCharacter, const struct FMetaHumanMigrationInfo& InMigrationInfo);

	/** Sets the character makeup parameters from the migration info and commits the changes using UMetaHumanCharacterEditorSubsystem */
	void SetMakeup(TNotNull<class UMetaHumanCharacter*> InCharacter, const struct FMetaHumanMigrationInfo& InMigrationInfo);

	/** Sets the character eye parameters from the migration info and commits the changes using UMetaHumanCharacterEditorSubsystem */
	void SetEyes(TNotNull<class UMetaHumanCharacter*> InCharacter, const struct FMetaHumanMigrationInfo& InMigrationInfo);

	/** Sets the character grooms in its palette from the migration info. Appends the instance parameter names of valid grooms in OutInstanceParameters */
	void SetGrooms(
		TNotNull<class UMetaHumanCharacter*> InCharacter,
		const struct FMetaHumanMigrationInfo& InMigrationInfo,
		TMap<EMetaHumanMigrationDataAssetType, struct FMetaHumanPipelineSlotSelection>& OutSlotSelections);

	/** Assigns wardrobe items to the character and updates the material property values from the migration info data */
	void UpdateWardrobe(
		TNotNull<class UMetaHumanCharacter*> InCharacter,
		const struct FMetaHumanMigrationInfo& InMigrationInfo,
		const TMap<EMetaHumanMigrationDataAssetType, struct FMetaHumanPipelineSlotSelection>& InSlotSelections);

	/** Logs warnings or errors in the MessageLog. It also sets the bHasWarnings or bHasErrors flags for further tracking */
	void LogWarning(const FText& InMessage);
	void LogError(const FText& InMessage);

private:

	// Pointer to the current active message log
	class FMessageLog* MessageLogPtr = nullptr;

	// Keep track of whether errors or warnings were raised during the migration process
	bool bHasErrors = false;
	bool bHasWarnings = false;

	// Which migration action to take. Used internally to persist the value between function calls
	EMetaHumanCharacterMigrationAction MigrateActionInternal;
};