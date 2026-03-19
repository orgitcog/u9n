// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProfileTree/ILaunchProfileTreeBuilder.h"
#include "Model/ProjectLauncherModel.h"

class SSearchableComboBox;
class SCustomLaunchMapListView;
class SCustomLaunchDeviceListView;
namespace ESelectInfo { enum Type : int; }

namespace ProjectLauncher
{
	/**
	 * Base class for a profile tree builder that creates FLaunchProfileTreeData from a given ILauncherProfile.
	 * 
	 * Expected to be created by an instance of ILaunchProfileTreeBuilderFactory, for example:
	 * 
	 *    TSharedPtr<ILaunchProfileTreeBuilder> FMyProfileTreeBuilderFactory::TryCreateTreeBuilder( const ILauncherProfileRef& InProfile, const TSharedRef<FModel>& InModel )
	 *    {
	 *        return MakeShared<FMyProfileTreeBuilder>(InProfile, InModel);
	 *    }
	 */
	class PROJECTLAUNCHER_API FGenericProfileTreeBuilder : public ILaunchProfileTreeBuilder, public TSharedFromThis<FGenericProfileTreeBuilder>
	{
	public:
		FGenericProfileTreeBuilder( const ILauncherProfileRef& Profile, const ILauncherProfileRef& InDefaultProfile, const TSharedRef<FModel>& InModel );
		virtual ~FGenericProfileTreeBuilder() = default;

		virtual void Construct() override;

		virtual FLaunchProfileTreeDataRef GetProfileTree() override
		{
			return TreeData;
		}

		virtual bool AllowExtensionsUI() const override
		{
			return true;
		}

	protected:
		// default property creation functions
		void AddProjectProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddTargetProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddPlatformProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddConfigurationProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddContentSchemeProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddCompressPakFilesProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddUseIoStoreProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddGenerateChunksProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddImportZenSnapshotProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddZenPakStreamingPathProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddIncrementalCookProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddCookProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddMapsToCookProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddAdditionalCookerOptionsProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddBuildProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddForceBuildProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddBuidUATProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddArchitectureProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddStagingDirectoryProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddArchiveBuildProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddArchiveBuildDirectoryProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddDeployProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddIncrementalDeployProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddTargetDeviceProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddRunProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddInitialMapProperty( FLaunchProfileTreeNode& HeadingNode );
		void AddCommandLineProperty( FLaunchProfileTreeNode& HeadingNode );

		// helper callbacks to simplify control enable/visibility
		FLaunchProfileTreeNode::FGetBool ForPak;
		FLaunchProfileTreeNode::FGetBool ForZenWS;
		FLaunchProfileTreeNode::FGetBool ForCooked;
		FLaunchProfileTreeNode::FGetBool ForEnabledCooked;
		FLaunchProfileTreeNode::FGetBool ForContent;
		FLaunchProfileTreeNode::FGetBool ForCode;
		FLaunchProfileTreeNode::FGetBool ForDeployment;
		FLaunchProfileTreeNode::FGetBool ForRun;
		FLaunchProfileTreeNode::FGetString EmptyString;



		TArray<FString> CachedMapsToCook;
		enum class EMapOption : uint8
		{
			Startup,
			Selected,
		};
		EMapOption MapOption = EMapOption::Startup;

		TArray<FString> CachedDeployDeviceIDs;
		enum class EDeployDeviceOption : uint8
		{
			Default,
			Selected,
		};
		EDeployDeviceOption DeployDeviceOption = EDeployDeviceOption::Default;


		FString GetProjectPath() const;
		void SetProjectName(FString ProjectPath);
		bool HasProject() const;

		FString GetBuildTarget() const;
		void SetBuildTarget(FString BuildTarget);
		TArray<EBuildTargetType> GetSupportedBuildTargetTypes() const;

		void SetBuildConfiguration(EBuildConfiguration BuildConfiguration);
		EBuildConfiguration GetBuildConfiguration() const;

		void SetContentScheme(EContentScheme ContentScheme);
		bool IsContentSchemeAvailable(EContentScheme, FText& OutReason) const;

		FString GetCommandLine() const;
		void SetCommandLine( const FString& NewCommandLine );

		void SetSelectedPlatforms( TArray<FString> SelectedPlatforms );
		TArray<FString> GetSelectedPlatforms() const;

		void SetCook( bool bCook );
		bool GetCook( ILauncherProfilePtr InProfile = nullptr ) const;

		void SetIncrementalCookMode( ELauncherProfileIncrementalCookMode::Type Mode );
		ELauncherProfileIncrementalCookMode::Type GetIncrementalCookMode() const;

		void SetMapsToCook(TArray<FString> MapsToCook);
		TArray<FString> GetMapsToCook() const;
		EMapOption GetMapOption() const;
		void SetMapOption( EMapOption MapOption );
		float GetMapListHeight() const;
		void SetMapListHeight( float NewHeight );
		TSharedRef<SWidget> CreateMapListWidget();
		float MapListHeight = 300.0f;


		void SetDeployDeviceIDs(TArray<FString> DeployDeviceIDs);
		TArray<FString> GetDeployDeviceIDs() const;
		EDeployDeviceOption GetDeployDeviceOption() const;
		void SetDeployDeviceOption( EDeployDeviceOption DeployDeviceOption );
		float GetDeployDeviceListHeight() const;
		void SetDeployDeviceListHeight( float NewHeight );
		void OnDeviceRemoved(FString DeviceID);
		TSharedRef<SWidget> CreateDeployDeviceWidget();
		float DeployDeviceListHeight = 150.0f;

		void SetBuild( bool bBuild );
		bool GetBuild( ILauncherProfilePtr InProfile = nullptr ) const;

		void SetForceBuild( bool bForceBuild );
		bool GetForceBuild( ILauncherProfilePtr InProfile = nullptr ) const;

		void SetArchitecture( FString Architecture );
		FString GetArchitecture() const;
		FText GetArchitectureDisplayName( FString Architecture );

		void SetDeployToDevice( bool bDeployToDevice );
		bool GetDeployToDevice( ILauncherProfilePtr InProfile = nullptr ) const;

		void SetIsRunning( bool bRun );
		bool GetIsRunning( ILauncherProfilePtr InProfile = nullptr ) const;

		void OnInitialMapChanged( TSharedPtr<FString> InitialMap, ESelectInfo::Type );
		TSharedPtr<FString> GetInitialMap() const;



	protected:

		virtual void OnPropertyChanged() override;
		void RefreshContentScheme();
		void CacheStartupMapList() const;
		void CacheArchitectures();
		TSharedRef<SWidget> OnGenerateComboWidget( TSharedPtr<FString> InComboString );


		FLaunchProfileTreeDataRef TreeData;
		const ILauncherProfileRef Profile;
		const ILauncherProfileRef DefaultProfile;
		EProfileType ProfileType;

		const TSharedRef<FModel> Model;

		EContentScheme ContentScheme;
		bool bShouldCook;
		mutable bool bStartupMapCacheDirty = true;
		mutable TArray<TSharedPtr<FString>> CachedStartupMaps;
		TSharedPtr<SSearchableComboBox> InitalMapCombo;
		TSharedPtr<SCustomLaunchMapListView> MapListView;
		TSharedPtr<SCustomLaunchDeviceListView> DeployDeviceListView;
		TArray<FString> CachedArchitectures;
		EBuildTargetType CachedBuildTargetType = EBuildTargetType::Game;
	};
}
