// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "PSDQuadsFactory.generated.h"

class APSDQuadActor;
class APSDQuadMeshActor;
class UPSDDocument;
class UWorld;

UCLASS()
class UPSDQuadsFactory : public UObject
{
	GENERATED_BODY()

public:
	virtual ~UPSDQuadsFactory() override = default;

	APSDQuadActor* CreateQuadActor(UWorld& InWorld, UPSDDocument& InDocument) const;

	void CreateQuads(APSDQuadActor& InQuadActor) const;

protected:
	APSDQuadMeshActor* CreateQuad(APSDQuadActor& InQuadActor, int32 InLayerIndex) const;
};
