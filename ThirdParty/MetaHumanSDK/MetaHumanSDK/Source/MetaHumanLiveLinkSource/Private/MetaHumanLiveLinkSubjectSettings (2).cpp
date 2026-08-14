// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLiveLinkSubjectSettings.h"

#include "UObject/Package.h"



UMetaHumanLiveLinkSubjectSettings::UMetaHumanLiveLinkSubjectSettings()
{
	// Calibration
	Properties = FMetaHumanRealtimeCalibration::GetDefaultProperties();

	// Smoothing
	static constexpr const TCHAR* SmoothingPath = TEXT("/MetaHumanCoreTech/RealtimeMono/DefaultSmoothing.DefaultSmoothing");
	Parameters = LoadObject<UMetaHumanRealtimeSmoothingParams>(GetTransientPackage(), SmoothingPath);
}

#if WITH_EDITOR
void UMetaHumanLiveLinkSubjectSettings::PostEditChangeChainProperty(struct FPropertyChangedChainEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(InPropertyChangedEvent);

	const FProperty* Property = InPropertyChangedEvent.Property;

	// Calibration
	if (Calibration)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, Properties))
		{
			Calibration->SetProperties(Properties);
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, Alpha))
		{
			Calibration->SetAlpha(Alpha);
		}
		else if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, NeutralFrame))
		{
			Calibration->SetNeutralFrame(NeutralFrame);
		}
	}

	// Smoothing
	if (Smoothing)
	{
		if (Property->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, Parameters))
		{
			Smoothing.Reset();
		}
	}
}
#endif

bool UMetaHumanLiveLinkSubjectSettings::PreProcess(const FLiveLinkBaseStaticData& InStaticData, FLiveLinkBaseFrameData& InOutFrameData)
{
	bool bOK = true;

	TArray<float>& FrameData = InOutFrameData.PropertyValues;

	const double Now = FPlatformTime::Seconds();
	const double DeltaTime = Now - LastTime;
	LastTime = Now;

	// Calibration
	if (!Calibration)
	{
		Calibration = MakeShared<FMetaHumanRealtimeCalibration>(Properties, NeutralFrame, Alpha);
	}

	if (Calibration && CaptureNeutralFrameCountdown == -1) // Dont calibrate while capturing calibration neutral
	{
		bOK &= Calibration->ProcessFrame(InStaticData.PropertyNames, FrameData);
	}

	// Smoothing
	if (!Smoothing && Parameters)
	{
		Smoothing = MakeShared<FMetaHumanRealtimeSmoothing>(Parameters->Parameters);
	}

	if (Smoothing)
	{
		bOK &= Smoothing->ProcessFrame(InStaticData.PropertyNames, FrameData, DeltaTime);
	}

	// Set calibration neutral
	if (Calibration && CaptureNeutralFrameCountdown == 0)
	{
		NeutralFrame = FrameData;

		Calibration->SetNeutralFrame(NeutralFrame);
	}

	if (CaptureNeutralFrameCountdown != -1)
	{
		CaptureNeutralFrameCountdown--;
	}

	// Head translation
	const int32 HeadXIndex = InStaticData.PropertyNames.Find("HeadTranslationX");
	const int32 HeadYIndex = InStaticData.PropertyNames.Find("HeadTranslationY");
	const int32 HeadZIndex = InStaticData.PropertyNames.Find("HeadTranslationZ");

	if (!ensureMsgf(HeadXIndex != INDEX_NONE, TEXT("Can not find HeadTranslationX property")))
	{
		return false;
	}

	if (!ensureMsgf(HeadYIndex != INDEX_NONE, TEXT("Can not find HeadTranslationY property")))
	{
		return false;
	}

	if (!ensureMsgf(HeadZIndex != INDEX_NONE, TEXT("Can not find HeadTranslationZ property")))
	{
		return false;
	}

	const FVector HeadTranslation = FVector(FrameData[HeadXIndex], FrameData[HeadYIndex], FrameData[HeadZIndex]);

	if (CaptureNeutralHeadTranslationCountdown == 0)
	{
		NeutralHeadTranslation = HeadTranslation;
	}

	if (CaptureNeutralHeadTranslationCountdown != -1)
	{
		CaptureNeutralHeadTranslationCountdown--;
	}

	if (InOutFrameData.MetaData.StringMetaData.Contains("HeadPoseMode"))
	{
		const int32 HeadPoseMode = FCString::Atoi(*InOutFrameData.MetaData.StringMetaData["HeadPoseMode"]);

		if (HeadPoseMode == 1 && CaptureNeutralHeadTranslationCountdown == -1 && !NeutralHeadTranslation.IsZero()) // Camera relative head translation, convert into body relative if translation has finished smoothing
		{
			FrameData[HeadXIndex] = HeadTranslation.X - NeutralHeadTranslation.X;
			FrameData[HeadYIndex] = HeadTranslation.Y - NeutralHeadTranslation.Y;
			FrameData[HeadZIndex] = HeadTranslation.Z - NeutralHeadTranslation.Z;
		}
		else
		{
			FrameData[HeadXIndex] = 0;
			FrameData[HeadYIndex] = 0;
			FrameData[HeadZIndex] = 0;
		}
	}

	return bOK;
}

void UMetaHumanLiveLinkSubjectSettings::CaptureNeutrals()
{
	CaptureNeutralFrame();
	CaptureNeutralHeadTranslation();
}

void UMetaHumanLiveLinkSubjectSettings::CaptureNeutralFrame()
{
	// Somewhat arbitrary number of frames to wait before capturing the calibration
	// neutral values. The calibration neutrals needs to be captured after smoothing
	// but without any previous calibration applied. Turning off the previous calibration
	// in order to capture a new one causes a jump in animation values and that needs
	// time to be smoothed out. Ideally here we would switch off the usual smoothing while
	// capturing a neutral and instead apply a known size rolling average since the head
	// should be steady while capturing a neutral and we are only interesting in removing
	// the noise introduced by the solve and not trying to smooth out any head motion.

	CaptureNeutralFrameCountdown = 5;
}

void UMetaHumanLiveLinkSubjectSettings::CaptureNeutralHeadTranslation()
{
	// See comment above. Larger number used here to first capture neutral, then wait until
	// that is smoothed before capturing head translation.

	CaptureNeutralHeadTranslationCountdown = 10;
}
