// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Delegates/Delegate.h"
#include "ILauncherProfileManager.h"
#include "Templates/SharedPointer.h"
#include "Misc/DataDrivenPlatformInfoRegistry.h"



namespace PlatformInfo
{
	struct FTargetPlatformInfo;
};

class ITargetDeviceProxyManager;
class ITargetDeviceProxy;
class ILauncher;
struct FTargetInfo;

namespace ProjectLauncher
{
	enum class EContentScheme : uint8
	{
		PakFiles,
		ZenStreaming,
		ZenPakStreaming,
		DevelopmentPackage,
		LooseFiles,
		CookOnTheFly,
		MAX
	};
	TArray<EContentScheme> GetAllContentSchemes();
	FText GetContentSchemeDisplayName(EContentScheme ContentScheme);
	FText GetContentSchemeToolTip(EContentScheme ContentScheme);
	FText GetProfileLaunchErrorMessage(ILauncherProfilePtr Profile);
	const TCHAR* LexToString( const ProjectLauncher::EContentScheme& ContentScheme);
	bool LexTryParseString( ProjectLauncher::EContentScheme& OutContentScheme, const TCHAR* String );

	enum class EProfileType : uint8
	{
		Invalid,
		Basic,
		Custom,
		Advanced,
	};

	struct FLaunchLogMessage
	{
		TSharedRef<FString> Message;
		ELogVerbosity::Type Verbosity;

		FLaunchLogMessage(const FString& InMessage, ELogVerbosity::Type InVerbosity)
			: Message(MakeShared<FString>(InMessage))
			, Verbosity(InVerbosity)
		{}
	};

	struct FProjectSettings
	{
		// whether this project is the one that is currently open in the editor
		bool bIsCurrentEditorProject = false;

		// whether Zen Store is enabled via the project settings - in this case, we can't use Loose Files
		bool bUseZenStore = false;

		// whether a workspace will be created automatically when using Zen Pak streaming
		bool bHasAutomaticZenPakStreamingWorkspaceCreation = false;

		// whether Zen Server is allowed to accept connections from remote machines (i.e. console devkits, phones etc)
		bool bAllowRemoteNetworkService = false;
	};


