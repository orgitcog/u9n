// Copyright Epic Games, Inc. All Rights Reserved.

#include "CSVToTelemetry.h"
#include "CSVProfilerUtils.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Analytics.h"
#include "AnalyticsET.h"
#include "StudioTelemetry.h"
#include "Logging/LogMacros.h"
#include "ProjectUtilities.h"


DEFINE_LOG_CATEGORY_STATIC(LogCSVToTelemetry, Log, All);
IMPLEMENT_APPLICATION(CSVToTelemetry, "CSVToTelemetry");

static int GenerateTelemetryFromCSVFile(const FString& FilePath)
{
	int32 Result = 1;

	FString EventName;

	if (FParse::Value(FCommandLine::Get(), TEXT("event="), EventName) == false)
	{
		UE_LOG(LogCSVToTelemetry, Error, TEXT("Must provide a Event name with -event=[eventname]"));
		return Result;
	}

	if (FilePath.Len() > 0)
	{
		TArray<FString> Rows;

		// Load all the rows from the file into memory
		if (FFileHelper::LoadFileToStringArray(Rows, *FilePath))
		{
			UE_LOG(LogCSVToTelemetry, Display, TEXT("Imported %d rows from file"), Rows.Num());

			FStudioTelemetry::Get().StartSession();

			// Start the telemetry session and record each row as a single telemetry event 
			if (FStudioTelemetry::Get().IsSessionRunning())
			{
				uint32 TotalUploadSize = 0;
				uint32 RowIndex = 0;
				uint32 KeySize = 0;
				uint32 ValueSize = 0;

				TArray<FString> KeyArray;

				FString Columns;

				int32 SchemaVersion(-1);

				TSharedPtr<IAnalyticsProvider> Provider = FStudioTelemetry::Get().GetProvider().Pin();
				TArray<FAnalyticsEventAttribute> DefaultAttributes = Provider->GetDefaultEventAttributesSafe();

				if (FParse::Value(FCommandLine::Get(), TEXT("schema="), SchemaVersion) == true)
				{
					DefaultAttributes.Emplace(TEXT("SchemaVersion"), SchemaVersion);
				}

				Provider->SetDefaultEventAttributes(MoveTemp(DefaultAttributes));

				if (FParse::Value(FCommandLine::Get(), TEXT("columns="), Columns) == true)
				{
					// We have specified the CSV columns on the command line 
					Columns.ParseIntoArray(KeyArray, TEXT("|"));
					KeySize = Columns.Len();
				}

				for (const FString& Row : Rows)
				{
					if (KeyArray.IsEmpty())
					{
						// Parse the keys on the first row
						Row.ParseIntoArray(KeyArray, TEXT(","));
						KeySize = Row.Len();

						for (int32 i = 0; i < KeyArray.Num(); ++i)
						{
							KeyArray[i].RemoveSpacesInline();
						}
					}
					else
					{
						// Parse the values from subsequent rows
						TArray<FString> ValueArray;
						Row.ParseIntoArray(ValueArray, TEXT(","));

						// Keys and values count should always be the same
						if (KeyArray.Num() >= ValueArray.Num())
						{
							ValueSize = Row.Len();

							TArray<FAnalyticsEventAttribute> Attributes;

							// Add the Key/Value pair to the attribute list
							for (int32 i = 0; i < ValueArray.Num(); ++i)
							{
								ValueArray[i].RemoveSpacesInline();
								Attributes.Emplace(KeyArray[i], ValueArray[i]);
							}

							// Keep track of the total data we're uploading
							TotalUploadSize += KeySize + ValueSize;

							// Now we have a complete event so send it
							FStudioTelemetry::Get().RecordEvent(EventName, Attributes);
						}
						else
						{
							// The key/value counts don't match
							UE_LOG(LogCSVToTelemetry, Warning, TEXT("Row %d contains the incorrect value count of %d ( expected %d ) and will be skipped."), RowIndex, ValueArray.Num(), KeyArray.Num());
						}
					}

					RowIndex++;
				}

				UE_LOG(LogCSVToTelemetry, Display, TEXT("Generated %d bytes of event data"), TotalUploadSize);

				FStudioTelemetry::Get().EndSession();

				Result = 0;
			}
		}
		else
		{
			UE_LOG(LogCSVToTelemetry, Error, TEXT("Unable to read rows from CSV file %s"), *FilePath);
		}
	}

	return Result;
}

