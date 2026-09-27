// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "SAssetEditorViewport.h"
#include "SMetaHumanEditorViewportToolBar.h"
#include "MetaHumanABCommandList.h"
#include "SMetaHumanOverlayWidget.h"
#include "STrackerImageViewer.h"

/**
 * A viewport to manage AB views with the ability to display editable tracker contours
 * as an overlay. This viewport can also display an extra widget at the bottom represented by the Content default slot.
 */
class METAHUMANTOOLKIT_API SMetaHumanEditorViewport
	: public SAssetEditorViewport
{
public:

	SLATE_BEGIN_ARGS(SMetaHumanEditorViewport) {}

		/** Extra content to display in the bottom of the viewport */
		SLATE_DEFAULT_SLOT(FArguments, Content)

		/** A reference to the viewport client that controls this viewport */
		SLATE_ARGUMENT(TSharedPtr<class FMetaHumanEditorViewportClient>, ViewportClient)

		/** The command list containing commands bound to views A and B */
		SLATE_ARGUMENT(FMetaHumanABCommandList, ABCommandList)

		/** Delegated called to the contents of the view menus on both A and B sides */
		SLATE_EVENT(FOnGetABMenuContents, OnGetABViewMenuContents)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FAssetEditorViewportConstructionArgs& InViewportConstructionArgs);

	/** Returns a reference to the STrackerImageViewer we are controlling */
	TSharedRef<SMetaHumanOverlayWidget<STrackerImageViewer>> GetTrackerImageViewer() const;

	//~Begin SAssetEditorViewport interface
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime);
	virtual void BindCommands() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> InOverlay) override;
	virtual void OnFocusViewportToSelection() override;
	virtual FReply OnKeyDown(const FGeometry& InMyGeometry, const FKeyEvent& InKeyEvent) override;
	//~End SAssetEditorViewport interface

private:

	bool IsShowingCurvesForCurrentView() const;
	bool IsShowingPointsForCurrentView() const;

	TSharedRef<class FMetaHumanEditorViewportClient> GetMetaHumanViewportClient() const;

private:

	/** A reference to the tracker image viewer overlay */
	TSharedPtr<SMetaHumanOverlayWidget<STrackerImageViewer>> TrackerImageViewer;

	/** Holds the current geometry of the widget. Used to trigger the ViewportSizeChangedDelegate if the size changes */
	FGeometry CurrentViewportGeometry;

	/** Triggered to request the context menu for view A and B */
	FOnGetABMenuContents OnGetABViewMenuContentsDelegate;

	/** The AB command list used when showing the view menus A and B */
	FMetaHumanABCommandList ABCommandList;
};