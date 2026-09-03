// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "Framework/Application/AnalogCursor.h"
#include "InputCoreTypes.h"
#include "Rendering/SlateRenderTransform.h"

#define UE_API COMMONUI_API

class UCommonUIActionRouterBase;
class UCommonInputSubsystem;
class SWidget;
class UWidget;
class UGameViewportClient;

struct FInputEvent;
enum class ECommonInputType : uint8;
enum EOrientation : int;

/**
 * Analog cursor preprocessor that tastefully hijacks things a bit to support controller navigation by moving a hidden cursor around based on focus.
 *
 * Introduces a separate focus-driven mode of operation, wherein the cursor is made invisible and automatically updated
 * to be centered over whatever widget is currently focused (except the game viewport - we completely hide it then)
 */
class FCommonAnalogCursor : public FAnalogCursor
{
public:
	template <typename AnalogCursorT = FCommonAnalogCursor>
	static TSharedRef<AnalogCursorT> CreateAnalogCursor(const UCommonUIActionRouterBase& InActionRouter)
	{
		TSharedRef<AnalogCursorT> NewCursor = MakeShareable(new AnalogCursorT(InActionRouter));
		NewCursor->Initialize();
		return NewCursor;
	}

	UE_API virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	UE_API virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	UE_API virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	UE_API virtual bool CanReleaseMouseCapture() const;

	UE_API virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	UE_API virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	UE_API virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;
	UE_API virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;

	UE_API void SetCursorMovementStick(EAnalogStick InCursorMovementStick);

	UE_API virtual int32 GetOwnerUserIndex() const override;

	UE_API virtual void ShouldHandleRightAnalog(bool bInShouldHandleRightAnalog);

	virtual bool IsAnalogMovementEnabled() const { return bIsAnalogMovementEnabled; }

	UE_API virtual bool ShouldVirtualAcceptSimulateMouseButton(const FKeyEvent& InKeyEvent, EInputEvent InputEvent) const;

protected:
	UE_API FCommonAnalogCursor(const UCommonUIActionRouterBase& InActionRouter);
	UE_API virtual void Initialize();
	
	UE_API virtual EOrientation DetermineScrollOrientation(const UWidget& Widget) const;

	UE_API virtual bool IsRelevantInput(const FKeyEvent& KeyEvent) const override;
	UE_API virtual bool IsRelevantInput(const FAnalogInputEvent& AnalogInputEvent) const override;
	
	UE_API void SetNormalizedCursorPosition(const FVector2D& RelativeNewPosition);
	UE_API bool IsInViewport(const FVector2D& Position) const;
	UE_API FVector2D ClampPositionToViewport(const FVector2D& InPosition) const;
	UE_API void HideCursor();

	UE_API UGameViewportClient* GetViewportClient() const;
	
	/**
	 * A ridiculous function name, but we have this exact question in a few places.
	 * We don't care about input while our owning player's game viewport isn't involved in the focus path,
	 * but we also want to hold off doing anything while that game viewport has full capture.
	 * So we need that "relevant, but not exclusive" sweet spot.
	 */
	UE_API bool IsGameViewportInFocusPathWithoutCapture() const;

	UE_API virtual void RefreshCursorSettings();
	UE_API virtual void RefreshCursorVisibility();
	
	UE_API virtual void HandleInputMethodChanged(ECommonInputType NewInputMethod);
	
	UE_API bool IsUsingGamepad() const;

	UE_API bool ShouldHideCursor() const;

	// Knowingly unorthodox member reference to a UObject - ok because we are a subobject of the owning router and will never outlive it
	const UCommonUIActionRouterBase& ActionRouter;
	ECommonInputType ActiveInputMethod;

	bool bIsAnalogMovementEnabled = false;
	
	bool bShouldHandleRightAnalog = true;
	
private:

	/** The current set of pointer buttons being used as keys. */
	TSet<FKey> PointerButtonDownKeys;

	TWeakPtr<SWidget> LastCursorTarget;
	FSlateRenderTransform LastCursorTargetTransform;

	float TimeUntilScrollUpdate = 0.f;
	
#if !UE_BUILD_SHIPPING
	enum EShoulderButtonFlags
	{
		None = 0,
		LeftShoulder = 1 << 0,
		RightShoulder = 1 << 1,
		LeftTrigger = 1 << 2,
		RightTrigger = 1 << 3,
		All = LeftShoulder | RightShoulder | LeftTrigger | RightTrigger
	};
	int32 ShoulderButtonStatus = EShoulderButtonFlags::None;
#endif
};

#undef UE_API
