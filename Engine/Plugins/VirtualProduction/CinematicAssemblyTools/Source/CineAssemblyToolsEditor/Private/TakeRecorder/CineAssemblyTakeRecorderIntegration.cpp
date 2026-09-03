// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblyTakeRecorderIntegration.h"

#include "CineAssembly.h"
#include "CineAssemblyFactory.h"
#include "CineAssemblySchema.h"
#include "CineAssemblyTakeRecorderSettings.h"
#include "CineAssemblyToolsAnalytics.h"
#include "ProductionSettings.h"
#include "ITakeRecorderModule.h"
#include "Recorder/TakeRecorder.h"
#include "Sections/MovieSceneSubSection.h"
#include "TakesUtils.h"

FCineAssemblyTakeRecorderIntegration::FCineAssemblyTakeRecorderIntegration()
{
	UCineAssemblyTakeRecorderSettings* Settings = GetMutableDefault<UCineAssemblyTakeRecorderSettings>();

	ITakeRecorderModule& TakeRecorderModule = ITakeRecorderModule::Get();
	TakeRecorderModule.RegisterSettingsObject(Settings);

	UTakeRecorder::OnRecordingInitialized().AddRaw(this, &FCineAssemblyTakeRecorderIntegration::OnRecordingInitialized);
}

FCineAssemblyTakeRecorderIntegration::~FCineAssemblyTakeRecorderIntegration()
{
	UTakeRecorder::OnRecordingInitialized().RemoveAll(this);
}

void FCineAssemblyTakeRecorderIntegration::OnRecordingInitialized(UTakeRecorder* TakeRecorder)
{
	if (TakeRecorder)
	{
		TakeRecorder->OnRecordingStarted().AddRaw(this, &FCineAssemblyTakeRecorderIntegration::OnRecordingStarted);
		TakeRecorder->OnTickRecording().AddRaw(this, &FCineAssemblyTakeRecorderIntegration::OnTickRecording);
		TakeRecorder->OnRecordingStopped().AddRaw(this, &FCineAssemblyTakeRecorderIntegration::OnRecordingStopped);

		if (UCineAssembly* Assembly = Cast<UCineAssembly>(TakeRecorder->GetSequence()))
		{
			FString AssemblyName;
			Assembly->GetName(AssemblyName);

			Assembly->AssemblyName.Template = AssemblyName;
			Assembly->AssemblyName.Resolved = FText::FromString(AssemblyName);

			Assembly->Level = FSoftObjectPath(TakesUtils::DiscoverSourceWorld());

			// Set the production of this recorded subsequence to the current active production
			const UProductionSettings* ProductionSettings = GetDefault<UProductionSettings>();
			TOptional<const FCinematicProduction> ActiveProduction = ProductionSettings->GetActiveProduction();
			if (ActiveProduction.IsSet())
			{
				Assembly->Production = ActiveProduction->ProductionID;
				Assembly->ProductionName = ActiveProduction->ProductionName;
			}

			// Reset the assembly's playback range, since it was initialized to some default value when it was created, but we do not know what its final range will be until we finish recording.
			if (UMovieScene* MovieScene = Assembly->GetMovieScene())
			{
				const FFrameNumber StartFrame = MovieScene->GetPlaybackRange().GetLowerBoundValue();
				MovieScene->SetPlaybackRange(TRange<FFrameNumber>(StartFrame, StartFrame));
			}

			UCineAssemblyTakeRecorderSettings* Settings = GetMutableDefault<UCineAssemblyTakeRecorderSettings>();
			if (!Settings->AssemblySchema.IsNull())
			{
				UCineAssemblySchema* Schema = Settings->AssemblySchema.LoadSynchronous();
				Assembly->SetSchema(Schema);

				// If the schema defines some additional default assembly path, move the assembly asset accordingly
				if (!Schema->DefaultAssemblyPath.IsEmpty())
				{
					FString CurrentAssemblyPath;
					Assembly->GetPathName(nullptr, CurrentAssemblyPath);
					CurrentAssemblyPath = FPaths::GetPath(CurrentAssemblyPath);

					FString UniquePackageName;
					FString UniqueAssetName;
					UCineAssemblyFactory::MakeUniqueNameAndPath(Assembly, CurrentAssemblyPath, UniquePackageName, UniqueAssetName);

					UPackage* Package = CreatePackage(*UniquePackageName);
					Assembly->Rename(*UniqueAssetName, Package);
				}

				Assembly->CreateSubAssemblies();
			}

			UE::CineAssemblyToolsAnalytics::RecordEvent_RecordAssembly();
		}
	}
}

void FCineAssemblyTakeRecorderIntegration::OnRecordingStarted(UTakeRecorder* TakeRecorder)
{
	if (TakeRecorder)
	{
		if (UCineAssembly* Assembly = Cast<UCineAssembly>(TakeRecorder->GetSequence()))
		{
			// Reset the playback range of each subsection and its underlying sequence's movie scene.
			// These were initialized to default ranges when the assets were created, but when recording, we do not want to use the default range.
			for (UMovieSceneSubSection* SubSection : Assembly->SubAssemblies)
			{
				const FFrameNumber StartTime = SubSection->GetRange().GetLowerBoundValue();
				SubSection->SetRange(TRange<FFrameNumber>(StartTime, StartTime));

				if (UMovieSceneSequence* SubSequence = SubSection->GetSequence())
				{
					if (UMovieScene* MovieScene = SubSequence->GetMovieScene())
					{
						MovieScene->SetTickResolutionDirectly(Assembly->GetMovieScene()->GetTickResolution());
						MovieScene->SetDisplayRate(Assembly->GetMovieScene()->GetDisplayRate());

						MovieScene->SetPlaybackRange(TRange<FFrameNumber>(0, 0));
					}
				}
			}
		}
	}
}

void FCineAssemblyTakeRecorderIntegration::OnTickRecording(UTakeRecorder* TakeRecorder, const FQualifiedFrameTime& CurrentFrameTime)
{
	if (TakeRecorder)
	{
		if (UCineAssembly* Assembly = Cast<UCineAssembly>(TakeRecorder->GetSequence()))
		{
			FFrameNumber EndFrame = CurrentFrameTime.Time.CeilToFrame();

			// Expand the the frame range of each subsequence (this causes the tracks to grow longer during recording)
			for (UMovieSceneSubSection* SubSection : Assembly->SubAssemblies)
			{
				SubSection->ExpandToFrame(EndFrame);
			}
		}
	}
}

void FCineAssemblyTakeRecorderIntegration::OnRecordingStopped(UTakeRecorder* TakeRecorder)
{
	if (TakeRecorder)
	{
		if (UCineAssembly* Assembly = Cast<UCineAssembly>(TakeRecorder->GetSequence()))
		{
			// Finalize the frame range for each subsequence, lock them, and save them
			for (UMovieSceneSubSection* SubSection : Assembly->SubAssemblies)
			{
				if (UMovieSceneSequence* SubSequence = SubSection->GetSequence())
				{
					if (UMovieScene* MovieScene = SubSequence->GetMovieScene())
					{
						MovieScene->SetPlaybackRange(SubSection->GetRange());
						MovieScene->SetReadOnly(true);
					}

					TakesUtils::SaveAsset(SubSequence);
				}
			}
		}

		TakeRecorder->OnRecordingStarted().RemoveAll(this);
		TakeRecorder->OnTickRecording().RemoveAll(this);
		TakeRecorder->OnRecordingStopped().RemoveAll(this);
	}
}
