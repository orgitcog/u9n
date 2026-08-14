// Copyright Epic Games, Inc. All Rights Reserved.

#include "Common/DisplayClusterMediaCameraCommon.h"

#include "Components/DisplayClusterICVFXCameraComponent.h"
#include "Game/IDisplayClusterGameManager.h"

#include "DisplayClusterRootActor.h"
#include "IDisplayCluster.h"


FDisplayClusterMediaCameraCommon::FDisplayClusterMediaCameraCommon(const FString& InCameraId)
	: CameraId(InCameraId)
{
}

UDisplayClusterICVFXCameraComponent* FDisplayClusterMediaCameraCommon::GetCameraComponent() const
{
	if (const ADisplayClusterRootActor* const DCRA = IDisplayCluster::Get().GetGameMgr()->GetRootActor())
	{
		if (UDisplayClusterICVFXCameraComponent* ICVFXCameraComponent = DCRA->GetComponentByName<UDisplayClusterICVFXCameraComponent>(CameraId))
		{
			return ICVFXCameraComponent;
		}
	}

	return nullptr;
}

void FDisplayClusterMediaCameraCommon::GetLateOCIOParameters(bool& bOutLateOCIOEnabled, bool& bOutTransferPQ) const
{
	if (const UDisplayClusterICVFXCameraComponent* const ICVFXCameraComponent = GetCameraComponent())
	{
		// Check if the main OCIO switch is on
		const bool bEnabledOCIO = ICVFXCameraComponent->CameraSettings.CameraOCIO.AllNodesOCIOConfiguration.bIsEnabled;
		// Check if media is enabled on this camera
		const bool bEnabledMedia = ICVFXCameraComponent->CameraSettings.RenderSettings.Media.bEnable;
		// Check if the late OCIO flag is on
		const bool bEnabledLateOCIO = ICVFXCameraComponent->CameraSettings.RenderSettings.Media.bLateOCIOPass;

		// Return OCIO configuration parameters
		bOutLateOCIOEnabled = bEnabledOCIO && bEnabledMedia && bEnabledLateOCIO;
		bOutTransferPQ = ICVFXCameraComponent->CameraSettings.RenderSettings.Media.bTransferPQ;
	}
}
