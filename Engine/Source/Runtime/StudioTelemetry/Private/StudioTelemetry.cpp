// Copyright Epic Games, Inc. All Rights Reserved.

#include "StudioTelemetry.h"
#include "StudioTelemetryLog.h"
#include "Analytics.h"
#include "AnalyticsProviderBroadcast.h"
#include "AnalyticsTracer.h"
#include "BuildSettings.h"
#include "RHI.h"

#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Thread.h"

#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineBuildSettings.h"

#include "GenericPlatform/GenericPlatformMisc.h"

DEFINE_LOG_CATEGORY(LogStudioTelemetry);

IMPLEMENT_MODULE(FStudioTelemetry, StudioTelemetry)

FString GetEnvironmentValriable(const FString& CommandLineVar, const FString& EnvironmentVar)
{
	FString Result;
	if (false == FParse::Value(FCommandLine::Get(), *CommandLineVar, Result))
	{
		Result = FPlatformMisc::GetEnvironmentVariable(*EnvironmentVar);
	}
	return Result;
}

FStudioTelemetry& FStudioTelemetry::Get()
{
	static FStudioTelemetry StudioTelemetryInstance;
	return StudioTelemetryInstance;
}

void FStudioTelemetry::SetRecordEventCallback(OnRecordEventCallback Callback )
{
	RecordEventCallback = Callback;

	// If the provider already exists then set the callback
	if (AnalyticsProvider.IsValid())
	{	
		AnalyticsProvider->SetRecordEventCallback(RecordEventCallback);
	}
}


