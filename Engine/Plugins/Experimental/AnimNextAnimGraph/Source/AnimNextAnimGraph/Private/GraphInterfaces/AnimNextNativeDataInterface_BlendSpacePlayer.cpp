// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphInterfaces/AnimNextNativeDataInterface_BlendSpacePlayer.h"
#include "Animation/BlendSpace.h"

void FAnimNextNativeDataInterface_BlendSpacePlayer::BindToFactoryObject(const FBindToFactoryObjectContext& InContext)
{
	BlendSpace = CastChecked<UBlendSpace>(InContext.FactoryObject);
}
