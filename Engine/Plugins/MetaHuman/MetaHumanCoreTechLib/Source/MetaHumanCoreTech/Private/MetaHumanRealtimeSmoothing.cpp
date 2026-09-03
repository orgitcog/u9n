// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanRealtimeSmoothing.h"

#include "GuiToRawControlsUtils.h"
#include "Math/TransformCalculus3D.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanRealtimeSmoothing, Log, All)



void UMetaHumanRealtimeSmoothingParams::PostInitProperties()
{
	Super::PostInitProperties();

#if 0 // To overwrite any content with the default values
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		Parameters = FMetaHumanRealtimeSmoothing::GetDefaultSmoothingParams();
	}
#endif
}



// If you change these defaults remember to recreate the DefaultSmoothing asset

#if 1 // Switch this to change between default and heavy smoothing
TMap<FName, uint8> FMetaHumanRealtimeSmoothing::DefaultRollingAverage = {
{ "CTRL_expressions_browDownL", 3 },
{ "CTRL_expressions_browDownR", 3 },
{ "CTRL_expressions_browLateralL", 3 },
{ "CTRL_expressions_browLateralR", 3 },
{ "CTRL_expressions_browRaiseInL", 3 },
{ "CTRL_expressions_browRaiseInR", 3 },
{ "CTRL_expressions_browRaiseOuterL", 3 },
{ "CTRL_expressions_browRaiseOuterR", 3 },
{ "CTRL_expressions_eyeWidenL", 2 },
{ "CTRL_expressions_eyeWidenR", 2 },
{ "CTRL_expressions_eyeSquintInnerL", 2 },
{ "CTRL_expressions_eyeSquintInnerR", 2 },
{ "CTRL_expressions_eyeCheekRaiseL", 3 },
{ "CTRL_expressions_eyeCheekRaiseR", 3 },
{ "CTRL_expressions_eyeFaceScrunchL", 2 },
{ "CTRL_expressions_eyeFaceScrunchR", 2 },
{ "CTRL_expressions_mouthCheekSuckL", 2 },
{ "CTRL_expressions_mouthCheekSuckR", 2 },
{ "CTRL_expressions_mouthCheekBlowL", 2 },
{ "CTRL_expressions_mouthCheekBlowR", 2 },
{ "CTRL_expressions_mouthCornerPullL", 3 },
{ "CTRL_expressions_mouthCornerPullR", 3 },
{ "CTRL_expressions_mouthStretchL", 3 },
{ "CTRL_expressions_mouthStretchR", 3 },
{ "CTRL_expressions_mouthDimpleL", 4 },
{ "CTRL_expressions_mouthDimpleR", 4 },
{ "CTRL_expressions_mouthCornerDepressL", 3 },
{ "CTRL_expressions_mouthCornerDepressR", 3 },
{ "CTRL_expressions_mouthUpperLipBiteL", 2 },
{ "CTRL_expressions_mouthUpperLipBiteR", 2 },
{ "CTRL_expressions_mouthLowerLipBiteL", 2 },
{ "CTRL_expressions_mouthLowerLipBiteR", 2 },
{ "CTRL_expressions_mouthLipsTightenUL", 2 },
{ "CTRL_expressions_mouthLipsTightenUR", 2 },
{ "CTRL_expressions_mouthLipsTightenDL", 2 },
{ "CTRL_expressions_mouthLipsTightenDR", 2 },
{ "CTRL_expressions_mouthLipsPressL", 2 },
{ "CTRL_expressions_mouthLipsPressR", 2 },
{ "CTRL_expressions_mouthSharpCornerPullL", 4 },
{ "CTRL_expressions_mouthSharpCornerPullR", 4 },
{ "CTRL_expressions_mouthLipsPushUL", 3 },
{ "CTRL_expressions_mouthLipsPushUR", 3 },
{ "CTRL_expressions_mouthLipsPushDL", 3 },
{ "CTRL_expressions_mouthLipsPushDR", 3 },
{ "CTRL_expressions_mouthLipsPullUL", 3 },
{ "CTRL_expressions_mouthLipsPullUR", 3 },
{ "CTRL_expressions_mouthLipsPullDL", 3 },
{ "CTRL_expressions_mouthLipsPullDR", 3 },
{ "CTRL_expressions_mouthLipsThinUL", 3 },
{ "CTRL_expressions_mouthLipsThinUR", 3 },
{ "CTRL_expressions_mouthLipsThinDL", 3 },
{ "CTRL_expressions_mouthLipsThinDR", 3 },
{ "CTRL_expressions_mouthLipsThickUL", 3 },
{ "CTRL_expressions_mouthLipsThickUR", 3 },
{ "CTRL_expressions_mouthLipsThickDL", 3 },
{ "CTRL_expressions_mouthLipsThickDR", 3 },
{ "CTRL_expressions_mouthCornerSharpenUL", 2 },
{ "CTRL_expressions_mouthCornerSharpenUR", 2 },
{ "CTRL_expressions_mouthCornerSharpenDL", 2 },
{ "CTRL_expressions_mouthCornerSharpenDR", 2 },
{ "CTRL_expressions_mouthCornerRounderUL", 2 },
{ "CTRL_expressions_mouthCornerRounderUR", 2 },
{ "CTRL_expressions_mouthCornerRounderDL", 2 },
{ "CTRL_expressions_mouthCornerRounderDR", 2 },
{ "CTRL_expressions_mouthUpperLipRollInL", 2 },
{ "CTRL_expressions_mouthUpperLipRollInR", 2 },
{ "CTRL_expressions_mouthUpperLipRollOutL", 2 },
{ "CTRL_expressions_mouthUpperLipRollOutR", 2 },
{ "CTRL_expressions_mouthLowerLipRollInL", 2 },
{ "CTRL_expressions_mouthLowerLipRollInR", 2 },
{ "CTRL_expressions_mouthLowerLipRollOutL", 2 },
{ "CTRL_expressions_mouthLowerLipRollOutR", 2 },
{ "CTRL_expressions_mouthCornerUpL", 3 },
{ "CTRL_expressions_mouthCornerUpR", 3 },
{ "CTRL_expressions_mouthCornerDownL", 3 },
{ "CTRL_expressions_mouthCornerDownR", 3 },
{ "CTRL_expressions_jawOpen", 2 },
{ "CTRL_expressions_jawLeft", 2 },
{ "CTRL_expressions_jawRight", 2 }
};

