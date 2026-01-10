// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkHubDiscoveryEditorModule.h"

#include "DiscoveryResponder.h"

void FLiveLinkHubDiscoveryEditor::StartupModule()
{
	DiscoveryResponder = MakeUnique<UE::CaptureManager::FDiscoveryResponder>();
}
void FLiveLinkHubDiscoveryEditor::ShutdownModule()
{
	DiscoveryResponder.Reset();
}

IMPLEMENT_MODULE(FLiveLinkHubDiscoveryEditor, LiveLinkHubDiscoveryEditor);