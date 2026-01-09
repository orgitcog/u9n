// Copyright Epic Games, Inc.All Rights Reserved.

#include "MetaHumanFootageComponent.h"

#include "MetaHumanViewportModes.h"
#include "Utils/CustomMaterialUtils.h"

#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"

#include "Kismet/GameplayStatics.h"
#include "MediaTexture.h"
#include "OpenCVHelperLocal.h"
#include "SceneView.h"
#include "UObject/ConstructorHelpers.h"

/////////////////////////////////////////////////////
// UFootageSceneComponent

UMetaHumanFootageComponent::UMetaHumanFootageComponent()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane"));
	check(PlaneMesh.Object);

	for (EABImageViewMode ViewMode : { EABImageViewMode::A, EABImageViewMode::B })
	{
		const FString ViewModeName = StaticEnum<EABImageViewMode>()->GetNameStringByValue(static_cast<int64>(ViewMode));
		const FString FootagePlaneName = FString::Format(TEXT("Footage Plane {0}"), { ViewModeName });
		UStaticMeshComponent* FootagePlane = CreateDefaultSubobject<UStaticMeshComponent>(*FootagePlaneName);

		FootagePlane->SetStaticMesh(PlaneMesh.Object);
		FootagePlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FootagePlane->bVisibleInRayTracing = false;
		FootagePlane->SetupAttachment(this);

		FFootagePlaneData FootagePlaneData;
		FootagePlaneData.FootagePlane = FootagePlane;

		FootagePlanes.Emplace(ViewMode, MoveTemp(FootagePlaneData));

		FootagePlane->SetupAttachment(this);
	}

	CreateFootageMaterialInstances();
}

void UMetaHumanFootageComponent::PostLoad()
{
	Super::PostLoad();

	// CreateMovieContourDepthMaterial returns a transient material, so when loading this component check to see if the materials are valid
	CreateFootageMaterialInstances();

	for (const TPair<EABImageViewMode, FFootagePlaneData>& FootagePlanePair : FootagePlanes)
	{
		if (UStaticMeshComponent* FootagePlane = FootagePlanePair.Value.FootagePlane)
		{
			// Update the ComponentToWorld directly as this is not serialized
			FootagePlane->SetComponentToWorld(FootagePlane->GetRelativeTransform());
			FootagePlane->UpdateBounds();
		}
	}
}

void UMetaHumanFootageComponent::SetFootageResolution(const FVector2D& InResolution)
{
	EffectiveCalibration.ImageSize = InResolution;
	EffectiveCalibration.PrincipalPoint = EffectiveCalibration.ImageSize / 2;
	EffectiveCalibration.FocalLength = EffectiveCalibration.ImageSize; // arbitrary value, but this gives a fov that looks good
}

void UMetaHumanFootageComponent::SetCameraCalibration(class UCameraCalibration* InCameraCalibration)
{
	CameraCalibration = InCameraCalibration;

	if (CameraCalibration)
	{
		FExtendedLensFile* NonDepthCamera = CameraCalibration->CameraCalibrations.FindByPredicate([](const FExtendedLensFile& InCalibration)
		{
			return !InCalibration.IsDepthCamera;
		});

		if (ensure(NonDepthCamera))
		{
			Camera = NonDepthCamera->Name;
		}
	}
	else
	{
		Camera = "";
	}
}

void UMetaHumanFootageComponent::SetCamera(const FString& InCamera)
{
	Camera = InCamera;
}

void UMetaHumanFootageComponent::ConfigurePlane(EABImageViewMode InView)
{
	TArray<FCameraCalibration> Calibrations;
	int32 CalibrationIndex = 0;

	if (CameraCalibration)
	{
		TArray<TPair<FString, FString>> StereoPairs;
		CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoPairs);
		CalibrationIndex = CameraCalibration->GetCalibrationIndexByName(Camera);
	}
	else
	{
		Calibrations.Add(EffectiveCalibration);
		CalibrationIndex = 0;
	}

	const FVector2D FootagePlaneSize{ 100.0, 100.0 }; // The default size of the plane mesh
	const FVector2D FootageResolution(Calibrations[CalibrationIndex].ImageSize.X, Calibrations[CalibrationIndex].ImageSize.Y);
	FTransform Transform = FTransform::Identity;
	Transform *= FTransform{ FRotator{ 0.0 }, FVector{ 0.0 }, FVector{ FootageResolution / FootagePlaneSize, 1.0 } }; // scale so size now in pixels
	Transform *= FTransform(FVector(FootageResolution.X / 2.0 - Calibrations[CalibrationIndex].PrincipalPoint.X,
		FootageResolution.Y / 2.0 - Calibrations[CalibrationIndex].PrincipalPoint.Y, 0)); // account for principle point
	const float DesiredDistance = (DepthDataFar + 10.0f); // scale so that when object is placed at the desired distance from camera it fills the FoV

	check(DesiredDistance > 0.0f);
	const float DistanceScale = Calibrations[CalibrationIndex].FocalLength.X / DesiredDistance;

	check(DistanceScale > 0.0f);
	Transform *= FTransform(FRotator(0), FVector(0), FVector(1.0 / DistanceScale, 1.0 / DistanceScale, 1));
	Transform *= FTransform(FRotator(0, 90, 0)); // rotate 90 about Z axis
	Transform *= FTransform(FRotator(90, 0, 0)); // rotate 90 about Y axis
	Transform *= FTransform(FVector(DesiredDistance, 0, 0)); // translate along X axis

	FTransform InverseCameraExtrinsics = FTransform(Calibrations[CalibrationIndex].Transform.Inverse());
	FOpenCVHelperLocal::ConvertOpenCVToUnreal(InverseCameraExtrinsics);
	Transform *= InverseCameraExtrinsics;

	UStaticMeshComponent* FootagePlane = FootagePlanes[InView].FootagePlane;
	FootagePlane->SetWorldTransform(Transform);
	FootagePlane->SetComponentToWorld(Transform);
	FootagePlane->UpdateBounds();
}

