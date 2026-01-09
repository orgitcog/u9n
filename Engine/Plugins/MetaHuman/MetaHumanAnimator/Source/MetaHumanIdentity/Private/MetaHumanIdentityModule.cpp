// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityModule.h"

#include "MetaHumanIdentityStyle.h"

void FMetaHumanIdentityModule::StartupModule()
{
	// Register the style used for the Identity editors
	FMetaHumanIdentityStyle::Register();
}

void FMetaHumanIdentityModule::ShutdownModule()
{
	// Unregister the styles used by the Identity module
	FMetaHumanIdentityStyle::Unregister();
}

IMPLEMENT_MODULE(FMetaHumanIdentityModule, MetaHumanIdentity)
