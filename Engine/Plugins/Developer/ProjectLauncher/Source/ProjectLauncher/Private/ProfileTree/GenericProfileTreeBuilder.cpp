// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProfileTree/GenericProfileTreeBuilder.h"
#include "Widgets/Shared/SCustomLaunchProjectCombo.h"
#include "Widgets/Shared/SCustomLaunchBuildTargetCombo.h"
#include "Widgets/Shared/SCustomLaunchPlatformCombo.h"
#include "Widgets/Shared/SCustomLaunchContentSchemeCombo.h"
#include "Widgets/Shared/SCustomLaunchDeviceListView.h"
#include "Widgets/Shared/SCustomLaunchMapListView.h"
#include "Widgets/Shared/SCustomLaunchCombo.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "SResizeBox.h"
#include "SSearchableComboBox.h"
#include "PlatformInfo.h"
#include "IDesktopPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"
#include "GameProjectHelper.h"
#include "InstalledPlatformInfo.h"



#define LOCTEXT_NAMESPACE "CustomProfileTreeBuilder"


namespace ProjectLauncher
{
	FGenericProfileTreeBuilder::FGenericProfileTreeBuilder( const ILauncherProfileRef& InProfile, const ILauncherProfileRef& InDefaultProfile, const TSharedRef<FModel>& InModel )
		: TreeData( MakeShared<FLaunchProfileTreeData>(InProfile, InModel, this) )
		, Profile(InProfile)
		, DefaultProfile(InDefaultProfile)
		, Model(InModel)
	{
		ProfileType = Model->GetProfileType(Profile);

		ForPak = [this]()
		{
			return (ContentScheme == EContentScheme::PakFiles || ContentScheme == EContentScheme::DevelopmentPackage);
		};

		ForZenWS = [this]()
		{
			return (ContentScheme == EContentScheme::ZenPakStreaming);
		};

		ForCooked = [this]()
		{
			return (ContentScheme != EContentScheme::ZenPakStreaming && ContentScheme != EContentScheme::CookOnTheFly);
		};

		ForEnabledCooked = [this]()
		{
			return (bShouldCook && ContentScheme != EContentScheme::ZenPakStreaming && ContentScheme != EContentScheme::CookOnTheFly);
		};

		ForContent = [this]()
		{
			return (ContentScheme != EContentScheme::ZenPakStreaming);
		};

		ForCode = [this]()
		{
			return GetBuild();
		};

		ForDeployment = [this]()
		{
			return GetDeployToDevice();
		};

		ForRun = [this]()
		{
			return GetIsRunning();
		};

		EmptyString = []()
		{
			return FString();
		};

	}

	void FGenericProfileTreeBuilder::Construct()
	{
		const TArray<FString>& DeviceIDs = Profile->GetDeployedDeviceGroup()->GetDeviceIDs();

		MapOption = Profile->GetCookedMaps().Num() > 0 ? EMapOption::Selected : EMapOption::Startup;
		DeployDeviceOption = DeviceIDs.Num() > 0 && !DeviceIDs[0].IsEmpty() ? EDeployDeviceOption::Selected : EDeployDeviceOption::Default;
		bShouldCook = GetCook();
		ContentScheme = Model->DetermineProfileContentScheme(Profile);
		CachedBuildTargetType = Model->GetBuildTargetInfo(Profile).Type;
	
		CacheArchitectures();
	}









