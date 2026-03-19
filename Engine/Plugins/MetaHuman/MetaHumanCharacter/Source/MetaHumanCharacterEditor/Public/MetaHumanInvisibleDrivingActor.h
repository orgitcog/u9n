// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "GameFramework/Actor.h"
#include "Animation/SkeletalMeshActor.h"

#include "MetaHumanInvisibleDrivingActor.generated.h"

enum class EMetaHumanCharacterAnimationPlayState : uint8
{
	PlayingForward = 0,
	PlayingBackwards,
	Paused
};

UCLASS(Transient, NotPlaceable, Blueprintable)
class AMetaHumanInvisibleDrivingActor : public ASkeletalMeshActor
{
	GENERATED_BODY()

public:
	AMetaHumanInvisibleDrivingActor();
	void SetBodySkeletalMesh(TNotNull<USkeletalMesh*> InBodyMesh) const;
	void SetDefaultBodySkeletalMesh();
	void ResetAnimInstance();
	void InitPreviewAnimInstance();
	void InitLiveLinkAnimInstance();

	void PlayAnimation() const;
	void PlayAnimationReverse() const;
	void PauseAnimation() const;
	void StopAnimation() const;
	void ScrubAnimation(float NormalizedTime) const;
	void BeginAnimationScrubbing() const;
	void EndAnimationScrubbing() const;
	void SetAnimationPlayRate(float NewPlayRate) const;
	float GetAnimationLength() const;
	EMetaHumanCharacterAnimationPlayState GetAnimationPlayState() const;
	float GetCurrentPlayTime() const;
	int32 GetNumberOfAnimationKeys() const;

	void SetAnimation(UAnimSequence* FaceAnimSequence, UAnimSequence* BodyAnimSequence) const;

	void SetLiveLinkSubjectNameChanged(FName InLiveLinkSubjectName, bool bInitAnimInstance = true);

	class UMetaHumanCharacterAnimInstance* GetPreviewAnimInstance() const;

private:
	UPROPERTY()
	TSubclassOf<class UAnimInstance> PreviewAnimInstanceClass;

	UPROPERTY()
	TSubclassOf<class UAnimInstance> LiveLinkAnimInstanceClass;

	UPROPERTY()
	FName LiveLinkSubjectName;
};