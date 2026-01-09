// Copyright Epic Games, Inc. All Rights Reserved.

#include "Extension/LaunchExtension.h"
#include "Model/ProjectLauncherModel.h"
#include "ProfileTree/ILaunchProfileTreeBuilder.h"
#include "Containers/Array.h"
#include "Templates/SharedPointer.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "ILauncherServicesModule.h"

#define LOCTEXT_NAMESPACE "FLaunchExtensionInstance"


namespace ProjectLauncher
{
	static TArray<TSharedPtr<FLaunchExtension>> GExtensions;
	static FCriticalSection GExtensionsCS;


	void RegisterExtension( TSharedRef<FLaunchExtension> Extension )
	{
		FScopeLock Lock(&GExtensionsCS);
		GExtensions.Add(Extension);
	}

	void UnregisterExtension( TSharedRef<FLaunchExtension> Extension )
	{
		FScopeLock Lock(&GExtensionsCS);
		GExtensions.Remove(Extension);
	}


	void ApplyExtensionVariables( const ILauncherProfileRef& InProfile, FString& InOutCommandLine, TSharedRef<FModel> InModel )
	{
		// take a snapshot of the extension list
		TArray<TSharedPtr<FLaunchExtension>> Extensions;
		{
			FScopeLock Lock(&GExtensionsCS);
			Extensions = GExtensions;
		}


		for (TSharedPtr<FLaunchExtension> Extension : Extensions)
		{
			// instantiate the extension and apply the extension variables
			FLaunchExtensionInstance::FArgs Args
			{
				.Profile = InProfile,
				.TreeBuilder = nullptr,
				.Model = InModel,
				.Extension = Extension.ToSharedRef(),
			};
			TSharedPtr<FLaunchExtensionInstance> ExtensionInstance = Extension->CreateInstanceForProfile(Args);
			if (ExtensionInstance.IsValid())
			{
				// apply variable substitutions
				TArray<FString> Variables;
				if (ExtensionInstance->GetExtensionVariables(Variables))
				{
					for (const FString& Variable : Variables)
					{
						if (InOutCommandLine.Contains(Variable, ESearchCase::IgnoreCase))
						{
							FString VariableValue;
							if (ExtensionInstance->GetExtensionVariableValue( Variable, VariableValue ))
							{
								InOutCommandLine.ReplaceInline( *Variable, *VariableValue, ESearchCase::IgnoreCase );
							}
						}
					}
				}

				// allow for advanced command line customization
				ExtensionInstance->CustomizeLaunchCommandLine( InOutCommandLine );
			}
		}
	}




	TArray<TSharedPtr<FLaunchExtensionInstance>> FLaunchExtension::CreateExtensionInstancesForProfile( ILauncherProfileRef InProfile, TSharedRef<FModel> InModel, ILaunchProfileTreeBuilder* InTreeBuilder )
	{
		// take a snapshot of the extension list
		TArray<TSharedPtr<FLaunchExtension>> Extensions;
		{
			FScopeLock Lock(&GExtensionsCS);
			Extensions = GExtensions;
		}

		// attempt to instantiate all extensions
		TArray<TSharedPtr<FLaunchExtensionInstance>> Result;
		for (TSharedPtr<FLaunchExtension> Extension : Extensions)
		{
			FLaunchExtensionInstance::FArgs Args
			{
				.Profile = InProfile,
				.TreeBuilder = InTreeBuilder,
				.Model = InModel,
				.Extension = Extension.ToSharedRef(),
			};

			TSharedPtr<FLaunchExtensionInstance> ExtensionInstance = Extension->CreateInstanceForProfile(Args);
			if (ExtensionInstance.IsValid())
			{
				Result.Add(ExtensionInstance);
			}
		}

		return Result;
	}











	FLaunchExtensionInstance::FLaunchExtensionInstance( FArgs& InArgs )
		: Profile(InArgs.Profile)
		, TreeBuilder(InArgs.TreeBuilder)
		, Model(InArgs.Model)
		, Extension(InArgs.Extension)
	{
	}

	FLaunchExtensionInstance::~FLaunchExtensionInstance()
	{
	}

	bool FLaunchExtensionInstance::GetExtensionVariables( TArray<FString>& OutVariables ) const
	{
		return false;
	}

	bool FLaunchExtensionInstance::GetExtensionVariableValue( const FString& InParameter, FString& OutValue ) const
	{
		return false;
	}

	FText FLaunchExtensionInstance::GetExtensionParameterDisplayName( const FString& InParameter ) const
	{
		return FText::FromString(InParameter);
	}

	bool FLaunchExtensionInstance::GetExtensionParameters( TArray<FString>& OutParameters ) const
	{
		return false;
	}

