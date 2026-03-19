// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "SBaseCharacterFXEditorViewport.h"
#include "SCommonEditorViewportToolbarBase.h"

class ADataflowActor;
class FAdvancedPreviewScene;
class UDataflowEditorMode;
class FDataflowConstructionViewportClient;
class FDataflowConstructionScene;
class SRichTextBlock;

// ----------------------------------------------------------------------------------

class SDataflowConstructionViewport : public SBaseCharacterFXEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:
	SLATE_BEGIN_ARGS(SDataflowConstructionViewport) {}
	SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, ViewportClient)
	SLATE_END_ARGS()

	SDataflowConstructionViewport();

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

	FDataflowConstructionScene* GetConstructionScene() const;

	// SEditorViewport
	virtual void BindCommands() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual TSharedPtr<SWidget> BuildViewportToolbar() override;
	virtual TSharedPtr<IPreviewProfileController> CreatePreviewProfileController() override;
	virtual bool IsVisible() const override;
	virtual void OnFocusViewportToSelection() override;

	// ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<class SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;

	FText GetOverlayText() const;

private:
	FMargin GetOverlayMargin() const;

	/** Pointer to the box into which the overlay text items are added */
	TSharedPtr<SRichTextBlock> OverlayText;

	UDataflowEditorMode* GetEdMode() const;
};

