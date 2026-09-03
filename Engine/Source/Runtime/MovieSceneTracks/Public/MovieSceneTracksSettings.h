// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "MovieSceneTracksSettings.generated.h"

/** Options for some of the Sequencer systems in this module. */
UCLASS(config=EditorPerProjectUserSettings, PerObjectConfig)
class MOVIESCENETRACKS_API UMovieSceneTracksSettings : public UObject
{
	GENERATED_BODY()

public:

	UMovieSceneTracksSettings(const FObjectInitializer& ObjectInitializer);

	/**
	 * Gets whether camera cut tracks should take control of the viewport in SIE, or PIE while ejected from the player controller.
	 */
	bool GetPreviewCameraCutsInSimulate() const { return bPreviewCameraCutsInSimulate; }

	/**
	 * Sets whether camera cut tracks should take control of the viewport in SIE, or PIE while ejected from the player controller.
	 */
	void SetPreviewCameraCutsInSimulate(bool bInPreviewCameraCutsInSimulate);

protected:

	/**
	 * Whether camera cut tracks should take control of the viewport in SIE (Simulate in Editor) or after ejecting
	 * from the player controller while in PIE.
	 */
	UPROPERTY(config, EditAnywhere, Category=General)
	bool bPreviewCameraCutsInSimulate;
};

