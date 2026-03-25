// Copyright Epic Games, Inc. All Rights Reserved.

#include "Speech2Face.h"
#include "Speech2FaceInternal.h"

#include "DataDefs.h"

#if WITH_EDITOR

FSpeech2Face::~FSpeech2Face() = default;

TUniquePtr<FSpeech2Face> FSpeech2Face::Create()
{
	FAudioDrivenAnimationModels DefaultModels;
	return FSpeech2Face::Create(DefaultModels);
}

TUniquePtr<FSpeech2Face> FSpeech2Face::Create(const FAudioDrivenAnimationModels& InModels)
{
	TUniquePtr<FSpeech2FaceInternal> Pimpl = FSpeech2FaceInternal::Create(InModels);

	if (!Pimpl)
	{
		return nullptr;
	}
	return TUniquePtr<FSpeech2Face>(new FSpeech2Face(MoveTemp(Pimpl)));
}

void FSpeech2Face::SetMood(const EAudioDrivenAnimationMood& InMood)
{
	Pimpl->SetMood(InMood);
}

void FSpeech2Face::SetMoodIntensity(const float InMoodIntensity)
{
	Pimpl->SetMoodIntensity(InMoodIntensity);
}

FSpeech2Face::FSpeech2Face(TUniquePtr<FSpeech2FaceInternal> InPimpl) : Pimpl(MoveTemp(InPimpl))
{
	check(Pimpl);
}

bool FSpeech2Face::GenerateFaceAnimation(const FAudioParams& InAudioParams,
	float InOutputAnimationFps,
	bool bInGenerateBlinks,
	TFunction<bool()> InShouldCancelCallback,
	TArray<FAnimationFrame>& OutAnimation,
	TArray<FAnimationFrame>& OutHeadAnimation)
{
	return Pimpl->GenerateFaceAnimation(InAudioParams, InOutputAnimationFps, bInGenerateBlinks, InShouldCancelCallback, OutAnimation, OutHeadAnimation);
}

#endif //WITH_EDITOR

namespace UE::MetaHuman
{
void ReplaceHeadGuiControlsWithRaw(TMap<FString, float>& OutControlMap)
{
	for (const TPair<FString, FString>& GuiToRaw : HeadControlsGuiToRawLookupTable)
	{
		float ControlValue;
		const bool bWasFound = OutControlMap.RemoveAndCopyValue(GuiToRaw.Key, ControlValue);

		if (bWasFound)
		{
			OutControlMap.Emplace(GuiToRaw.Value, ControlValue);
		}
	}
}

TSet<FString> GetMouthOnlyRawControls()
{
	return MouthOnlyRawControls;
}

FTransform GetHeadPoseTransformFromRawControls(const TMap<FString, float>& InAnimationData)
{
	const float* Rx = InAnimationData.Find(TEXT("mha_head_ik_ctrl.rx"));
	check(Rx);

	const float* Ry = InAnimationData.Find(TEXT("mha_head_ik_ctrl.ry"));
	check(Ry);

	const float* Rz = InAnimationData.Find(TEXT("mha_head_ik_ctrl.rz"));
	check(Rz);

	const float* Tx = InAnimationData.Find(TEXT("mha_head_ik_ctrl.tx"));
	check(Tx);

	const float* Ty = InAnimationData.Find(TEXT("mha_head_ik_ctrl.ty"));
	check(Ty);

	const float* Tz = InAnimationData.Find(TEXT("mha_head_ik_ctrl.tz"));
	check(Tz);

	if (Rx && Ry && Rz && Tx && Ty && Tz)
	{
		// We need to account for differences between the model and UE coordinate systems

		FRotator Rotator;
		Rotator.Roll = *Rx;
		Rotator.Pitch = *Ry * -1.0;
		Rotator.Yaw = *Rz * -1.0;

		FVector Translation;
		Translation.X = *Tx;
		Translation.Y = *Ty * -1.0;
		Translation.Z = *Tz;

		FTransform HeadPoseTransform = FTransform(Rotator, Translation);
		return HeadPoseTransform;
	}

	return FTransform::Identity;
}

} // namespace UE::MetaHuman