void UMetaHumanFootageComponent::CreateFootageMaterialInstances()
{
	// CreateMovieContourDepthMaterial returns a transient material, so when creating this component check to see if the materials are valid
	for (const TPair<EABImageViewMode, FFootagePlaneData>& FootagePlanePair : FootagePlanes)
	{
		if (UStaticMeshComponent* FootagePlane = FootagePlanePair.Value.FootagePlane)
		{
			// TODO: This can't be used directly as it belongs to MetaHumanImageViewer
			const bool bUseExternalSampler = false;
			const int32 DepthComponent = 0;
			UMaterialInstanceDynamic* FootageMaterial = CustomMaterialUtils::CreateMovieContourDepthMaterial(TEXT("Footage CaptureData Material"), bUseExternalSampler, DepthComponent);

			FootagePlane->SetMaterial(0, FootageMaterial);
		}
	}
}

UMaterialInstanceDynamic* UMetaHumanFootageComponent::GetFootageMaterialInstance(EABImageViewMode InViewMode)
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);
	return Cast<UMaterialInstanceDynamic>(FootagePlanes[InViewMode].FootagePlane->GetMaterial(0));
}

TArray<UStaticMeshComponent*> UMetaHumanFootageComponent::GetFootagePlaneComponents() const
{
	TArray<UStaticMeshComponent*> PlaneComponents;
	Algo::Transform(FootagePlanes, PlaneComponents, [](const TPair<EABImageViewMode, FFootagePlaneData>& FootagePlanePair)
	{
		return FootagePlanePair.Value.FootagePlane;
	});
	return PlaneComponents;
}

UStaticMeshComponent* UMetaHumanFootageComponent::GetFootagePlaneComponent(EABImageViewMode InViewMode) const
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);
	return FootagePlanes[InViewMode].FootagePlane;
}

void UMetaHumanFootageComponent::SetMediaTextures(UTexture* InColorMediaTexture, UTexture* InDepthMediaTexture, bool bNotifyMaterial)
{
	for (TPair<EABImageViewMode, FFootagePlaneData>& FootagePlanePair : FootagePlanes)
	{
		const EABImageViewMode ViewMode = FootagePlanePair.Key;
		FFootagePlaneData& FootagePlaneData = FootagePlanePair.Value;

		FootagePlaneData.ColorMediaTexture = InColorMediaTexture;
		FootagePlaneData.DepthMediaTexture = InDepthMediaTexture;

		// When changing the media textures, enable the display of the color channel
		ShowColorChannel(ViewMode);

		// Notify the material about the change to ensure that the texture resource has finished updating
		if (bNotifyMaterial)
		{
			if (UMaterialInstanceDynamic* FootageMaterial = GetFootageMaterialInstance(ViewMode))
			{
				check(FootageMaterial->GetMaterial());
				FootageMaterial->GetMaterial()->PostEditChange();
			}
		}
	}
}

void UMetaHumanFootageComponent::SetDepthRange(int32 InDepthNear, int32 InDepthFar)
{
	DepthDataNear = InDepthNear;
	DepthDataFar = InDepthFar;

	for (TPair<EABImageViewMode, FFootagePlaneData>& FootagePlanePair : FootagePlanes)
	{
		const EABImageViewMode ViewMode = FootagePlanePair.Key;
		FFootagePlaneData& FootagePlaneData = FootagePlanePair.Value;
		if (UMaterialInstanceDynamic* FootageMaterial = GetFootageMaterialInstance(ViewMode))
		{
			FootageMaterial->SetScalarParameterValue(TEXT("DepthNear"), DepthDataNear);
			FootageMaterial->SetScalarParameterValue(TEXT("DepthFar"), DepthDataFar);
		}
	}
}

void UMetaHumanFootageComponent::SetFootageVisible(EABImageViewMode InViewMode, bool bInIsVisible)
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);
	FootagePlanes[InViewMode].FootagePlane->SetVisibility(bInIsVisible, true);
}

