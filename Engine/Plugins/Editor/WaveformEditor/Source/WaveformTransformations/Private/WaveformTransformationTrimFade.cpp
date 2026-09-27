// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformTransformationTrimFade.h"
#include "Sound/SoundWave.h"

const TMap<EWaveEditorFadeMode, float> UWaveformTransformationTrimFade::FadeModeToCurveValueMap =
{
	{EWaveEditorFadeMode::Linear, 1.0f},
	{EWaveEditorFadeMode::Exponetial, 3.0f},
	{EWaveEditorFadeMode::Logarithmic, 0.25f},
	{EWaveEditorFadeMode::Sigmoid, -0.1f}
};

namespace WaveformTransformationTrimFadeNames
{
	static FLazyName StartTimeName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, StartTime));
	static FLazyName EndTimeName(GET_MEMBER_NAME_CHECKED(UWaveformTransformationTrimFade, EndTime));
}

static void ApplyFadeIn(Audio::FAlignedFloatBuffer& InputAudio, const float FadeLength, const float FadeCurve, const float SCurveSharpness, const int32 NumChannels, const float SampleRate)
{
	check(NumChannels > 0);

	if (InputAudio.Num() < NumChannels || FadeLength < SMALL_NUMBER)
	{
		return;
	}

	const int32 FadeNumFrames = FMath::Min(FadeLength * SampleRate, InputAudio.Num() / NumChannels);
	float* InputPtr = InputAudio.GetData();

	for (int32 FrameIndex = 0; FrameIndex < FadeNumFrames; ++FrameIndex)
	{
		const float FadeFraction = (float)FrameIndex / FadeNumFrames;
		const float EnvValue = UWaveformTransformationTrimFade::GetFadeInCurveValue(FadeCurve, FadeFraction, SCurveSharpness);

		for (int32 ChannelIt = 0; ChannelIt < NumChannels; ++ChannelIt)
		{
			*(InputPtr + ChannelIt) *= EnvValue;
		}

		InputPtr += NumChannels;
	}
}

static void ApplyFadeOut(Audio::FAlignedFloatBuffer& InputAudio, const float FadeLength, const float FadeCurve, const float SCurveSharpness, const int32 NumChannels, const float SampleRate)
{
	check(NumChannels > 0);

	if (InputAudio.Num() < NumChannels || FadeLength < SMALL_NUMBER)
	{
		return;
	}

	const int32 FadeNumFrames = FMath::Min(FadeLength * SampleRate, InputAudio.Num() / NumChannels);
	const int32 StartSampleIndex = InputAudio.Num() - (FadeNumFrames * NumChannels);
	float* InputPtr = &InputAudio[StartSampleIndex];

	for (int32 FrameIndex = 0; FrameIndex < FadeNumFrames; ++FrameIndex)
	{
		const float FadeFraction = (float)FrameIndex / FadeNumFrames;
		float EnvValue = UWaveformTransformationTrimFade::GetFadeOutCurveValue(FadeCurve, FadeFraction, SCurveSharpness);

		for (int32 ChannelIt = 0; ChannelIt < NumChannels; ++ChannelIt)
		{
			*(InputPtr + ChannelIt) *= EnvValue;
		}

		InputPtr += NumChannels;
	}
}

FWaveTransformationTrimFade::FWaveTransformationTrimFade(double InStartTime, double InEndTime, float InStartFadeTime, float InStartFadeCurve, float InStartSCurveSharpness, float InEndFadeTime, float InEndFadeCurve, float InEndSCurveSharpness)
	: StartTime(InStartTime)
	, EndTime(InEndTime)
	, StartFadeTime(InStartFadeTime)
	, StartFadeCurve(FMath::Max(InStartFadeCurve, -0.1f))
	, StartSCurveSharpness(InStartSCurveSharpness)
	, EndFadeTime(InEndFadeTime)
	, EndFadeCurve(FMath::Max(InEndFadeCurve, -0.1f))
	, EndSCurveSharpness(InEndSCurveSharpness){}

