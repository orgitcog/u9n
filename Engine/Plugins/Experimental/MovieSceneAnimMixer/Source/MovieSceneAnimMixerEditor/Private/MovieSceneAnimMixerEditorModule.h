// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

namespace UE::MovieScene
{
	class FMovieSceneAnimMixerEditorModule : public IModuleInterface
	{
	public:

		// IModuleInterface interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		FDelegateHandle AnimationTrackEditorHandle;
	};
}


