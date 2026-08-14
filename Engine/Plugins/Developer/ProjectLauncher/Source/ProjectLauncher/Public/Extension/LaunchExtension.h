// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "ProfileTree/LaunchProfileTreeData.h"
#include "ILauncherProfile.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

namespace ProjectLauncher
{
	class FLaunchExtension;

	/**
	 * Base class for a launch extension instance.
	 * Used while editing a specific profile, and when finalizing the command line arguments during profile launch.
	 * 
	 * Created by a specialization of FLaunchExtension as follows:
	 * 
	 * TSharedPtr<ProjectLauncher:FLaunchExtensinInstance> FMyLaunchExtension::CreateInstanceForProfile( ProjectLauncher::FLaunchExtensionInstance::FArgs& InArgs )
	 * {
	 *     return MakeShared<FMyLaunchExtensionInstance>(InArgs);
	 * }
	 */
	class PROJECTLAUNCHER_API FLaunchExtensionInstance : public TSharedFromThis<FLaunchExtensionInstance>
	{
	public:
		struct FArgs
		{
			ILauncherProfileRef Profile;
			ILaunchProfileTreeBuilder* TreeBuilder;
			TSharedRef<FModel> Model;
			TSharedRef<FLaunchExtension> Extension;
		};

		FLaunchExtensionInstance( FArgs& InArgs );
		virtual ~FLaunchExtensionInstance();

		/**
		 * Returns the parameters that this extension provides. They will be added to the submenu.
		 */
		virtual bool GetExtensionParameters( TArray<FString>& OutParameters ) const;

		/**
		 * Returns the user-facing name for the given parameter. It will default to the parameter itself.
		 */
		virtual FText GetExtensionParameterDisplayName( const FString& InParameter ) const;

		/**
		 * Returns the user facing variables that this extension provides, in "$(name)" format
		 */
		virtual bool GetExtensionVariables( TArray<FString>& OutVariables ) const;

		/**
		 * Returns the current value for the given variable
		 */
		virtual bool GetExtensionVariableValue( const FString& InParameter, FString& OutValue ) const;

		/**
		 * Hook to allow the extension to extend the extension parameters menu
		 */
		virtual void CustomizeParametersSubmenu( FMenuBuilder& MenuBuilder ) {};

		/**
		 * Hook to allow the extension to add extra fields to the property editing tree, if the tree builder allows it
		 * Property tree items should be hidden until the user has selected something to make it relevent, to avoid cluttering the UI.
		 */
		virtual void CustomizeTree( FLaunchProfileTreeData& ProfileTreeData ) {};

		/**
		 * Advanced hook to allow any advanced modification of the command line when our profile is launched
		 */
		virtual void CustomizeLaunchCommandLine( FString& InOutCommandLine ) {};


		/**
		 * Determine if this extension also provides items in the bespoke extensions menu
		 */
		virtual bool HasCustomExtensionMenu() const { return false; }

		/*
		 * Populate the custom extension menu for this extension. This menu would typically contain
		 * items that might enable bespoke tree customization options for example.
		 */
		virtual void MakeCustomExtensionSubmenu(FMenuBuilder& MenuBuilder) { checkNoEntry(); }


	protected:

		/**
		 * Determine if the given parameter is on the command line
 		 * 
		 * @param InParameter the parameter to test, e.g. -key=value or -param
		 * @returns whether the parameter is being used
		 */
		bool IsParameterUsed( const FString& InParameter ) const;

		/** 
		 * Add or remove the given parameter on the command line
		 * 
		 * @param InParameter the parameter to add or remove, e.g. -key=value or -param
		 * @param bUsed whether to add or remove the parameter
		 */
		void SetParameterUsed( const FString& InParameter, bool bUsed );

		/** 
		 * Add the given parameter to the command line
		 * 
		 * @param InParameter the parameter to add, e.g. -key=value or -param
		 */
		void AddParameter( const FString& InParameter );

		/** 
		 * Remove the given parameter from the command line
		 * 
		 * @param InParameter the parameter to remove, e.g. -key=value or -param
		 */
		void RemoveParameter( const FString& InParameter );

		/** 
		 * Retrieve the current value for the given command line parameter
		 * 
		 * @param InParameter the parameter to query, e.g. -key=value
		 * @returns the value of the parameter
		 */
		FString GetParameterValue( const FString& InParameter ) const;

		/** 
		 * Update the value of the given command line parameter
		 * 
		 * @param InParameter the parameter to modify, e.g. -key=value or -key=
		 * @param InNewValue the new value to use
		 * @returns true unless InParameter is not a key/value property
		 */
		bool UpdateParameterValue( const FString& InParameter, const FString& InNewValue );

		/** 
		 * Get the current state of the given command line parameter, allowing for user-changed values
		 * 
		 * @param InParameter the parameter to test, e.g. -key=value
		 * @returns The parameter as it is now, allowing for user modifications, e.g. -key=value or -key=some_new_value
		 */
		FString GetFinalParameter( const FString& InParameter ) const;

