// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "Input/UIActionBindingHandle.h"

#include "CommonUserWidget.generated.h"

#define UE_API COMMONUI_API

class UCommonInputSubsystem;
class UCommonUISubsystemBase;
class FSlateUser;

struct FUIActionTag;
struct FBindUIActionArgs;
enum class ECommonInputMode : uint8;

UCLASS(MinimalAPI, ClassGroup = UI, meta = (Category = "Common UI", DisableNativeTick))
class UCommonUserWidget : public UUserWidget
{
	GENERATED_UCLASS_BODY()

public:
	/** Sets whether or not this widget will consume ALL pointer input that reaches it */
	UFUNCTION(BlueprintCallable, Category = CommonUserWidget)
	UE_API void SetConsumePointerInput(bool bInConsumePointerInput);

	/** Add a widget to the list of widgets to get scroll events for this input root node */
	UFUNCTION(BlueprintCallable, Category = CommonUserWidget)
	UE_API void RegisterScrollRecipientExternal(const UWidget* AnalogScrollRecipient);

	/** Remove a widget from the list of widgets to get scroll events for this input root node */
	UFUNCTION(BlueprintCallable, Category = CommonUserWidget)
	UE_API void UnregisterScrollRecipientExternal(const UWidget* AnalogScrollRecipient);

public:

	const TArray<FUIActionBindingHandle>& GetActionBindings() const { return ActionBindings; }
	const TArray<TWeakObjectPtr<const UWidget>> GetScrollRecipients() const { return ScrollRecipients; }

	/**
	 * Convenience methods for menu action registrations (any UWidget can register via FCommonUIActionRouter directly, though generally that shouldn't be needed).
	 * Persistent bindings are *always* listening for input while registered, while normal bindings are only listening when all of this widget's activatable parents are activated.
	 */
	UE_API FUIActionBindingHandle RegisterUIActionBinding(const FBindUIActionArgs& BindActionArgs);

	UE_API void RemoveActionBinding(FUIActionBindingHandle ActionBinding);
	UE_API void AddActionBinding(FUIActionBindingHandle ActionBinding);

protected:
	virtual ERequiresLegacyPlayer GetLegacyPlayerRequirement() const override { return ERequiresLegacyPlayer::No; }

	UE_API virtual void OnWidgetRebuilt() override;
	UE_API virtual void NativeDestruct() override;
	
	UE_API virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	UE_API virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	UE_API virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	UE_API virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	UE_API virtual FReply NativeOnTouchGesture(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	UE_API virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	
	UE_API UCommonInputSubsystem* GetInputSubsystem() const;
	UE_API UCommonUISubsystemBase* GetUISubsystem() const;
	UE_API TSharedPtr<FSlateUser> GetOwnerSlateUser() const;

	template <typename GameInstanceT = UGameInstance>
	GameInstanceT& GetGameInstanceChecked() const
	{
		GameInstanceT* GameInstance = GetGameInstance<GameInstanceT>();
		check(GameInstance);
		return *GameInstance;
	}

	template <typename PlayerControllerT = APlayerController>
	PlayerControllerT& GetOwningPlayerChecked() const
	{
		PlayerControllerT* PC = GetOwningPlayer<PlayerControllerT>();
		check(PC);
		return *PC;
	}

	UE_API void RegisterScrollRecipient(const UWidget& AnalogScrollRecipient);
	UE_API void UnregisterScrollRecipient(const UWidget& AnalogScrollRecipient);

#if WITH_EDITOR
	UE_API virtual const FText GetPaletteCategory() override;
#endif // WITH_EDITOR

	/** True to generally display this widget's actions in the action bar, assuming it has actions. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = true))
	bool bDisplayInActionBar = false;

private:

	/** Set this to true if you don't want any pointer (mouse and touch) input to bubble past this widget */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input, meta = (AllowPrivateAccess = true))
	bool bConsumePointerInput = false;

private:

	TArray<FUIActionBindingHandle> ActionBindings;
	TArray<TWeakObjectPtr<const UWidget>> ScrollRecipients;
};

#undef UE_API