void FStudioTelemetry::StartSession()
{
	if (IsSessionRunning())
	{
		return;
	}

	// Load the configuration
	LoadConfiguration();

	if (Config.bSendTelemetry == false)
	{
		// We did not wish to send any telemetry events
		return;
	}

	FScopeLock ScopeLock(&CriticalSection);

	AnalyticsProvider = FAnalyticsProviderBroadcast::CreateAnalyticsProvider();

	if (AnalyticsProvider.IsValid())
	{
		TArray<FAnalyticsEventAttribute> DefaultEventAttributes;

		const FString UserID = FPlatformProcess::UserName(false);
		const FString ProjectName = FApp::GetProjectName();
		FString ComputerName = FPlatformProcess::ComputerName();

		FString ProjectIDString;
		GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectID"), ProjectIDString, GGameIni);

		FGuid ProjectID;

		if (!ProjectIDString.IsEmpty())
		{
			TArray<FString> Elements;
			if (ProjectIDString.ParseIntoArray(Elements, TEXT("=")) == 5)
			{
				ProjectID = FGuid(FCString::Atoi(*(Elements[1])), FCString::Atoi(*(Elements[2])), FCString::Atoi(*(Elements[3])), FCString::Atoi(*(Elements[4])));
			}
			else
			{
				ProjectID = FGuid(ProjectIDString);
			}
		}

		FGuid SessionID = FApp::GetInstanceId();

		// Set the default event attributes, these will be appended to every event
		DefaultEventAttributes.Emplace(TEXT("ProjectName"), ProjectName);
		DefaultEventAttributes.Emplace(TEXT("ProjectID"), ProjectID);

		DefaultEventAttributes.Emplace(TEXT("Session_ID"), SessionID.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		DefaultEventAttributes.Emplace(TEXT("Session_StartUTC"), FDateTime::UtcNow().ToUnixTimestampDecimal());

		FString SessionLabel;
		if (FParse::Value(FCommandLine::Get(), TEXT("SessionLabel="), SessionLabel))
		{
			DefaultEventAttributes.Emplace(TEXT("Session_Label"), SessionLabel);
		}

		DefaultEventAttributes.Emplace(TEXT("Build_Configuration"), LexToString(FApp::GetBuildConfiguration()));
		DefaultEventAttributes.Emplace(TEXT("Build_BranchName"), FApp::GetBranchName().ToLower());
		DefaultEventAttributes.Emplace(TEXT("Build_Changelist"), BuildSettings::GetCurrentChangelist());

		DefaultEventAttributes.Emplace(TEXT("Config_IsEditor"), GIsEditor);
		DefaultEventAttributes.Emplace(TEXT("Config_IsBuildMachine"), GIsBuildMachine);
		DefaultEventAttributes.Emplace(TEXT("Config_IsRunningCommandlet"), IsRunningCommandlet());

		// Only send user data if requested
		if (Config.bSendUserData == true)
		{
			DefaultEventAttributes.Emplace(TEXT("User_ID"), UserID);
			DefaultEventAttributes.Emplace(TEXT("Application_Commandline"), FCommandLine::Get());
		}

		// ALways send the platform
		DefaultEventAttributes.Emplace(TEXT("Hardware_Platform"), FString(FPlatformProperties::IniPlatformName()));

		// Only send detailed hardware data if requested
		if (Config.bSendHardwareData == true)
		{
			DefaultEventAttributes.Emplace(TEXT("Hardware_GPU"), GRHIAdapterName);
			DefaultEventAttributes.Emplace(TEXT("Hardware_CPU"), FPlatformMisc::GetCPUBrand());
			DefaultEventAttributes.Emplace(TEXT("Hardware_CPU_Cores_Physical"), FPlatformMisc::NumberOfCores());
			DefaultEventAttributes.Emplace(TEXT("Hardware_CPU_Cores_Logical"), FPlatformMisc::NumberOfCoresIncludingHyperthreads());
			DefaultEventAttributes.Emplace(TEXT("Hardware_RAM"), static_cast<uint64>(FPlatformMemory::GetStats().TotalPhysical));
			DefaultEventAttributes.Emplace(TEXT("Hardware_ComputerName"), ComputerName);
		}

		// Only send OS data if requested
		if (Config.bSendOSData == true)
		{
			FString OSVersionLabel;
			FString OSSubVersionLabel;

			FPlatformMisc::GetOSVersions(OSVersionLabel, OSSubVersionLabel);

			DefaultEventAttributes.Emplace(TEXT("OS_Version"), FPlatformMisc::GetOSVersion());
			DefaultEventAttributes.Emplace(TEXT("OS_VersionLabel"), OSVersionLabel);
			DefaultEventAttributes.Emplace(TEXT("OS_VersionSubLabel"), OSSubVersionLabel);
			DefaultEventAttributes.Emplace(TEXT("OS_ID"), FPlatformMisc::GetOperatingSystemId());
		}

		const FString HordeJobId = GetEnvironmentValriable(TEXT("HordeJobId="), TEXT("UE_HORDE_JOBID")) ;

		// Send Horde environment if available
		if (!HordeJobId.IsEmpty())
		{
			DefaultEventAttributes.Emplace(TEXT("Horde_JobID"), HordeJobId);
			DefaultEventAttributes.Emplace(TEXT("Horde_StepID"), GetEnvironmentValriable(TEXT("HordeStepId="), TEXT("UE_HORDE_STEPID")));
			DefaultEventAttributes.Emplace(TEXT("Horde_StepName"), GetEnvironmentValriable(TEXT("HordeStepName="), TEXT("UE_HORDE_STEPNAME")));
			DefaultEventAttributes.Emplace(TEXT("Horde_ServerURL"), GetEnvironmentValriable(TEXT("HordeServerUrl="), TEXT("UE_HORDE_URL")));
			DefaultEventAttributes.Emplace(TEXT("Horde_TemplateName"), GetEnvironmentValriable(TEXT("HordeTemplateName="), TEXT("UE_HORDE_TEMPLATENAME")));	
		}

		// Set up the analytics provider
		AnalyticsProvider->SetUserID(UserID);
		AnalyticsProvider->SetSessionID(SessionID.ToString(EGuidFormats::DigitsWithHyphensInBraces));
		AnalyticsProvider->SetDefaultEventAttributes(MoveTemp(DefaultEventAttributes));
		AnalyticsProvider->SetRecordEventCallback(RecordEventCallback);

		// Start the analytics session
		AnalyticsProvider->StartSession();

		// Create the IAnalyticsTracer interface
		AnalyticsTracer = FAnalytics::Get().CreateAnalyticsTracer();
		AnalyticsTracer->SetProvider(AnalyticsProvider);
		AnalyticsTracer->StartSession();

		// Bind the pre-exit callback
		FCoreDelegates::OnEnginePreExit.AddRaw(&FStudioTelemetry::Get(), &FStudioTelemetry::EndSession);

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(TEXT("SchemaVersion"), 1);
		AnalyticsProvider->RecordEvent(TEXT("StudioTelemetry.SessionStart"), Attributes);

		OnStartSession.Broadcast();

		UE_LOG(LogStudioTelemetry, Log, TEXT("Started StudioTelemetry Session"));
	}
}

void FStudioTelemetry::EndSession()
{
	if (IsSessionRunning())
	{
		FScopeLock ScopeLock(&CriticalSection);

		OnEndSession.Broadcast();

		TArray<FAnalyticsEventAttribute> Attributes;
		Attributes.Emplace(TEXT("SchemaVersion"), 1);
		AnalyticsProvider->RecordEvent(TEXT("StudioTelemetry.SessionEnd"), Attributes);

		// End session for the tracer and the provider
		if (AnalyticsTracer.IsValid())
		{
			AnalyticsTracer->EndSession();
			AnalyticsTracer.Reset();
		}

		if (AnalyticsProvider.IsValid())
		{
			AnalyticsProvider->EndSession();
			AnalyticsProvider->FlushEvents();
			AnalyticsProvider.Reset();
		}

		UE_LOG(LogStudioTelemetry, Log, TEXT("Ended StudioTelemetry Session"));
	}
}

bool FStudioTelemetry::IsSessionRunning() const
{
	return AnalyticsProvider.IsValid();
}

void FStudioTelemetry::LoadConfiguration()
{
	const FString TelemetryConfigurationSection(TEXT("StudioTelemetry.Config"));

	// Look for the configuration settings in the Engine.ini files
	TArray<FString> SectionNames;

	if (GConfig->GetSectionNames(GEngineIni, SectionNames))
	{
		for (const FString& SectionName : SectionNames)
		{
			if (SectionName.Find(TelemetryConfigurationSection) != INDEX_NONE)
			{
				GConfig->GetBool(*SectionName, TEXT("SendTelemetry="), Config.bSendTelemetry, GEngineIni);
				GConfig->GetBool(*SectionName, TEXT("SendUserData="), Config.bSendUserData, GEngineIni);
				GConfig->GetBool(*SectionName, TEXT("SendHardwareData="), Config.bSendHardwareData, GEngineIni);
				GConfig->GetBool(*SectionName, TEXT("SendOSData="), Config.bSendOSData, GEngineIni);
			}
		}
	}

	// Parse the commandline for any local configuration overrides
	FParse::Bool(FCommandLine::Get(), TEXT("ST_SendTelemetry="), Config.bSendTelemetry);
	FParse::Bool(FCommandLine::Get(), TEXT("ST_SendUserData="), Config.bSendUserData);
	FParse::Bool(FCommandLine::Get(), TEXT("ST_SendHardwareData="), Config.bSendHardwareData);
	FParse::Bool(FCommandLine::Get(), TEXT("ST_SendOSData="), Config.bSendOSData);
}

void FStudioTelemetry::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->RecordEvent(EventName, Attributes);
		OnRecordEvent.Broadcast(EventName, Attributes);
	}
}