		/** 
		 * Get the current command line
		 * Extensions should generally go through this function instead of querying the Profile directlry
		 *
		 * @returns the current command line string
		 */
		FString GetCommandLine() const;

		/** 
		 * Update the current command line
		 * Extensions should generally go through this function instead of modifying the Profile directlry
		 * 
		 * @param CommandLine the new command line
		 */
		void SetCommandLine( const FString& CommandLine );

		/** 
		 * Get the profile we were instantiated for
		 */
		inline ILauncherProfileRef GetProfile() const { return Profile; }

		/** 
		 * Get the model, for general purpose helper functions
		 */
		inline TSharedRef<FModel> GetModel() const { return Model; }

		/** 
		 * Get the extension that instantiated us
		 */
		inline TSharedRef<FLaunchExtension> GetExtension() const { return Extension; }


		/** 
		* Enumeration to choose where a value should be stored
		*/
		enum class EConfig : uint8
		{
			User_Common,     ///< value is shared between all instances of this extension
			User_PerProfile, ///< value is specific to this profile & extension
			PerProfile,      ///< value is saved with the profile
		};

		/**
		 * Read a configuration string value
		 * 
		 * @param Config where the value is stored
		 * @param Name name of the value to read
		 * @param DefaultValue value to return if the value isn't found
		 * @returns the value of the string, or DefaultValue if it isn't found
		 */
		FString GetConfigString( EConfig Config, const TCHAR* Name, const TCHAR* DefaultValue = TEXT("") ) const;

		/**
		 * Read a configuration bool value
		 * 
		 * @param Config where the value is stored
		 * @param Name name of the value to read
		 * @param DefaultValue value to return if the value isn't found
		 * @returns the value of the bool, or DefaultValue if it isn't found
		 */
		bool GetConfigBool( EConfig Config, const TCHAR* Name, bool DefaultValue = false ) const;

		/**
		 * Write a configuration string value
		 * 
		 * @param Config where the value is stored
		 * @param Name name of the value to write
		 * @param Value value to write
		 */
		void SetConfigString( EConfig Config, const TCHAR* Name, const FString& Value ) const;

		/**
		 * Write a configuration bool value
		 * 
		 * @param Config where the value is stored
		 * @param Name name of the value to write
		 * @param Value value to write
		 */
		void SetConfigBool( EConfig Config, const TCHAR* Name, const bool& Value ) const;

		/**
		 * Get the final key name to use for reading & writing a configuration value
		 * 
		 * @param Config where the value is stored
		 * @param Name name of the value
		 * 
		 * @returns namespaced version of Name
		 */
		FString GetConfigKeyName( EConfig Config, const TCHAR* Name ) const;



	private:
		bool IsParameterGroup( const FString& InParameter ) const;
		bool TryRemoveParameterGroup( const FString& InParameter );


		ILauncherProfileRef Profile;
		ILaunchProfileTreeBuilder* TreeBuilder;
		TSharedRef<FModel> Model;
		TSharedRef<FLaunchExtension> Extension;

		friend class FLaunchProfileTreeNode;
		void MakeCommandLineSubmenu( FMenuBuilder& MenuBuilder );

	};



	/**
	 * Base class for a launch extension.
	 * 
	 * Singleton instance is registered with this plugin during initialization as follows:
	 * 
	 *   TSharedPtr<FMyLaunchExtension> MyExtension = MakeShared<FMyLaunchExtension>();
	 *   IProjectLauncherModule::Get().RegisterExtension( MyExtension.ToSharedRef() );
	 */
	class PROJECTLAUNCHER_API FLaunchExtension : public TSharedFromThis<FLaunchExtension>
	{
	public:
		virtual ~FLaunchExtension() = default;

		/**
		 * Create an instance of the launch extension for the given profile
		 *
		 * @returns: new instance, or null if it isn't appropriate
		 */
		virtual TSharedPtr<FLaunchExtensionInstance> CreateInstanceForProfile( FLaunchExtensionInstance::FArgs& InArgs ) = 0;

		/**
		 * Returns the debug name for this extension
		 */
		virtual const TCHAR* GetInternalName() const = 0;

		/**
		 * Returns the user-facing name for this extension
		 */
		virtual FText GetDisplayName() const = 0;


		/**
		 * Instantiate all compatible extensions
		 * 
		 * @param InProfile the current profile 
		 * @param InTreeBuilder the active tree builder
		 * @param InModel helper class
		 * @returns array of all compatible extensions
		 */
		static TArray<TSharedPtr<FLaunchExtensionInstance>> CreateExtensionInstancesForProfile( ILauncherProfileRef InProfile, TSharedRef<FModel> InModel, ILaunchProfileTreeBuilder* InTreeBuilder );
	};


};

