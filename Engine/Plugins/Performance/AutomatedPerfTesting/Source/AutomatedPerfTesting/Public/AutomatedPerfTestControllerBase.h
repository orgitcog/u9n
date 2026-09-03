// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AutomatedPerfTestProjectSettings.h"
#include "GauntletTestController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

#include "AutomatedPerfTestControllerBase.generated.h"

#define UE_API AUTOMATEDPERFTESTING_API

UENUM()
enum class EAutomatedPerfTestCSVOutputMode : uint8
{
	Single UMETA(DisplayName = "Single CSV", ToolTip = "Output a single CSV with all of the results for the entire session, from SetupTest to ExitTest."),
	Separate UMETA(DisplayName = "Separate CSVs", ToolTip = "Output CSVs from RunTest to TeardownTest. May result into multiple output CSVs that require special processing."),
	Granular UMETA(DisplayName = "Granular CSVs", ToolTip = "Output granular CSVs during the test run, resulting in multiple CSVs between RunTest and TeardownTest.")
};


class AGameModeBase;

namespace AutomatedPerfTest
{
	static UWorld* FindCurrentWorld()
	{
		UWorld* World = nullptr;
		for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
		{
			if (WorldContext.WorldType == EWorldType::Game)
			{
				World = WorldContext.World();
			}
#if WITH_EDITOR
			else if (GIsEditor && WorldContext.WorldType == EWorldType::PIE)
			{
				World = WorldContext.World();
				if (World)
				{
					return World;
				}
			}
#endif
		}

		return World;
	}
}
/**
 * 
 */
UCLASS(MinimalAPI)
class UAutomatedPerfTestControllerBase : public UGauntletTestController
{
	GENERATED_BODY()
public:
	UE_API void OnPreWorldInitializeInternal(UWorld* World, const UWorld::InitializationValues IVS);
	UE_API virtual void OnPreWorldInitialize(UWorld* World);

	UFUNCTION()
	UE_API void TryEarlyExec(UWorld* const World);
	
	UFUNCTION()
	UE_API void OnWorldBeginPlay();

	UFUNCTION()
	UE_API void OnGameStateSet(AGameStateBase* const GameStateBase);

// Base functionality
public:
	UE_API UAutomatedPerfTestControllerBase(const FObjectInitializer& ObjectInitializer);

	UE_API FString GetTestName();
	UE_API FString GetDeviceProfile();
	UE_API virtual FString GetTestID();
	UE_API FString GetOverallRegionName();
	UE_API FString GetTraceChannels();
	
	UE_API bool RequestsInsightsTrace() const;
	UE_API bool RequestsCSVProfiler() const;
	UE_API bool RequestsFPSChart() const;
	UE_API bool RequestsVideoCapture() const;
	UE_API bool RequestsLockedDynRes() const;

	UE_API bool TryStartInsightsTrace();
	UE_API bool TryStopInsightsTrace();

	UE_API virtual bool TryStartCSVProfiler();
	UE_API virtual bool TryStartCSVProfiler(FString CSVFileName);
	UE_API bool TryStopCSVProfiler();
	
	UE_API bool TryStartFPSChart();
	UE_API bool TryStopFPSChart();

	UE_API bool TryStartVideoCapture();
	UE_API bool TryFinalizingVideoCapture(const bool bStopAutoContinue = false);

	UE_API virtual void SetupTest();
	UE_API virtual void RunTest();
	UE_API virtual void TeardownTest(bool bExitAfterTeardown = true);
	UE_API virtual void TriggerExitAfterDelay();
	UE_API virtual void Exit();

	UE_API AGameModeBase* GetGameMode() const;

	UE_API void TakeScreenshot(FString ScreenshotName);

	// you'll need to set this via your subclass if you want to customize the behavior otherwise it will default to a single CSV per session
	UE_API void SetCSVOutputMode(EAutomatedPerfTestCSVOutputMode NewOutputMode);
	UE_API EAutomatedPerfTestCSVOutputMode GetCSVOutputMode() const;
	
protected:
	// ~Begin UGauntletTestController Interface
	UE_API virtual void OnInit() override;
	UE_API virtual void OnTick(float TimeDelta) override;
	UE_API virtual void OnStateChange(FName OldState, FName NewState) override;
	UE_API virtual void OnPreMapChange() override;
	UE_API virtual void BeginDestroy() override;
	// ~End UGauntletTestController Interface

	UFUNCTION()
	UE_API virtual void EndAutomatedPerfTest(const int32 ExitCode = 0);

	UFUNCTION()
	UE_API virtual void OnVideoRecordingFinalized(bool Succeeded, const FString& FilePath);
	
	UE_API virtual void UnbindAllDelegates();

	UE_API void SetupGameModeInstance();

	UE_API virtual FString GetInsightsFilename();
	UE_API virtual FString GetCSVFilename();


private:
	FString TraceChannels;
	FString TestDatetime;
	FString TestID;
	FString DeviceProfileOverride;
	bool bRequestsFPSChart;
	bool bRequestsInsightsTrace;
	bool bRequestsCSVProfiler;
	bool bRequestsVideoCapture;
	bool bRequestsLockedDynRes;

	FText VideoRecordingTitle;
	
	const TArray<FString> CmdsToExecEarly = { };
	
	AGameModeBase* GameMode;

	FDelegateHandle CsvProfilerDelegateHandle;

	EAutomatedPerfTestCSVOutputMode CSVOutputMode;
};

#undef UE_API
