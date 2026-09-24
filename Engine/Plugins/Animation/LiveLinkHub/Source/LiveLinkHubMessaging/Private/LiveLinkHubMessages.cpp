// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubMessages.h"

#include "Engine/Engine.h"
#include "Engine/SystemTimeTimecodeProvider.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "ILiveLinkClient.h"
#include "ILiveLinkModule.h"

const FName UE::LiveLinkHub::Private::LiveLinkHubProviderType = TEXT("LiveLinkHub");
FName FLiveLinkHubMessageAnnotation::ProviderTypeAnnotation = TEXT("ProviderType");
FName FLiveLinkHubMessageAnnotation::AutoConnectModeAnnotation = TEXT("AutoConnect");
FName FLiveLinkHubMessageAnnotation::IdAnnotation = TEXT("Id");


DEFINE_LOG_CATEGORY_STATIC(LogLiveLinkHubMessages, Log, All);


namespace UE::LiveLinkHubMessages::Private
{
	TOptional<FLiveLinkSubjectKey> FindSubjectKey(ILiveLinkClient* LiveLinkClient, FName SubjectName)
	{
		TArray<FLiveLinkSubjectKey> Subjects = LiveLinkClient->GetSubjects(true, true);
		// We need to map the named subject to the list of subject keys available.
		for (const FLiveLinkSubjectKey& Key : Subjects)
		{
			if (Key.SubjectName == SubjectName)
			{
				return Key;
			}
		}
		return {};
	}
}

void FLiveLinkHubCustomTimeStepSettings::AssignCustomTimeStepToEngine() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName))
	{
		return;
	}

	if (GEngine == nullptr)
	{
		return;
	}

	if (bResetCustomTimeStep)
	{
		UEngineCustomTimeStep* CurrentCustomTimeStep = GEngine->GetCustomTimeStep();

		if (Cast<ULiveLinkHubCustomTimeStep>(CurrentCustomTimeStep))
		{
			UE_LOG(LogLiveLinkHubMessages, Display, TEXT("CustomTimeStep reset event"));

			// We only issue a timestep reset if we are a LiveLinkHubCustomTimeStep.  This way we don't reset any custom time step that the user
			// may have set in the editor.
			GEngine->Exec(GEngine->GetCurrentPlayWorld(nullptr), TEXT("CustomTimeStep.reset"));
		}
		return;
	}
	
	UE_LOG(LogLiveLinkHubMessages, Display, TEXT("CustomTimeStep change event %s - %s"), *SubjectName.ToString(), *CustomTimeStepRate.ToPrettyText().ToString());

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	ULiveLinkHubCustomTimeStep* NewCustomTimeStep = NewObject<ULiveLinkHubCustomTimeStep>(GEngine, ULiveLinkHubCustomTimeStep::StaticClass());
	NewCustomTimeStep->LiveLinkDataRate = CustomTimeStepRate;
	NewCustomTimeStep->bLockStepMode = bLockStepMode;
	NewCustomTimeStep->FrameRateDivider = FrameRateDivider;

	if (TOptional<FLiveLinkSubjectKey> Target = UE::LiveLinkHubMessages::Private::FindSubjectKey(LiveLinkClient, SubjectName))
	{
		NewCustomTimeStep->SubjectKey = *Target;
	}
	else
	{
		// Note: We must set the subject name because the livelink custom time step will use it to try to match new subjects being added.
		NewCustomTimeStep->SubjectKey = FLiveLinkSubjectKey{FGuid(), SubjectName};
	}

	// Override the custom timestep for the engine.
	GEngine->SetCustomTimeStep(NewCustomTimeStep);
}

void FLiveLinkHubTimecodeSettings::AssignTimecodeSettingsAsProviderToEngine() const
{
	IModularFeatures& ModularFeatures = IModularFeatures::Get();
	if (!ModularFeatures.IsModularFeatureAvailable(ILiveLinkClient::ModularFeatureName) || GEngine == nullptr)
	{
		return;
	}

	ILiveLinkClient* LiveLinkClient = &ModularFeatures.GetModularFeature<ILiveLinkClient>(ILiveLinkClient::ModularFeatureName);

	UE_LOG(LogLiveLinkHubMessages, Display, TEXT("Time code change event %s - %s"), *UEnum::GetValueAsName(Source).ToString(), *SubjectName.ToString());
	if (Source == ELiveLinkHubTimecodeSource::SystemTimeEditor)
	{
		// If we are using system time, construct a new system time code provider with the target framerate.
		FName ObjectName = MakeUniqueObjectName(GEngine, USystemTimeTimecodeProvider::StaticClass(), "DefaultTimecodeProvider");
		USystemTimeTimecodeProvider* NewTimecodeProvider = NewObject<USystemTimeTimecodeProvider>(GEngine, ObjectName);
		NewTimecodeProvider->FrameRate = DesiredFrameRate;
		NewTimecodeProvider->FrameDelay = FrameDelay;
		GEngine->SetTimecodeProvider(NewTimecodeProvider);
		UE_LOG(LogLiveLinkHubMessages, Display, TEXT("System Time Timecode provider set."));
	}
	else if (Source == ELiveLinkHubTimecodeSource::UseSubjectName)
	{
		TOptional<FLiveLinkSubjectKey> Target = UE::LiveLinkHubMessages::Private::FindSubjectKey(LiveLinkClient, SubjectName);

		if (Target)
		{
			FName ObjectName = MakeUniqueObjectName(GEngine, ULiveLinkTimecodeProvider::StaticClass(), "DefaultLiveLinkTimecodeProvider");
			ULiveLinkTimecodeProvider* LiveLinkProvider = NewObject<ULiveLinkTimecodeProvider>(GEngine, ObjectName);
			LiveLinkProvider->SetTargetSubjectKey(*Target);
			LiveLinkProvider->OverrideFrameRate = DesiredFrameRate;
			LiveLinkProvider->FrameDelay = FrameDelay;
			LiveLinkProvider->BufferSize = BufferSize;

			GEngine->SetTimecodeProvider(LiveLinkProvider);
			UE_LOG(LogLiveLinkHubMessages, Display, TEXT("Live Link Timecode provider assigned to %s."), *SubjectName.ToString());
		}
		else
		{
			UE_LOG(LogLiveLinkHubMessages, Warning, TEXT("Failed to assign Live Link Timecode provider to %s."), *SubjectName.ToString());
		}
	}
	else
	{
		// Force the timecode provider to reset back to the default setting.
		GEngine->Exec( GEngine->GetCurrentPlayWorld(nullptr), TEXT( "TimecodeProvider.reset" ) );
	}
}
