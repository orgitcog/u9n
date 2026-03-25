// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanRealtimeCalibration.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanRealtimeCalibration, Log, All);

// 'estimated_raw_no_gaze.json' from MH-13695 with '.' replaced with '_' for each property name.
TArray<FName> FMetaHumanRealtimeCalibration::DefaultProperties = {
	"CTRL_expressions_browDownL",
    "CTRL_expressions_browDownR",
    "CTRL_expressions_browLateralL",
    "CTRL_expressions_browLateralR",
    "CTRL_expressions_browRaiseInL",
    "CTRL_expressions_browRaiseInR",
    "CTRL_expressions_browRaiseOuterL",
    "CTRL_expressions_browRaiseOuterR",
    "CTRL_expressions_eyeBlinkL",
    "CTRL_expressions_eyeBlinkR",
    "CTRL_expressions_eyeWidenL",
    "CTRL_expressions_eyeWidenR",
    "CTRL_expressions_eyeSquintInnerL",
    "CTRL_expressions_eyeSquintInnerR",
    "CTRL_expressions_eyeCheekRaiseL",
    "CTRL_expressions_eyeCheekRaiseR",
    "CTRL_expressions_eyeFaceScrunchL",
    "CTRL_expressions_eyeFaceScrunchR",
    "CTRL_expressions_noseWrinkleL",
    "CTRL_expressions_noseWrinkleR",
    "CTRL_expressions_noseNostrilDepressL",
    "CTRL_expressions_noseNostrilDepressR",
    "CTRL_expressions_noseNostrilDilateL",
    "CTRL_expressions_noseNostrilDilateR",
    "CTRL_expressions_noseNostrilCompressL",
    "CTRL_expressions_noseNostrilCompressR",
    "CTRL_expressions_noseNasolabialDeepenL",
    "CTRL_expressions_noseNasolabialDeepenR",
    "CTRL_expressions_mouthCheekSuckL",
    "CTRL_expressions_mouthCheekSuckR",
    "CTRL_expressions_mouthCheekBlowL",
    "CTRL_expressions_mouthCheekBlowR",
    "CTRL_expressions_mouthLeft",
    "CTRL_expressions_mouthRight",
    "CTRL_expressions_mouthUpperLipRaiseL",
    "CTRL_expressions_mouthUpperLipRaiseR",
    "CTRL_expressions_mouthLowerLipDepressL",
    "CTRL_expressions_mouthLowerLipDepressR",
    "CTRL_expressions_mouthCornerPullL",
    "CTRL_expressions_mouthCornerPullR",
    "CTRL_expressions_mouthStretchL",
    "CTRL_expressions_mouthStretchR",
    "CTRL_expressions_mouthDimpleL",
    "CTRL_expressions_mouthDimpleR",
    "CTRL_expressions_mouthCornerDepressL",
    "CTRL_expressions_mouthCornerDepressR",
    "CTRL_expressions_mouthLipsPurseUL",
    "CTRL_expressions_mouthLipsPurseUR",
    "CTRL_expressions_mouthLipsPurseDL",
    "CTRL_expressions_mouthLipsPurseDR",
    "CTRL_expressions_mouthLipsTowardsUL",
    "CTRL_expressions_mouthLipsTowardsUR",
    "CTRL_expressions_mouthLipsTowardsDL",
    "CTRL_expressions_mouthLipsTowardsDR",
    "CTRL_expressions_mouthFunnelUL",
    "CTRL_expressions_mouthFunnelUR",
    "CTRL_expressions_mouthFunnelDL",
    "CTRL_expressions_mouthFunnelDR",
    "CTRL_expressions_mouthLipsTogetherUL",
    "CTRL_expressions_mouthLipsTogetherUR",
    "CTRL_expressions_mouthLipsTogetherDL",
    "CTRL_expressions_mouthLipsTogetherDR",
    "CTRL_expressions_mouthUpperLipBiteL",
    "CTRL_expressions_mouthUpperLipBiteR",
    "CTRL_expressions_mouthLowerLipBiteL",
    "CTRL_expressions_mouthLowerLipBiteR",
    "CTRL_expressions_mouthLipsTightenUL",
    "CTRL_expressions_mouthLipsTightenUR",
    "CTRL_expressions_mouthLipsTightenDL",
    "CTRL_expressions_mouthLipsTightenDR",
    "CTRL_expressions_mouthLipsPressL",
    "CTRL_expressions_mouthLipsPressR",
    "CTRL_expressions_mouthSharpCornerPullL",
    "CTRL_expressions_mouthSharpCornerPullR",
    "CTRL_expressions_mouthStickyUC",
    "CTRL_expressions_mouthStickyUINL",
    "CTRL_expressions_mouthStickyUINR",
    "CTRL_expressions_mouthStickyUOUTL",
    "CTRL_expressions_mouthStickyUOUTR",
    "CTRL_expressions_mouthStickyDC",
    "CTRL_expressions_mouthStickyDINL",
    "CTRL_expressions_mouthStickyDINR",
    "CTRL_expressions_mouthStickyDOUTL",
    "CTRL_expressions_mouthStickyDOUTR",
    "CTRL_expressions_mouthLipsPushUL",
    "CTRL_expressions_mouthLipsPushUR",
    "CTRL_expressions_mouthLipsPushDL",
    "CTRL_expressions_mouthLipsPushDR",
    "CTRL_expressions_mouthLipsPullUL",
    "CTRL_expressions_mouthLipsPullUR",
    "CTRL_expressions_mouthLipsPullDL",
    "CTRL_expressions_mouthLipsPullDR",
    "CTRL_expressions_mouthLipsThinUL",
    "CTRL_expressions_mouthLipsThinUR",
    "CTRL_expressions_mouthLipsThinDL",
    "CTRL_expressions_mouthLipsThinDR",
    "CTRL_expressions_mouthLipsThickUL",
    "CTRL_expressions_mouthLipsThickUR",
    "CTRL_expressions_mouthLipsThickDL",
    "CTRL_expressions_mouthLipsThickDR",
    "CTRL_expressions_mouthCornerSharpenUL",
    "CTRL_expressions_mouthCornerSharpenUR",
    "CTRL_expressions_mouthCornerSharpenDL",
    "CTRL_expressions_mouthCornerSharpenDR",
    "CTRL_expressions_mouthCornerRounderUL",
    "CTRL_expressions_mouthCornerRounderUR",
    "CTRL_expressions_mouthCornerRounderDL",
    "CTRL_expressions_mouthCornerRounderDR",
    "CTRL_expressions_mouthUpperLipShiftLeft",
    "CTRL_expressions_mouthUpperLipShiftRight",
    "CTRL_expressions_mouthLowerLipShiftLeft",
    "CTRL_expressions_mouthLowerLipShiftRight",
    "CTRL_expressions_mouthUpperLipRollInL",
    "CTRL_expressions_mouthUpperLipRollInR",
    "CTRL_expressions_mouthUpperLipRollOutL",
    "CTRL_expressions_mouthUpperLipRollOutR",
    "CTRL_expressions_mouthLowerLipRollInL",
    "CTRL_expressions_mouthLowerLipRollInR",
    "CTRL_expressions_mouthLowerLipRollOutL",
    "CTRL_expressions_mouthLowerLipRollOutR",
    "CTRL_expressions_mouthCornerUpL",
    "CTRL_expressions_mouthCornerUpR",
    "CTRL_expressions_mouthCornerDownL",
    "CTRL_expressions_mouthCornerDownR",
    "CTRL_expressions_jawOpen",
    "CTRL_expressions_jawLeft",
    "CTRL_expressions_jawRight",
    "CTRL_expressions_jawFwd",
    "CTRL_expressions_jawBack",
    "CTRL_expressions_jawChinRaiseDL",
    "CTRL_expressions_jawChinRaiseDR",
    "CTRL_expressions_jawOpenExtreme"
};

