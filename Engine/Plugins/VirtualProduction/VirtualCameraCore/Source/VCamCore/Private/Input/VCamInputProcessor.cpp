// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamInputProcessor.h"

#include "Input/InputVCamSubsystem.h"

#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerInput.h"
#include "Input/Events.h"
#include "InputKeyEventArgs.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

namespace UE::VCamCore
{
	FVCamInputProcessor::FVCamInputProcessor(UInputVCamSubsystem& OwningSubsystem)
		: OwningSubsystem(&OwningSubsystem)
	{}

	void FVCamInputProcessor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
	{
		ProcessAccumulatedPointerInput(DeltaTime);
	}

	bool FVCamInputProcessor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			InKeyEvent.GetInputDeviceId(),
			InKeyEvent.GetKey(),
			IE_Pressed,
			InKeyEvent.GetEventTimestamp());

		Args.DeltaTime = SlateApp.GetDeltaTime();
		Args.NumSamples = Args.Key.IsAnalog() ? 1 : 0;

		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			InKeyEvent.GetInputDeviceId(),
			InKeyEvent.GetKey(),
			IE_Released,
			/*AmountDepressed*/0.0f,
			/*bIsTouchEvent*/false,
			InKeyEvent.GetEventTimestamp());

		Args.DeltaTime = SlateApp.GetDeltaTime();
		Args.NumSamples = Args.Key.IsAnalog() ? 1 : 0;
		
		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			InAnalogInputEvent.GetInputDeviceId(),
			InAnalogInputEvent.GetKey(),
			/*AmountDepressed*/InAnalogInputEvent.GetAnalogValue(),
			/*deltaTime*/SlateApp.GetDeltaTime(),
			/*numsamples*/1,
			InAnalogInputEvent.GetEventTimestamp());
		
		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		UpdateCachedPointerPosition(MouseEvent);
		return IInputProcessor::HandleMouseMoveEvent(SlateApp, MouseEvent);
	}

	bool FVCamInputProcessor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			MouseEvent.GetInputDeviceId(),
			MouseEvent.GetEffectingButton(),
			IE_Pressed,
			1.0f,
			MouseEvent.IsTouchEvent(),
			MouseEvent.GetEventTimestamp());

		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			MouseEvent.GetInputDeviceId(),
			MouseEvent.GetEffectingButton(),
			IE_Released,
			0.0f,
			MouseEvent.IsTouchEvent(),
			MouseEvent.GetEventTimestamp());
		
		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
	{
		FInputKeyEventArgs Args(
			nullptr,
			MouseEvent.GetInputDeviceId(),
			MouseEvent.GetEffectingButton(),
			IE_DoubleClick,
			1.0f,
			MouseEvent.IsTouchEvent(),
			MouseEvent.GetEventTimestamp());
		
		return InputKeyToSubsystem(Args);
	}

	bool FVCamInputProcessor::HandleMouseWheelOrGestureEvent(FSlateApplication& SlateApp, const FPointerEvent& InWheelEvent, const FPointerEvent* InGestureEvent)
	{
		const FKey MouseWheelKey = InWheelEvent.GetWheelDelta() < 0 ? EKeys::MouseScrollDown : EKeys::MouseScrollUp;

		// Input the Mouse wheel key events (mouse scroll down or scroll up) as being pressed and released this frame
		// The SceneViewport Inputs the Mouse Scroll Wheel buttons up and down in the same frame, this replicates that behavior
		{
			FInputKeyEventArgs PressedArgs(
				nullptr,
				InWheelEvent.GetInputDeviceId(),
				MouseWheelKey,
				IE_Pressed,
				1.0f,
				InWheelEvent.IsTouchEvent(),
				InWheelEvent.GetEventTimestamp());
			
			FInputKeyEventArgs ReleasedArgs = PressedArgs;
			ReleasedArgs.Event = IE_Released;

			InputKeyToSubsystem(PressedArgs);
			InputKeyToSubsystem(ReleasedArgs);	
		}
		// Input the wheel axis delta to get the MouseWheelAxis button working
		{
			FInputKeyEventArgs Args(
				nullptr,
				InWheelEvent.GetInputDeviceId(),
				EKeys::MouseWheelAxis,
				InWheelEvent.GetWheelDelta(),
				SlateApp.GetDeltaTime(),
				1,
				InWheelEvent.GetEventTimestamp());
			
			InputKeyToSubsystem(Args);
		}
		
		return IInputProcessor::HandleMouseWheelOrGestureEvent(SlateApp, InWheelEvent, InGestureEvent);
	}

	void FVCamInputProcessor::UpdateCachedPointerPosition(const FPointerEvent& MouseEvent)
	{
		CachedCursorDelta = MouseEvent.GetCursorDelta();
		
		++NumCursorSamplesThisFrame.X;
		++NumCursorSamplesThisFrame.Y;
	}

	void FVCamInputProcessor::ProcessAccumulatedPointerInput(float DeltaTime)
	{
		const FInputDeviceId DefaultInputDevice = IPlatformInputDeviceMapper::Get().GetDefaultInputDevice();
		
		// Input the MouseX value
		{
			FInputKeyEventArgs MouseXArgs (
				nullptr,
				DefaultInputDevice,
				EKeys::MouseX,
				CachedCursorDelta.X,
				DeltaTime,
				NumCursorSamplesThisFrame.X,
				0u);
			
			InputKeyToSubsystem(MouseXArgs);
		}

		// Input the MouseY value
		{
			FInputKeyEventArgs MouseYArgs (
				nullptr,
				DefaultInputDevice,
				EKeys::MouseX,
				CachedCursorDelta.Y,
				DeltaTime,
				NumCursorSamplesThisFrame.Y,
				0u);
			
			InputKeyToSubsystem(MouseYArgs);
		}
		
		NumCursorSamplesThisFrame = FIntPoint::ZeroValue;
		CachedCursorDelta = FVector2D::ZeroVector;
	}

	bool FVCamInputProcessor::InputKeyToSubsystem(const FInputKeyEventArgs& Params)
	{
		// Even after our owning subsystem is destroyed, the core input system may hold onto us for just a little bit longer due to how the input system is designed
		if (OwningSubsystem.IsValid())
		{
			return OwningSubsystem->InputKey(Params);
		}
		
		return false;
	}
}
