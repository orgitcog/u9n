// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterAnimInstance.h"
#include "AnimationRuntime.h"
#include "Animation/AnimSequence.h"
#include "MetaHumanCharacter.h"


void UMetaHumanCharacterAnimInstance::PlayAnimation()
{
	bIsAnimationPlaying = true;
	bIsPaused = false;

	if(PlayRate < 0) 
	{
		PlayRate = - PlayRate;
	}
}

void UMetaHumanCharacterAnimInstance::PlayReverseAnimation()
{
	bIsAnimationPlaying = true;
	bIsPaused = false;
	if(PlayRate > 0)
	{
		PlayRate = - PlayRate;
	}
}

void UMetaHumanCharacterAnimInstance::PauseAnimation()
{
	bIsPaused = true;
}

void UMetaHumanCharacterAnimInstance::StopAnimation()
{
	bIsAnimationPlaying = false;
}

void UMetaHumanCharacterAnimInstance::BeginScrubbingAnimation()
{
	bIsAnimationPlaying = true;
	bIsScrubbing = true;
}

void UMetaHumanCharacterAnimInstance::ScrubAnimation(float InScrubValue)
{	
	CurrentPlayTime = InScrubValue;
}

void UMetaHumanCharacterAnimInstance::EndScrubbingAnimation()
{
	bIsScrubbing = false;
}

void UMetaHumanCharacterAnimInstance::SetAnimationPlayRate(float InNewPlayRate)
{
	if(PlayRate < 0 )
	{
		PlayRate = -InNewPlayRate;
	}
	else
	{
		PlayRate = InNewPlayRate;
	}
}

float UMetaHumanCharacterAnimInstance::GetAnimationLenght() const
{
	if(IsValid(PrimaryAnimation)) 
	{
		return PrimaryAnimation->GetPlayLength();
	}

	return 0.f;
}

float UMetaHumanCharacterAnimInstance::GetCurrentPlayTime() const
{
	return CurrentPlayTime;
}

int32 UMetaHumanCharacterAnimInstance::GetNumberOfKeys() const
{
	if (IsValid(PrimaryAnimation))
	{
		return PrimaryAnimation->GetNumberOfSampledKeys();
	}

	return 0;
}


void UMetaHumanCharacterAnimInstance::SetAnimation(UAnimSequence* FaceAnimation, UAnimSequence* BodyAnimation)
{
	if (BodyAnimation)
	{
		PrimaryAnimation = BodyAnimation;
		SecondaryAnimation = FaceAnimation;
	}
	else
	{
		PrimaryAnimation = FaceAnimation;
		SecondaryAnimation = nullptr;
	}
}

void UMetaHumanCharacterAnimInstance::NativeInitializeAnimation()
{
	// NOTE: Default settings for AnimBP, this will probably change in next iterations.
	// The settings were taken from the AnimBP before re-parenting so we have the same initialized state
	// of the animation.
	bIsScrubbing = false;
	bIsPaused = true;
	PlayRate = 1.0f;
	PrimaryAnimation = nullptr;
	SecondaryAnimation = nullptr;
	bIsAnimationPlaying = false;
	CurrentPlayTime = 0.0f;
}

void UMetaHumanCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
		
	if(PrimaryAnimation)
	{
		if(!bIsAnimationPlaying)
		{
			CurrentPlayTime = 0.0f;
		}
	}
}