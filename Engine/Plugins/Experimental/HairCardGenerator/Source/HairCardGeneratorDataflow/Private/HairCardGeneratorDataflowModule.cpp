// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairCardGeneratorDataflowModule.h"
#include "GenerateCardsClumpsNode.h"
#include "BuildCardsSettingsNode.h"
#include "GenerateCardsGeometryNode.h"
#include "GenerateCardsTexturesNode.h"
#include "CardsAssetTerminalNode.h"
#include "HairCardDataflowRendering.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "HairCardGeneratorDataflow"

void FHairCardGeneratorDataflowModule::StartupModule()
{
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateCardsClumpsNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateCardsGeometryNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FGenerateCardsTexturesNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FBuildCardsSettingsNode);
	DATAFLOW_NODE_REGISTER_CREATION_FACTORY(FCardsAssetTerminalNode);

	UE::CardGen::Private::RegisterRenderingCallbacks();
}

void FHairCardGeneratorDataflowModule::ShutdownModule()
{
	UE::CardGen::Private::DeregisterRenderingCallbacks();
}

IMPLEMENT_MODULE(FHairCardGeneratorDataflowModule, HairCardGeneratorDataflow)

#undef LOCTEXT_NAMESPACE
