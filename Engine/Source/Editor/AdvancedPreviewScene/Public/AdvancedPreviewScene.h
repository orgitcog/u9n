// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrettyPreviewScene.h: Pretty preview scene definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "AssetViewerSettings.h"
#include "UnrealClient.h"
#include "Stats/Stats.h"
#include "InputCoreTypes.h"
#include "PreviewScene.h"
#include "TickableEditorObject.h"

class FViewport;
class UAssetViewerSettings;
class UMaterialInstanceConstant;
class UPostProcessComponent;
class USkyLightComponent;
class UStaticMeshComponent;
class USphereReflectionCaptureComponent;
struct FPreviewSceneProfile;
class FUICommandList;

class ADVANCEDPREVIEWSCENE_API FAdvancedPreviewScene : public FPreviewScene, public FTickableEditorObject
{
public:
	/** An event for when a profile changes. Includes the changed profile and (optionally) the name of the property in the profile that changed. */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnProfileChanged, const FPreviewSceneProfile&, FName);

	FAdvancedPreviewScene(ConstructionValues CVS, float InFloorOffset = 0.0f);
	~FAdvancedPreviewScene();

	void UpdateScene(FPreviewSceneProfile& Profile, bool bUpdateSkyLight = true, bool bUpdateEnvironment = true, bool bUpdatePostProcessing = true, bool bUpdateDirectionalLight = true);

	/** Begin FPreviewScene */
	virtual FLinearColor GetBackgroundColor() const override;
	/** End FPreviewScene */

	/* Begin FTickableEditorObject */
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	/* End FTickableEditorObject */
	
	FOnProfileChanged& OnProfileChanged() { return OnProfileChangedDelegate; }

	const bool HandleViewportInput(FViewport* InViewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples, bool bGamepad);
	const bool HandleInputKey(const FInputKeyEventArgs& EventArgs);

	void SetSkyRotation(const float SkyRotation);
	/* Sets the visiblity state for the floor/environment by storing it in the scene profile and refreshing the scene, in case bDirect is true it sets the visibility directly and leaves the profile untouched. */
	bool GetFloorVisibility() const;
	void SetFloorVisibility(const bool bVisible, const bool bDirect = false);
	void SetEnvironmentVisibility(const bool bVisible, const bool bDirect = false);
	float GetFloorOffset() const;
	void SetFloorOffset(const float InFloorOffset);
	void SetProfileIndex(const int32 InProfileIndex);
	FPreviewSceneProfile* GetCurrentProfile() const;

	const UStaticMeshComponent* GetFloorMeshComponent() const;
	const float GetSkyRotation() const;
	const int32 GetCurrentProfileIndex() const;
	const bool IsUsingPostProcessing() const;

	TSharedPtr<FUICommandList> GetCommandList() const { return UICommandList; }

	/** Toggle the sky sphere on and off */
	void HandleToggleEnvironment();
	
	/** Whether the sky sphere is on. */
	bool IsEnvironmentEnabled() const;

	/** Toggle the floor mesh on and off */
	void HandleToggleFloor();

	/** Whether the floor is on */
	bool IsFloorEnabled() const;

	/** Toggle the grid on and off. */
	void HandleToggleGrid();
	
	/** Whether the grid is enabled. */
	bool IsGridEnabled() const;

	/** Toggle post processing on and off */
	void HandleTogglePostProcessing();
	
	/** Whether post processing is enabled */
	bool IsPostProcessingEnabled() const;

protected:
	/** Create and map the command list. */
	virtual void BindCommands();
	
	/** Add commands to a provided command list. */
	virtual TSharedRef<FUICommandList> CreateCommandList();
	
	/** Handle refreshing the scene when settings change */
	void OnAssetViewerSettingsRefresh(const FName& InPropertyName);

protected:
	FOnProfileChanged OnProfileChangedDelegate;

	UStaticMeshComponent* SkyComponent;
	UMaterialInstanceConstant* InstancedSkyMaterial;
	UPostProcessComponent* PostProcessComponent;
	UStaticMeshComponent* FloorMeshComponent;
	UAssetViewerSettings* DefaultSettings;	
	bool bRotateLighting;

	float CurrentRotationSpeed;
	float PreviousRotation;
	float UILightingRigRotationDelta;

	bool bSkyChanged;
	bool bPostProcessing;

	int32 CurrentProfileIndex;

	/** Command list for input handling */
	TSharedPtr<FUICommandList> UICommandList;

	/** Delegate handle used to refresh the scene when settings change */
	FDelegateHandle RefreshDelegate;
};
