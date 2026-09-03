// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/IInputInterface.h"
#if !PLATFORM_TVOS
#import <CoreMotion/CoreMotion.h>
#endif
#import <GameController/GameController.h>
#include "Misc/CoreMisc.h"
#include "Math/Quat.h"
#include "Math/Vector.h"
#include "Math/Color.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"
#include "AppleControllerInterface.h"


#define KEYCODE_ENTER 1000
#define KEYCODE_BACKSPACE 1001
#define KEYCODE_ESCAPE 1002
#define KEYCODE_TAB 1003
#define KEYCODE_LEFT 1004
#define KEYCODE_RIGHT 1005
#define KEYCODE_DOWN 1006
#define KEYCODE_UP 1007
#define KEYCODE_LEFT_CONTROL 1008
#define KEYCODE_LEFT_SHIFT 1009
#define KEYCODE_LEFT_ALT 1010
#define KEYCODE_LEFT_COMMAND 1011
#define KEYCODE_CAPS_LOCK 1012
#define KEYCODE_RIGHT_CONTROL 1013
#define KEYCODE_RIGHT_SHIFT 1014
#define KEYCODE_RIGHT_ALT 1015
#define KEYCODE_RIGHT_COMMAND 1016
#define KEYCODE_F1 1017
#define KEYCODE_F2 1018
#define KEYCODE_F3 1019
#define KEYCODE_F4 1020
#define KEYCODE_F5 1021
#define KEYCODE_F6 1022
#define KEYCODE_F7 1023
#define KEYCODE_F8 1024
#define KEYCODE_F9 1025
#define KEYCODE_F10 1026
#define KEYCODE_F11 1027
#define KEYCODE_F12 1028
#define KEYCODE_F13 1029
#define KEYCODE_F14 1030
#define KEYCODE_F15 1031
#define KEYCODE_F16 1032
#define KEYCODE_F17 1033
#define KEYCODE_F18 1034
#define KEYCODE_F19 1035
#define KEYCODE_F20 1036

enum TouchType
{
	TouchBegan,
	TouchMoved,
	TouchEnded,
	ForceChanged,
	FirstMove,
};

struct TouchInput
{
	int Handle;
	TouchType Type;
	FVector2D LastPosition;
	FVector2D Position;
	float Force;
};

enum class EIOSEventType : int32
{
    Invalid = 0,
    LeftMouseDown = 1,
    LeftMouseUp = 2,
    RightMouseDown = 3,
    RightMouseUp = 4,
    KeyDown = 10,
    KeyUp = 11,
    MiddleMouseDown = 25,
    MiddleMouseUp = 26,
    ThumbDown = 50,
    ThumbUp = 70,
};

struct FDeferredIOSEvent
{
    EIOSEventType type;
    uint32 keycode;
    uint32 charcode;
};

/**
 * Interface class for IOS input devices
 */
class FIOSInputInterface : public FAppleControllerInterface, FSelfRegisteringExec
{
public:

	static TSharedRef< FIOSInputInterface > Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );
	static TSharedPtr< FIOSInputInterface > Get();

public:

	virtual ~FIOSInputInterface() {}
	
	/**
	 * Poll for controller state and send events if needed
	 */
	void SendControllerEvents();
	
	/**
	 * IInputInterface implementation
	 */
	virtual void SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value) override;
	virtual void SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &values) override;

	static APPLICATIONCORE_API void QueueTouchInput(const TArray<TouchInput>& InTouchEvents);
	static void QueueKeyInput(int32 Key, int32 Char);

	void SetGamepadsAllowed(bool bAllowed) { bAllowControllers = bAllowed; }
	void SetGamepadsBlockDeviceFeedback(bool bBlock) { bControllersBlockDeviceFeedback = bBlock; }

	void EnableMotionData(bool bEnable);
	bool IsMotionDataEnabled() const;
    
    static void SetKeyboardInhibited(bool bInhibited) { bKeyboardInhibited = bInhibited; }
    static bool IsKeyboardInhibited() { return bKeyboardInhibited; }
    
    NSData* GetGamepadGlyphRawData(const FGamepadKeyNames::Type& ButtonKey, uint32 ControllerIndex);

protected:

	//~ Begin Exec Interface
	virtual bool Exec_Runtime(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	//~ End Exec Interface

private:

	FIOSInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

    // handle disconnect and connect events
    void HandleMouseConnection(GCMouse* Mouse);
    void HandleMouseDisconnect(GCMouse* Mouse);
    void HandleKeyboardConnection(GCKeyboard* Keyboard);
    void HandleKeyboardDisconnect(GCKeyboard* Keyboard);

    /**
	 * Get the current Movement data from the device
	 *
	 * @param Attitude The current Roll/Pitch/Yaw of the device
	 * @param RotationRate The current rate of change of the attitude
	 * @param Gravity A vector that describes the direction of gravity for the device
	 * @param Acceleration returns the current acceleration of the device
	 */
	void GetMovementData(FVector& Attitude, FVector& RotationRate, FVector& Gravity, FVector& Acceleration);

	/**
	 * Calibrate the devices motion
	 */
	void CalibrateMotion(uint32 PlayerIndex);

private:
	void ProcessTouchesAndKeys(uint32 ControllerId, const TArray<TouchInput>& InTouchInputStack, const TArray<int32>& InKeyInputStack);
    void ProcessDeferredEvents();
    void ProcessEvent(const FDeferredIOSEvent& Event);
	
	// can the remote be rotated to landscape
	bool bAllowRemoteRotation;

	// can the game handle multiple gamepads at the same time (siri remote is a gamepad) ?
	bool bGameSupportsMultipleActiveControllers;

	// bluetooth connected controllers will block force feedback.
	bool bControllersBlockDeviceFeedback;
	
	/** Is motion paused or not? */
	bool bPauseMotion;

#if !PLATFORM_TVOS
	/** Access to the ios devices motion */
	CMMotionManager* MotionManager;

	/** Access to the ios devices tilt information */
	CMAttitude* ReferenceAttitude;
#endif

	/** Last frames roll, for calculating rate */
	float LastRoll;

	/** Last frames pitch, for calculating rate */
	float LastPitch;

	/** True if a calibration is requested */
	bool bIsCalibrationRequested;

	/** The center roll value for tilt calibration */
	float CenterRoll;

	/** The center pitch value for tilt calibration */
	float CenterPitch;

	/** When using just acceleration (without full motion) we store a frame of accel data to filter by */
	FVector FilteredAccelerometer;

	/** Last value sent to mobile haptics */
	float LastHapticValue;
	
	int HapticFeedbackSupportLevel;
    
    FCriticalSection EventsMutex;
        TArray<FDeferredIOSEvent> DeferredEvents;
        float MouseDeltaX;
        float MouseDeltaY;
        float ScrollDeltaY;
        bool bHaveMouse;
        static bool bKeyboardInhibited;
};