static int GenerateTelemetryFromCSVProfileFile(const FString& FilePath)
{
	int32 Result = 1;

	FString EventName;

	if (FParse::Value(FCommandLine::Get(), TEXT("event="), EventName) == false)
	{
		UE_LOG(LogCSVToTelemetry, Error, TEXT("Must provide a Event name with -event=[eventname]"));
		return Result;
	}

	CsvUtils::FCsvProfilerCapture Capture;

	if (FilePath.Len() > 0)
	{
		if (FilePath.Contains(".bin"))
		{
			// Binary CSV file
			CsvUtils::ReadFromCsvBin(Capture, *FilePath);
		}
		else
		{
			// Text CSV file
			CsvUtils::ReadFromCsv(Capture, *FilePath);
		}
	}

	if (Capture.Events.Num())
	{
		FStudioTelemetry::Get().StartSession();

		if (FStudioTelemetry::Get().IsSessionRunning())
		{
			TSharedPtr<IAnalyticsProvider> Provider = FStudioTelemetry::Get().GetProvider().Pin();
			TArray<FAnalyticsEventAttribute> DefaultAttributes = Provider->GetDefaultEventAttributesSafe();

			for (TMap<FString, FString>::TConstIterator It(Capture.Metadata); It; ++It)
			{
				DefaultAttributes.Emplace(It->Key, It->Value);
			}

			Provider->SetDefaultEventAttributes(MoveTemp(DefaultAttributes));

			for (const CsvUtils::FCsvProfilerEvent& Event : Capture.Events)
			{
				int32 Frame = Event.Frame;

				TArray<FAnalyticsEventAttribute> Attributes;

				Attributes.Emplace(TEXT("Name"), Event.Name);

				for (TMap<FString, CsvUtils::FCsvProfilerSample>::TConstIterator It(Capture.Samples); It; ++It)
				{
					const FString& Name = It->Key;
					const CsvUtils::FCsvProfilerSample& Sample = It->Value;

					if (Frame < Sample.Values.Num())
					{
						Attributes.Emplace(Name, Sample.Values[Frame]);
					}
				}

				FStudioTelemetry::Get().RecordEvent(EventName, Attributes);
			}

			FStudioTelemetry::Get().FlushEvents();
			FStudioTelemetry::Get().EndSession();

			Result = 0;
		}
	}

	return Result;
}

static int ShowHelp()
{
	UE_LOG(LogCSVToTelemetry, Display, TEXT("\n\nCSVToTelemetry Help\n\nUsage:\n\tCSVToTelemetry.exe -csv=[filename] -event=[eventname] ( -schema=[value] -column=[name1|name2|....] )\n\tCSVToTelemetry.exe -csvprofile=[filename] -event=[eventname]\n\tCSVToTelemetry.exe -help\n\nRequired:\n\t-csv=[filename]\t\t\tGeneric text based csv input file.\n\t-csvprofile=[filename]\t\tCSVProfiler csv input file. Denote binary with .csv.bin otherwise assumed text.\n\t-event=[name]\t\t\tName of telemetry event to send each row to.\nOptional:\n\t-schema=[value]\t\t\tEvent schema value.\n\t-columns=[name1|name2|....]\tColumn include filter." ) );
	return 0;
}

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	FDateTime StartTime = FDateTime::UtcNow();

	// Allows this program to accept a project argument on the command-line and use project-specific config
	UE::ProjectUtilities::ParseProjectDirFromCommandline(ArgC, ArgV);

	// start up the main loop,
	GEngineLoop.PreInit(ArgC, ArgV);

	int32 Result = 1;

	FString FilePath;
	
	if (FParse::Value(FCommandLine::Get(), TEXT("help"), FilePath) == true)
	{
		// Show help message
		Result = ShowHelp();
	}
	else
	{ 
		if (FParse::Value(FCommandLine::Get(), TEXT("csvprofile="), FilePath) == true)
		{
			Result = GenerateTelemetryFromCSVProfileFile(FilePath);
		}
		else if (FParse::Value(FCommandLine::Get(), TEXT("csv="), FilePath) == true)
		{
			Result = GenerateTelemetryFromCSVFile(FilePath);
		}
	
		if (Result == 0)
		{
			// Upload completed successfully
			UE_LOG(LogCSVToTelemetry, Display, TEXT("CSVToTelemetry upload succeeded in %0.2f seconds"), (FDateTime::UtcNow() - StartTime).GetTotalSeconds());
		}
	}

	if (Result != 0)
	{
		// Always show the usage example if we have not been successful
		UE_LOG(LogCSVToTelemetry, Error, TEXT("\nUsage:\n\tCSVToTelemetry.exe -csv=[filename] -event=[eventname] ( -schema=[value] -columns=[name1|name2|....] )\n\tCSVToTelemetry.exe -csvprofile=[filename] -event=[eventname]\n\tCSVToTelemetry.exe -help"));
	}
	
	if (FParse::Param(FCommandLine::Get(), TEXT("fastexit")))
	{
		FPlatformMisc::RequestExitWithStatus(true, Result);
	}

	GLog->Flush();

	RequestEngineExit(TEXT("CSVToTelemetry Exiting"));

	FEngineLoop::AppPreExit();
	FModuleManager::Get().UnloadModulesAtShutdown();
	FEngineLoop::AppExit();

	return Result;
}