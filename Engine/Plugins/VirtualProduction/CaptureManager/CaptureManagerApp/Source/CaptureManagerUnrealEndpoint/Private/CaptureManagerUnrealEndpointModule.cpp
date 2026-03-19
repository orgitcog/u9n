// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerUnrealEndpointModule.h"

void FCaptureManagerUnrealEndpointModule::StartupModule()
{
	EndpointManager = MakeShared<UE::CaptureManager::FUnrealEndpointManager>();
}

void FCaptureManagerUnrealEndpointModule::ShutdownModule()
{
	EndpointManager.Reset();
}

TSharedRef<UE::CaptureManager::FUnrealEndpointManager> FCaptureManagerUnrealEndpointModule::GetEndpointManager() const
{
	return EndpointManager.ToSharedRef();
}

TOptional<TSharedRef<UE::CaptureManager::FUnrealEndpointManager>> FCaptureManagerUnrealEndpointModule::GetEndpointManagerIfValid() const
{
	if (EndpointManager.IsValid())
	{
		return EndpointManager.ToSharedRef();
	}

	return {};
}

IMPLEMENT_MODULE(FCaptureManagerUnrealEndpointModule, CaptureManagerUnrealEndpoint);