// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationMarkers.h"
#include "Sound/SoundWave.h"

DEFINE_LOG_CATEGORY(LogWaveformTransformationMarkers);

FWaveTransformationMarkers::FWaveTransformationMarkers(double InStartLoopTime, double InEndLoopTime)
: StartLoopTime(InStartLoopTime)
, EndLoopTime(InEndLoopTime){}

void FWaveTransformationMarkers::ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const
{
	check(InOutWaveInfo.SampleRate > 0.f && InOutWaveInfo.Audio != nullptr);

	Audio::FAlignedFloatBuffer& InputAudio = *InOutWaveInfo.Audio;

	if (InputAudio.Num() == 0)
	{
		return;
	}

	check(StartLoopTime >= 0);
	check(InOutWaveInfo.NumChannels > 0);

	const int64 StartSampleOffset = InOutWaveInfo.StartFrameOffset;

	int64 StartSample = FMath::RoundToInt32(StartLoopTime * InOutWaveInfo.SampleRate) * InOutWaveInfo.NumChannels;
	StartSample = FMath::Max(0, StartSample - StartSampleOffset);

	if (StartSample > InputAudio.Num())
	{
		return;
	}
	
	int64 EndSample = InputAudio.Num() - 1;

	if (EndLoopTime > 0.f)
	{
		const int64 EndFrame = FMath::RoundToInt32(EndLoopTime * InOutWaveInfo.SampleRate) * InOutWaveInfo.NumChannels;
		EndSample = FMath::Max(StartSample + 1, EndFrame - 1 - StartSampleOffset);

		// EndLoopTime can be beyond the length of the file if there is a Trim
		if (EndSample > InputAudio.Num() - 1)
		{
			EndSample = InputAudio.Num() - 1;
			UE_LOG(LogWaveformTransformationMarkers, Warning, TEXT("Cutting a loop point with a trim!"));
		}

		EndSample = FMath::Min(EndSample, InputAudio.Num() - 1);
	}

	const int32 FinalSize = EndSample - StartSample + 1;

	if (FinalSize <= 2)
	{
		UE_LOG(LogWaveformTransformationMarkers, Warning, TEXT("Previewing loop of sample size 1!"));
		
		return;
	}

	InOutWaveInfo.StartFrameOffset = StartSample - (StartSample % InOutWaveInfo.NumChannels);
	InOutWaveInfo.NumEditedSamples = FinalSize;

	// Audio needs no trimming
	if (FinalSize == InputAudio.Num())
	{
		return;
	}

	// Apply trim to the audio to audition the desired loop region
	TArray<float> TempBuffer;
	TempBuffer.AddUninitialized(FinalSize);

	FMemory::Memcpy(TempBuffer.GetData(), &InputAudio[StartSample], FinalSize * sizeof(float));

	InputAudio.Empty();
	InputAudio.AddUninitialized(FinalSize);

	FMemory::Memcpy(InputAudio.GetData(), TempBuffer.GetData(), FinalSize * sizeof(float));
}

UWaveformTransformationMarkers::UWaveformTransformationMarkers(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	if(Markers == nullptr)
	{
		Markers = ObjectInitializer.CreateDefaultSubobject<UWaveCueArray>(this, TEXT("Markers"));
	}
}

