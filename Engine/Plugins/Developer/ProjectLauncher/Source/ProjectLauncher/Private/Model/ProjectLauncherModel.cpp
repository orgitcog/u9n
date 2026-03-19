// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/ProjectLauncherModel.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ConfigContext.h"
#include "Misc/App.h"
#include "Misc/ScopedSlowTask.h"
#include "PlatformInfo.h"
#include "GameProjectHelper.h"
#include "ITargetDeviceProxy.h"
#include "ITargetDeviceProxyManager.h"
#include "ITargetDeviceServicesModule.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Modules/ModuleManager.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Experimental/ZenServerInterface.h"

#if WITH_EDITOR
#include "Engine/World.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/AssetData.h"
#endif

#define LOCTEXT_NAMESPACE "SCustomLaunchProfileSelector"

namespace ProjectLauncher
{
	// this option filters the platforms & targets based on each other. 
	// it makes it easier to choose the desired build target but the logic under the hood is more complex
	bool bUseFriendlyBuildTargetSelection = true;


	FModel::FModel(const TSharedRef<ITargetDeviceProxyManager>& InDeviceProxyManager, const TSharedRef<ILauncher>& InLauncher, const TSharedRef<ILauncherProfileManager>& InProfileManager)
		: DeviceProxyManager(InDeviceProxyManager)
		, Launcher(InLauncher)
		, ProfileManager(InProfileManager)
	{
		// register callbacks
		ProfileManager->OnPostProcessLaunchCommandLine().AddRaw( this, &FModel::OnModifyLaunchCommandLine );

		ProfileManager->OnProfileAdded().AddRaw(this, &FModel::HandleProfileManagerProfileAdded);
		ProfileManager->OnProfileRemoved().AddRaw(this, &FModel::HandleProfileManagerProfileRemoved);

		DeviceProxyManager->OnProxyAdded().AddRaw(this, &FModel::HandleDeviceProxyAdded);
		DeviceProxyManager->OnProxyRemoved().AddRaw(this, &FModel::HandleDeviceProxyRemoved);


		// ensure there's a project when we're in the editor
		// this means we don't need to display the global project selector in the editor
		if (GIsEditor && FPaths::IsProjectFilePathSet())
		{
			ProfileManager->SetProjectPath(FPaths::GetProjectFilePath());
		}


		// prepare profiles
		BasicLaunchProfile = CreateBasicLaunchProfile();
		bHasSetBasicLaunchProfilePlatform = (BasicLaunchProfile->GetCookedPlatforms().Num() > 0);

		AllProfiles = ProfileManager->GetAllProfiles();
		AllProfiles.Add(BasicLaunchProfile);
		SortProfiles();

		DefaultBasicLaunchProfile = CreateBasicLaunchProfile();
		DefaultCustomLaunchProfile = CreateCustomProfile(TEXT("DefaultCustomProfile"));


		// use a custom ini file so that it can be shared between UnrealFrontend and the editor
		ConfigFileName = FPaths::EngineSavedDir() / TEXT("Config") / TEXT("ProjectLauncher") / TEXT("UserSettings.ini");
		FConfigContext::ReadSingleIntoGConfig().Load(*ConfigFileName);

		LoadConfig();
	}



	FModel::~FModel()
	{
		ProfileManager->OnPostProcessLaunchCommandLine().RemoveAll( this );

		ProfileManager->OnProfileAdded().RemoveAll(this);
		ProfileManager->OnProfileRemoved().RemoveAll(this);

		DeviceProxyManager->OnProxyAdded().RemoveAll(this);
		DeviceProxyManager->OnProxyRemoved().RemoveAll(this);

		SaveConfig();
	}


	void FModel::SelectProfile(const ILauncherProfilePtr& NewProfile)
	{
		if (SelectedProfile != NewProfile)
		{
			ILauncherProfilePtr PreviousProfile = SelectedProfile;
			SelectedProfile = NewProfile;

			ProfileSelectedDelegate.Broadcast(SelectedProfile, PreviousProfile);
		}
	}


	EProfileType FModel::GetProfileType(const ILauncherProfileRef& Profile) const
	{
		if (IsAdvancedProfile(Profile))
		{
			return EProfileType::Advanced;
		}
		else if (Profile == BasicLaunchProfile)
		{
			return EProfileType::Basic;
		}
		else
		{
			return EProfileType::Custom;
		}
	}

