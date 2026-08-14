// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "Clients/LiveLinkHubProvider.h"
#include "Engine/Engine.h"
#include "Engine/EngineCustomTimeStep.h"
#include "Engine/TimecodeProvider.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "LiveLinkHub.h"
#include "LiveLinkHubMessages.h"
#include "LiveLinkCustomTimeStep.h"
#include "LiveLinkHubLog.h"
#include "LiveLinkTimecodeProvider.h"
#include "Misc/FrameRate.h"
#include "UnrealEngine.h"

#include "LiveLinkHubTimeAndSyncSettings.generated.h"

/**
 * Settings for LiveLinkHub's timecode and genlock.
 */
UCLASS(config=EditorPerProjectUserSettings)
class ULiveLinkHubTimeAndSyncSettings : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(config, EditAnywhere, Category = "Timecode")
	FLiveLinkHubTimecodeSettings TimecodeSettings;

	/** Whether the hub should be used as a timecode source for connected clients. */
	UPROPERTY(config, EditAnywhere, Category = "Timecode", DisplayName = "Enable")
	bool bUseLiveLinkHubAsTimecodeSource = false;

	/** Custom time step */
	UPROPERTY(config, EditAnywhere, Category = "Frame Lock")
	FLiveLinkHubCustomTimeStepSettings CustomTimeStepSettings;

	/** Whether the hub should be used as a CustomTimeStep source for connected clients. */
	UPROPERTY(config, EditAnywhere, Category = "Frame Lock", DisplayName = "Enable")
	bool bUseLiveLinkHubAsCustomTimeStepSource = false;

public:
#if WITH_EDITOR
	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override
	{
		const FName PropertyName = (PropertyChangedEvent.MemberProperty != nullptr) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ULiveLinkHubTimeAndSyncSettings, TimecodeSettings))
		{
			if (bUseLiveLinkHubAsTimecodeSource)
			{
				OnToggleTimecodeSettings();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULiveLinkHubTimeAndSyncSettings, CustomTimeStepSettings))
		{
			if (bUseLiveLinkHubAsCustomTimeStepSource)
			{
				OnToggleCustomTimeStepSettings();
			}
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULiveLinkHubTimeAndSyncSettings, bUseLiveLinkHubAsTimecodeSource))
		{
			OnToggleTimecodeSettings();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(ULiveLinkHubTimeAndSyncSettings, bUseLiveLinkHubAsCustomTimeStepSource))
		{
			OnToggleCustomTimeStepSettings();
		}
	}
	//~ End UObject interface
#endif

	/** Returns whether the timecode configuration is valid. */
	bool IsTimecodeProviderValid() const
	{
		if (TimecodeSettings.Source == ELiveLinkHubTimecodeSource::UseSubjectName)
		{
			ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
			return LiveLinkClient->IsSubjectValid(TimecodeSettings.SubjectName);
		}
		return true;
	}

	/** Returns whether the custom time step configuration is valid. */
	bool IsCustomTimeStepValid() const
	{
		ILiveLinkClient* LiveLinkClient = &IModularFeatures::Get().GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);
		return LiveLinkClient->IsSubjectValid(CustomTimeStepSettings.SubjectName);
	}

	/** Apply this timecode configuration to this instance of Live Link Hub. */
	void ApplyTimecodeProvider() const
	{
		TimecodeSettings.AssignTimecodeSettingsAsProviderToEngine();
	}

	/** Apply this custom time step configuration to this instance of Live Link Hub. */
	void ApplyCustomTimeStep() const
	{
		CustomTimeStepSettings.AssignCustomTimeStepToEngine();
	}

private:
	/** Handles broadcasting timecode settings when they're enabled/disabled. */
	void OnToggleTimecodeSettings()
	{
		if (const TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider = FLiveLinkHub::Get()->GetLiveLinkProvider())
		{
			if (bUseLiveLinkHubAsTimecodeSource)
			{
				ApplyTimecodeProvider();
				LiveLinkProvider->UpdateTimecodeSettings(TimecodeSettings);
			}
			else
			{
				// If we're disabling LLH as a TimecodeProvider, reset the timecode provider on the engine.
				GEngine->Exec(GEngine->GetCurrentPlayWorld(nullptr), TEXT("TimecodeProvider.reset"));
				LiveLinkProvider->ResetTimecodeSettings();
			}
		}
	}

	/** Handles broadcasting custom time step settings when they're enabled/disabled. */
	void OnToggleCustomTimeStepSettings()
	{
		if (const TSharedPtr<FLiveLinkHubProvider> LiveLinkProvider = FLiveLinkHub::Get()->GetLiveLinkProvider())
		{
			if (bUseLiveLinkHubAsCustomTimeStepSource)
			{
				ApplyCustomTimeStep();
				LiveLinkProvider->UpdateCustomTimeStepSettings(CustomTimeStepSettings);
			}
			else
			{
				// If we're disabling LLH as a CustomTimeStep source, reset the custom time step on the engine.
				GEngine->Exec(GEngine->GetCurrentPlayWorld(nullptr), TEXT("CustomTimeStep.reset"));
				LiveLinkProvider->ResetCustomTimeStepSettings();
			}
		}
	}
};
