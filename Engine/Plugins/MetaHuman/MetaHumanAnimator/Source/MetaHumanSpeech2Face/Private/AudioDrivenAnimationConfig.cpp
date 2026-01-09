// Copyright Epic Games, Inc. All Rights Reserved.

#include "AudioDrivenAnimationConfig.h"

FAudioDrivenAnimationModels::FAudioDrivenAnimationModels()
{
	AudioEncoder = TEXT("/MetaHuman/Speech2Face/NNE_AudioDrivenAnimation_AudioEncoder.NNE_AudioDrivenAnimation_AudioEncoder");
	AnimationDecoder = TEXT("/MetaHuman/Speech2Face/NNE_AudioDrivenAnimation_AnimationDecoder.NNE_AudioDrivenAnimation_AnimationDecoder");
}
