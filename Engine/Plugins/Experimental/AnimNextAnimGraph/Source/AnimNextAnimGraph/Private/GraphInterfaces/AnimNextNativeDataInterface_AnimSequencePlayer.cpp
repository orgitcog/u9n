// Copyright Epic Games, Inc. All Rights Reserved.

#include "GraphInterfaces/AnimNextNativeDataInterface_AnimSequencePlayer.h"
#include "Animation/AnimSequence.h"

void FAnimNextNativeDataInterface_AnimSequencePlayer::BindToFactoryObject(const FBindToFactoryObjectContext& InContext)
{
	AnimSequence = CastChecked<UAnimSequence>(InContext.FactoryObject);
}
