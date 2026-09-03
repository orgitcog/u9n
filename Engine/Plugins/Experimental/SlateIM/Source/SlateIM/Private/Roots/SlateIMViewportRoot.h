// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_ENGINE
#include "ISlateIMRoot.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/Layout/SConstraintCanvas.h"

class SImWrapper;

namespace SlateIM
{
	struct FViewportRootLayout;
}

class UGameViewportClient;
class ULocalPlayer;

#if WITH_EDITOR
class IAssetViewport;
#endif

class FSlateIMViewportRoot : public ISlateIMRoot
{
	SLATE_IM_TYPE_DATA(FSlateIMViewportRoot, ISlateIMRoot)

public:
	FSlateIMViewportRoot(UGameViewportClient* InGameViewport);
	FSlateIMViewportRoot(ULocalPlayer* InLocalPlayer);
#if WITH_EDITOR
	FSlateIMViewportRoot(TSharedPtr<IAssetViewport> InLevelViewport);
#endif
	virtual ~FSlateIMViewportRoot() override;

	virtual void UpdateChild(TSharedRef<SWidget> Child, const FSlateIMSlotData& AlignmentData) override;
	virtual bool IsVisible() const override
	{
		return true;
	}
	virtual FSlateIMInputState& GetInputState() override;

	void UpdateViewport(const SlateIM::FViewportRootLayout& Layout);

	TWeakObjectPtr<UGameViewportClient> GameViewport;
	TWeakObjectPtr<ULocalPlayer> LocalPlayer;
#if WITH_EDITOR
	TWeakPtr<IAssetViewport> AssetViewport;
#endif

private:
	TSharedPtr<SImWrapper> ViewportRoot;
	SConstraintCanvas::FSlot* Slot = nullptr;
};
#endif // WITH_ENGINE