void FWaveTransformationTrimFade::ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const
{
	check(InOutWaveInfo.SampleRate > 0.f && InOutWaveInfo.Audio != nullptr);

	Audio::FAlignedFloatBuffer& InputAudio = *InOutWaveInfo.Audio;

	if(InputAudio.Num() == 0)
	{
		return;
	}

	const int32 StartSample = FMath::RoundToInt32(StartTime * InOutWaveInfo.SampleRate) * InOutWaveInfo.NumChannels;
	int32 EndSample = InputAudio.Num() - 1;
	
	if(EndTime > 0.f)
	{	
		const int32 EndFrame = FMath::RoundToInt32(EndTime * InOutWaveInfo.SampleRate);
		EndSample = EndFrame * InOutWaveInfo.NumChannels - 1;
		EndSample = FMath::Min(EndSample, InputAudio.Num() - 1);
	}

	const int32 FinalSize = EndSample - StartSample + 1;

	InOutWaveInfo.StartFrameOffset = StartSample - (StartSample % InOutWaveInfo.NumChannels);
	InOutWaveInfo.NumEditedSamples = FinalSize;
	
	if (FinalSize > InputAudio.Num() || FinalSize <= 0)
	{
		return;
	}

	const bool bProcessFades = StartFadeTime > 0.f || EndFadeTime > 0.f;

	if (!bProcessFades && FinalSize == InputAudio.Num())
	{
		return;
	}

	TArray<float> TempBuffer;
	TempBuffer.AddUninitialized(FinalSize);

	FMemory::Memcpy(TempBuffer.GetData(), &InputAudio[StartSample], FinalSize * sizeof(float));

	InputAudio.Empty();
	InputAudio.AddUninitialized(FinalSize);

	FMemory::Memcpy(InputAudio.GetData(), TempBuffer.GetData(), FinalSize * sizeof(float));


	if(StartFadeTime > 0.f)
	{
		ApplyFadeIn(InputAudio, StartFadeTime, StartFadeCurve, StartSCurveSharpness, InOutWaveInfo.NumChannels, InOutWaveInfo.SampleRate);
	}
	
	if(EndFadeTime > 0.f)
	{
		ApplyFadeOut(InputAudio, EndFadeTime, EndFadeCurve, EndSCurveSharpness, InOutWaveInfo.NumChannels, InOutWaveInfo.SampleRate);
	}
}

Audio::FTransformationPtr UWaveformTransformationTrimFade::CreateTransformation() const
{
	return MakeUnique<FWaveTransformationTrimFade>(StartTime, EndTime, StartFadeTime, StartFadeCurve, StartSCurveSharpness, EndFadeTime, EndFadeCurve, EndSCurveSharpness);
}

void UWaveformTransformationTrimFade::UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration)
{
	UpdateDurationProperties(InOutConfiguration.EndTime - InOutConfiguration.StartTime);

	InOutConfiguration.StartTime = StartTime;
	InOutConfiguration.EndTime = EndTime;
}

const double UWaveformTransformationTrimFade::GetFadeInCurveValue(const float StartFadeCurve, const double FadeFraction, const float SCurveSharpness)
{
	double CurveValue = 1;

	if (StartFadeCurve < 0)
	{
		double Slope = 10 * SCurveSharpness * (PI + 1);
		
		if (FadeFraction <= 0.5)
		{
			CurveValue = (((FMath::Tanh(((Slope * FadeFraction * PI) / 2) - ((Slope * PI) / 4))) + 1) / 2) * (2 * FadeFraction);
		}
		else
		{
			CurveValue = ((FMath::Tanh((-Slope * (-FadeFraction + 1) * PI) / 2 + ((Slope * PI) / 4)) - 1) / 2) * (2 * (-FadeFraction + 1)) + 1;
		}
	}
	else
	{
		CurveValue = FMath::Pow(FadeFraction, StartFadeCurve);
	}

	return CurveValue;
}

const double UWaveformTransformationTrimFade::GetFadeOutCurveValue(const float EndFadeCurve, const double FadeFraction, const float SCurveSharpness)
{
	double CurveValue = 1;

	if (EndFadeCurve < 0)
	{
		double Slope = 10 * SCurveSharpness * (PI + 1);
		
		if (FadeFraction <= 0.5)
		{
			CurveValue = -(((FMath::Tanh(((Slope * FadeFraction * PI) / 2) - ((Slope * PI) / 4))) + 1) / 2) * (2 * FadeFraction) + 1;
		}
		else
		{
			CurveValue = -((FMath::Tanh((-Slope * (-FadeFraction + 1) * PI) / 2 + ((Slope * PI) / 4)) - 1) / 2) * (2 * (-FadeFraction + 1));
		}
	}
	else
	{
		// Changed from 1.f - FMath::Pow(FadeFraction, FadeCurve) so that the fade out curve is a horizontally mirrored version of fade in 
		// instead of vertically mirrored
		CurveValue = FMath::Pow(-FadeFraction + 1, EndFadeCurve);
	}

	return CurveValue;
}

void UWaveformTransformationTrimFade::UpdateDurationProperties(const float InAvailableDuration)
{
	check(InAvailableDuration > 0)
	AvailableWaveformDuration = InAvailableDuration;
	StartTime = FMath::Clamp(StartTime, 0.f, AvailableWaveformDuration - UE_KINDA_SMALL_NUMBER);
	EndTime = EndTime < 0 ? EndTime = AvailableWaveformDuration : FMath::Clamp(EndTime, StartTime + UE_KINDA_SMALL_NUMBER, AvailableWaveformDuration);
}