	FString FLaunchExtensionInstance::GetParameterValue( const FString& InParameter ) const
	{
		FString CommandLine = GetCommandLine();

		// InParameter is -Key=
		FString ParamValue;
		if (FParse::Value(*CommandLine, *InParameter, ParamValue, false))
		{
			return ParamValue;
		}

		// InParameter is -Key=Value
		FString ParamKey;
		if (InParameter.Split(TEXT("="), &ParamKey, &ParamValue))
		{
			ParamKey += TEXT("=");
			if (FParse::Value(*CommandLine, *ParamKey, ParamValue, false))
			{
				return ParamValue;
			}
		}

		return FString();
	}

	bool FLaunchExtensionInstance::UpdateParameterValue( const FString& InParameter, const FString& NewValue )
	{
		FString ParamKey, ParamValue;
		if (InParameter.Split(TEXT("="), &ParamKey, &ParamValue))
		{
			RemoveParameter(InParameter);

			FString NewParameter = ParamKey + "=" + NewValue;
			AddParameter(NewParameter);
			return true;
		}

		return false;

	}



	FString FLaunchExtensionInstance::GetFinalParameter( const FString& InParameter ) const
	{
		FString CommandLine = GetCommandLine();

		// get the parameter's key & value
		FString ParamName, ParamValue;
		bool bParameterHasValue = InParameter.Split(TEXT("="), &ParamName, &ParamValue);
		if (!bParameterHasValue)
		{
			ParamName = InParameter;
		}

		// the parameter's value may have been added or alterered - return how it is now
		FString ParamKey = ParamName + TEXT("=");
		if (FParse::Value(*CommandLine, *ParamKey, ParamValue, false))
		{
			return ParamKey + ParamValue;
		}
		
		// the parameter may not have a value or it has been removed - return how it is now
		FString Param = ParamName;
		if (Param.RemoveFromStart(TEXT("-")) && FParse::Param(*CommandLine, *Param))
		{
			return ParamName;
		}

		// return the parameter as-is
		return InParameter;
	}

	void FLaunchExtensionInstance::AddParameter( const FString& InParameter )
	{
		FString CommandLine = GetCommandLine() + TEXT(" ") + InParameter;
		SetCommandLine(CommandLine);
	}

	void FLaunchExtensionInstance::RemoveParameter( const FString& InParameter )
	{
		if (TryRemoveParameterGroup(InParameter))
		{
			return;
		}

		FString Parameter = GetFinalParameter(InParameter);
		FString CommandLine = GetCommandLine();

		// first try to remove the parameter and the preceding space
		FString ParameterWithSpace = TEXT(" ") + Parameter;
		if (CommandLine.ReplaceInline(*ParameterWithSpace, TEXT(""), ESearchCase::IgnoreCase) > 0)
		{
			SetCommandLine(CommandLine);
			return;
		}

		// next try to remove the parameter with a trailing space
		ParameterWithSpace = Parameter + TEXT(" ");
		if (CommandLine.ReplaceInline(*ParameterWithSpace, TEXT(""), ESearchCase::IgnoreCase) > 0)
		{
			SetCommandLine(CommandLine);
			return;
		}

		// a little unexpected... just remove the parameter on its own
		if (CommandLine.ReplaceInline(*Parameter, TEXT(""), ESearchCase::IgnoreCase) > 0)
		{
			SetCommandLine(CommandLine);
			return;
		}
	}


	bool FLaunchExtensionInstance::IsParameterUsed( const FString& InParameter ) const
	{
		FString Parameter = GetFinalParameter(InParameter);
		return GetCommandLine().Contains(Parameter, ESearchCase::IgnoreCase);
	}

	void FLaunchExtensionInstance::SetParameterUsed( const FString& InParameter, bool bUsed )
	{
		bool bIsUsed = IsParameterUsed(InParameter);
		if (bIsUsed != bUsed)
		{
			if (bIsUsed)
			{
				RemoveParameter(InParameter);
			}
			else
			{
				AddParameter(InParameter);
			}
		}
	}


	bool FLaunchExtensionInstance::IsParameterGroup( const FString& InParameter ) const
	{
		// see if InParameter contains just a simple -Key=Value or -Param
		FString ParsedParameter = InParameter;
		FString ParamKey, ParamValue;
		if (InParameter.Split(TEXT("="), &ParamKey, &ParamValue))
		{
			ParamKey += TEXT("=");
			if (FParse::Value(*InParameter, *ParamKey, ParamValue, false))
			{
				ParsedParameter = ParamKey + ParamValue;
			}
		}
		
		return (ParsedParameter.TrimStartAndEnd() != InParameter.TrimStartAndEnd());
	}

	bool FLaunchExtensionInstance::TryRemoveParameterGroup( const FString& InParameter )
	{
		if (!IsParameterGroup(InParameter))
		{
			return false;
		}

		// it may contain a group - handle them separately
		const TCHAR* Ptr = InParameter.GetCharArray().GetData();
		FString SubParameter = FParse::Token(Ptr, false);
		while (!SubParameter.IsEmpty())
		{
			RemoveParameter(SubParameter);
			SubParameter = FParse::Token(Ptr, false);
		}

		return true;
	}
		



