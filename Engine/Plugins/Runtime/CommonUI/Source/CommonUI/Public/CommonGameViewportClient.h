// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/GameViewportClient.h"
#include "CommonGameViewportClient.generated.h"

#define UE_API COMMONUI_API

class FReply;

DECLARE_DELEGATE_FourParams(FOnRerouteInputDelegate, FInputDeviceId /* InputDeviceId */, FKey /* Key */, EInputEvent /* EventType */, FReply& /* Reply */);
DECLARE_DELEGATE_FourParams(FOnRerouteAxisDelegate, FInputDeviceId /* InputDeviceId */, FKey /* Key */, float /* Delta */, FReply& /* Reply */);

UE_DEPRECATED(5.6, "Use the version which takes an FInputDeviceId instead (FOnRerouteTouchInputDelegate).")
DECLARE_DELEGATE_FiveParams(FOnRerouteTouchDelegate, int32 /* ControllerId */, uint32 /* TouchId */, ETouchType::Type /* TouchType */, const FVector2D& /* TouchLocation */, FReply& /* Reply */);

DECLARE_DELEGATE_FiveParams(FOnRerouteTouchInputDelegate, FInputDeviceId /* Deviceid */, uint32 /* TouchId */, ETouchType::Type /* TouchType */, const FVector2D& /* TouchLocation */, FReply& /* Reply */);

/**
* CommonUI Viewport to reroute input to UI first. Needed to allow CommonUI to route / handle inputs.
*/
UCLASS(MinimalAPI, Within = Engine, transient, config = Engine)
class UCommonGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:

	UE_API UCommonGameViewportClient(FVTableHelper& Helper);

	// UGameViewportClient interface begin
	UE_API virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	UE_API virtual bool InputAxis(const FInputKeyEventArgs& Args) override;
	UE_API virtual bool InputTouch(FViewport* Viewport, const FInputDeviceId DeviceId, uint32 Handle, ETouchType::Type Type, const FVector2D& TouchLocation, float Force, uint32 TouchpadIndex, const uint64 Timestamp) override;
	UE_API virtual void MouseMove(FViewport* InViewport, int32 X, int32 Y) override;
	UE_API virtual void CapturedMouseMove(FViewport* InViewport, int32 X, int32 Y) override;
	// UGameViewportClient interface end

	FOnRerouteInputDelegate& OnRerouteInput() { return RerouteInput; }
	FOnRerouteAxisDelegate& OnRerouteAxis() { return RerouteAxis; }
	
	UE_DEPRECATED(5.6, "Use the version which takes a FInputDeviceId instead (OnRerouteTouchInput).")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnRerouteTouchDelegate& OnRerouteTouch() { return RerouteTouch; }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	FOnRerouteTouchInputDelegate& OnRerouteTouchInput() { return RerouteTouchInput; }

	FOnRerouteInputDelegate& OnRerouteBlockedInput() { return RerouteBlockedInput; }

	/** Default Handler for Key input. */
	UE_API virtual void HandleRerouteInput(FInputDeviceId DeviceId, FKey Key, EInputEvent EventType, FReply& Reply);

	/** Default Handler for Axis input. */
	UE_API virtual void HandleRerouteAxis(FInputDeviceId DeviceId, FKey Key, float Delta, FReply& Reply);

	/** Default Handler for Touch input. */
	UE_DEPRECATED(5.6, "Use the version which takes a FInputDeviceId instead.")
	UE_API virtual void HandleRerouteTouch(int32 ControllerId, uint32 TouchId, ETouchType::Type TouchType, const FVector2D& TouchLocation, FReply& Reply) final;
	
	UE_API virtual void HandleRerouteTouch(FInputDeviceId DeviceId, uint32 TouchId, ETouchType::Type TouchType, const FVector2D& TouchLocation, FReply& Reply);

protected:
	
	/** Console window & fullscreen shortcut have higher priority than UI */
	UE_API virtual bool IsKeyPriorityAboveUI(const FInputKeyEventArgs& EventArgs);

	FOnRerouteInputDelegate RerouteInput;
	FOnRerouteAxisDelegate RerouteAxis;
	
	UE_DEPRECATED(5.6, "Use RerouteTouchInput instead.")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FOnRerouteTouchDelegate RerouteTouch;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	FOnRerouteTouchInputDelegate RerouteTouchInput;

	FOnRerouteInputDelegate RerouteBlockedInput;
};

#undef UE_API
