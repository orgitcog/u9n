// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Animation/AnimInstance.h"
#include "MetaHumanCharacterAnimInstance.generated.h"

UCLASS()
class UMetaHumanCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	bool bIsPaused;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	bool bIsScrubbing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	float PlayRate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	TObjectPtr<class UAnimSequence> PrimaryAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	TObjectPtr<class UAnimSequence> SecondaryAnimation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	bool bIsAnimationPlaying;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	float CurrentPlayTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MetaHumanCharacter|Animation")
	int32 NumberOfKeys;

	void PlayAnimation();
	void PlayReverseAnimation();
	void StopAnimation();
	void PauseAnimation();
	void BeginScrubbingAnimation();
	void ScrubAnimation(float InScrubValue);
	void EndScrubbingAnimation();
	void SetAnimationPlayRate(float InNewPlayRate);
	float GetAnimationLenght() const;
	float GetCurrentPlayTime() const;
	int32 GetNumberOfKeys() const;

	void SetAnimation(UAnimSequence* FaceAnimation, UAnimSequence* BodyAnimation);

private:

	/** UAnimInstance interface begin*/
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	/** UAnimInstance interface end*/

	
};

