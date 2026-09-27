// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureFactoryNode.h"

#if WITH_ENGINE
#include "Engine/TextureCubeArray.h"
#endif


#include "InterchangeTextureCubeArrayFactoryNode.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UInterchangeTextureCubeArrayFactoryNode : public UInterchangeTextureFactoryNode
{
	GENERATED_BODY()

private:
	virtual UClass* GetObjectClass() const override
	{
		return UTextureCubeArray::StaticClass();
	}
};
