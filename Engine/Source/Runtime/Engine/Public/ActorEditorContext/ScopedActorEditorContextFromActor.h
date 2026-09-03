// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"

class AActor;

/**
 * Pushes a new context initialized from the provided actor.
 */
class FScopedActorEditorContextFromActor
{
public:
	ENGINE_API FScopedActorEditorContextFromActor(AActor* InActor);
	ENGINE_API ~FScopedActorEditorContextFromActor();
};

#endif
