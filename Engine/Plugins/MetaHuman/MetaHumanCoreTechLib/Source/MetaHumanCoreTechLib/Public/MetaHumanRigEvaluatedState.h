// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Math/Vector.h"

/**
* Helper struct to contain evaluated state for either the face or the body model
*/
struct FMetaHumanRigEvaluatedState
{
	TArray<FVector3f> Vertices;
	TArray<FVector3f> VertexNormals;
};