	bool FModel::IsAdvancedProfile(const ILauncherProfileRef& Profile) const
	{
		return Profile->GetCookedPlatforms().Num() > 1 ||
				Profile->GetCookedCultures().Num() > 1 ||
				Profile->IsCreatingDLC() ||
				Profile->IsCreatingReleaseVersion() ||
				Profile->IsGeneratingPatch() ||				
				Profile->GetPackagingMode() == ELauncherProfilePackagingModes::SharedRepository ||
				Profile->GetDeploymentMode() == ELauncherProfileDeploymentModes::CopyRepository ||
				Profile->GetLaunchMode() == ELauncherProfileLaunchModes::CustomRoles
			;
	}




	const TCHAR* FModel::GetConfigSection() const
	{
		return TEXT("ProjectLauncher"); 
	}

	const FString& FModel::GetConfigIni() const
	{
		return ConfigFileName;
	}

	void FModel::LoadConfig()
	{
		if (!GIsEditor)
		{
			// restore the previous project selection
			FString ProjectPath;
	
			if (FPaths::IsProjectFilePathSet())
			{
				ProjectPath = FPaths::GetProjectFilePath();
			}
			else if (FGameProjectHelper::IsGameAvailable(FApp::GetProjectName()))
			{
				ProjectPath = FPaths::RootDir() / FApp::GetProjectName() / FApp::GetProjectName() + TEXT(".uproject");
			}
			else if (GConfig != nullptr)
			{
				GConfig->GetString(GetConfigSection(), TEXT("SelectedProjectPath"), ProjectPath, GetConfigIni());
			}
	
			ProfileManager->SetProjectPath(ProjectPath);
		}
	}



	void FModel::SaveConfig()
	{
		if (!GIsEditor && GConfig != nullptr && !FPaths::IsProjectFilePathSet() && !FGameProjectHelper::IsGameAvailable(FApp::GetProjectName()))
		{
			FString ProjectPath = ProfileManager->GetProjectPath();
			GConfig->SetString(GetConfigSection(), TEXT("SelectedProjectPath"), *ProjectPath, GetConfigIni());
		}
	}



	const PlatformInfo::FTargetPlatformInfo* FModel::GetPlatformInfo(const ILauncherProfilePtr& Profile)
	{
		if (Profile.IsValid() && Profile->GetCookedPlatforms().Num() > 0)
		{
			FString SelectedPlatform = Profile->GetCookedPlatforms()[0];
			return PlatformInfo::FindPlatformInfo(FName(SelectedPlatform));
		}

		return nullptr;
	}

	const PlatformInfo::FTargetPlatformInfo* FModel::GetPlatformInfo(FName PlatformName, const FTargetInfo& BuildTargetInfo)
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(PlatformName);
		if (PlatformInfo == nullptr)
		{
			return nullptr;
		}

		// see if we found the platform immediately
		if (BuildTargetInfo.Name.IsEmpty() || PlatformInfo->PlatformType == BuildTargetInfo.Type)
		{
			return PlatformInfo;
		}

		// try to find a matching flavor for the given platform & build target
		if (PlatformInfo->VanillaInfo->PlatformType == BuildTargetInfo.Type && PlatformInfo->VanillaInfo->PlatformFlavor == PlatformInfo->PlatformFlavor)
		{
			return PlatformInfo->VanillaInfo;
		}
		for (const PlatformInfo::FTargetPlatformInfo* PlatformInfoFlavor : PlatformInfo->VanillaInfo->Flavors)
		{
			if (PlatformInfoFlavor->PlatformType == BuildTargetInfo.Type && PlatformInfoFlavor->PlatformFlavor == PlatformInfo->PlatformFlavor)
			{
				return PlatformInfoFlavor;
			}
		}

