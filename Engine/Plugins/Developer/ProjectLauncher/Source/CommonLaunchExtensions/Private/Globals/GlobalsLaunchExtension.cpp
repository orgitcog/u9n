// Copyright Epic Games, Inc. All Rights Reserved.

#include "Globals/GlobalsLaunchExtension.h"
#include "SocketSubsystem.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Trace/Trace.h"

#define LOCTEXT_NAMESPACE "FGlobalsLaunchExtensionInstance"


const TCHAR* FGlobalsLaunchExtensionInstance::LocalHostVariable     = TEXT("$(LocalHost)");
const TCHAR* FGlobalsLaunchExtensionInstance::ProjectNameVariable   = TEXT("$(ProjectName)");
const TCHAR* FGlobalsLaunchExtensionInstance::ProjectPathVariable   = TEXT("$(ProjectPath)");
const TCHAR* FGlobalsLaunchExtensionInstance::TargetNameVariable    = TEXT("$(TargetName)");
const TCHAR* FGlobalsLaunchExtensionInstance::PlatformNameVariable  = TEXT("$(Platform)");
const TCHAR* FGlobalsLaunchExtensionInstance::ConfigurationVariable = TEXT("$(Configuration)");

bool FGlobalsLaunchExtensionInstance::GetExtensionVariables( TArray<FString>& OutVariables ) const
{
	OutVariables.Add(LocalHostVariable);
	OutVariables.Add(ProjectNameVariable);
	OutVariables.Add(ProjectPathVariable);
	OutVariables.Add(TargetNameVariable);
	OutVariables.Add(PlatformNameVariable);
	OutVariables.Add(ConfigurationVariable);
	return true;
}

bool FGlobalsLaunchExtensionInstance::GetExtensionVariableValue( const FString& InVariable, FString& OutValue ) const
{
	if (InVariable == LocalHostVariable)
	{
		// @todo: what if we have multiple host addresses etc etc. ?
		if (ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
		{
			bool bCanBindAll;
			TSharedPtr<FInternetAddr> LocalHostAddr = SocketSubsystem->GetLocalHostAddr(*GLog, bCanBindAll);

			if (LocalHostAddr->IsValid())
			{
				constexpr bool bAppendPort = false;
				OutValue = LocalHostAddr->ToString(bAppendPort);
				return true;
			}
		}

		OutValue = TEXT("localhost");
		return true;
	}

	else if (InVariable == ProjectNameVariable)
	{
		OutValue = GetProfile()->GetProjectName();
		return true;
	}
	else if (InVariable == ProjectPathVariable)
	{
		OutValue = GetProfile()->GetProjectPath();
		return true;
	}
	else if (InVariable == TargetNameVariable)
	{
		OutValue = GetProfile()->GetBuildTarget();
		return true;
	}
	else if (InVariable == PlatformNameVariable)
	{
		OutValue = FString::Join( GetProfile()->GetCookedPlatforms(), TEXT("+") );
		return true;
	}
	else if (InVariable == ConfigurationVariable)
	{
		OutValue = LexToString( GetProfile()->GetBuildConfiguration() );
	}

	return false;
}



TSharedPtr<ProjectLauncher::FLaunchExtensionInstance> FGlobalsLaunchExtension::CreateInstanceForProfile( ProjectLauncher::FLaunchExtensionInstance::FArgs& InArgs )
{
	return MakeShared<FGlobalsLaunchExtensionInstance>(InArgs);
}

const TCHAR* FGlobalsLaunchExtension::GetInternalName() const
{
	return TEXT("Globals");
}

FText FGlobalsLaunchExtension::GetDisplayName() const
{
	return LOCTEXT("ExtensionName", "Globals");
}


#undef LOCTEXT_NAMESPACE
