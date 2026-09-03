// Copyright Deep Tree Echo. All Rights Reserved.

#include "DeepTreeEchoAvatar.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FDeepTreeEchoAvatarModule"

void FDeepTreeEchoAvatarModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoAvatar module starting up - AI Controller layer initialized"));
}

void FDeepTreeEchoAvatarModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoAvatar module shutting down"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDeepTreeEchoAvatarModule, DeepTreeEchoAvatar)
