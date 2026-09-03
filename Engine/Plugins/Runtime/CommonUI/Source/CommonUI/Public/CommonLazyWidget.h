// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CommonLoadGuard.h"

#include "Engine/StreamableManager.h"
#include "CommonLazyWidget.generated.h"

#define UE_API COMMONUI_API

class UCommonMcpItemDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLazyContentChangedEvent, UUserWidget*);

/**
 * A special Image widget that can show unloaded images and takes care of the loading for you!
 */
UCLASS(MinimalAPI)
class UCommonLazyWidget : public UWidget
{
	GENERATED_UCLASS_BODY()

public:
	/**  */
	UFUNCTION(BlueprintCallable, Category = LazyContent)
	UE_API void SetLazyContent(const TSoftClassPtr<UUserWidget> SoftWidget);

	/**  */
	UFUNCTION(BlueprintCallable, Category = LazyContent)
	UUserWidget* GetContent() const { return Content; }

	UFUNCTION(BlueprintCallable, Category = LazyContent)
	UE_API bool IsLoading() const;

	FOnLazyContentChangedEvent& OnContentChanged() { return OnContentChangedEvent; }
	FOnLoadGuardStateChangedEvent& OnLoadingStateChanged() { return OnLoadingStateChangedEvent; }

protected:
	UE_API virtual TSharedRef<SWidget> RebuildWidget() override;
	UE_API virtual void OnWidgetRebuilt() override;
	UE_API virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	UE_API virtual void SynchronizeProperties() override;

	UE_API void SetForceShowSpinner(bool bShowLoading);

	UE_API void CancelStreaming();
	UE_API void OnStreamingStarted(TSoftClassPtr<UObject> SoftObject);
	UE_API void OnStreamingComplete(TSoftClassPtr<UObject> LoadedSoftObject);

#if WITH_EDITOR
	UE_API virtual const FText GetPaletteCategory() override;
#endif	

private:
	UE_API void SetLoadedContent(UUserWidget* InContent);
	UE_API void RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, TFunction<void()>&& Callback);
	UE_API void RequestAsyncLoad(TSoftClassPtr<UObject> SoftObject, FStreamableDelegate DelegateToCall);
	UE_API void HandleLoadGuardStateChanged(bool bIsLoading);

	/** The loading throbber brush */
	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush LoadingThrobberBrush;

	UPROPERTY(EditAnywhere, Category = Appearance)
	FSlateBrush LoadingBackgroundBrush;

	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Content;

	TSharedPtr<FStreamableHandle> StreamingHandle;
	FSoftObjectPath StreamingObjectPath;

	UPROPERTY(BlueprintAssignable, Category = LazyImage, meta = (DisplayName = "On Loading State Changed", ScriptName = "OnLoadingStateChanged"))
	FOnLoadGuardStateChangedDynamic BP_OnLoadingStateChanged;

	TSharedPtr<SLoadGuard> MyLoadGuard;
	FOnLoadGuardStateChangedEvent OnLoadingStateChangedEvent;

	FOnLazyContentChangedEvent OnContentChangedEvent;
};

#undef UE_API
