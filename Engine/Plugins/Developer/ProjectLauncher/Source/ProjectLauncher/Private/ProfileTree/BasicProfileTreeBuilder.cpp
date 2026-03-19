// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfileTree/BasicProfileTreeBuilder.h"

#define LOCTEXT_NAMESPACE "BasicProfileTreeBuilder"

namespace ProjectLauncher
{
	FBasicProfileTreeBuilder::FBasicProfileTreeBuilder( const ILauncherProfileRef& InProfile, const TSharedRef<FModel>& InModel )
		: FGenericProfileTreeBuilder( InProfile, InModel->GetDefaultBasicLaunchProfile(), InModel )
	{
	}


	void FBasicProfileTreeBuilder::Construct()
	{
		FGenericProfileTreeBuilder::Construct();

		FLaunchProfileTreeNode& GeneralSettingsHeader = TreeData->AddHeading( LOCTEXT("GeneralSettingsHeading", "General Settings") );
		AddProjectProperty(GeneralSettingsHeader);
		AddTargetProperty(GeneralSettingsHeader);
		AddConfigurationProperty(GeneralSettingsHeader);
		AddContentSchemeProperty(GeneralSettingsHeader);
		AddTargetDeviceProperty(GeneralSettingsHeader);
		AddCommandLineProperty(GeneralSettingsHeader);
	}



	TSharedPtr<ILaunchProfileTreeBuilder> FBasicProfileTreeBuilderFactory::TryCreateTreeBuilder( const ILauncherProfileRef& InProfile, const TSharedRef<FModel>& InModel )
	{
		return MakeShared<FBasicProfileTreeBuilder>(InProfile, InModel);
	}


}

#undef LOCTEXT_NAMESPACE