#if WITH_EDITOR
void UWaveformTransformationMarkers::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformTransformationMarkers, Markers))
	{
		check(Markers);
		bool bIsSelectedCuePresent = false;
		
		for (FSoundWaveCuePoint& CuePoint : Markers->CuesAndLoops)
		{
			if (!bIsSelectedCuePresent && Markers->SelectedCue == CuePoint.CuePointID)
			{
				bIsSelectedCuePresent = true;
			}

			if(CuePoint.IsLoopRegion())
			{
				// Update LoopRegion Preview window if a loop is selected and the user edits the loop bounds using the properties window
				if (bIsPreviewingLoopRegion && Markers->SelectedCue == CuePoint.CuePointID)
				{
					check(SampleRate > 0.f);
					check(EndLoopTime > 0.0);

					const int64 StartLoopFramePos = StartLoopTime * SampleRate;
					const int64 EndLoopFramePos = EndLoopTime * SampleRate;

					if (CuePoint.FramePosition != StartLoopFramePos)
					{
						StartLoopTime = static_cast<float>(CuePoint.FramePosition) / SampleRate;
					}

					if (CuePoint.FramePosition + CuePoint.FrameLength != EndLoopFramePos)
					{
						EndLoopTime = static_cast<float>(CuePoint.FramePosition + CuePoint.FrameLength) / SampleRate;
					}
				}

				if (CuePoint.FrameLength < Markers->MinLoopSize)
				{
					CuePoint.FrameLength = FMath::Max(Markers->MinLoopSize, static_cast<int64>(AvailableWaveformDuration * SampleRate * 0.1f));
				}
			}
		}

		if (!bIsSelectedCuePresent && Markers->SelectedCue != INDEX_NONE)
		{
			Markers->SelectedCue = INDEX_NONE;
		}

		if (Markers->SelectedCue == INDEX_NONE && (bIsPreviewingLoopRegion || StartLoopTime != 0.0 || EndLoopTime != -1.0))
		{
			ResetLoopPreviewing();
		}
	}
}
#endif // WITH_EDITOR

Audio::FTransformationPtr UWaveformTransformationMarkers::CreateTransformation() const
{
	return MakeUnique<FWaveTransformationMarkers>(StartLoopTime, EndLoopTime);
}

void UWaveformTransformationMarkers::UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration)
{
	check(Markers != nullptr);
	Markers->InitMarkersIfNotSet(InOutConfiguration.WaveCues);
	
	const float InOutDuration = InOutConfiguration.EndTime - InOutConfiguration.StartTime;

	// Assert that InOutConfiguration is initialized and valid
	check(InOutConfiguration.SampleRate > 0.f);
	check(InOutConfiguration.StartTime >= 0.f);
	check(InOutDuration > 0.f);

	StartFrameOffset = static_cast<int64>(InOutConfiguration.StartTime * InOutConfiguration.SampleRate);
	SampleRate = InOutConfiguration.SampleRate;
	AvailableWaveformDuration = InOutDuration;

	if (!bCachedIsPreviewingLoopRegion)
	{
		bCachedSoundWaveLoopState = InOutConfiguration.bCachedSoundWaveLoopState;
		EndLoopTime = -1;
	}
	else
	{
		InOutConfiguration.StartTime = StartLoopTime;
	}

	// Update after setting bCachedSoundWaveLoopState so bCachedSoundWaveLoopState isn't overwritten
	bCachedIsPreviewingLoopRegion = bIsPreviewingLoopRegion;

	InOutConfiguration.bIsPreviewingLoopRegion = bIsPreviewingLoopRegion;
	InOutConfiguration.bCachedSoundWaveLoopState = bCachedSoundWaveLoopState;
}

void UWaveformTransformationMarkers::OverwriteTransformation()
{
	check(Markers != nullptr);
	Markers->Reset();
}

void UWaveformTransformationMarkers::ModifyMarkerLoopRegion(ELoopModificationControls Modification) const
{
	check(Markers != nullptr);
	Markers->ModifyMarkerLoop.ExecuteIfBound(Modification);
}

void UWaveformTransformationMarkers::CycleMarkerLoopRegion(ELoopModificationControls Modification) const
{
	check(Markers != nullptr);
	Markers->CycleMarkerLoop.ExecuteIfBound(Modification);
}

void UWaveformTransformationMarkers::ResetLoopPreviewing()
{
	bIsPreviewingLoopRegion = false;
	StartLoopTime = 0.0;
	EndLoopTime = -1.0;
}

