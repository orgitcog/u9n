// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

struct FAnimationUpdateContext;

namespace UE::AnimNext
{
	struct FEvaluationVM;
}

namespace UE::MovieScene
{

class FMovieScenePoseSearchTracksModule : public IModuleInterface
{
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

}