		return nullptr;
	}

	bool FModel::IsHostPlatform(const ILauncherProfilePtr& Profile)
	{
		if (Profile.IsValid() && Profile->GetCookedPlatforms().Num() > 0)
		{
			FString SelectedPlatform = Profile->GetCookedPlatforms()[0];
			return IsHostPlatform(FName(SelectedPlatform));
		}

		return false;
	}

	bool FModel::IsHostPlatform(FName PlatformName)
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(PlatformName);

		if (PlatformInfo != nullptr && PlatformInfo->IniPlatformName == FPlatformProperties::IniPlatformName())
		{
			return true;
		}

		return false;
	}



	FTargetInfo FModel::GetBuildTargetInfo( const FString& BuildTargetName, const FString& ProjectPath )
	{
		FTargetInfo Result;
		if (!BuildTargetName.IsEmpty())
		{
			const TArray<FTargetInfo>& BuildTargets = FDesktopPlatformModule::Get()->GetTargetsForProject(ProjectPath);
			for (const FTargetInfo& BuildTarget : BuildTargets)
			{
				if (BuildTarget.Name == BuildTargetName)
				{
					Result = BuildTarget;
					break;
				}
			}
		}

		return MoveTemp(Result);
	}

	FTargetInfo FModel::GetBuildTargetInfo( const ILauncherProfileRef& Profile )
	{
		const FString& BuildTargetName = Profile->GetBuildTarget();
		const FString& ProjectPath = Profile->GetProjectPath();
		return GetBuildTargetInfo(BuildTargetName, ProjectPath);
	}

	FString FModel::GetVanillaPlatformName( const FString& PlatformName )
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = PlatformInfo::FindPlatformInfo(*PlatformName);
		if (PlatformInfo != nullptr)
		{
			return PlatformInfo->VanillaInfo->Name.ToString();
		}

		return PlatformName;
	}

	FString FModel::GetBuildTargetPlatformName( const FString& PlatformName, const FTargetInfo& BuildTargetInfo )
	{
		const PlatformInfo::FTargetPlatformInfo* PlatformInfo = ProjectLauncher::FModel::GetPlatformInfo(*PlatformName, BuildTargetInfo);
		if (PlatformInfo != nullptr)
		{
			return PlatformInfo->Name.ToString();
		}

		return PlatformName;

	}



	void FModel::SortProfiles()
	{
		AllProfiles.Sort( [this](const ILauncherProfilePtr& ProfileA, const ILauncherProfilePtr& ProfileB )
		{
			if (ProfileA == BasicLaunchProfile)
			{
				return true;
			}
			else if (ProfileB == BasicLaunchProfile)
			{
				return false;
			}
			else
			{
				return ProfileA->GetName() < ProfileB->GetName();
			}
		});
	}

	void FModel::HandleProfileManagerProfileAdded(const ILauncherProfileRef& Profile)
	{
		AllProfiles.Add(Profile);
	}



	void FModel::HandleProfileManagerProfileRemoved(const ILauncherProfileRef& Profile)
	{
		AllProfiles.Remove(Profile);

		if (Profile == SelectedProfile)
		{
			SelectProfile(BasicLaunchProfile);
		}
	}




	void FModel::HandleDeviceProxyAdded(const TSharedRef<ITargetDeviceProxy>& DeviceProxy )
	{
		if (!bHasSetBasicLaunchProfilePlatform)
		{
			UpdatedCookedPlatformsFromDeployDeviceProxy(BasicLaunchProfile.ToSharedRef(), DeviceProxy);
			bHasSetBasicLaunchProfilePlatform = true;
		}
	}

	void FModel::HandleDeviceProxyRemoved(const TSharedRef<ITargetDeviceProxy>& DeviceProxy )
	{
	}




	TSharedPtr<ITargetDeviceProxy> FModel::GetDeviceProxy(const ILauncherProfileRef& Profile)
	{
		ILauncherDeviceGroupPtr DeployedDeviceGroup = Profile->GetDeployedDeviceGroup();
		if (DeployedDeviceGroup != nullptr && DeployedDeviceGroup->GetDeviceIDs().Num() > 0)
		{
			const FString& DeviceID = DeployedDeviceGroup->GetDeviceIDs()[0];

			ITargetDeviceServicesModule& TargetDeviceServicesModule = FModuleManager::LoadModuleChecked<ITargetDeviceServicesModule>("TargetDeviceServices");

			return TargetDeviceServicesModule.GetDeviceProxyManager()->FindProxyDeviceForTargetDevice(DeviceID);
		}

		return nullptr;
	}


	ILauncherProfileRef FModel::CreateCustomProfile( const TCHAR* Name )
	{
		//create the profile
		ILauncherProfileRef Profile = ProfileManager->CreateUnsavedProfile(Name);

		// set defaults
		SetProfileContentScheme(EContentScheme::ZenStreaming, Profile );
		Profile->SetBuildConfiguration(EBuildConfiguration::Development);
		Profile->SetLaunchMode(ELauncherProfileLaunchModes::DefaultRole);
		Profile->SetBuildMode(ELauncherProfileBuildModes::Auto);
		Profile->SetBuildUAT(false);
		// @fixme: set all defaults here 
		// ...


		// make sure there is a device & deploy group
		ILauncherDeviceGroupRef DeployDeviceGroup = ProfileManager->AddNewDeviceGroup();
		Profile->SetDeployedDeviceGroup(DeployDeviceGroup);

		TArray<TSharedPtr<ITargetDeviceProxy>> DeviceProxies;
		DeviceProxyManager->GetProxies(NAME_None, true, DeviceProxies);
		if (DeviceProxies.Num() > 0 && DeviceProxies[0].IsValid())
		{
			UpdatedCookedPlatformsFromDeployDeviceProxy(Profile, DeviceProxies[0].ToSharedRef());
		}

		return Profile;
	}

	ILauncherProfileRef FModel::CreateBasicLaunchProfile()
	{
		const FText BasicLaunchProfileName = LOCTEXT("BasicLaunchProfileName", "Basic Launch");
		const FText BasicLaunchProfileDescription = LOCTEXT("BasicLaunchProfileDescription", "Use this profile to launch on a device with the recommended defaults");

		ILauncherProfileRef Profile = CreateCustomProfile(*BasicLaunchProfileName.ToString());
		Profile->SetDescription(*BasicLaunchProfileDescription.ToString());
		Profile->SetProjectSpecified(false);
		Profile->SetBuildTargetSpecified(false);

		return Profile;
	}



	void FModel::UpdatedCookedPlatformsFromDeployDeviceProxy(const ILauncherProfileRef& Profile, TSharedPtr<ITargetDeviceProxy> DeviceProxy)
	{
		if (DeviceProxy.IsValid())
		{
			Profile->GetDeployedDeviceGroup()->RemoveAllDevices();
			Profile->GetDeployedDeviceGroup()->AddDevice(DeviceProxy->GetTargetDeviceId(NAME_None));
		}
		else if (Profile->GetDeployedDeviceGroup()->GetNumDevices() > 0)
		{
			DeviceProxy = GetDeviceProxy(Profile);
		}
		
		if (DeviceProxy.IsValid())
		{
			Profile->ClearCookedPlatforms();
		
			FTargetInfo BuildTargetInfo = GetBuildTargetInfo(Profile);
			FString PlatformName = DeviceProxy->GetTargetPlatformName(NAME_None);
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = GetPlatformInfo(*PlatformName, BuildTargetInfo);

			if (PlatformInfo != nullptr)
			{
				Profile->AddCookedPlatform(PlatformInfo->Name.ToString());
			}
			else
			{
				Profile->AddCookedPlatform(DeviceProxy->GetTargetPlatformName(NAME_None));
			}

			// @todo: is default deploy platform necessary now?
		}
	}

	void FModel::UpdateCookedPlatformsFromBuildTarget(const ILauncherProfileRef& Profile)
	{
		FTargetInfo BuildTargetInfo = GetBuildTargetInfo(Profile);

		TArray<FString> Platforms = Profile->GetCookedPlatforms();
		Profile->ClearCookedPlatforms();
		for (const FString& Platform : Platforms)
		{
			const PlatformInfo::FTargetPlatformInfo* PlatformInfo = GetPlatformInfo(*Platform, BuildTargetInfo);
			if (PlatformInfo != nullptr)
			{
				Profile->AddCookedPlatform(PlatformInfo->Name.ToString());
			}
			else
			{
				Profile->AddCookedPlatform(Platform);
			}
		}
	}


	ILauncherProfilePtr FModel::CloneCustomProfile(const ILauncherProfileRef& Profile)
	{
		ILauncherProfilePtr NewProfile;

		FBufferArchive Writer;
		if (Profile->Serialize(Writer))
		{
			NewProfile = CreateCustomProfile(TEXT("Cloned"));
			if (NewProfile.IsValid())
			{
				FMemoryReader Reader(Writer);
				NewProfile->Serialize(Reader);
				NewProfile->AssignId(true); // need to give it a new Id - don't use the cloned one!

				// @todo: add these to FLauncherProfile's Serialize() function & update the internal version. Waiting for all Zen properties to be decided (zen workspace etc)
				NewProfile->SetUseZenStore(Profile->IsUsingZenStore());
				// ...

				ILauncherDeviceGroupRef DeployDeviceGroup = ProfileManager->AddNewDeviceGroup();
				NewProfile->SetDeployedDeviceGroup(DeployDeviceGroup);
				for (const FString& DeviceID : Profile->GetDeployedDeviceGroup()->GetDeviceIDs())
				{
					DeployDeviceGroup->AddDevice(DeviceID);
				}
			}
		}

		return NewProfile;
	}

			

	EContentScheme FModel::DetermineProfileContentScheme(const ILauncherProfileRef& Profile) const
	{
		if (Profile->GetCookMode() == ELauncherProfileCookModes::OnTheFly)
		{
			return EContentScheme::CookOnTheFly;
		}
		else if (Profile->GetPackagingMode() != ELauncherProfilePackagingModes::DoNotPackage)
		{
			return EContentScheme::DevelopmentPackage;
		}
		else if (Profile->IsPackingWithUnrealPak())
		{
			return EContentScheme::PakFiles;
		}
		else if (Profile->IsUsingZenPakStreaming())
		{
			return EContentScheme::ZenPakStreaming;
		}
		else if (Profile->IsUsingZenStore())
		{
			return EContentScheme::ZenStreaming;
		}
		else
		{
			return EContentScheme::LooseFiles;
		}
	}



	void FModel::SetProfileContentScheme(EContentScheme ContentScheme, const ILauncherProfileRef& Profile, bool bWantToCook, ELauncherProfileDeploymentModes::Type DefaultDeploymentMode)
	{
		bool bPakFiles = (ContentScheme == EContentScheme::PakFiles || ContentScheme == EContentScheme::DevelopmentPackage);
		bool bUseZen = (ContentScheme != EContentScheme::LooseFiles && ContentScheme != EContentScheme::CookOnTheFly);
		bool bCOTF = (ContentScheme == EContentScheme::CookOnTheFly);
		bool bPackage = (ContentScheme == EContentScheme::DevelopmentPackage);
		bool bZenPakStreaming = (ContentScheme == EContentScheme::ZenPakStreaming);
		// FIXME: may need to turn off Zen/Pak in UAT somehow if it's set in project defaults?

		Profile->SetUseZenPakStreaming(bZenPakStreaming);
		Profile->SetDeployWithUnrealPak(bPakFiles);
		Profile->SetUseZenStore(bUseZen);
		if (!bPakFiles)
		{
			Profile->SetGenerateChunks(false);
			Profile->SetUseIoStore(false);
		}

		ELauncherProfileDeploymentModes::Type DeploymentMode = DefaultDeploymentMode;
		if (bCOTF)
		{
			Profile->SetCookMode(ELauncherProfileCookModes::OnTheFly);
			DeploymentMode = ELauncherProfileDeploymentModes::FileServer;
		}
		else if (bZenPakStreaming || !bWantToCook)
		{
			Profile->SetCookMode(ELauncherProfileCookModes::DoNotCook);
		}
		else
		{
			Profile->SetCookMode(ELauncherProfileCookModes::ByTheBook);
		}

		if (bPackage)
		{
			Profile->SetDeploymentMode(ELauncherProfileDeploymentModes::DoNotDeploy); // @todo: some platforms support package deployment. should update verification to check new function in ITargetPlatformControls
			Profile->SetPackagingMode(ELauncherProfilePackagingModes::Locally);
		}
		else
		{
			Profile->SetDeploymentMode(DeploymentMode);
			Profile->SetPackagingMode(ELauncherProfilePackagingModes::DoNotPackage);
		}
	}


	void FModel::ReadProjectSettingsFromConfig( FConfigCacheIni& InConfig, FProjectSettings& OutResult )
	{
		// read project packaging settings
		const TCHAR* ProjectPackagingConfigSection = TEXT("/Script/UnrealEd.ProjectPackagingSettings");
		InConfig.GetBool(ProjectPackagingConfigSection, TEXT("bUseZenStore"), OutResult.bUseZenStore, GGameIni);
		InConfig.GetBool(ProjectPackagingConfigSection, TEXT("bEnablePakStreaming"), OutResult.bHasAutomaticZenPakStreamingWorkspaceCreation, GGameIni);
		
		// read zen settings
#if UE_WITH_ZEN
		using namespace UE::Zen;
		const FServiceSettings& ZenServiceSettings = GetDefaultServiceInstance().GetServiceSettings();
		OutResult.bAllowRemoteNetworkService = ZenServiceSettings.IsAutoLaunch() && ZenServiceSettings.SettingsVariant.Get<FServiceAutoLaunchSettings>().bAllowRemoteNetworkService;
#else
		const TCHAR* ZenAutoLaunchSettings = TEXT("Zen.AutoLaunch");
		InConfig.GetBool(ZenAutoLaunchSettings, TEXT("AllowRemoteNetworkService"), OutResult.bAllowRemoteNetworkService, GEngineIni);
#endif
	}

	const FProjectSettings FModel::GetProjectSettings( const FString& InProjectPath )
	{
		FProjectSettings Result;

		FString ProjectPath = FPaths::ConvertRelativePathToFull(InProjectPath);
		FString ProjectName = FPaths::GetBaseFilename(ProjectPath);

		FString GlobalProjectPath;
		if (FPaths::IsProjectFilePathSet())
		{
			GlobalProjectPath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
		}

		if (!GlobalProjectPath.IsEmpty() && GlobalProjectPath == ProjectPath )
		{
			// read the current project's ini settings
			Result.bIsCurrentEditorProject = true;
			ReadProjectSettingsFromConfig(*GConfig, Result);

			// apply any in-memory properties @fixme: is this necessary?
			const UProjectPackagingSettings* ProjectPackagingSettings = UProjectPackagingSettings::StaticClass()->GetDefaultObject<UProjectPackagingSettings>();
			Result.bUseZenStore = ProjectPackagingSettings->bUseZenStore;

			// note this is not cached because the user could edit the properties from within the editor
		}
		else if (CachedProjectSettings.Contains(ProjectName))
		{
			Result = CachedProjectSettings[ProjectName];
		}
		else if (!ProjectPath.IsEmpty())
		{
			// load the other project's ini files into a temporary config cache
			FConfigCacheIni Config(EConfigCacheType::Temporary);
			FConfigContext Context = FConfigContext::ReadIntoConfigSystem(&Config, FString());
			Context.ProjectConfigDir = FPaths::Combine(FPaths::GetPath(ProjectPath), TEXT("Config/"));
			Config.InitializeKnownConfigFiles(Context);

			// read the ini settings
			Result.bIsCurrentEditorProject = false;
			ReadProjectSettingsFromConfig(Config, Result);

			// cache the result
			CachedProjectSettings.Add( ProjectName, Result);
		}

		return MoveTemp(Result);
	}

	const FProjectSettings FModel::GetProjectSettings( const ILauncherProfileRef& Profile )
	{
		return GetProjectSettings(*Profile->GetProjectPath());
	}



	bool FModel::IsBasicLaunchProfile( const ILauncherProfilePtr& Profile ) const
	{
		return (Profile == BasicLaunchProfile);
	}


	const ILauncherProfileRef FModel::GetDefaultBasicLaunchProfile() const
	{
		return DefaultBasicLaunchProfile.ToSharedRef();
	}

	const ILauncherProfileRef FModel::GetDefaultCustomLaunchProfile() const
	{
		return DefaultCustomLaunchProfile.ToSharedRef();
	}



	TSharedPtr<FLaunchLogMessage> FModel::AddLogMessage( const FString& InMessage, ELogVerbosity::Type InVerbosity )
	{
		TSharedPtr<FLaunchLogMessage> Message = MakeShared<FLaunchLogMessage>(InMessage, InVerbosity);
		LaunchLogMessages.Add(Message);
		return Message;
	}

	void FModel::ClearLogMessages()
	{
		LaunchLogMessages.Reset();
	}



	extern void ApplyExtensionVariables( const ILauncherProfileRef& InProfile, FString& InOutCommandLine, TSharedRef<FModel> InModel );

	void FModel::OnModifyLaunchCommandLine( const ILauncherProfileRef& InProfile, FString& InOutCommandLine )
	{
		ApplyExtensionVariables(InProfile, InOutCommandLine, AsShared() );
	}




	TArray<FString> FModel::GetAndCacheMapPaths(const FString& InOptionalProjectPath) // @todo: map list parsing should ideally be asyncronous, showing a spinner in the map selector controls until its finished etc.
	{
		// prepare values
		FString ProjectPath;
		FString ProjectName;
		FString ContentDir;
		bool bWantEngineMaps = InOptionalProjectPath.IsEmpty();
		if (bWantEngineMaps)
		{
			ContentDir = FPaths::Combine(FPaths::EngineContentDir(), TEXT("Maps"));
		}
		else
		{
			ProjectPath = FPaths::IsRelative(InOptionalProjectPath) ? FPaths::Combine(FPaths::RootDir(), InOptionalProjectPath) : InOptionalProjectPath;
			ProjectName = FPaths::GetBaseFilename(ProjectPath);
			ContentDir = FPaths::Combine(ProjectPath, TEXT("Content")); // @todo: do we want to show plugin maps as well?
		}
				
		// the editor has access to the asset registry which is must faster and will be up to date if new maps are added at runtime - if this is the project that's selected in the editor
#if WITH_EDITOR
		if (FPaths::IsProjectFilePathSet())
		{
			FString GlobalProjectFilePath = FPaths::ConvertRelativePathToFull(FPaths::GetProjectFilePath());
			FString GlobalProjectPath = FPaths::GetPath(GlobalProjectFilePath);

			if (!GlobalProjectPath.IsEmpty() && (GlobalProjectPath == ProjectPath || bWantEngineMaps) )
			{
				// gather all world asset metadata
				FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
				TArray<FAssetData> MapAssets;
				AssetRegistryModule.Get().GetAssetsByClass(UWorld::StaticClass()->GetClassPathName(), MapAssets, true);

				// build the list of map file names for the maps in the desired content directory
				TArray<FString> MapFileList;
				for (const FAssetData& MapAsset : MapAssets)
				{
					FString MapFileName = FPackageName::LongPackageNameToFilename(MapAsset.PackageName.ToString(), FPackageName::GetMapPackageExtension());
					MapFileName = FPaths::ConvertRelativePathToFull(MapFileName);

					if (FPaths::IsUnderDirectory(MapFileName, ContentDir))
					{
						MapFileList.Add( MapFileName );
					}
				}

				MapFileList.Sort();
				return MapFileList;
			}
		}
#endif

		// check to see if we've cached this project's maps already
		const TArray<FString>* CachedProjectMapListPtr = CachedMapPaths.Find(ProjectName);
		if (CachedProjectMapListPtr != nullptr)
		{
			return *CachedProjectMapListPtr;
		}
		else
		{
			// this is slow & currently blocking so show a wait dialog. 
			// annoyingly this doesn't show up in UnrealFrontend because that uses FFeedbackContext rather than FFeedbackContextEditor
			FScopedSlowTask SlowTask(0, LOCTEXT("CacheProjectMapsDesc","Caching project maps"));
			SlowTask.MakeDialog();

			// search for map files
			TArray<FString> MapFileList;
			const FString WildCard = FString::Printf(TEXT("*%s"), *FPackageName::GetMapPackageExtension());
			IFileManager::Get().FindFilesRecursive(MapFileList, *ContentDir, *WildCard, true, false);

			MapFileList.Sort();
			CachedMapPaths.Add(ProjectName, MapFileList);
			return MapFileList;
		}
	}

	TArray<FString> FModel::GetAvailableProjectMapNames( const FString& InProjectPath )
	{
		TArray<FString> Maps = GetAndCacheMapPaths(InProjectPath);
		for (FString& Map : Maps)
		{
			Map = FPaths::GetBaseFilename(Map);
		}
		return MoveTemp(Maps);
	}

	TArray<FString> FModel::GetAvailableProjectMapPaths( const FString& InProjectPath )
	{
		return GetAndCacheMapPaths(InProjectPath);
	}

	TArray<FString> FModel::GetAvailableEngineMapNames()
	{
		TArray<FString> Maps = GetAndCacheMapPaths(FString());
		for (FString& Map : Maps)
		{
			Map = FPaths::GetBaseFilename(Map);
		}
		return MoveTemp(Maps);

	}

	TArray<FString> FModel::GetAvailableEngineMapPaths()
	{
		return GetAndCacheMapPaths(FString());
	}







	TArray<EContentScheme> GetAllContentSchemes()
	{
		TArray<EContentScheme> ContentSchemes;
		for (uint8 Index = 0; Index < (uint8)EContentScheme::MAX; Index++)
		{
			ContentSchemes.Add((EContentScheme)Index);
		}

		return MoveTemp(ContentSchemes);
	}

	FText GetContentSchemeDisplayName(EContentScheme ContentScheme)
	{
		switch (ContentScheme)
		{
			case EContentScheme::PakFiles:           return LOCTEXT("ContentSchemePakFiles","Pak Files"); 
			case EContentScheme::ZenStreaming:       return LOCTEXT("ContentSchemeZenStreaming","Zen Streaming");
			case EContentScheme::ZenPakStreaming:    return LOCTEXT("ContentSchemeZenPakStreaming","Zen Pak Streaming"); 
			case EContentScheme::DevelopmentPackage: return LOCTEXT("ContentSchemeDevPackage","Development Package");
			case EContentScheme::LooseFiles:         return LOCTEXT("ContentSchemeLooseFiles","Loose Files (legacy)");
			case EContentScheme::CookOnTheFly:       return LOCTEXT("ContentSchemeCOTF","Cook On The Fly"); 
		}

		checkNoEntry();
		return FText::GetEmpty();

	}

	FText GetContentSchemeToolTip(EContentScheme ContentScheme)
	{
		switch (ContentScheme)
		{
			case EContentScheme::PakFiles:           return LOCTEXT("ContentSchemeTipPakFiles","Store cooked game content in one or more large Pak Files");
			case EContentScheme::ZenStreaming:       return LOCTEXT("ContentSchemeTipZenStreaming","Stream cooked game content from Zen Server");
			case EContentScheme::ZenPakStreaming:    return LOCTEXT("ContentSchemeTipZenPakStreaming","Stream an existing Pak Files build via a Zen");
			case EContentScheme::DevelopmentPackage: return LOCTEXT("ContentSchemeTipDevPackage","Package cooked game content into a single installable package file for development purposes, where available");
			case EContentScheme::LooseFiles:         return LOCTEXT("ContentSchemeTipLooseFiles","Store cooked game assets in individual files (legacy - recommend moving to Zen Streaming. This option will not work if the project is already configured with 'Use Zen Store')");
			case EContentScheme::CookOnTheFly:       return LOCTEXT("ContentSchemeTipCOTF","Only cook game assets when the game requires them, and send them over the network (legacy - slow)");
		}

		checkNoEntry();
		return FText::GetEmpty();

	}




	const TCHAR* LexToString( const ProjectLauncher::EContentScheme& ContentScheme)
	{
		switch (ContentScheme)
		{
			case ProjectLauncher::EContentScheme::PakFiles:           return TEXT("PakFiles");
			case ProjectLauncher::EContentScheme::ZenStreaming:       return TEXT("ZenStreaming");
			case ProjectLauncher::EContentScheme::ZenPakStreaming:    return TEXT("ZenPakStreaming");
			case ProjectLauncher::EContentScheme::DevelopmentPackage: return TEXT("DevelopmentPackage");
			case ProjectLauncher::EContentScheme::LooseFiles:         return TEXT("LooseFiles");
			case ProjectLauncher::EContentScheme::CookOnTheFly:       return TEXT("CookOnTheFly");
			default: return TEXT("Unknown");
		}
	}



	bool LexTryParseString( ProjectLauncher::EContentScheme& OutContentScheme, const TCHAR* String )
	{
		if (FCString::Stricmp(String, TEXT("PakFiles")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::PakFiles;
			return true;
		}
		else if (FCString::Stricmp(String, TEXT("ZenStreaming")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::ZenStreaming;
			return true;
		}
		else if (FCString::Stricmp(String, TEXT("ZenPakStreaming")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::ZenPakStreaming;
			return true;
		}
		else if (FCString::Stricmp(String, TEXT("DevelopmentPackage")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::DevelopmentPackage;
			return true;
		}
		else if (FCString::Stricmp(String, TEXT("LooseFiles")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::LooseFiles;
			return true;
		}
		else if (FCString::Stricmp(String, TEXT("CookOnTheFly")) == 0) 
		{
			OutContentScheme = ProjectLauncher::EContentScheme::CookOnTheFly;
			return true;
		}
		else
		{
			return false;
		}
	}



	FText GetProfileLaunchErrorMessage(ILauncherProfilePtr Profile)
	{
		if (!Profile.IsValid())
		{
			return LOCTEXT("LaunchErrNoProfileTip", "There is no profile selected");
		}

		if (Profile->IsValidForLaunch())
		{
			return LOCTEXT("LaunchProfileTip", "Launch this profile now");
		}

		FTextBuilder MsgTextBuilder;
		MsgTextBuilder.AppendLine(LOCTEXT("LaunchErrValidation", "There are validation errors with this profile. Please fix them before launching:"));
		MsgTextBuilder.Indent();
		for (int i = 0; i < (int)ELauncherProfileValidationErrors::Count; i++)
		{
			ELauncherProfileValidationErrors::Type Error = (ELauncherProfileValidationErrors::Type)i;
			if (Profile->HasValidationError(Error))
			{
				MsgTextBuilder.AppendLine(LexToStringLocalized(Error));
			}
		}
		MsgTextBuilder.Unindent();

		return MsgTextBuilder.ToText();
	}
}



#undef LOCTEXT_NAMESPACE
