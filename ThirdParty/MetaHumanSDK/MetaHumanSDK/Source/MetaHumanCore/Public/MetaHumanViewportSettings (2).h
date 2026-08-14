// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanViewportModes.h"
#include "Engine/EngineBaseTypes.h"

#include "MetaHumanViewportSettings.generated.h"

enum EViewModeIndex : int;

USTRUCT()
struct FMetaHumanViewportState
{
	GENERATED_BODY()

	FMetaHumanViewportState()
		: bShowCurves(true)
		, bShowControlVertices(true)
		, bShowSkeletalMesh(false)
		, bShowFootage(false)
		, bShowDepthMesh(false)
		, bShowUndistorted(false)
	{ }

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowCurves : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowControlVertices : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowSkeletalMesh : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowFootage : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowDepthMesh : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	uint8 bShowUndistorted : 1;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	TEnumAsByte<EViewModeIndex> ViewModeIndex = VMI_Lit;

	UPROPERTY(EditAnywhere, Category = "Viewport Settings")
	float FixedEV100 = 0.0f;
};

USTRUCT()
struct FMetaHumanViewportCameraState
{
	GENERATED_BODY()

	/** The current camera location for this Promoted Frame */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	FVector Location = FVector::ZeroVector;

	/** The current camera rotation for this Promoted Frame */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	FRotator Rotation = FRotator::ZeroRotator;

	/** The current camera LookAt position for this Promoted Frame */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	FVector LookAt = FVector::ZeroVector;

	/** The Camera FoV from when the view was promoted */
	UPROPERTY(VisibleAnywhere, Category = "Camera")
	float ViewFOV = 45.0f;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	int32 SpeedSetting = 2;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	float SpeedScalar = 1.0f;
};

UCLASS()
class METAHUMANCORE_API UMetaHumanViewportSettings
	: public UObject
{
	GENERATED_BODY()

public:

	UMetaHumanViewportSettings();

	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~End UObject interface

	/** Returns true if the luminance range variable is enabled */
	static bool IsExtendDefaultLuminanceRangeEnabled();

	/** Returns 0 if using ExtendDefaultLuminanceRange and 1 otherwise */
	static float GetDefaultViewportBrightness();

	/** Calls OnSettingsChangedDelegate to notify that something changed */
	void NotifySettingsChanged();

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	EViewModeIndex GetViewModeIndex(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	float GetEV100(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void SetEV100(EABImageViewMode InView, float InValue, bool bInNotify);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void SetViewModeIndex(EABImageViewMode InView, EViewModeIndex InViewModeIndex, bool bInNotify);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsShowingSingleView() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleShowCurves(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsShowingCurves(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleShowControlVertices(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsShowingControlVertices(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleSkeletalMeshVisibility(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsSkeletalMeshVisible(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleFootageVisibility(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsFootageVisible(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleDepthMeshVisibility(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsDepthMeshVisible(EABImageViewMode InView) const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	void ToggleDistortion(EABImageViewMode InView);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Viewport Settings")
	bool IsShowingUndistorted(EABImageViewMode InView) const;

	DECLARE_MULTICAST_DELEGATE(FOnMetaHumanViewportSettingsChanged);
	FOnMetaHumanViewportSettingsChanged OnSettingsChangedDelegate;

public:

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	EABImageViewMode CurrentViewMode;

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	float DepthNear;

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	float DepthFar;

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	FMetaHumanViewportCameraState CameraState;

protected:

	UPROPERTY(VisibleAnywhere, Category = "Viewport Settings")
	TMap<EABImageViewMode, FMetaHumanViewportState> ViewportState;

};