// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeTextureFactoryNode.h"

#if WITH_ENGINE
#include "Engine/VolumeTexture.h"
#endif


#include "InterchangeVolumeTextureFactoryNode.generated.h"

UCLASS(MinimalAPI, BlueprintType)
class UInterchangeVolumeTextureFactoryNode : public UInterchangeTextureFactoryNode
{
	GENERATED_BODY()

private:
	virtual UClass* GetObjectClass() const override
	{
		return UVolumeTexture::StaticClass();
	}
};
