// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Widget.h"
#include "CommonWidgetGroupBase.generated.h"

#define UE_API COMMONUI_API

/**
 * Base class for CommonUI widget groups, currently only used for button groups
 */
UCLASS(MinimalAPI, Abstract, BlueprintType)
class UCommonWidgetGroupBase : public UObject
{
	GENERATED_BODY()

public:
	UE_API UCommonWidgetGroupBase();

	virtual TSubclassOf<UWidget> GetWidgetType() const { return UWidget::StaticClass(); }

	UFUNCTION(BlueprintCallable, Category = Group)
	UE_API void AddWidget(UWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = Group)
	UE_API void AddWidgets(const TArray<UWidget*>& Widgets);

	UFUNCTION(BlueprintCallable, Category = Group)
	UE_API void RemoveWidget(UWidget* InWidget);

	UFUNCTION(BlueprintCallable, Category = Group)
	UE_API void RemoveAll();

	template <typename WidgetT>
	void AddWidgets(const TArray<WidgetT>& Widgets)
	{
		for (UWidget* Widget : Widgets)
		{
			AddWidget(Widget);
		}
	}

protected:
	UE_API virtual void OnWidgetAdded(UWidget* NewWidget) PURE_VIRTUAL(UCommonWidgetGroupBase::OnWidgetAdded, );
	UE_API virtual void OnWidgetRemoved(UWidget* OldWidget) PURE_VIRTUAL(UCommonWidgetGroupBase::OnWidgetRemoved, );
	UE_API virtual void OnRemoveAll() PURE_VIRTUAL(UCommonWidgetGroupBase::OnRemoveAll, );
};

#undef UE_API
