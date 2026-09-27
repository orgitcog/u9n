// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ProceduralMeshComponent.h"

#include "MetaHumanDepthMeshComponent.generated.h"

UCLASS()
class METAHUMANIMAGEVIEWEREDITOR_API UMetaHumanDepthMeshComponent
	: public UProceduralMeshComponent
{
	GENERATED_BODY()
public:

	UMetaHumanDepthMeshComponent(const FObjectInitializer& InObjectInitializer);

	//~ Begin UActorComponent Interface.
	void OnRegister() override;
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	//~ End USceneComponent Interface.

	/** Sets the texture with depth data to display the mesh */
	void SetDepthTexture(class UTexture* InDepthTexture);

	/** Sets the camera calibration to calculate the placement of the depth mesh on the viewport */
	void SetCameraCalibration(class UCameraCalibration* InCameraCalibration);

	/** Set the depth near and far planes to clamp the display of depth data */
	void SetDepthRange(float InDepthNear, float InDepthFar);

	/** Set the resolution of the depth mesh  */
	void SetSize(int32 InWidth, int32 InHeight);

private:
	/** Sets depth plane transform based on the depth far plane */
	void SetDepthPlaneTransform(bool bInNotifyMaterial = false);

	void UpdateMaterialDepth();
	void UpdateMaterialTexture();
	void UpdateMaterialCameraIntrinsics();

private:

	UPROPERTY()
	TObjectPtr<UCameraCalibration> CameraCalibration;

	UPROPERTY(EditAnywhere, Category = Texture)
	TObjectPtr<UTexture> DepthTexture;

	UPROPERTY()
	int32 Width = -1;

	UPROPERTY()
	int32 Height = -1;

	UPROPERTY(EditAnywhere, Category = DepthRange)
	float DepthNear = 10.0f;

	UPROPERTY(EditAnywhere, Category = DepthRange)
	float DepthFar = 55.5f;
};