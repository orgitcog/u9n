// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanInvisibleDrivingActor.h"
#include "MetaHumanCharacterAnimInstance.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "LiveLinkTypes.h"

AMetaHumanInvisibleDrivingActor::AMetaHumanInvisibleDrivingActor()
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		static ConstructorHelpers::FClassFinder<UAnimInstance> PreviewAnimBPClassFinder(TEXT("/" UE_PLUGIN_NAME "/Animation/ABP_AnimationPreview.ABP_AnimationPreview_C"));
		if (PreviewAnimBPClassFinder.Succeeded())
		{
			PreviewAnimInstanceClass = PreviewAnimBPClassFinder.Class;
		}

		static ConstructorHelpers::FClassFinder<UAnimInstance> LiveLinkAnimInstanceFinder(TEXT("/" UE_PLUGIN_NAME "/Animation/ABP_MH_LiveLink.ABP_MH_LiveLink_C"));
		if (LiveLinkAnimInstanceFinder.Succeeded())
		{
			LiveLinkAnimInstanceClass = LiveLinkAnimInstanceFinder.Class;
		}
	}
	
	SetActorEnableCollision(false);
}

void AMetaHumanInvisibleDrivingActor::SetDefaultBodySkeletalMesh()
{
	USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(StaticLoadObject(UObject::StaticClass(), nullptr, TEXT("/" UE_PLUGIN_NAME "/Body/IdentityTemplate/SKM_Body.SKM_Body")));
	SetBodySkeletalMesh(SkeletalMesh);
}

void AMetaHumanInvisibleDrivingActor::SetBodySkeletalMesh(TNotNull<USkeletalMesh*> InBodyMesh) const
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		if (!HasAnyFlags(RF_ClassDefaultObject))
		{
			SkelMeshComponent->SetSkeletalMesh(InBodyMesh);
		}

		// Hide the actor from the viewport.
		SkelMeshComponent->SetHiddenInGame(true);

		// Update animation even in case the actor isn't visible or outside of the view frustum.
		SkelMeshComponent->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	}
}

void AMetaHumanInvisibleDrivingActor::SetLiveLinkSubjectNameChanged(FName InLiveLinkSubjectName, bool bInitAnimInstance)
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		if (bInitAnimInstance)
		{
			InitLiveLinkAnimInstance();
		}

		if (UAnimInstance* AnimInstance = SkelMeshComponent->GetAnimInstance())
		{
			FProperty* Property = AnimInstance->GetClass()->FindPropertyByName(TEXT("LLink_Face_Subj"));
			if (Property)
			{
				if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					FLiveLinkSubjectName* PropertySubjectName = StructProperty->ContainerPtrToValuePtr<FLiveLinkSubjectName>(AnimInstance);
					if (PropertySubjectName)
					{
						LiveLinkSubjectName = InLiveLinkSubjectName;
						PropertySubjectName->Name = LiveLinkSubjectName;

						AnimInstance->Modify();
						FPropertyChangedEvent PropertyChangedEvent(Property);
						AnimInstance->PostEditChangeProperty(PropertyChangedEvent);
					}
				}
			}
		}
	}
}

void AMetaHumanInvisibleDrivingActor::ResetAnimInstance()
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		SkelMeshComponent->SetAnimInstanceClass(nullptr);
	}
}

void AMetaHumanInvisibleDrivingActor::InitLiveLinkAnimInstance()
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		SkelMeshComponent->SetAnimInstanceClass(LiveLinkAnimInstanceClass);
		SetLiveLinkSubjectNameChanged(LiveLinkSubjectName, /*bInitAnimInstance=*/false);
	}
}

void AMetaHumanInvisibleDrivingActor::InitPreviewAnimInstance()
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (ensure(SkelMeshComponent))
	{
		SkelMeshComponent->SetAnimInstanceClass(PreviewAnimInstanceClass);
	}
}

void AMetaHumanInvisibleDrivingActor::PlayAnimation() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->PlayAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::PlayAnimationReverse() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->PlayReverseAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::PauseAnimation() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->PauseAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::StopAnimation() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->StopAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::ScrubAnimation(float NormalizedTime) const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->ScrubAnimation(NormalizedTime);
	}
}

void AMetaHumanInvisibleDrivingActor::BeginAnimationScrubbing() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->BeginScrubbingAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::EndAnimationScrubbing() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->EndScrubbingAnimation();
	}
}

void AMetaHumanInvisibleDrivingActor::SetAnimationPlayRate(float NewPlayRate) const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->SetAnimationPlayRate(NewPlayRate);
	}
}

float AMetaHumanInvisibleDrivingActor::GetAnimationLength() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		return PreviewAnimInstance->GetAnimationLenght();
	}
	return 0.0f;
}

EMetaHumanCharacterAnimationPlayState AMetaHumanInvisibleDrivingActor::GetAnimationPlayState() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		if(PreviewAnimInstance->bIsAnimationPlaying && !PreviewAnimInstance->bIsPaused)
		{
			if(PreviewAnimInstance->PlayRate < 0)
			{
				return EMetaHumanCharacterAnimationPlayState::PlayingBackwards;
			}
			else
			{
				return EMetaHumanCharacterAnimationPlayState::PlayingForward;
			}

		}

	}
	return EMetaHumanCharacterAnimationPlayState::Paused;
}

float AMetaHumanInvisibleDrivingActor::GetCurrentPlayTime() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		return PreviewAnimInstance->GetCurrentPlayTime();
	}
	return 0.0f;
}

int32 AMetaHumanInvisibleDrivingActor::GetNumberOfAnimationKeys() const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		return PreviewAnimInstance->GetNumberOfKeys();
	}
	return 0;
}

void AMetaHumanInvisibleDrivingActor::SetAnimation(UAnimSequence* FaceAnimSequence, UAnimSequence* BodyAnimSequence) const
{
	if (UMetaHumanCharacterAnimInstance* PreviewAnimInstance = GetPreviewAnimInstance())
	{
		PreviewAnimInstance->SetAnimation(FaceAnimSequence, BodyAnimSequence);
	}
}

UMetaHumanCharacterAnimInstance* AMetaHumanInvisibleDrivingActor::GetPreviewAnimInstance() const
{
	USkeletalMeshComponent* SkelMeshComponent = GetSkeletalMeshComponent();
	if (!ensure(SkelMeshComponent))
	{
		return nullptr;
	}

	return Cast<UMetaHumanCharacterAnimInstance>(SkelMeshComponent->GetAnimInstance());
}
