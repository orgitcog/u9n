// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAnimationEditorViewport.h"
#include "AnimViewportContext.generated.h"

UCLASS()
class UAnimViewportContext : public UObject
{
	GENERATED_BODY()

public:
	TWeakPtr<SAnimationEditorViewportTabBody> ViewportTabBody;
	TWeakPtr<IPersonaPreviewScene> PersonaPreviewScene;
};