	void FGenericProfileTreeBuilder::AddProjectProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		if (ProfileType == EProfileType::Basic)
		{
			// this is adding a new widget to the property tree
			// - the first entry is the name of the property on the left-hand side
			// - the seecond parameter is the widget itself that appears on the right-hand side
			HeadingNode.AddWidget( LOCTEXT("ProjectLabel", "Project"), 
				SNew(SCustomLaunchProjectCombo)
				.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetProjectName)
				.SelectedProject(this, &FGenericProfileTreeBuilder::GetProjectPath)
				.HasProject(this, &FGenericProfileTreeBuilder::HasProject)
				.CurrentProjectOption(SCustomLaunchProjectCombo::ECurrentProjectOption::Empty)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			);
		}
		else
		{
			HeadingNode.AddWidget( LOCTEXT("ProjectLabel", "Project"), 
				SNew(SCustomLaunchProjectCombo)
				.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetProjectName)
				.SelectedProject(this, &FGenericProfileTreeBuilder::GetProjectPath)
				.HasProject(this, &FGenericProfileTreeBuilder::HasProject)
				.ShowAnyProjectOption(true)
				.CurrentProjectOption(SCustomLaunchProjectCombo::ECurrentProjectOption::ActualProject)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			);
		}
	}

	void FGenericProfileTreeBuilder::AddTargetProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		if (ProfileType == EProfileType::Basic)
		{
			// this is also adding a new widget to the property tree, as above.
			// in this example the new struct parameter defines several callbacks that handle the 'reset to default' functionality. there are also options for disabling & hiding.
			// the code is implemented with this syntax to avoid aid readability without using slate's TAttribute style functionality which seemed like an overkill for our simpler needs.
			HeadingNode.AddWidget( LOCTEXT("TargetLabel", "Target"), 
				{
					.IsDefault = [this]()		{ return !Profile->HasBuildTargetSpecified() || Profile->GetBuildTarget().IsEmpty(); },
					.SetToDefault = [this]()	{ SetBuildTarget(FString()); },
				},
				SNew(SCustomLaunchBuildTargetCombo)
				.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetBuildTarget)
				.SelectedBuildTarget(this, &FGenericProfileTreeBuilder::GetBuildTarget)
				.SelectedProject(this, &FGenericProfileTreeBuilder::GetProjectPath)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			);
		}
		else
		{
			HeadingNode.AddWidget( LOCTEXT("TargetLabel", "Target"), 
				{
					.IsDefault = [this]()		{ return !Profile->HasBuildTargetSpecified() || Profile->GetBuildTarget().IsEmpty(); },
					.SetToDefault = [this]()	{ SetBuildTarget(FString()); },
					.IsEnabled = [this]()		{ return Profile->HasProjectSpecified(); },
				},
				SNew(SCustomLaunchBuildTargetCombo)
				.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetBuildTarget)
				.SelectedBuildTarget(this, &FGenericProfileTreeBuilder::GetBuildTarget)
				.SupportedTargetTypes(this, &FGenericProfileTreeBuilder::GetSupportedBuildTargetTypes)
				.SelectedProject(this, &FGenericProfileTreeBuilder::GetProjectPath)
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			);
		}
	}

	void FGenericProfileTreeBuilder::AddPlatformProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddWidget( LOCTEXT("PlatformLabel", "Platform"), 
			SNew(SCustomLaunchPlatformCombo)
			.SelectedPlatforms(this, &FGenericProfileTreeBuilder::GetSelectedPlatforms)
			.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetSelectedPlatforms)
			.BasicPlatformsOnly(bUseFriendlyBuildTargetSelection)
			.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
		);
	}

	void FGenericProfileTreeBuilder::AddConfigurationProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		TArray<EBuildConfiguration> ValidConfigurations;
		
		static const EBuildConfiguration AllConfigurations[] = { EBuildConfiguration::Debug, EBuildConfiguration::DebugGame, EBuildConfiguration::Development, EBuildConfiguration::Test, EBuildConfiguration::Shipping };
		for (EBuildConfiguration Configuration : AllConfigurations)
		{
			// only show the configurations that are currently available. @todo: might be better to show all, but disable the ones that are unavailale
			if (FInstalledPlatformInfo::Get().IsValidConfiguration(Configuration))
			{
				ValidConfigurations.Add(Configuration);
			}
		}

		HeadingNode.AddWidget( LOCTEXT("ConfigurationLabel", "Configuration"), 
			SNew(SCustomLaunchLexToStringCombo<EBuildConfiguration>)
			.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetBuildConfiguration)
			.SelectedItem(this, &FGenericProfileTreeBuilder::GetBuildConfiguration)
			.Items(ValidConfigurations)
		);
	}

	void FGenericProfileTreeBuilder::AddContentSchemeProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddWidget( LOCTEXT("ContentSchemeLabel", "Content Scheme"), 
			SNew(SCustomLaunchContentSchemeCombo)
			.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetContentScheme)
			.SelectedContentScheme_Lambda( [this]() { return ContentScheme; } )
			.IsContentSchemeAvailable(this, &FGenericProfileTreeBuilder::IsContentSchemeAvailable)
			.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
		);
	}

	void FGenericProfileTreeBuilder::AddCompressPakFilesProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		// in this example we are adding a single boolean instead of a custom widget. the struct parameter defines how the value is accessed
		// the ForPak is the lambda functions created in the constructor and is again aimed at readability
		HeadingNode.AddBoolean( LOCTEXT("CompressPakFilesLabel", "Compress Pak Files"),
			{
				.GetValue = [this]()			{ return Profile->IsCompressed(); },
				.SetValue = [this](bool bValue)	{ Profile->SetCompressed(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsCompressed(); },
				.IsVisible = ForPak,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddUseIoStoreProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("UseIoStoreLabel", "Use Io Store"),
			{
				.GetValue = [this]()			{ return Profile->IsUsingIoStore(); },
				.SetValue = [this](bool bValue)	{ Profile->SetUseIoStore(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsUsingIoStore(); },
				.IsVisible = ForPak,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddGenerateChunksProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("GenerateChunksLabel", "Generate Chunks"),
			{
				.GetValue = [this]()			{ return Profile->IsGeneratingChunks(); },
				.SetValue = [this](bool bValue)	{ Profile->SetGenerateChunks(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsGeneratingChunks(); },
				.IsVisible = ForPak,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddImportZenSnapshotProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("ImportZenSnapshotLabel", "Import Best Match Zen Snapshot"),
			{
				.GetValue = [this]()			{ return Profile->IsImportingZenSnapshot(); },
				.SetValue = [this](bool bValue)	{ Profile->SetImportingZenSnapshot(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsImportingZenSnapshot(); },
				.IsVisible = ForContent,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddZenPakStreamingPathProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddDirectoryString( LOCTEXT("ZenPakStreamingPathLabel", "Zen Pak Streaming Path"),
			{
				.GetValue = [this]()				{ return Profile->GetZenPakStreamingPath(); },
				.SetValue = [this](FString Value)	{ Profile->SetZenPakStreamingPath(Value); },
				.GetDefaultValue = EmptyString,
				.IsVisible = ForZenWS,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddIncrementalCookProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		auto GetDisplayName = []( ELauncherProfileIncrementalCookMode::Type Mode)
		{
			switch (Mode)
			{
				case ELauncherProfileIncrementalCookMode::None:                    return LOCTEXT("IncrementalCookNone",                 "None");
				case ELauncherProfileIncrementalCookMode::ModifiedOnly:            return LOCTEXT("IncrementalCookModified",             "Modified Only (legacy)");
				case ELauncherProfileIncrementalCookMode::ModifiedAndDependencies: return LOCTEXT("IncrementalCookModifiedDependencies", "Modified & Dependencies (recommended)");
			}
			return FText::GetEmpty();
		};

		auto GetToolTip = []( ELauncherProfileIncrementalCookMode::Type Mode)
		{
			switch (Mode)
			{
				case ELauncherProfileIncrementalCookMode::None:                    return LOCTEXT("IncrementalCookNoneTip",                 "This will try to cook everything");
				case ELauncherProfileIncrementalCookMode::ModifiedOnly:            return LOCTEXT("IncrementalCookModifiedTip",             "This will only try to cook any modified assets but won't try to cook anything that depends on these assets. This is the old, legacy option and is faster but unreliable");
				case ELauncherProfileIncrementalCookMode::ModifiedAndDependencies: return LOCTEXT("IncrementalCookModifiedDependenciesTip", "This will try to cook any modified assets and those assets that depend on them. This is the new method and is much more reliable but is a little slower");
			}
			return FText::GetEmpty();
		};

		HeadingNode.AddWidget( LOCTEXT("IncrementalCookLabel", "Incremental Cook"),
			{
				.IsDefault = [this]()		{ return GetIncrementalCookMode() == DefaultProfile->GetIncrementalCookMode(); },
				.SetToDefault = [this]()	{ SetIncrementalCookMode(DefaultProfile->GetIncrementalCookMode()); },
				.IsVisible = ForEnabledCooked,
			},
			SNew(SCustomLaunchCombo<ELauncherProfileIncrementalCookMode::Type>)
			.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetIncrementalCookMode)
			.SelectedItem(this, &FGenericProfileTreeBuilder::GetIncrementalCookMode)
			.GetDisplayName_Lambda(GetDisplayName)
			.GetItemToolTip_Lambda(GetToolTip)
			.Items( TArray<ELauncherProfileIncrementalCookMode::Type>(
				{
					ELauncherProfileIncrementalCookMode::None,
					ELauncherProfileIncrementalCookMode::ModifiedOnly,
					ELauncherProfileIncrementalCookMode::ModifiedAndDependencies,
				})
			)
		);
	}



	void FGenericProfileTreeBuilder::AddCookProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("CookLabel", "Cook Content"),
			{
				.GetValue = [this]()			{ return GetCook(); },
				.SetValue = [this](bool bValue)	{ SetCook(bValue); },
				.GetDefaultValue = [this]()		{ return GetCook(DefaultProfile); },
				.IsVisible = ForCooked
			}
		);
	}

	void FGenericProfileTreeBuilder::AddMapsToCookProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddWidget( LOCTEXT("MapsToCookLabel", "Maps To Cook"), 
			{
				.IsVisible = ForEnabledCooked,
			},
			CreateMapListWidget()
		);
	}

	void FGenericProfileTreeBuilder::AddAdditionalCookerOptionsProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddString( LOCTEXT("AdditionalCookerOptionsLabel", "Additional Cooker Options"), 
			{
				.GetValue = [this]()				{ return Profile->GetCookOptions(); },
				.SetValue = [this](FString Value)	{ Profile->SetCookOptions( Value ); },
				.GetDefaultValue = EmptyString,
				.IsVisible = ForEnabledCooked,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddBuildProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("BuildLabel", "Build the game"),
			{
				.GetValue = [this]()			{ return GetBuild(); },
				.SetValue = [this](bool bValue)	{ SetBuild(bValue); },
				.GetDefaultValue = [this]()		{ return GetBuild(DefaultProfile); },
			}
		);
	}

	void FGenericProfileTreeBuilder::AddForceBuildProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("ForceBuildLabel", "Build even if a pre-built target exists"),
			{
				.GetValue = [this]()			{ return GetForceBuild(); },
				.SetValue = [this](bool bValue)	{ SetForceBuild(bValue); },
				.GetDefaultValue = [this]()		{ return GetForceBuild(DefaultProfile); },
				.IsEnabled = ForCode,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddBuidUATProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("BuildUATLabel", "Build UAT"),
			{
				.GetValue = [this]()			{ return Profile->IsBuildingUAT(); },
				.SetValue = [this](bool bValue)	{ Profile->SetBuildUAT(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsBuildingUAT(); },
				.IsEnabled = ForCode,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddArchitectureProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddWidget( LOCTEXT("ArchitectureLabel", "Architecture"),
			{
				.IsDefault = [this]()		{ return GetArchitecture().IsEmpty(); },
				.SetToDefault = [this]()	{ SetArchitecture(FString()); },
				.IsVisible = [this]()       { return CachedArchitectures.Num() > 0; }
			},
			SNew(SCustomLaunchStringCombo)
			.OnSelectionChanged( this, &FGenericProfileTreeBuilder::SetArchitecture )
			.SelectedItem( this, &FGenericProfileTreeBuilder::GetArchitecture)
			.GetDisplayName( this, &FGenericProfileTreeBuilder::GetArchitectureDisplayName )
			.Items_Lambda( [this]() { return CachedArchitectures;} )
		);
	}




	void FGenericProfileTreeBuilder::AddStagingDirectoryProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddDirectoryString( LOCTEXT("CustomStagingPathLabel", "Custom Stage Directory"),
			{
				.GetValue = [this]()				{ return Profile->GetPackageDirectory(); },
				.SetValue = [this](FString Value)	{ Profile->SetPackageDirectory(Value); },
				.GetDefaultValue = EmptyString,
				.IsVisible = ForCooked,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddArchiveBuildProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("ArchiveBuildLabel", "Archive Build"),
			{
				.GetValue = [this]()			{ return Profile->IsArchiving(); },
				.SetValue = [this](bool bValue)	{ Profile->SetArchive(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsArchiving(); },
				.IsVisible = ForCooked,
			}
		);
	}


	void FGenericProfileTreeBuilder::AddArchiveBuildDirectoryProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddDirectoryString( LOCTEXT("ArchivePathLabel", "Archive Directory"),
			{
				.GetValue = [this]()				{ return Profile->GetArchiveDirectory(); },
				.SetValue = [this](FString Value)	{ Profile->SetArchiveDirectory(Value); },
				.GetDefaultValue = EmptyString,
				.IsVisible = ForCooked,				
				.IsEnabled = [this]()				{ return Profile->IsArchiving(); }
			}
		);
	}



	void FGenericProfileTreeBuilder::AddDeployProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("DeployLabel", "Deploy To Device"),
			{
				.GetValue = [this]()			{ return GetDeployToDevice(); },
				.SetValue = [this](bool bValue)	{ SetDeployToDevice(bValue); },
				.GetDefaultValue = [this]()		{ return GetDeployToDevice(DefaultProfile); },
				.IsVisible = [this]()			{ return (ContentScheme != EContentScheme::CookOnTheFly); }
			}
		);
	}

	void FGenericProfileTreeBuilder::AddIncrementalDeployProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("DeployModifiedLabel", "Only Deploy Modified Content"),
			{
				.GetValue = [this]()			{ return Profile->IsDeployingIncrementally(); },
				.SetValue = [this](bool bValue)	{ Profile->SetIncrementalDeploying(bValue); },
				.GetDefaultValue = [this]()		{ return DefaultProfile->IsDeployingIncrementally(); },
				.IsEnabled = ForDeployment,
			}
		);
	}

	void FGenericProfileTreeBuilder::AddTargetDeviceProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddWidget( LOCTEXT("TargetDeviceLabel", "Target Device"), 
			{
				.IsEnabled = ForDeployment,
			},
			CreateDeployDeviceWidget()
		);
	}


	void FGenericProfileTreeBuilder::AddRunProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddBoolean( LOCTEXT("RunLabel", "Run"),
			{
				.GetValue = [this]()			{ return GetIsRunning(); },
				.SetValue = [this](bool bValue)	{ SetIsRunning(bValue); },
				.GetDefaultValue = [this]()		{ return GetIsRunning(DefaultProfile); },
			}
		);
	}


	void FGenericProfileTreeBuilder::AddInitialMapProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		// todo: custom map picker 
		HeadingNode.AddWidget( LOCTEXT("InitialMapLabel", "Initial Map"), 
			{
				.IsDefault = [this]()		{ return Profile->GetDefaultLaunchRole()->GetInitialMap().IsEmpty(); },
				.SetToDefault = [this]()	{ Profile->GetDefaultLaunchRole()->SetInitialMap(FString()); InitalMapCombo->SetSelectedItem(GetInitialMap()); },
				.IsEnabled = ForRun,
			},
			SAssignNew(InitalMapCombo, SSearchableComboBox)
			.OptionsSource(&CachedStartupMaps)
			.OnSelectionChanged( this, &FGenericProfileTreeBuilder::OnInitialMapChanged )
			.OnGenerateWidget( this, &FGenericProfileTreeBuilder::OnGenerateComboWidget )
			.OnComboBoxOpening( this, &FGenericProfileTreeBuilder::CacheStartupMapList )
			.Content()
			[
				SNew(STextBlock)
				.Text_Lambda( [this]() { return FText::FromString( *Profile->GetDefaultLaunchRole()->GetInitialMap() ); } )
				.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
			]
		);
	}


	void FGenericProfileTreeBuilder::AddCommandLineProperty( FLaunchProfileTreeNode& HeadingNode )
	{
		HeadingNode.AddCommandLineString( LOCTEXT("CommandLineLabel", "Additional Command Line"), 
			{
				.GetValue = [this]()				{ return GetCommandLine(); },
				.SetValue = [this](FString Value)	{ SetCommandLine(Value); },
				.GetDefaultValue = EmptyString,
				.IsEnabled = ForRun
			}
		);
	}























	void FGenericProfileTreeBuilder::OnPropertyChanged()
	{
		if (ProfileType == EProfileType::Basic)
		{
			// do not save basic profiles - they're transient
		}
		else
		{
			Model->GetProfileManager()->SaveJSONProfile(Profile);
		}

		TreeData->RequestTreeRefresh();
	}


	void FGenericProfileTreeBuilder::CacheStartupMapList() const
	{
		if (!bStartupMapCacheDirty)
		{
			return;
		}
		bStartupMapCacheDirty = false;

		CachedStartupMaps.Reset();
		CachedStartupMaps.Add(MakeShared<FString>());
		for ( const FString& Map : Model->GetAvailableProjectMapNames(Profile->GetProjectBasePath()))
		{
			CachedStartupMaps.Add(MakeShared<FString>(Map));
		}

		if (InitalMapCombo.IsValid())
		{
			InitalMapCombo->SetSelectedItem( GetInitialMap(), ESelectInfo::Direct );
			InitalMapCombo->RefreshOptions();
		}
	}

	void FGenericProfileTreeBuilder::CacheArchitectures()
	{
		CachedArchitectures.Reset();

		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = Model->GetPlatformInfo(Profile);
		if (PlatformInfo != nullptr)
		{
			const ITargetPlatform* TargetPlatform = GetTargetPlatformManager()->FindTargetPlatform(PlatformInfo->IniPlatformName);
			if (TargetPlatform)
			{
				TargetPlatform->GetPossibleArchitectures(CachedArchitectures);
				if (CachedArchitectures.Num() > 0)
				{
					CachedArchitectures.Insert(FString(), 0); // empty string for "project default" option
				}
			}
		}
	}



	TSharedRef<SWidget> FGenericProfileTreeBuilder::OnGenerateComboWidget( TSharedPtr<FString> InComboString )
	{
		return SNew(STextBlock)
			.Text(InComboString.IsValid() ? FText::FromString(*InComboString) : FText::GetEmpty() )
			.Font(FCoreStyle::Get().GetFontStyle("SmallFont"));

	}



	void FGenericProfileTreeBuilder::SetSelectedPlatforms( TArray<FString> SelectedPlatforms )
	{
		Profile->ClearCookedPlatforms();

		if (bUseFriendlyBuildTargetSelection)
		{
			FTargetInfo BuildTargetInfo = FModel::GetBuildTargetInfo(GetBuildTarget(), GetProjectPath());

			for (const FString& Platform : SelectedPlatforms)
			{
				Profile->AddCookedPlatform( FModel::GetBuildTargetPlatformName(Platform, BuildTargetInfo));
			}
		}
		else
		{
			for (const FString& Platform : SelectedPlatforms)
			{
				Profile->AddCookedPlatform(Platform);
			}
		}
		OnPropertyChanged();

		DeployDeviceListView->OnSelectedPlatformChanged();

		CacheArchitectures();
	}

	TArray<FString> FGenericProfileTreeBuilder::GetSelectedPlatforms() const
	{
		TArray<FString> Platforms;

		if (bUseFriendlyBuildTargetSelection)
		{
			for (const FString& Platform : Profile->GetCookedPlatforms())
			{
				Platforms.Add(FModel::GetVanillaPlatformName(Platform));
			}
		}
		else
		{
			Platforms = Profile->GetCookedPlatforms();
		}

		return MoveTemp(Platforms);
	}


	FString FGenericProfileTreeBuilder::GetProjectPath() const
	{
		if (ProfileType == EProfileType::Basic)
		{
			if (Profile->HasProjectSpecified())
			{
				return Profile->GetProjectPath();
			}
			else
			{
				return Model->GetProfileManager()->GetProjectPath();
			}
		}
		else
		{
			if (Profile->HasProjectSpecified())
			{
				return Profile->GetProjectPath();
			}

			return FString();
		}
	}

	void FGenericProfileTreeBuilder::SetProjectName(FString ProjectPath)
	{
		Profile->SetProjectSpecified(!ProjectPath.IsEmpty());
		Profile->SetProjectPath(ProjectPath);

		if (bUseFriendlyBuildTargetSelection && ProfileType != EProfileType::Basic )
		{
			Model->UpdateCookedPlatformsFromBuildTarget(Profile);
		}

		OnPropertyChanged();

		if (MapListView.IsValid())
		{
			MapListView->RefreshMapList();
		}

		bStartupMapCacheDirty = true;
	}

	bool FGenericProfileTreeBuilder::HasProject() const
	{
		return Profile->HasProjectSpecified();
	}


	FString FGenericProfileTreeBuilder::GetBuildTarget() const
	{
		if (ProfileType == EProfileType::Basic)
		{
			if (Profile->HasBuildTargetSpecified())
			{
				return Profile->GetBuildTarget();
			}
			else
			{
				return Model->GetProfileManager()->GetBuildTarget();
			}
		}
		else
		{
			return Profile->GetBuildTarget();
		}
	}

	void FGenericProfileTreeBuilder::SetBuildTarget(FString BuildTarget)
	{
		Profile->SetBuildTargetSpecified(!BuildTarget.IsEmpty());
		Profile->SetBuildTarget(BuildTarget);
	
		if (ProfileType == EProfileType::Basic )
		{
			Model->UpdatedCookedPlatformsFromDeployDeviceProxy(Profile);
		}
		else if (bUseFriendlyBuildTargetSelection)
		{
			Model->UpdateCookedPlatformsFromBuildTarget(Profile);
		}

		CachedBuildTargetType = Model->GetBuildTargetInfo(Profile).Type;
		OnPropertyChanged();
	}

	TArray<EBuildTargetType> FGenericProfileTreeBuilder::GetSupportedBuildTargetTypes() const
	{
		TArray<EBuildTargetType> Result;

		if (Profile->GetCookedPlatforms().Num() > 0)
		{
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(FName(Profile->GetCookedPlatforms()[0]));
			if (PlatformInfo)
			{
				if (bUseFriendlyBuildTargetSelection)
				{
					Result.Add(PlatformInfo->VanillaInfo->PlatformType);

					for (const PlatformInfo::FTargetPlatformInfo* PlatformFlavorInfo : PlatformInfo->VanillaInfo->Flavors)
					{
						Result.AddUnique(PlatformFlavorInfo->PlatformType);
					}
				}
				else
				{
					Result.Add(PlatformInfo->PlatformType);
				}
			}
		}

		return MoveTemp(Result);
	}

	void FGenericProfileTreeBuilder::SetBuildConfiguration(EBuildConfiguration BuildConfiguration)
	{
		Profile->SetBuildConfiguration(BuildConfiguration);
		OnPropertyChanged();
	}

	EBuildConfiguration FGenericProfileTreeBuilder::GetBuildConfiguration() const
	{
		return Profile->GetBuildConfiguration();
	}



	void FGenericProfileTreeBuilder::RefreshContentScheme()
	{
		EContentScheme CurrentContentScheme = Model->DetermineProfileContentScheme(Profile);
		SetContentScheme(CurrentContentScheme);
	}

	void FGenericProfileTreeBuilder::SetContentScheme(EContentScheme InContentScheme)
	{
		ELauncherProfileDeploymentModes::Type DeploymentMode = GetDeployToDevice() ? ELauncherProfileDeploymentModes::CopyToDevice : ELauncherProfileDeploymentModes::DoNotDeploy;
		Model->SetProfileContentScheme(InContentScheme, Profile, bShouldCook, DeploymentMode );
		OnPropertyChanged();

		// refresh the cached content scheme in case the option that was selected is not available
		ContentScheme = Model->DetermineProfileContentScheme(Profile);
	}

	bool FGenericProfileTreeBuilder::IsContentSchemeAvailable(EContentScheme InContentScheme, FText& OutReason) const
	{
		FProjectSettings ProjectSettings = Model->GetProjectSettings(Profile);

		// basic launch is aimed at launching the current content, not an external build
		if (ProfileType == EProfileType::Basic && InContentScheme == EContentScheme::ZenPakStreaming)
		{
			// don't set a reason - just hide the item for Basic Launch
			return false;
		}

		// loose files can't be selected if the project is using zen store because there's no way to opt-out of Zen Store from the UAT command line.
		if (InContentScheme == EContentScheme::LooseFiles && ProjectSettings.bUseZenStore)
		{
			OutReason = LOCTEXT("NoLooseFilesReason", "Loose Files cannot be used when Zen Store is enabled in Project Settings");
			return false;
		}

		// don't show zen pak streaming option if it isn't going to be set up automatically by UAT
		// @todo: could potentially look at shelling out to Zen & querying if we have a dynamic workspace? no support for async config queries in this tool yet though
		if (InContentScheme == EContentScheme::ZenPakStreaming && !ProjectSettings.bHasAutomaticZenPakStreamingWorkspaceCreation)
		{
			OutReason = LOCTEXT("NoZenPakReason", "Automatic Zen Pak streaming workspace creation has not been enabled in Project Settings");
			return false;
		}
	
		// cannot launch via Zen if we're targeting a remote device and Zen isn't accepting external connections
		if ((InContentScheme == EContentScheme::ZenStreaming || InContentScheme == EContentScheme::ZenPakStreaming) 
			&& !ProjectSettings.bAllowRemoteNetworkService && !Model->IsHostPlatform(Profile))
		{
			OutReason = LOCTEXT("NoZenReason", "Zen Streaming to a remote device requires AllowRemoteNetworkService");
			return false;
		}

		return true;
	}



	FString FGenericProfileTreeBuilder::GetCommandLine() const
	{
		// get unified command line from these two fields. the first is presented in "Build" for old Project Launcher, and the latter is presented in "Launch" for old Project Launcher)
		// when we save back to the profile, this will be stored just in the "Build" one for clarity (because multiple roles are not supported in old or new project launcher)
		FString CommandLine = Profile->GetAdditionalCommandLineParameters().TrimStartAndEnd() + TEXT(" ") + Profile->GetDefaultLaunchRole()->GetUATCommandLine().TrimStartAndEnd();
		CommandLine.TrimStartAndEndInline();

		return CommandLine;
	}

	void FGenericProfileTreeBuilder::SetCommandLine( const FString& NewCommandLine )
	{
		Profile->SetAdditionalCommandLineParameters(NewCommandLine);
		Profile->GetDefaultLaunchRole()->SetCommandLine(TEXT(""));
		OnPropertyChanged();
	}




	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	TSharedRef<SWidget> FGenericProfileTreeBuilder::CreateMapListWidget()
	{
		MapListView = 
			SNew(SCustomLaunchMapListView, Model)
			.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetMapsToCook)
			.SelectedMaps(this, &FGenericProfileTreeBuilder::GetMapsToCook)
			.ProjectPath(this, &FGenericProfileTreeBuilder::GetProjectPath)
		;

		return SNew(SVerticalBox)

			// map option controls
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0,2)
			[
				SNew(SHorizontalBox)

				// map option
				+SHorizontalBox::Slot()
				.Padding(0, 0)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SSegmentedControl<EMapOption>)
					.Value( this, &FGenericProfileTreeBuilder::GetMapOption)
					.OnValueChanged(this, &FGenericProfileTreeBuilder::SetMapOption)

					+SSegmentedControl<EMapOption>::Slot(EMapOption::Startup)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("StartupMapsLabel", "Startup Maps"))
						.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
					]

					+SSegmentedControl<EMapOption>::Slot(EMapOption::Selected)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("SelectedMapsLabel", "Selected Maps"))
						.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
					]
				]

				// map selector controls (search etc)
				+SHorizontalBox::Slot()
				.Padding(8, 0)
				.FillWidth(1)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.Visibility_Lambda([this](){ return GetMapOption() == EMapOption::Selected  ? EVisibility::Visible : EVisibility::Collapsed; } )
					[
						MapListView->MakeControlsWidget()
					]
				]
			]

			// map list
			+SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SVerticalResizeBox)
				.Visibility_Lambda([this](){ return (GetMapOption() == EMapOption::Selected) ? EVisibility::Visible : EVisibility::Collapsed; } )
				.HandleHeight(4)
				.ContentHeight(this, &FGenericProfileTreeBuilder::GetMapListHeight)
				.ContentHeightChanged(this, &FGenericProfileTreeBuilder::SetMapListHeight)
				.HandleColor( FAppStyle::Get().GetSlateColor("Colors.Secondary").GetSpecifiedColor() )
				[
					MapListView.ToSharedRef()
				]
			]
		;
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	void FGenericProfileTreeBuilder::SetCook( bool bCook )
	{
		bShouldCook = bCook;
		RefreshContentScheme();
	}

	bool FGenericProfileTreeBuilder::GetCook(ILauncherProfilePtr InProfile) const
	{
		if (InProfile == nullptr)
		{
			InProfile = Profile;
		}
		return (InProfile->GetCookMode() != ELauncherProfileCookModes::DoNotCook);
	}


	void FGenericProfileTreeBuilder::SetIncrementalCookMode( ELauncherProfileIncrementalCookMode::Type Mode )
	{
		Profile->SetIncrementalCookMode(Mode);
		// should always use unversioned, except the LegacyIterative cook (-iterate commandline argument) because it does not handle invalidation due to c++ changes.
		Profile->SetUnversionedCooking(Mode != ELauncherProfileIncrementalCookMode::ModifiedOnly);
		OnPropertyChanged();
	}

	ELauncherProfileIncrementalCookMode::Type FGenericProfileTreeBuilder::GetIncrementalCookMode() const
	{
		return Profile->GetIncrementalCookMode();
	}



	void FGenericProfileTreeBuilder::SetMapsToCook(TArray<FString> MapsToCook)
	{
		Profile->ClearCookedMaps(); 
		for (const FString& Map : MapsToCook)
		{
			Profile->AddCookedMap(Map);
		}

		OnPropertyChanged();
	}

	TArray<FString> FGenericProfileTreeBuilder::GetMapsToCook() const
	{
		return Profile->GetCookedMaps();
	}

	float FGenericProfileTreeBuilder::GetMapListHeight() const
	{
		return MapListHeight;
	}

	void FGenericProfileTreeBuilder::SetMapListHeight( float NewHeight )
	{
		static const float MinMapListHeight = 100.0f;

		MapListHeight = FMath::Max(NewHeight, MinMapListHeight);

		TreeData->RequestTreeRefresh();
	}

	FGenericProfileTreeBuilder::EMapOption FGenericProfileTreeBuilder::GetMapOption() const
	{
		return MapOption;
	}

	void FGenericProfileTreeBuilder::SetMapOption( EMapOption NewMapOption )
	{
		bool bShow = (NewMapOption == EMapOption::Selected);

		MapOption = NewMapOption;

		if (bShow)
		{
			// restore the cooked maps again, if any
			if (CachedMapsToCook.Num() > 0 && Profile->GetCookedMaps().Num() == 0)
			{
				SetMapsToCook(CachedMapsToCook);
				CachedMapsToCook.Reset();
			}
		}
		else
		{
			// to set the 'cook startup maps only', its necessary to remove all the cooked maps - take a copy of the values to allow it to be restored
			CachedMapsToCook = Profile->GetCookedMaps();
			SetMapsToCook(TArray<FString>());
		}

		OnPropertyChanged();

		MapListView->RefreshMapList();
	}




	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	TSharedRef<SWidget> FGenericProfileTreeBuilder::CreateDeployDeviceWidget()
	{
		if (ProfileType == EProfileType::Basic)
		{
			return SNew(SCustomLaunchDeviceListView)
				.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetDeployDeviceIDs)
				.SelectedDevices(this, &FGenericProfileTreeBuilder::GetDeployDeviceIDs)
				.AllPlatforms(true)
				.SingleSelect(true)
			;
		}
		else
		{
			return SNew(SVerticalBox)

				// device picker options
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0,2)
				[
					SNew(SHorizontalBox)

					+SHorizontalBox::Slot()
					.Padding(0, 0)
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SSegmentedControl<EDeployDeviceOption>)
						.Value( this, &FGenericProfileTreeBuilder::GetDeployDeviceOption)
						.OnValueChanged(this, &FGenericProfileTreeBuilder::SetDeployDeviceOption)

						+SSegmentedControl<EDeployDeviceOption>::Slot(EDeployDeviceOption::Default)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("DefaultDeviceLabel", "Default Device"))
							.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
						]

						+SSegmentedControl<EDeployDeviceOption>::Slot(EDeployDeviceOption::Selected)
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("SelectedDevicesLabel", "Selected Devices"))
							.Font(FCoreStyle::Get().GetFontStyle("SmallFont"))
						]
					]
				]

				// device picker list
				+SVerticalBox::Slot()
				.FillHeight(1)
				[
					SNew(SVerticalResizeBox)
					.Visibility_Lambda([this](){ return (GetDeployDeviceOption() == EDeployDeviceOption::Selected) ? EVisibility::Visible : EVisibility::Collapsed; } )
					.HandleHeight(4)
					.ContentHeight(this, &FGenericProfileTreeBuilder::GetDeployDeviceListHeight)
					.ContentHeightChanged(this, &FGenericProfileTreeBuilder::SetDeployDeviceListHeight)
					.HandleColor( FAppStyle::Get().GetSlateColor("Colors.Secondary").GetSpecifiedColor() )
					[
						SAssignNew(DeployDeviceListView, SCustomLaunchDeviceListView)
						.OnDeviceRemoved( this, &FGenericProfileTreeBuilder::OnDeviceRemoved)
						.OnSelectionChanged(this, &FGenericProfileTreeBuilder::SetDeployDeviceIDs)
						.SelectedDevices(this, &FGenericProfileTreeBuilder::GetDeployDeviceIDs)
						.Platforms(this, &FGenericProfileTreeBuilder::GetSelectedPlatforms)
					]
				]
			;
		}
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION


	void FGenericProfileTreeBuilder::SetDeployDeviceIDs(TArray<FString> DeployDeviceIDs)
	{
		if (Profile->GetDeployedDeviceGroup() == nullptr)
		{
			Profile->SetDeployedDeviceGroup(Model->GetProfileManager()->AddNewDeviceGroup());
		}

		Profile->GetDeployedDeviceGroup()->RemoveAllDevices(); 
		for ( const FString& DeviceID : DeployDeviceIDs )
		{
			Profile->GetDeployedDeviceGroup()->AddDevice(DeviceID);
		}

		if (ProfileType == EProfileType::Basic) // @fixme: should this be done for custom profiles too?
		{
			Model->UpdatedCookedPlatformsFromDeployDeviceProxy(Profile);
		}

		OnPropertyChanged();
	}

	TArray<FString> FGenericProfileTreeBuilder::GetDeployDeviceIDs() const
	{
		return Profile->GetDeployedDeviceGroup()->GetDeviceIDs();
	}

	float FGenericProfileTreeBuilder::GetDeployDeviceListHeight() const
	{
		return DeployDeviceListHeight;
	}

	void FGenericProfileTreeBuilder::SetDeployDeviceListHeight( float NewHeight )
	{
		static const float MinDeployDeviceListHeight = 100.0f;

		DeployDeviceListHeight = FMath::Max(NewHeight, MinDeployDeviceListHeight);

		TreeData->RequestTreeRefresh();
	}

	void FGenericProfileTreeBuilder::OnDeviceRemoved(FString DeviceID)
	{
		CachedDeployDeviceIDs.Remove(DeviceID);
	}

	FGenericProfileTreeBuilder::EDeployDeviceOption FGenericProfileTreeBuilder::GetDeployDeviceOption() const
	{
		return DeployDeviceOption;
	}

	void FGenericProfileTreeBuilder::SetDeployDeviceOption( EDeployDeviceOption NewDeployDeviceOption )
	{
		bool bShow = (NewDeployDeviceOption == EDeployDeviceOption::Selected);

		DeployDeviceOption = NewDeployDeviceOption;

		if (bShow)
		{
			// restore the deployed device list again, if any
			if (CachedDeployDeviceIDs.Num() > 0 && Profile->GetDeployedDeviceGroup()->GetDeviceIDs().Num() == 0)
			{
				SetDeployDeviceIDs(Profile->GetDeployedDeviceGroup()->GetDeviceIDs());
				CachedDeployDeviceIDs.Reset();
			}
		}
		else
		{
			// to set the 'default' deploy option, its necessary to remove all the devices - take a copy of the values to allow it to be restored
			CachedDeployDeviceIDs = Profile->GetDeployedDeviceGroup()->GetDeviceIDs();
			SetDeployDeviceIDs(TArray<FString>());
		}

		OnPropertyChanged();

		DeployDeviceListView->RefreshDeviceList();
	}


	void FGenericProfileTreeBuilder::SetBuild( bool bBuild )
	{
		if (!bBuild)
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::DoNotBuild);
		}
		else if (GetForceBuild(Profile))
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::Build);
		}
		else
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::Auto);
		}

		OnPropertyChanged();
	}

	bool FGenericProfileTreeBuilder::GetBuild(ILauncherProfilePtr InProfile) const
	{
		if (InProfile == nullptr)
		{
			InProfile = Profile;
		}
		return (InProfile->GetBuildMode() != ELauncherProfileBuildModes::DoNotBuild);
	}

	void FGenericProfileTreeBuilder::SetForceBuild( bool bForceBuild )
	{
		if (!GetBuild())
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::DoNotBuild);
		}
		else if (bForceBuild)
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::Build);
		}
		else
		{
			Profile->SetBuildMode(ELauncherProfileBuildModes::Auto);
		}

		OnPropertyChanged();
	}

	bool FGenericProfileTreeBuilder::GetForceBuild(ILauncherProfilePtr InProfile) const
	{
		if (InProfile == nullptr)
		{
			InProfile = Profile;
		}
		return (InProfile->GetBuildMode() == ELauncherProfileBuildModes::Build);
	}

	void FGenericProfileTreeBuilder::SetArchitecture( FString Architecture )
	{
		// clear existing architectures
		TArray<FString> Empty;
		Profile->SetServerArchitectures(Empty);
		Profile->SetEditorArchitectures(Empty);
		Profile->SetClientArchitectures(Empty);

		// set new single architecture
		if (!Architecture.IsEmpty())
		{
			TArray<FString> Single;
			Single.Add(Architecture);

			switch (CachedBuildTargetType)
			{
				case EBuildTargetType::Server: Profile->SetServerArchitectures(Single); break;
				case EBuildTargetType::Editor: Profile->SetEditorArchitectures(Single); break;
				default:                       Profile->SetClientArchitectures(Single); break;
			}
		}

		OnPropertyChanged();
	}

	FString FGenericProfileTreeBuilder::GetArchitecture() const
	{
		TArray<FString> Architectures;

		switch (CachedBuildTargetType)
		{
			case EBuildTargetType::Server: Architectures = Profile->GetServerArchitectures(); break;
			case EBuildTargetType::Editor: Architectures = Profile->GetEditorArchitectures(); break;
			default:                       Architectures = Profile->GetClientArchitectures(); break;
		}

		return (Architectures.Num() > 0) ? Architectures[0] : FString();
	}

	FText FGenericProfileTreeBuilder::GetArchitectureDisplayName( FString Architecture )
	{
		if (Architecture.IsEmpty())
		{
			return LOCTEXT("DefaultArchName", "Project Default");
		}
		else if (Architecture == FPlatformMisc::GetHostArchitecture() && Model->IsHostPlatform(Profile))
		{
			return FText::Format(LOCTEXT("HostArchLabel", "{0} (this platform)"), FText::FromString(Architecture) );
		}
		else
		{
			return FText::FromString(Architecture);
		}
	}



	void FGenericProfileTreeBuilder::SetDeployToDevice( bool bDeployToDevice )
	{
		if (ContentScheme != EContentScheme::CookOnTheFly)
		{
			Profile->SetDeploymentMode(bDeployToDevice ? ELauncherProfileDeploymentModes::CopyToDevice : ELauncherProfileDeploymentModes::DoNotDeploy );
			OnPropertyChanged();
		}
	}

	bool FGenericProfileTreeBuilder::GetDeployToDevice(ILauncherProfilePtr InProfile) const
	{
		if (InProfile == nullptr)
		{
			InProfile = Profile;
		}
		return InProfile->GetDeploymentMode() != ELauncherProfileDeploymentModes::DoNotDeploy;
	}


	void FGenericProfileTreeBuilder::SetIsRunning( bool bRun )
	{
		Profile->SetLaunchMode(bRun ? ELauncherProfileLaunchModes::DefaultRole : ELauncherProfileLaunchModes::DoNotLaunch);
	}

	bool FGenericProfileTreeBuilder::GetIsRunning( ILauncherProfilePtr InProfile) const
	{
		if (InProfile == nullptr)
		{
			InProfile = Profile;
		}
	
		return InProfile->GetLaunchMode() != ELauncherProfileLaunchModes::DoNotLaunch;
	}

	void FGenericProfileTreeBuilder::OnInitialMapChanged( TSharedPtr<FString> InitialMap, ESelectInfo::Type )
	{
		if (InitialMap.IsValid())
		{
			Profile->GetDefaultLaunchRole()->SetInitialMap(*InitialMap);
		}
		else
		{
			Profile->GetDefaultLaunchRole()->SetInitialMap(FString());
		}

		OnPropertyChanged();
	}

	TSharedPtr<FString> FGenericProfileTreeBuilder::GetInitialMap() const
	{
		CacheStartupMapList();

		FString InitialMap = Profile->GetDefaultLaunchRole()->GetInitialMap();
		const TSharedPtr<FString>* FoundMapPtr = CachedStartupMaps.FindByPredicate( [InitialMap]( const TSharedPtr<FString>& Map )
		{
			return Map.IsValid() && (InitialMap == *Map);
		});

		if (FoundMapPtr)
		{
			return *FoundMapPtr;
		}
		else
		{
			return MakeShared<FString>();
		}
	}

}

#undef LOCTEXT_NAMESPACE