	DECLARE_DELEGATE_OneParam(FOnProfileClicked, const ILauncherProfilePtr&)

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSelectedProfileChanged, const ILauncherProfilePtr& /*new*/, const ILauncherProfilePtr& /*old*/)
	DECLARE_MULTICAST_DELEGATE(FOnSelectedProjectChanged)
	DECLARE_MULTICAST_DELEGATE(FOnBuildTargetChanged)
	DECLARE_MULTICAST_DELEGATE(FOnSelectedPlatformChanged)

	extern bool bUseFriendlyBuildTargetSelection;

	class PROJECTLAUNCHER_API FModel : public TSharedFromThis<FModel>
	{
	public:
		FModel(const TSharedRef<ITargetDeviceProxyManager>& InDeviceProxyManager, const TSharedRef<ILauncher>& InLauncher, const TSharedRef<ILauncherProfileManager>& InProfileManager);
		~FModel();


		const TSharedRef<ITargetDeviceProxyManager>& GetDeviceProxyManager() const
		{
			return DeviceProxyManager;
		}

		const TSharedRef<ILauncher>& GetLauncher() const
		{
			return Launcher;
		}

		const TSharedRef<ILauncherProfileManager>& GetProfileManager() const
		{
			return ProfileManager;
		}


		const TArray<ILauncherProfilePtr>& GetAllProfiles() const
		{
			return AllProfiles;
		}

		ILauncherProfilePtr GetBasicLaunchProfile() const
		{
			return BasicLaunchProfile;
		}

		const ILauncherProfilePtr& GetSelectedProfile() const
		{
			return SelectedProfile;
		}

		FOnSelectedProfileChanged& OnProfileSelected()
		{
			return ProfileSelectedDelegate;
		}

		void SelectProfile(const ILauncherProfilePtr& Profile);
		void SortProfiles();

		EProfileType GetProfileType(const ILauncherProfileRef& Profile) const;
		bool IsAdvancedProfile(const ILauncherProfileRef& Profile) const;
		bool IsBasicLaunchProfile( const ILauncherProfilePtr& Profile ) const;

		ILauncherProfileRef CreateCustomProfile( const TCHAR* Name );
		ILauncherProfileRef CreateBasicLaunchProfile();
		ILauncherProfilePtr CloneCustomProfile(const ILauncherProfileRef& Profile);

		const ILauncherProfileRef GetDefaultBasicLaunchProfile() const;
		const ILauncherProfileRef GetDefaultCustomLaunchProfile() const;

		EContentScheme DetermineProfileContentScheme(const ILauncherProfileRef& Profile) const;
		void SetProfileContentScheme(EContentScheme ContentScheme, const ILauncherProfileRef& Profile, bool bWantToCook = true, ELauncherProfileDeploymentModes::Type DefaultDeploymentMode = ELauncherProfileDeploymentModes::CopyToDevice);

		TSharedPtr<FLaunchLogMessage> AddLogMessage( const FString& InMessage, ELogVerbosity::Type InVerbosity = ELogVerbosity::Log );
		void ClearLogMessages();
		int32 GetNumLogMessages() const { return LaunchLogMessages.Num(); }


		static const PlatformInfo::FTargetPlatformInfo* GetPlatformInfo(const ILauncherProfilePtr& Profile);
		static const PlatformInfo::FTargetPlatformInfo* GetPlatformInfo(FName PlatformName, const FTargetInfo& BuildTargetInfo);

		static bool IsHostPlatform(const ILauncherProfilePtr& Profile);
		static bool IsHostPlatform(FName PlatformName);

		static FTargetInfo GetBuildTargetInfo( const FString& BuildTargetName, const FString& ProjectPath );
		static FTargetInfo GetBuildTargetInfo( const ILauncherProfileRef& Profile );

		static TSharedPtr<ITargetDeviceProxy> GetDeviceProxy(const ILauncherProfileRef& Profile);
		static void UpdatedCookedPlatformsFromDeployDeviceProxy(const ILauncherProfileRef& Profile, TSharedPtr<ITargetDeviceProxy> DeviceProxy = nullptr);
		static void UpdateCookedPlatformsFromBuildTarget(const ILauncherProfileRef& Profile);

		static FString GetVanillaPlatformName( const FString& PlatformName );
		static FString GetBuildTargetPlatformName( const FString& PlatformName, const FTargetInfo& BuildTargetInfo );

		const FProjectSettings GetProjectSettings( const FString& ProjectPath );
		const FProjectSettings GetProjectSettings( const ILauncherProfileRef& Profile );

		const TCHAR* GetConfigSection() const;
		const FString& GetConfigIni() const;

		TArray<FString> GetAvailableProjectMapNames( const FString& InProjectPath );
		TArray<FString> GetAvailableProjectMapPaths( const FString& InProjectPath );
		TArray<FString> GetAvailableEngineMapNames();
		TArray<FString> GetAvailableEngineMapPaths();

	private:
		FString ConfigFileName;

		void LoadConfig();
		void SaveConfig();

		void HandleProfileManagerProfileAdded(const ILauncherProfileRef& Profile);
		void HandleProfileManagerProfileRemoved(const ILauncherProfileRef& Profile);
		void HandleDeviceProxyAdded(const TSharedRef<ITargetDeviceProxy>& DeviceProxy );
		void HandleDeviceProxyRemoved(const TSharedRef<ITargetDeviceProxy>& DeviceProxy );

		void ReadProjectSettingsFromConfig( FConfigCacheIni& InConfig, FProjectSettings& OutResult );

		void OnModifyLaunchCommandLine( const ILauncherProfileRef& InProfile, FString& InOutCommandLine );

		TArray<FString> GetAndCacheMapPaths(const FString& InOptionalProjectPath);

		TSharedRef<ITargetDeviceProxyManager> DeviceProxyManager;
		TSharedRef<ILauncher> Launcher;
		TSharedRef<ILauncherProfileManager> ProfileManager;

		ILauncherProfilePtr SelectedProfile;
		TArray<ILauncherProfilePtr> AllProfiles;
		ILauncherProfilePtr BasicLaunchProfile;
		bool bHasSetBasicLaunchProfilePlatform = false;

		friend class FLaunchLogTextLayoutMarshaller;
		TArray<TSharedPtr<FLaunchLogMessage>> LaunchLogMessages;

		TMap<FString,FProjectSettings> CachedProjectSettings;
		TMap<FString,TArray<FString>> CachedMapPaths;

		FOnSelectedProfileChanged ProfileSelectedDelegate;
		FOnSelectedPlatformChanged PlatformChangedDelegate;

		ILauncherProfilePtr DefaultBasicLaunchProfile;
		ILauncherProfilePtr DefaultCustomLaunchProfile;
	};
}
