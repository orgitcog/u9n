// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDisplayClusterICVFXCameraComponent;


/**
 * Common camera logic for media
 */
class FDisplayClusterMediaCameraCommon
{
public:

	FDisplayClusterMediaCameraCommon(const FString& CameraId);
	virtual ~FDisplayClusterMediaCameraCommon() = default;

protected:

	/** Finds an ICVFX camera component by name, otherwise nullptr */
	UDisplayClusterICVFXCameraComponent* GetCameraComponent() const;

	/** Returns late OCIO parameters of the current camera */
	void GetLateOCIOParameters(bool& bOutLateOCIOEnabled, bool& bOutTransferPQ) const;

private:

	/** Holds ICVFX camera component name associated with this media adapter */
	const FString CameraId;
};