void FStudioTelemetry::RecordEvent(const FName CategoryName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->RecordEvent(EventName, Attributes);
		OnRecordEvent.Broadcast(EventName, Attributes);
	}
}

void FStudioTelemetry::FlushEvents()
{
	FScopeLock ScopeLock(&CriticalSection);
	if (AnalyticsProvider.IsValid())
	{
		AnalyticsProvider->FlushEvents();
	}
}

void FStudioTelemetry::RecordEventToProvider(const FString& ProviderName, const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	FScopeLock ScopeLock(&CriticalSection);
	TSharedPtr<IAnalyticsProvider> NamedProvider = GetProvider(ProviderName).Pin();

	if (NamedProvider.IsValid())
	{
		NamedProvider->RecordEvent(EventName, Attributes);
	}
}

TWeakPtr<IAnalyticsProvider> FStudioTelemetry::GetProvider()
{
	return AnalyticsProvider;
}

TWeakPtr<IAnalyticsProvider> FStudioTelemetry::GetProvider(const FString& Name)
{
	return AnalyticsProvider.IsValid()? AnalyticsProvider->GetAnalyticsProvider(Name) : TWeakPtr<IAnalyticsProvider>();
}

TWeakPtr<IAnalyticsTracer> FStudioTelemetry::GetTracer()
{
	return AnalyticsTracer;
}

TSharedPtr<IAnalyticsSpan> FStudioTelemetry::StartSpan(const FName Name, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->StartSpan(Name, AdditionalAttributes) : TSharedPtr<IAnalyticsSpan>();
}

TSharedPtr<IAnalyticsSpan> FStudioTelemetry::StartSpan(const FName Name, TSharedPtr<IAnalyticsSpan> ParentSpan, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->StartSpan(Name, ParentSpan, AdditionalAttributes)  : TSharedPtr<IAnalyticsSpan>();
}

bool FStudioTelemetry::EndSpan(TSharedPtr<IAnalyticsSpan> Span, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->EndSpan(Span, AdditionalAttributes) : false;
}

bool FStudioTelemetry::EndSpan(const FName Name, const TArray<FAnalyticsEventAttribute>& AdditionalAttributes)
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->EndSpan(Name, AdditionalAttributes) : false;
}

TSharedPtr<IAnalyticsSpan> FStudioTelemetry::GetSpan(const FName Name)
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->GetSpan(Name) : TSharedPtr<IAnalyticsSpan>();
}

TSharedPtr<IAnalyticsSpan> FStudioTelemetry::GetSessionSpan() const
{
	return AnalyticsTracer.IsValid() ? AnalyticsTracer->GetSessionSpan() : TSharedPtr<IAnalyticsSpan>();
}


