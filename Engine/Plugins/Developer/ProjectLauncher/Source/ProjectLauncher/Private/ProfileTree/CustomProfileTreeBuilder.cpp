// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfileTree/CustomProfileTreeBuilder.h"


#define LOCTEXT_NAMESPACE "CustomProfileTreeBuilder"

namespace ProjectLauncher
{
	FCustomProfileTreeBuilder::FCustomProfileTreeBuilder( const ILauncherProfileRef& InProfile, const TSharedRef<FModel>& InModel )
		: FGenericProfileTreeBuilder( InProfile, InModel->GetDefaultCustomLaunchProfile(), InModel )
	{
	}

	void FCustomProfileTreeBuilder::Construct()
	{
		FGenericProfileTreeBuilder::Construct();

		FLaunchProfileTreeNode& GeneralSettingsHeader = TreeData->AddHeading( LOCTEXT("GeneralSettingsHeading", "General Settings") );
		AddProjectProperty(GeneralSettingsHeader);
		AddTargetProperty(GeneralSettingsHeader);
		AddPlatformProperty(GeneralSettingsHeader);
		AddConfigurationProperty(GeneralSettingsHeader);
		AddContentSchemeProperty(GeneralSettingsHeader);

		FLaunchProfileTreeNode& ContentSchemeHeader = TreeData->AddHeading( LOCTEXT("ContentSchemeHeading", "Content Scheme") );
		AddCompressPakFilesProperty(ContentSchemeHeader);
		AddUseIoStoreProperty(ContentSchemeHeader);
		AddGenerateChunksProperty(ContentSchemeHeader);
		AddImportZenSnapshotProperty(ContentSchemeHeader);
		AddZenPakStreamingPathProperty(ContentSchemeHeader);

		FLaunchProfileTreeNode& MapsAndCookingHeader = TreeData->AddHeading( LOCTEXT("CookingHeading", "Maps And Cooking") );
		AddCookProperty(MapsAndCookingHeader);
		AddIncrementalCookProperty(MapsAndCookingHeader);
		AddMapsToCookProperty(MapsAndCookingHeader);
		AddAdditionalCookerOptionsProperty(MapsAndCookingHeader);

		FLaunchProfileTreeNode& BuildHeader = TreeData->AddHeading( LOCTEXT("BuildHeading", "Build") );
		AddBuildProperty(BuildHeader);
		AddForceBuildProperty(BuildHeader);
		AddArchitectureProperty(BuildHeader);

		FLaunchProfileTreeNode& DirectoryHeader = TreeData->AddHeading( LOCTEXT("DirectoryHeading", "Directory") );
		AddArchiveBuildProperty(DirectoryHeader);
		AddArchiveBuildDirectoryProperty(DirectoryHeader);

		FLaunchProfileTreeNode& DeployAndRunHeader = TreeData->AddHeading( LOCTEXT("DeployAndRunHeading", "Deploy And Run") );
		AddDeployProperty(DeployAndRunHeader);
		AddTargetDeviceProperty(DeployAndRunHeader);
		AddRunProperty(DeployAndRunHeader);
		AddInitialMapProperty(DeployAndRunHeader);
		AddCommandLineProperty(DeployAndRunHeader);
	}



	TSharedPtr<ILaunchProfileTreeBuilder> FCustomProfileTreeBuilderFactory::TryCreateTreeBuilder( const ILauncherProfileRef& InProfile, const TSharedRef<FModel>& InModel )
	{
		return MakeShared<FCustomProfileTreeBuilder>(InProfile, InModel);
	}


}

#undef LOCTEXT_NAMESPACE
