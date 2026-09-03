// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneAnimMixerModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMovieSceneAnimMixer);

namespace UE::MovieScene
{

void FMovieSceneAnimMixerModule::StartupModule()
{
}

void FMovieSceneAnimMixerModule::ShutdownModule()
{

}

}

IMPLEMENT_MODULE(UE::MovieScene::FMovieSceneAnimMixerModule, MovieSceneAnimMixer)
