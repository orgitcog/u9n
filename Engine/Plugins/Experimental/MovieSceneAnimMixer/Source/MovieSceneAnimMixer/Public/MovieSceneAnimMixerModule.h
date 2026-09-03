// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

MOVIESCENEANIMMIXER_API DECLARE_LOG_CATEGORY_EXTERN(LogMovieSceneAnimMixer, Log, All);

struct FAnimationUpdateContext;

namespace UE::AnimNext
{
	struct FEvaluationVM;
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FPreEvaluateMixerTasks, const FAnimationUpdateContext&, UE::AnimNext::FEvaluationVM&);

namespace UE::MovieScene
{


class FMovieSceneAnimMixerModule : public IModuleInterface
{
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

}
