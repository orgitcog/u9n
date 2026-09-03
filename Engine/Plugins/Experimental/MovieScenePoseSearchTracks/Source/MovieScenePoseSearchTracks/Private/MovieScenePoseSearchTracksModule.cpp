// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieScenePoseSearchTracksModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneAnimMixerModule.h"
#include "PoseSearch/PoseHistoryProvider.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "Animation/AnimNodeBase.h"
#include "EvaluationVM/EvaluationVM.h"

namespace UE::MovieScene
{

void FMovieScenePoseSearchTracksModule::StartupModule()
{
}

void FMovieScenePoseSearchTracksModule::ShutdownModule()
{

}

}

IMPLEMENT_MODULE(UE::MovieScene::FMovieScenePoseSearchTracksModule, MovieScenePoseSearchTracks)
