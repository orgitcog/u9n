// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"

class SCaptureManagerWidget;
class UMetaHumanCaptureSource;
class SWidget;

class FCaptureManager:
	public TSharedFromThis<SWidget>		// Enables 'this->AsShared()'
{
public:
	static FCaptureManager* Get();
	static void Initialize();
	static void Terminate();
	void Show();

	TWeakPtr<SDockTab> ShowMonitoringTab(UMetaHumanCaptureSource* CaptureSource); //TODO: add parameter to determine which tab should be shown

private:

	void OnCaptureManagerTabClosed(TSharedRef<class SDockTab> ClosedTab);
	bool OnCanCloseCaptureTab();
	void OnMapOpened(const FString& FileName, bool bAsTemplate);

	FCaptureManager();
	~FCaptureManager();
	void RegisterTabSpawner();
	void UnregisterTabSpawner();

private:
	static FCaptureManager* Instance;
	TSharedPtr<SCaptureManagerWidget> CaptureManagerWidget;

	TSharedPtr<class FCaptureManagerCommands> Commands;
	FDelegateHandle OnMapOpenedDelegateHandle;
};
