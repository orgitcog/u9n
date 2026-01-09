// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureDataUtils.h"

#include "CaptureData.h"
#include "CameraCalibration.h"
#include "MetaHumanFootageComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"

#include "Engine/SkeletalMesh.h"

namespace MetaHumanCaptureDataUtils
{
	USceneComponent* CreatePreviewComponentFromFootage(UFootageCaptureData* InCaptureData, UObject* InObject)
	{
		USceneComponent* PreviewComponent = nullptr;

		if (!InCaptureData->CameraCalibrations.IsEmpty())
		{
			TObjectPtr<UCameraCalibration> CameraCalibration = InCaptureData->CameraCalibrations[0];

			if (CameraCalibration != nullptr)
			{
				UMetaHumanFootageComponent* FootageSceneComponent = NewObject<UMetaHumanFootageComponent>(InObject, NAME_None, RF_Transactional);
				FootageSceneComponent->SetCameraCalibration(CameraCalibration);

				PreviewComponent = FootageSceneComponent;
			}
		}

		return PreviewComponent;
	}

	USceneComponent* CreatePreviewComponentFromMesh(UMeshCaptureData* InCaptureData, UObject* InObject)
	{
		USceneComponent* PreviewComponent = nullptr;

		TObjectPtr<UObject> TargetMesh = InCaptureData->TargetMesh;

		if (TargetMesh != nullptr)
		{
			if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(TargetMesh))
			{
				UStaticMeshComponent* StaticMeshComponent = NewObject<UStaticMeshComponent>(InObject, NAME_None, RF_Transactional);
				StaticMeshComponent->SetStaticMesh(StaticMesh);

				PreviewComponent = StaticMeshComponent;
			}

			if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(TargetMesh))
			{
				USkeletalMeshComponent* SkeletalMeshComponent = NewObject<USkeletalMeshComponent>(InObject, NAME_None, RF_Transactional);
				SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);

				PreviewComponent = SkeletalMeshComponent;
			}

			if (PreviewComponent != nullptr)
			{
				PreviewComponent->SetMobility(EComponentMobility::Movable);
			}
		}

		return PreviewComponent;
	}

	USceneComponent* CreatePreviewComponent(UCaptureData* InCaptureData, UObject* InObject)
	{
		if (InCaptureData->IsA<UFootageCaptureData>())
		{
			return CreatePreviewComponentFromFootage(static_cast<UFootageCaptureData*>(InCaptureData), InObject);
		}
		else if (InCaptureData->IsA<UMeshCaptureData>())
		{
			return CreatePreviewComponentFromMesh(static_cast<UMeshCaptureData*>(InCaptureData), InObject);
		}
		return nullptr;
	}
}
