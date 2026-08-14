// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"

class UExternalDataLayerAsset;

/**
 * Pushes a copy of the existing context and overrides the current External Data Layer Asset.
 */
class FScopedActorEditorContextSetExternalDataLayerAsset
{
public:
	ENGINE_API FScopedActorEditorContextSetExternalDataLayerAsset(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	ENGINE_API ~FScopedActorEditorContextSetExternalDataLayerAsset();
};

#endif
