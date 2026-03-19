// Copyright Epic Games, Inc. All Rights Reserved.

#include "HarmonixMetasound/Components/MusicClockComponent.h"
#include "HarmonixMetasound/Utils/MusicTempometerUtilities.h"
#include "HarmonixMidi/MidiSongPos.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MusicTempometerUtilities)

void MusicTempometerUtilities::UpdateMaterialParameterCollectionFromClock(const UObject* InWorldContextObject, TWeakObjectPtr<UMaterialParameterCollectionInstance>& InOutMaterialParameterCollectionInstance, const TObjectPtr<UMaterialParameterCollection>& InMaterialParameterCollection, const FMusicTempometerMPCParameters& InMCPParameters, const UMusicClockComponent* InClockComponent)
{
	if (InClockComponent)
	{
		const FMidiSongPos MidiSongPos = InClockComponent->GetCurrentVideoRenderSongPos();
		UpdateMaterialParameterCollectionFromSongPos(InWorldContextObject, InOutMaterialParameterCollectionInstance, InMaterialParameterCollection, InMCPParameters, MidiSongPos);
	}
}

void MusicTempometerUtilities::UpdateMaterialParameterCollectionFromSongPos(const UObject* InWorldContextObject, TWeakObjectPtr<UMaterialParameterCollectionInstance>& InOutMaterialParameterCollectionInstance, const TObjectPtr<UMaterialParameterCollection>& InMaterialParameterCollection, const FMusicTempometerMPCParameters& InMCPParameters, const FMidiSongPos& InMidiSongPos)
{
	// Find a MaterialParameterCollectionInstance to update
	if (!InOutMaterialParameterCollectionInstance.IsValid())
	{
		if (InMCPParameters.IsValid())
		{
			if (InWorldContextObject)
			{
				if (UWorld* World = InWorldContextObject->GetWorld())
				{
					InOutMaterialParameterCollectionInstance = World->GetParameterCollectionInstance(InMaterialParameterCollection);
				}
			}
		}

		if (!InOutMaterialParameterCollectionInstance.IsValid())
		{
			return;
		}
	}

	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.SecondsIncludingCountInParameterName, InMidiSongPos.SecondsIncludingCountIn);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.BarsIncludingCountInParameterName, InMidiSongPos.BarsIncludingCountIn);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.BeatsIncludingCountInParameterName, InMidiSongPos.BeatsIncludingCountIn);

	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.SecondsFromBarOneParameterName, InMidiSongPos.SecondsFromBarOne);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TimestampBarParameterName, static_cast<float>(InMidiSongPos.Timestamp.Bar));
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TimestampBeatInBarParameterName, InMidiSongPos.Timestamp.Beat);

	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.BarProgressParameterName, FMath::Fractional(InMidiSongPos.BarsIncludingCountIn));
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.BeatProgressParameterName, FMath::Fractional(InMidiSongPos.BeatsIncludingCountIn));

	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TimeSignatureNumeratorParameterName, InMidiSongPos.TimeSigNumerator);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TimeSignatureDenominatorParameterName, InMidiSongPos.TimeSigDenominator);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TempoParameterName, InMidiSongPos.Tempo);
	InOutMaterialParameterCollectionInstance->SetScalarParameterValue(InMCPParameters.CurrentFrameParameterNames.TimestampValidParameterName, InMidiSongPos.IsValid() ? 1.0f : 0.0f);
}