TMap<FName, TPair<float, float>> FMetaHumanRealtimeSmoothing::DefaultOneEuro = {
{ "CTRL_expressions_eyeLookUpL", { 1000, 10 } },
{ "CTRL_expressions_eyeLookUpR", { 1000, 10 } },
{ "CTRL_expressions_eyeLookDownL", { 1000, 10 } },
{ "CTRL_expressions_eyeLookDownR", { 1000, 10 } },
{ "CTRL_expressions_eyeLookLeftL", { 1000, 10 } },
{ "CTRL_expressions_eyeLookLeftR", { 1000, 10 } },
{ "CTRL_expressions_eyeLookRightL", { 1000, 10 } },
{ "CTRL_expressions_eyeLookRightR", { 1000, 10 } },
{ "CTRL_expressions_noseWrinkleL", { 5000, 5 } },
{ "CTRL_expressions_noseWrinkleR", { 5000, 5 } },
{ "CTRL_expressions_noseWrinkleUpperL", { 5000, 5 } },
{ "CTRL_expressions_noseWrinkleUpperR", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilDepressL", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilDepressR", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilDilateL", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilDilateR", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilCompressL", { 5000, 5 } },
{ "CTRL_expressions_noseNostrilCompressR", { 5000, 5 } },
{ "CTRL_expressions_noseNasolabialDeepenL", { 5000, 5 } },
{ "CTRL_expressions_noseNasolabialDeepenR", { 5000, 5 } },
{ "CTRL_expressions_mouthLeft", { 5000, 5 } },
{ "CTRL_expressions_mouthRight", { 5000, 5 } },
{ "CTRL_expressions_mouthUpperLipRaiseL", { 1000, 10 } },
{ "CTRL_expressions_mouthUpperLipRaiseR", { 1000, 10 } },
{ "CTRL_expressions_mouthLowerLipDepressL", { 1000, 10 } },
{ "CTRL_expressions_mouthLowerLipDepressR", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsPurseUL", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsPurseUR", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsPurseDL", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsPurseDR", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsTowardsUL", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsTowardsUR", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsTowardsDL", { 1000, 10 } },
{ "CTRL_expressions_mouthLipsTowardsDR", { 1000, 10 } },
{ "CTRL_expressions_mouthFunnelUL", { 1000, 10 } },
{ "CTRL_expressions_mouthFunnelUR", { 1000, 10 } },
{ "CTRL_expressions_mouthFunnelDL", { 1000, 10 } },
{ "CTRL_expressions_mouthFunnelDR", { 1000, 10 } },
{ "CTRL_expressions_mouthUpperLipShiftLeft", { 5000, 5 } },
{ "CTRL_expressions_mouthUpperLipShiftRight", { 5000, 5 } },
{ "CTRL_expressions_mouthLowerLipShiftLeft", { 5000, 5 } },
{ "CTRL_expressions_mouthLowerLipShiftRight", { 5000, 5 } },
{ "CTRL_expressions_jawFwd", { 5000, 5 } },
{ "CTRL_expressions_jawBack", { 5000, 5 } },
{ "CTRL_expressions_jawChinRaiseDL", { 1000, 10 } },
{ "CTRL_expressions_jawChinRaiseDR", { 1000, 10 } }
};
#else
TMap<FName, uint8> FMetaHumanRealtimeSmoothing::DefaultRollingAverage = {
 { "CTRL_expressions_browDownL", 6 },
 { "CTRL_expressions_browDownR", 6 },
 { "CTRL_expressions_browLateralL", 6 },
 { "CTRL_expressions_browLateralR", 6 },
 { "CTRL_expressions_browRaiseInL", 6 },
 { "CTRL_expressions_browRaiseInR", 6 },
 { "CTRL_expressions_browRaiseOuterL", 6 },
 { "CTRL_expressions_browRaiseOuterR", 6 },
 { "CTRL_expressions_eyeCheekRaiseL", 6 },
 { "CTRL_expressions_eyeCheekRaiseR", 6 },
 { "CTRL_expressions_eyeFaceScrunchL", 4 },
 { "CTRL_expressions_eyeFaceScrunchR", 4 },
 { "CTRL_expressions_mouthCheekSuckL", 4 },
 { "CTRL_expressions_mouthCheekSuckR", 4 },
 { "CTRL_expressions_mouthCheekBlowL", 4 },
 { "CTRL_expressions_mouthCheekBlowR", 4 },
 { "CTRL_expressions_mouthLeft", 4 },
 { "CTRL_expressions_mouthRight", 4 },
 { "CTRL_expressions_mouthDimpleL", 4 },
 { "CTRL_expressions_mouthDimpleR", 4 },
 { "CTRL_expressions_mouthUpperLipBiteL", 4 },
 { "CTRL_expressions_mouthUpperLipBiteR", 4 },
 { "CTRL_expressions_mouthLowerLipBiteL", 4 },
 { "CTRL_expressions_mouthLowerLipBiteR", 4 },
 { "CTRL_expressions_mouthLipsTightenUL", 4 },
 { "CTRL_expressions_mouthLipsTightenUR", 4 },
 { "CTRL_expressions_mouthLipsTightenDL", 4 },
 { "CTRL_expressions_mouthLipsTightenDR", 4 },
 { "CTRL_expressions_mouthLipsPressL", 4 },
 { "CTRL_expressions_mouthLipsPressR", 4 },
 { "CTRL_expressions_mouthSharpCornerPullL", 4 },
 { "CTRL_expressions_mouthSharpCornerPullR", 4 },
 { "CTRL_expressions_mouthLipsPushUL", 4 },
 { "CTRL_expressions_mouthLipsPushUR", 4 },
 { "CTRL_expressions_mouthLipsPushDL", 4 },
 { "CTRL_expressions_mouthLipsPushDR", 4 },
 { "CTRL_expressions_mouthLipsPullUL", 4 },
 { "CTRL_expressions_mouthLipsPullUR", 4 },
 { "CTRL_expressions_mouthLipsPullDL", 4 },
 { "CTRL_expressions_mouthLipsPullDR", 4 },
 { "CTRL_expressions_mouthLipsThinUL", 4 },
 { "CTRL_expressions_mouthLipsThinUR", 4 },
 { "CTRL_expressions_mouthLipsThinDL", 4 },
 { "CTRL_expressions_mouthLipsThinDR", 4 },
 { "CTRL_expressions_mouthLipsThickUL", 4 },
 { "CTRL_expressions_mouthLipsThickUR", 4 },
 { "CTRL_expressions_mouthLipsThickDL", 4 },
 { "CTRL_expressions_mouthLipsThickDR", 4 },
 { "CTRL_expressions_mouthCornerSharpenUL", 4 },
 { "CTRL_expressions_mouthCornerSharpenUR", 4 },
 { "CTRL_expressions_mouthCornerSharpenDL", 4 },
 { "CTRL_expressions_mouthCornerSharpenDR", 4 },
 { "CTRL_expressions_mouthCornerRounderUL", 4 },
 { "CTRL_expressions_mouthCornerRounderUR", 4 },
 { "CTRL_expressions_mouthCornerRounderDL", 4 },
 { "CTRL_expressions_mouthCornerRounderDR", 4 },
 { "CTRL_expressions_mouthUpperLipShiftLeft", 4 },
 { "CTRL_expressions_mouthUpperLipShiftRight", 4 },
 { "CTRL_expressions_mouthLowerLipShiftLeft", 4 },
 { "CTRL_expressions_mouthLowerLipShiftRight", 4 },
 { "CTRL_expressions_mouthUpperLipRollInL", 4 },
 { "CTRL_expressions_mouthUpperLipRollInR", 4 },
 { "CTRL_expressions_mouthUpperLipRollOutL", 4 },
 { "CTRL_expressions_mouthUpperLipRollOutR", 4 },
 { "CTRL_expressions_mouthLowerLipRollInL", 4 },
 { "CTRL_expressions_mouthLowerLipRollInR", 4 },
 { "CTRL_expressions_mouthLowerLipRollOutL", 4 },
 { "CTRL_expressions_mouthLowerLipRollOutR", 4 },
 { "CTRL_expressions_mouthCornerUpL", 4 },
 { "CTRL_expressions_mouthCornerUpR", 4 },
 { "CTRL_expressions_mouthCornerDownL", 4 },
 { "CTRL_expressions_mouthCornerDownR", 4 }
};

TMap<FName, TPair<float, float>> FMetaHumanRealtimeSmoothing::DefaultOneEuro = {
 { "CTRL_expressions_eyeBlinkL", { 5000, 5 } },
 { "CTRL_expressions_eyeBlinkR", { 5000, 5 } },
 { "CTRL_expressions_eyeWidenL", { 5000, 5 } },
 { "CTRL_expressions_eyeWidenR", { 5000, 5 } },
 { "CTRL_expressions_eyeSquintInnerL", { 5000, 5 } },
 { "CTRL_expressions_eyeSquintInnerR", { 5000, 5 } },
 { "CTRL_expressions_eyeLookUpL", { 5000, 5 } },
 { "CTRL_expressions_eyeLookUpR", { 5000, 5 } },
 { "CTRL_expressions_eyeLookDownL", { 5000, 5 } },
 { "CTRL_expressions_eyeLookDownR", { 5000, 5 } },
 { "CTRL_expressions_eyeLookLeftL", { 5000, 5 } },
 { "CTRL_expressions_eyeLookLeftR", { 5000, 5 } },
 { "CTRL_expressions_eyeLookRightL", { 5000, 5 } },
 { "CTRL_expressions_eyeLookRightR", { 5000, 5 } },
 { "CTRL_expressions_noseWrinkleL", { 5000, 5 } },
 { "CTRL_expressions_noseWrinkleR", { 5000, 5 } },
 { "CTRL_expressions_noseWrinkleUpperL", { 5000, 5 } },
 { "CTRL_expressions_noseWrinkleUpperR", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilDepressL", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilDepressR", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilDilateL", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilDilateR", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilCompressL", { 5000, 5 } },
 { "CTRL_expressions_noseNostrilCompressR", { 5000, 5 } },
 { "CTRL_expressions_noseNasolabialDeepenL", { 5000, 5 } },
 { "CTRL_expressions_noseNasolabialDeepenR", { 5000, 5 } },
 { "CTRL_expressions_mouthUp", { 5000, 5 } },
 { "CTRL_expressions_mouthDown", { 5000, 5 } },
 { "CTRL_expressions_mouthUpperLipRaiseL", { 5000, 5 } },
 { "CTRL_expressions_mouthUpperLipRaiseR", { 5000, 5 } },
 { "CTRL_expressions_mouthLowerLipDepressL", { 5000, 5 } },
 { "CTRL_expressions_mouthLowerLipDepressR", { 5000, 5 } },
 { "CTRL_expressions_mouthCornerPullL", { 5000, 5 } },
 { "CTRL_expressions_mouthCornerPullR", { 5000, 5 } },
 { "CTRL_expressions_mouthStretchL", { 5000, 5 } },
 { "CTRL_expressions_mouthStretchR", { 5000, 5 } },
 { "CTRL_expressions_mouthCornerDepressL", { 5000, 5 } },
 { "CTRL_expressions_mouthCornerDepressR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsPurseUL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsPurseUR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsPurseDL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsPurseDR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTowardsUL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTowardsUR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTowardsDL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTowardsDR", { 5000, 5 } },
 { "CTRL_expressions_mouthFunnelUL", { 5000, 5 } },
 { "CTRL_expressions_mouthFunnelUR", { 5000, 5 } },
 { "CTRL_expressions_mouthFunnelDL", { 5000, 5 } },
 { "CTRL_expressions_mouthFunnelDR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTogetherUL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTogetherUR", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTogetherDL", { 5000, 5 } },
 { "CTRL_expressions_mouthLipsTogetherDR", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyUC", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyUINL", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyUINR", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyUOUTL", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyUOUTR", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyDC", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyDINL", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyDINR", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyDOUTL", { 5000, 5 } },
 { "CTRL_expressions_mouthStickyDOUTR", { 5000, 5 } },
 { "CTRL_expressions_jawOpen", { 5000, 5 } },
 { "CTRL_expressions_jawLeft", { 5000, 5 } },
 { "CTRL_expressions_jawRight", { 5000, 5 } },
 { "CTRL_expressions_jawFwd", { 5000, 5 } },
 { "CTRL_expressions_jawBack", { 5000, 5 } },
 { "CTRL_expressions_jawChinRaiseDL", { 5000, 5 } },
 { "CTRL_expressions_jawChinRaiseDR", { 5000, 5 } },
 { "CTRL_expressions_jawOpenExtreme", { 5000, 5 } },
 { "CTRL_expressions_neckStretchL", { 5000, 5 } }
};
#endif

FMetaHumanRealtimeSmoothing::FMetaHumanRealtimeSmoothing(const TMap<FName, FMetaHumanRealtimeSmoothingParam>& InSmoothingParams) : SmoothingParams(InSmoothingParams)
{
	for (const TPair<FName, FMetaHumanRealtimeSmoothingParam>& SmoothingParam : SmoothingParams)
	{
		if (SmoothingParam.Value.Method == EMetaHumanRealtimeSmoothingParamMethod::RollingAverage)
		{
			const uint8 FrameCount = SmoothingParam.Value.RollingAverageFrame;
			if (FrameCount > RollingAverageMaxBufferSize)
			{
				RollingAverageMaxBufferSize = FrameCount;
			}
		}
		else if (SmoothingParam.Value.Method == EMetaHumanRealtimeSmoothingParamMethod::OneEuro)
		{
			FMetaHumanOneEuroFilter OneEuroFilter;

			OneEuroFilter.SetCutoffSlope(SmoothingParam.Value.OneEuroSlope);
			OneEuroFilter.SetMinCutoff(SmoothingParam.Value.OneEuroMinCutoff);

			if (IsOrientation(SmoothingParam.Key))
			{
				for (int32 Index = 0; Index < 3; ++Index)
				{
					OneEuroYAxis[Index] = OneEuroFilter;
					OneEuroXAxis[Index] = OneEuroFilter;
				}
			}
			else
			{

				OneEuroFilters.Add(SmoothingParam.Key, OneEuroFilter);
			}
		}
		else
		{
			check(false);
		}
	}
}

TMap<FName, FMetaHumanRealtimeSmoothingParam> FMetaHumanRealtimeSmoothing::GetDefaultSmoothingParams()
{
	TMap<FName, FMetaHumanRealtimeSmoothingParam> SmoothingParams;

	TMap<FString, float> RawControls = GuiToRawControlsUtils::ConvertGuiToRawControls(TMap<FString, float> {});
	RawControls.Add("HeadOrientation", 0); // Add in orientation and translation controls to the list.
	RawControls.Add("HeadTranslationX", 0);
	RawControls.Add("HeadTranslationY", 0);
	RawControls.Add("HeadTranslationZ", 0);

	for (const TPair<FString, float>& RawControl : RawControls)
	{
		const FName Key = FName(RawControl.Key);

		FMetaHumanRealtimeSmoothingParam SmoothingParam;

		if (DefaultOneEuro.Contains(Key))
		{
			SmoothingParam.Method = EMetaHumanRealtimeSmoothingParamMethod::OneEuro;
			SmoothingParam.OneEuroSlope = DefaultOneEuro[Key].Key;
			SmoothingParam.OneEuroMinCutoff = DefaultOneEuro[Key].Value;
		}
		else
		{
			uint8 FrameCount = DefaultRollingAverageFrameCount;

			if (DefaultRollingAverage.Contains(Key))
			{
				FrameCount = DefaultRollingAverage[Key];
			}

			SmoothingParam.Method = EMetaHumanRealtimeSmoothingParamMethod::RollingAverage;
			SmoothingParam.RollingAverageFrame = FrameCount;
		}

		SmoothingParams.Emplace(Key, SmoothingParam);
	}

	return SmoothingParams;
}

bool FMetaHumanRealtimeSmoothing::ProcessFrame(const TArray<FName>& InPropertyNames, TArray<float>& InOutFrame, double InDeltaTime)
{
	if (RollingAverageMaxBufferSize == 0)
	{
		UE_LOG(LogMetaHumanRealtimeSmoothing, Warning, TEXT("Invalid max buffer size. All values are unmodified."));
		return false;
	}

	if (RollingAverageBuffer.Num() >= RollingAverageMaxBufferSize)
	{
		RollingAverageBuffer.RemoveAt(0);
	}

	RollingAverageBuffer.Add(InOutFrame);

	for (const TPair<FName, FMetaHumanRealtimeSmoothingParam>& SmoothingParamPair : SmoothingParams)
	{
		const FName PropertyName = SmoothingParamPair.Key;
		const FMetaHumanRealtimeSmoothingParam& SmoothingParam = SmoothingParamPair.Value;

		const bool bIsOrientation = IsOrientation(PropertyName);

		int32 PropertyIndex = INDEX_NONE;
		int32 RollIndex = INDEX_NONE;
		int32 PitchIndex = INDEX_NONE;
		int32 YawIndex = INDEX_NONE;

		if (bIsOrientation)
		{
			RollIndex = InPropertyNames.Find("HeadRoll");
			PitchIndex = InPropertyNames.Find("HeadPitch");
			YawIndex = InPropertyNames.Find("HeadYaw");

			if (RollIndex == INDEX_NONE || PitchIndex == INDEX_NONE || YawIndex == INDEX_NONE)
			{
				UE_LOG(LogMetaHumanRealtimeSmoothing, Warning, TEXT("Specified property name HeadRoll/HeadPitch/HeadYaw not found. Check pre processor configuration."));
				continue;
			}
		}
		else
		{
			PropertyIndex = InPropertyNames.Find(PropertyName);

			if (PropertyIndex == INDEX_NONE)
			{
				UE_LOG(LogMetaHumanRealtimeSmoothing, Warning, TEXT("Specified property name %s not found. Check pre processor configuration."), *PropertyName.ToString());
				continue;
			}
		}

		if (SmoothingParam.Method == EMetaHumanRealtimeSmoothingParamMethod::RollingAverage)
		{
			const uint8 FrameCount = SmoothingParam.RollingAverageFrame;

			if (FrameCount <= 0)
			{
				UE_LOG(LogMetaHumanRealtimeSmoothing, Error, TEXT("Encountered invalid frame count %d for property name %s. Skipping."), FrameCount, *PropertyName.ToString());
				continue;
			}

			const int32 FinalBufferIndex = RollingAverageBuffer.Num() - 1;
			const int32 MinIndex = FMath::Max(FinalBufferIndex - (FrameCount - 1), 0);

			float Total = 0.0;
			FVector YAxis = FVector::ZeroVector; // Look at vector
			FVector XAxis = FVector::ZeroVector; // Up vector

			// Iterate the buffer array in reverse order to ensure the latest frame values are used when we don't use the max buffer size.
			for (int32 BufferIndex = FinalBufferIndex; BufferIndex >= MinIndex; --BufferIndex)
			{
				const TArray<float>& BufferedFrameData = RollingAverageBuffer[BufferIndex];

				if (bIsOrientation)
				{
					const FTransform Transform(FRotator(BufferedFrameData[PitchIndex], BufferedFrameData[YawIndex], BufferedFrameData[RollIndex]));

					YAxis += Transform.GetUnitAxis(EAxis::Y);
					XAxis += Transform.GetUnitAxis(EAxis::X);
				}
				else
				{
					Total += BufferedFrameData[PropertyIndex];
				}
			}

			if (bIsOrientation)
			{
				const FMatrix RotationMatrix = FRotationMatrix::MakeFromYX(YAxis, XAxis);
				const FRotator Rotator = TransformConverter<FRotator>::Convert(RotationMatrix);

				InOutFrame[RollIndex] = Rotator.Roll;
				InOutFrame[PitchIndex] = Rotator.Pitch;
				InOutFrame[YawIndex] = Rotator.Yaw;
			}
			else
			{
				InOutFrame[PropertyIndex] = Total / (FinalBufferIndex - MinIndex + 1);
			}
		}
		else if (SmoothingParam.Method == EMetaHumanRealtimeSmoothingParamMethod::OneEuro)
		{
			if (bIsOrientation)
			{
				const FTransform Transform(FRotator(InOutFrame[PitchIndex], InOutFrame[YawIndex], InOutFrame[RollIndex]));

				FVector YAxis = Transform.GetUnitAxis(EAxis::Y);
				FVector XAxis = Transform.GetUnitAxis(EAxis::X);

				for (int32 Index = 0; Index < 3; ++Index)
				{
					YAxis[Index] = OneEuroYAxis[Index].Filter(YAxis[Index], InDeltaTime);
					XAxis[Index] = OneEuroXAxis[Index].Filter(XAxis[Index], InDeltaTime);
				}

				const FMatrix RotationMatrix = FRotationMatrix::MakeFromYX(YAxis, XAxis);
				const FRotator Rotator = TransformConverter<FRotator>::Convert(RotationMatrix);

				InOutFrame[RollIndex] = Rotator.Roll;
				InOutFrame[PitchIndex] = Rotator.Pitch;
				InOutFrame[YawIndex] = Rotator.Yaw;
			}
			else
			{
				InOutFrame[PropertyIndex] = OneEuroFilters[PropertyName].Filter(InOutFrame[PropertyIndex], InDeltaTime);
			}
		}
		else
		{
			check(false);
		}
	}

	return true;
}

bool FMetaHumanRealtimeSmoothing::IsOrientation(const FName& InProperty) const
{
	return InProperty == "HeadOrientation";
}



#if 0 // Code to dump out an asset as code values
UMetaHumanRealtimeSmoothingParams* SmoothingAsset = LoadObject<UMetaHumanRealtimeSmoothingParams>(GetTransientPackage(), TEXT("/MetaHumanCoreTech/RealtimeMono/DefaultSmoothing.DefaultSmoothing"));

FString Rolling = "ROLL\n";
FString Euro = "EURO\n";

for (const TPair<FName, FMetaHumanRealtimeSmoothingParam>& SmoothingParamPair : SmoothingAsset->Parameters)
{
	if (SmoothingParamPair.Value.Method == EMetaHumanRealtimeSmoothingParamMethod::RollingAverage)
	{
		if (SmoothingParamPair.Value.RollingAverageFrame > 1)
		{
			FString Line = FString::Printf(TEXT(" { \"%s\", %i },\n"), *SmoothingParamPair.Key.ToString(), SmoothingParamPair.Value.RollingAverageFrame);
			Rolling += Line;
		}
	}
	else
	{
		FString Line = FString::Printf(TEXT(" { \"%s\", { %i, %i } },\n"), *SmoothingParamPair.Key.ToString(), (int)SmoothingParamPair.Value.OneEuroSlope, (int)SmoothingParamPair.Value.OneEuroMinCutoff);
		Euro += Line;
	}
}

UE_LOG(LogTemp, Warning, TEXT("%s"), *Rolling);
UE_LOG(LogTemp, Warning, TEXT("%s"), *Euro);

return true;
#endif