FMetaHumanRealtimeCalibration::FMetaHumanRealtimeCalibration(const TArray<FName>& InProperties, const TArray<float>& InNeutralFrame, const float InAlpha)
	: Properties(InProperties)
	, NeutralFrame(InNeutralFrame)
	, Alpha(InAlpha)
{}

TArray<FName> FMetaHumanRealtimeCalibration::GetDefaultProperties()
{
	return DefaultProperties;
}

void FMetaHumanRealtimeCalibration::SetProperties(const TArray<FName>& InProperties)
{
	Properties = InProperties;
}

void FMetaHumanRealtimeCalibration::SetAlpha(float InAlpha)
{
	Alpha = InAlpha;
}

void FMetaHumanRealtimeCalibration::SetNeutralFrame(const TArray<float>& InNeutralFrame)
{
	NeutralFrame = InNeutralFrame;
}

bool FMetaHumanRealtimeCalibration::ProcessFrame(const TArray<FName>& InPropertyNames, TArray<float>& InOutFrame) const
{
	// NeutralFrame needs a matching number of values in order for calibration to take place.
	// The index of each neutral value must match that of the incoming frame.
	if (NeutralFrame.Num() != InOutFrame.Num())
	{
		return false;
	}

	for (const FName& Property : Properties)
	{
		const int32 PropertyIndex = InPropertyNames.Find(Property);

		if (PropertyIndex == INDEX_NONE)
		{
			UE_LOG(LogMetaHumanRealtimeCalibration, Warning, TEXT("Specified property name %s not found. Check pre processor configuration."), *Property.ToString());
			continue;
		}
		
		const float RawValue = InOutFrame[PropertyIndex];
		const float NeutralValue = NeutralFrame[PropertyIndex];

		// Avoid a divide by zero error that could occur if the NeutralValue is 1.0
		// (1 - NeutralValue * Alpha) will be zero if NeutralValue == 1.0
		if (NeutralValue < 1.0)
		{
			// new_coefficient = (coefficient - calibration_value) / (1 - calibration_value * alpha)
			const float CalibratedValue = (RawValue - NeutralValue) / (1 - NeutralValue * Alpha);
			InOutFrame[PropertyIndex] = CalibratedValue;
		}
	}

	return true;
}