void UMetaHumanFootageComponent::ShowColorChannel(EABImageViewMode InViewMode)
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);
	if (UMaterialInstanceDynamic* FootageMaterial = GetFootageMaterialInstance(InViewMode))
	{
		FootageMaterial->SetTextureParameterValue(TEXT("Movie"), FootagePlanes[InViewMode].ColorMediaTexture);

		ConfigurePlane(InViewMode);
	}
}

void UMetaHumanFootageComponent::SetUndistortionEnabled(EABImageViewMode InViewMode, bool bUndistort)
{
	check(InViewMode == EABImageViewMode::A || InViewMode == EABImageViewMode::B);

	if (!CameraCalibration)
	{
		return;
	}

	TArray<FCameraCalibration> Calibrations;
	TArray<TPair<FString, FString>> StereoPairs;
	CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoPairs);

	int32 CalibrationIndex = CameraCalibration->GetCalibrationIndexByName(Camera);

	const FCameraCalibration& Calib = Calibrations[CalibrationIndex];

	if (UMaterialInstanceDynamic* FootageMaterial = GetFootageMaterialInstance(InViewMode))
	{
		FootageMaterial->SetScalarParameterValue(TEXT("Undistort"), bUndistort ? 1.0f : 0.0f);
		FootageMaterial->SetScalarParameterValue(TEXT("cx"), Calib.PrincipalPoint.X);
		FootageMaterial->SetScalarParameterValue(TEXT("cy"), Calib.PrincipalPoint.Y);
		FootageMaterial->SetScalarParameterValue(TEXT("fx"), Calib.FocalLength.X);
		FootageMaterial->SetScalarParameterValue(TEXT("fy"), Calib.FocalLength.Y);
		FootageMaterial->SetScalarParameterValue(TEXT("k1"), Calib.K1);
		FootageMaterial->SetScalarParameterValue(TEXT("k2"), Calib.K2);
		FootageMaterial->SetScalarParameterValue(TEXT("k3"), Calib.K3);
		FootageMaterial->SetScalarParameterValue(TEXT("p1"), Calib.P1);
		FootageMaterial->SetScalarParameterValue(TEXT("p2"), Calib.P2);
	}
}

void UMetaHumanFootageComponent::GetFootageScreenRect(const FVector2D& InViewportSize, float& OutFieldOfView, FBox2D& OutScreenRect, FTransform& OutCameraTransform) const
{
	TArray<FCameraCalibration> Calibrations;
	int32 CalibrationIndex = 0;

	if (CameraCalibration)
	{
		TArray<TPair<FString, FString>> StereoPairs;
		CameraCalibration->ConvertToTrackerNodeCameraModels(Calibrations, StereoPairs);
		CalibrationIndex = CameraCalibration->GetCalibrationIndexByName(Camera);
	}
	else
	{
		Calibrations.Add(EffectiveCalibration);
		CalibrationIndex = 0;
	}

	const FVector2D ColorResolution(Calibrations[CalibrationIndex].ImageSize.X, Calibrations[CalibrationIndex].ImageSize.Y);

	if (InViewportSize.ComponentwiseAllGreaterThan(FVector2D::ZeroVector) && ColorResolution.ComponentwiseAllGreaterThan(FVector2D::ZeroVector))
	{
		const float ViewportAspect = InViewportSize.X / InViewportSize.Y;
		const float ImageAspect = ColorResolution.X / ColorResolution.Y;

		double FitToSize = ColorResolution.X;
		double Scale = InViewportSize.X / ColorResolution.X;

		if (ImageAspect < ViewportAspect)
		{
			FitToSize = ColorResolution.Y * ViewportAspect;
			Scale = InViewportSize.Y / ColorResolution.Y;
		}

		OutFieldOfView = 2.0f * FMath::RadiansToDegrees(FMath::Atan2(FitToSize / 2.0f, Calibrations[CalibrationIndex].FocalLength.X));

		FTransform InverseCameraExtrinsics = FTransform(Calibrations[CalibrationIndex].Transform.Inverse());
		FOpenCVHelperLocal::ConvertOpenCVToUnreal(InverseCameraExtrinsics);

		OutCameraTransform = InverseCameraExtrinsics;

		const double ScreenRectXMin = InViewportSize.X / 2 - (Calibrations[CalibrationIndex].PrincipalPoint.X * Scale);
		const double ScreenRectXMax = InViewportSize.X / 2 + ((ColorResolution.X - Calibrations[CalibrationIndex].PrincipalPoint.X) * Scale);
		const double ScreenRectYMin = InViewportSize.Y / 2 - (Calibrations[CalibrationIndex].PrincipalPoint.Y * Scale);
		const double ScreenRectYMax = InViewportSize.Y / 2 + ((ColorResolution.Y - Calibrations[CalibrationIndex].PrincipalPoint.Y) * Scale);

		OutScreenRect = FBox2D{ FVector2D(ScreenRectXMin, ScreenRectYMin), FVector2D(ScreenRectXMax, ScreenRectYMax) };
	}
}