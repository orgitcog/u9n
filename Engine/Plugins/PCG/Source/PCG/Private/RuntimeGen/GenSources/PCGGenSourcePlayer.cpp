// Copyright Epic Games, Inc. All Rights Reserved.

#include "RuntimeGen/GenSources/PCGGenSourcePlayer.h"

#include "SceneView.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGGenSourcePlayer)

void UPCGGenSourcePlayer::Tick()
{
	ViewFrustum = TOptional<FConvexVolume>();

	const ULocalPlayer* LocalPlayer = PlayerController.IsValid() ? PlayerController->GetLocalPlayer() : nullptr;

	if (LocalPlayer && LocalPlayer->ViewportClient)
	{
		FSceneViewProjectionData ProjectionData;

		if (LocalPlayer->GetProjectionData(LocalPlayer->ViewportClient->Viewport, ProjectionData))
		{
			FConvexVolume ConvexVolume;
			GetViewFrustumBounds(ConvexVolume, ProjectionData.ComputeViewProjectionMatrix(), /*bUseNearPlane=*/true, /*bUseFarPlane=*/true);

			ViewFrustum = ConvexVolume;
		}
	}
}

TOptional<FVector> UPCGGenSourcePlayer::GetPosition() const
{
	if (PlayerController.IsValid())
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
		return CameraLocation;
	}

	return TOptional<FVector>();
}

TOptional<FVector> UPCGGenSourcePlayer::GetDirection() const
{
	if (PlayerController.IsValid())
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->GetPlayerViewPoint(CameraLocation, CameraRotation);
		return CameraRotation.Vector();
	}

	return TOptional<FVector>();
}

void UPCGGenSourcePlayer::SetPlayerController(const APlayerController* InPlayerController)
{
	PlayerController = InPlayerController;
}