	void FLaunchExtensionInstance::MakeCommandLineSubmenu( FMenuBuilder& MenuBuilder)
	{
		auto ToggleParameter = [this]( FString Parameter )
		{
			if (IsParameterUsed(Parameter))
			{
				RemoveParameter(Parameter);
			}
			else
			{
				AddParameter(Parameter);
			}
		};

		TArray<FString> Parameters;
		if (GetExtensionParameters(Parameters) && Parameters.Num() > 0)
		{
			for (const FString& Parameter : Parameters)
			{
				FText ParameterDisplayName = GetExtensionParameterDisplayName(Parameter);
				FText ParameterToolTip = FText::FromString(Parameter);

				MenuBuilder.AddMenuEntry( 
					ParameterDisplayName,
					ParameterToolTip,
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda(ToggleParameter, Parameter),
						FCanExecuteAction(),
						FIsActionChecked::CreateLambda( [this, Parameter]() { return IsParameterUsed(Parameter); } )
					),
					NAME_None,
					EUserInterfaceActionType::Check);
			}
		}

		CustomizeParametersSubmenu(MenuBuilder);
	}

	FString FLaunchExtensionInstance::GetCommandLine() const
	{
		return Profile->GetAdditionalCommandLineParameters();
	}

	void FLaunchExtensionInstance::SetCommandLine( const FString& CommandLine )
	{
		Profile->SetAdditionalCommandLineParameters(CommandLine);
		TreeBuilder->OnPropertyChanged();
	}




	FString FLaunchExtensionInstance::GetConfigString( EConfig Config, const TCHAR* Name, const TCHAR* DefaultValue ) const
	{
		FString KeyName = GetConfigKeyName(Config, Name);

		if (Config == EConfig::PerProfile)
		{
			const FString* ValuePtr = Profile->GetCustomStringProperties().Find( KeyName );
			return ValuePtr ? *ValuePtr : DefaultValue;
		}
		else
		{
			FString Value;
			return GConfig->GetString( Model->GetConfigSection() , *KeyName, Value, Model->GetConfigIni() ) ? Value : DefaultValue;
		}
	}

	bool FLaunchExtensionInstance::GetConfigBool( EConfig Config, const TCHAR* Name, bool DefaultValue ) const
	{
		FString KeyName = GetConfigKeyName(Config, Name);

		if (Config == EConfig::PerProfile)
		{
			const bool* ValuePtr = Profile->GetCustomBoolProperties().Find( KeyName );
			return ValuePtr ? *ValuePtr : DefaultValue;

		}
		else
		{
			bool Value;
			return GConfig->GetBool( Model->GetConfigSection() , *KeyName, Value, Model->GetConfigIni() ) ? Value : DefaultValue;
		}
	}


	void FLaunchExtensionInstance::SetConfigString( EConfig Config, const TCHAR* Name, const FString& Value ) const
	{
		FString KeyName = GetConfigKeyName(Config, Name);

		if (Config == EConfig::PerProfile)
		{
			if (Value.IsEmpty())
			{
				Profile->GetCustomStringProperties().Remove(KeyName);
			}
			else
			{
				Profile->GetCustomStringProperties().Add(KeyName, Value);
			}
			TreeBuilder->OnPropertyChanged();
		}
		else
		{
			GConfig->SetString( Model->GetConfigSection() , *KeyName, *Value, Model->GetConfigIni() );
		}
	}
	void FLaunchExtensionInstance::SetConfigBool( EConfig Config, const TCHAR* Name, const bool& Value ) const
	{
		FString KeyName = GetConfigKeyName(Config, Name);

		if (Config == EConfig::PerProfile)
		{
			if (Value == false)
			{
				Profile->GetCustomBoolProperties().Remove(KeyName);
			}
			else
			{
				Profile->GetCustomBoolProperties().Add(KeyName, Value);
			}
			TreeBuilder->OnPropertyChanged();
		}
		else
		{
			GConfig->SetBool( Model->GetConfigSection() , *KeyName, Value, Model->GetConfigIni() );
		}
	}

	FString FLaunchExtensionInstance::GetConfigKeyName( EConfig Config, const TCHAR* Name ) const
	{
		switch (Config)
		{
			case EConfig::PerProfile:
			case EConfig::User_Common:
			{
				return FString::Printf(TEXT("%s.%s"), Extension->GetInternalName(), Name );
			}
			break;

			case EConfig::User_PerProfile:
			{
				FGuid ProfileId = Profile->GetId();
				return FString::Printf(TEXT("%s.%s.%s"), Extension->GetInternalName(), *ProfileId.ToString(), Name );
			}
			break;
		}

		checkNoEntry();
		return Name;
	}
}

#undef LOCTEXT_NAMESPACE
