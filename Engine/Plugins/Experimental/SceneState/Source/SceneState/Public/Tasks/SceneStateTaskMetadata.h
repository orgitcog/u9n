// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateTaskMetadata.generated.h"

/** Metadata information about the Task. Available only in editor */
USTRUCT()
struct FSceneStateTaskMetadata
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid TaskId;
#endif
};
