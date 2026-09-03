// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneRenderBuilderInterface.h"
#include "SceneRendererInterface.h"
#include "EngineModule.h"

TUniquePtr<ISceneRenderBuilder> ISceneRenderBuilder::Create(FSceneInterface* SceneInterface)
{
	return GetRendererModule().CreateSceneRenderBuilder(SceneInterface);
}