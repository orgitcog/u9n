// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Button.h"
#include "Types/UIFSlotBase.h"
#include "Types/UIFEvents.h"
#include "UIFWidget.h"

#include "UIFButton.generated.h"

#define UE_API UIFRAMEWORK_API

struct FUIFrameworkWidgetId;

/**
 *
 */
UCLASS(MinimalAPI, DisplayName = "Button UIFramework")
class UUIFrameworkButton : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UE_API UUIFrameworkButton();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	UE_API void SetContent(FUIFrameworkSimpleSlot Content);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FUIFrameworkSimpleSlot GetContent() const
	{
		return Slot;
	}

	UE_API virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	UE_API virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	UE_API virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

protected:
	UE_API virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	UE_API void HandleClick();

	UFUNCTION(Server, Reliable)
	UE_API void ServerClick(APlayerController* PlayerController);

	UFUNCTION()
	UE_API void OnRep_Slot();

public:
	FUIFrameworkClickEvent OnClick;

private:
	UPROPERTY(/*ExposeOnSpawn, */ReplicatedUsing = OnRep_Slot)
	FUIFrameworkSimpleSlot Slot;
};

UCLASS(MinimalAPI)
class UUIFrameworkButtonWidget : public UButton
{
	GENERATED_BODY()

public:
	UE_API UUIFrameworkButtonWidget();
};

#undef UE_API