#if WITH_EDITOR
void UWaveformTransformationMarkers::OverwriteSoundWaveData(USoundWave& InOutSoundWave)
{
	// Overwriting soundwave data can cause a change in number of samples, invalidating the FramePositions of CuesAndLoops
	// Subtracting StartFrameOffset from FramePosition shifts the CuesAndLoops to the correct relative position
	TArray<FSoundWaveCuePoint> FrameAdjustedCuesAndLoops = Markers->CuesAndLoops;

	// Users can export a loop region, shift FramePositions relative to the Loop region
	if (bIsPreviewingLoopRegion)
	{
		check(InOutSoundWave.GetImportedSampleRate() > 0);
		int64 StartLoopFramePosition = StartLoopTime * InOutSoundWave.GetImportedSampleRate();
		StartFrameOffset = StartFrameOffset < StartLoopFramePosition ? StartLoopFramePosition : StartFrameOffset;
	}

	if (StartFrameOffset != 0)
	{
		for (FSoundWaveCuePoint& Marker : FrameAdjustedCuesAndLoops)
		{
			const int64 NewPosition = Marker.FramePosition - StartFrameOffset;

			// If loop region is cut, resize it to maintain proper relative loop end point
			// else maintain original loop region size
			if (NewPosition < 0 && Marker.IsLoopRegion())
			{
				Marker.FrameLength = Marker.FrameLength + NewPosition > 0 ? Marker.FrameLength + NewPosition : Marker.FrameLength;
				Marker.FrameLength = FMath::Max(Marker.FrameLength, UWaveCueArray::MinLoopSize);
			}

			Marker.FramePosition = FMath::Max(NewPosition, 0);
		}
	}

	InOutSoundWave.SetSoundWaveCuePoints(FrameAdjustedCuesAndLoops);
}

void UWaveformTransformationMarkers::GetTransformationInfo(FWaveformTransformationInfo& InOutTransformationInfo) const
{
	check(Markers != nullptr);
	InOutTransformationInfo.AllCuePoints.Append(Markers->CuesAndLoops);
}

void UWaveCueArray::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveCueArray, CuesAndLoops) && CuesAndLoops.Num() > 0)
	{
		int32 LargestCuePointID = 1;

		for (int32 Index = CuesAndLoops.Num() - 1; Index >= 0; Index--)
		{
			if (CuesAndLoops[Index].CuePointID >= LargestCuePointID)
			{
				LargestCuePointID = CuesAndLoops[Index].CuePointID + 1;
			}
		}

		// When an element is added or reset to default, CuePointID is equal to INDEX_NONE so only those cue points have to be addressed
		if (PropertyChangedEvent.ChangeType & (EPropertyChangeType::ArrayAdd | EPropertyChangeType::ResetToDefault |
				EPropertyChangeType::Unspecified | EPropertyChangeType::Redirected))
		{
			// Elements can be inserted at any index in the array
			for (FSoundWaveCuePoint& CuePoint : CuesAndLoops)
			{
				if (CuePoint.CuePointID == INDEX_NONE)
				{
					CuePoint.CuePointID = LargestCuePointID++;
				}
			}
		}
		// When an element is duplicated, the CuePointID is also duplicated so the new cue point needs a new unique ID
		// Only works for adjacent indicies which is sufficient for the way Duplicate works in the details panel
		else if (PropertyChangedEvent.ChangeType & EPropertyChangeType::Duplicate)
		{
			int32 PrevCuePointID = INDEX_NONE;

			for (FSoundWaveCuePoint& CuePoint : CuesAndLoops)
			{
				if (CuePoint.CuePointID == PrevCuePointID)
				{
					CuePoint.CuePointID = LargestCuePointID++;
				}

				PrevCuePointID = CuePoint.CuePointID;
			}
		}
	}

	CueChanged.ExecuteIfBound();
}

void UWaveCueArray::InitMarkersIfNotSet(const TArray<FSoundWaveCuePoint>& InMarkers)
{
	//Prevent USoundWave from overwriting the Transformation unintentionally
	if (!bIsInitialized)
	{
		CuesAndLoops = InMarkers;
		bIsInitialized = true;
	}
}

void UWaveCueArray::Reset()
{
	CuesAndLoops.Empty();
	bIsInitialized = false;
}
#endif

void UWaveCueArray::EnableLoopRegion(FSoundWaveCuePoint* OutSoundWaveCue)
{
	check(OutSoundWaveCue != nullptr);
	OutSoundWaveCue->SetLoopRegion(true);
}
