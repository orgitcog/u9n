// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Docking/TabManager.h"

class FAssetEditorToolkit;
class FLiveLinkHubSessionTabBase;
class FOutputLogController;
class ILiveLinkClient;
class FModalWindowManager;
class SWindow;

/** Responsible for creating the Slate window for the hub. */
class FLiveLinkHubWindowController : public TSharedFromThis<FLiveLinkHubWindowController>
{
public:
	
	FLiveLinkHubWindowController();
	~FLiveLinkHubWindowController();

	/** Get the root window. */
	TSharedPtr<SWindow> GetRootWindow() const { return RootWindow; }
	/** Restore the window's layout from a config. */
	void RestoreLayout(TSharedPtr<FAssetEditorToolkit> AssetEditorToolkit);
private:
	/** Create the main window. */
	TSharedRef<SWindow> CreateWindow();
	/** Create the slate application that hosts the livelink hub. */
	TSharedPtr<FModalWindowManager> InitializeSlateApplication();
	/** Window closed handler. */
	void OnWindowClosed(const TSharedRef<SWindow>& Window);
private:
	/** Pointer to the livelink client. */
	ILiveLinkClient* LiveLinkClient = nullptr;
	/** The ini file to use for saving the layout */
	FString LiveLinkHubLayoutIni;
	/** Holds the current layout for saving later. */
	TSharedPtr<FTabManager::FLayout> PersistentLayout;
	/** The main window being managed */
	TSharedPtr<SWindow> RootWindow;
	/** Manages modal windows for the application. */
	TSharedPtr<FModalWindowManager> ModalWindowManager;
	/** Menu bar widget for the hub. */
	TSharedPtr<class SWindowTitleBar> WindowTitleBar;

};
