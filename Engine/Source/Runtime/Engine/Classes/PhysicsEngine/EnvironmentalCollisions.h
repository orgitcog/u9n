// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EngineDefines.h"

struct FClothCollisionData;
class USceneComponent;

struct FEnvironmentalCollisions
{
	static ENGINE_API void AppendCollisionDataFromEnvironment(const USceneComponent* SceneComponent, FClothCollisionData& CollisionData);
};
