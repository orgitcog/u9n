// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanSDKSettings.h"
#include "ObjectTools.h"
#include "Misc/TransactionObjectEvent.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

UMetaHumanCharacterEditorSettings::UMetaHumanCharacterEditorSettings()
{
	SculptManipulatorMesh = FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_SculptTool_Gizmo.SM_SculptTool_Gizmo'"));
	MoveManipulatorMesh = FSoftObjectPath(TEXT("/Script/Engine.StaticMesh'/" UE_PLUGIN_NAME "/Tools/SM_MoveTool_Gizmo.SM_MoveTool_Gizmo'"));

	// Set the initial value of MigratePackagePath to be same as the one defined for cinematic characters in the SDK settings
	const UMetaHumanSDKSettings* MetaHumanSDKSettings = GetDefault<UMetaHumanSDKSettings>();
	MigratedPackagePath = MetaHumanSDKSettings->CinematicImportPath;

	// Add the default and optional template animation data table object paths.
	TemplateAnimationDataTableAssets.Add(FSoftObjectPath(TEXT("/Script/Engine.DataTable'/" UE_PLUGIN_NAME "/Animation/TemplateAnimations/DT_MH_TemplateAnimations.DT_MH_TemplateAnimations'")));
	TemplateAnimationDataTableAssets.Add(FSoftObjectPath(TEXT("/Script/Engine.DataTable'/" UE_PLUGIN_NAME "/Optional/Animation/TemplateAnimations/DT_MH_TemplateAnimations.DT_MH_TemplateAnimations'")));

	DefaultRenderingQualities.Add(EMetaHumanCharacterRenderingQuality::Medium);
	DefaultRenderingQualities.Add(EMetaHumanCharacterRenderingQuality::High);
	DefaultRenderingQualities.Add(EMetaHumanCharacterRenderingQuality::Epic);
}

void UMetaHumanCharacterEditorSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, MigratedPackagePath))
	{
		ObjectTools::SanitizeInvalidCharsInline(MigratedPackagePath.Path, INVALID_LONGPACKAGE_CHARACTERS);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, MigratedNamePrefix))
	{
		ObjectTools::SanitizeInvalidCharsInline(MigratedNamePrefix, INVALID_LONGPACKAGE_CHARACTERS);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, MigratedNameSuffix))
	{
		ObjectTools::SanitizeInvalidCharsInline(MigratedNameSuffix, INVALID_LONGPACKAGE_CHARACTERS);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, bEnableExperimentalWorkflows))
	{
		OnExperimentalAssemblyOptionsStateChanged.ExecuteIfBound();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths))
	{
		ObjectTools::SanitizeInvalidCharsInline(MigratedPackagePath.Path, INVALID_LONGPACKAGE_CHARACTERS);
		OnWardrobePathsChanged.Broadcast();
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, PresetsDirectories)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, PresetsDirectories))
	{
		for (FDirectoryPath& PresetDirectory : PresetsDirectories)
		{
			ObjectTools::SanitizeInvalidCharsInline(PresetDirectory.Path, INVALID_LONGPACKAGE_CHARACTERS);
		}

		OnPresetsDirectoriesChanged.ExecuteIfBound();
	}
}

void UMetaHumanCharacterEditorSettings::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);

	if (InTransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		TArray<FName> PropertiesChanged = InTransactionEvent.GetChangedProperties();

		if (PropertiesChanged.Contains(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacterEditorSettings, WardrobePaths)))
		{
			OnWardrobePathsChanged.Broadcast();
		}
	}
}

FText UMetaHumanCharacterEditorSettings::GetSectionText() const
{
	return LOCTEXT("MetaHumanCharacterEditorSettingsName", "MetaHuman Character");
}

FText UMetaHumanCharacterEditorSettings::GetSectionDescription() const
{
	return LOCTEXT("MetaHumanCharacterEditorSettingsDescription", "Configure the MetaHuman Character Editor plugin");
}

#undef LOCTEXT_NAMESPACE
