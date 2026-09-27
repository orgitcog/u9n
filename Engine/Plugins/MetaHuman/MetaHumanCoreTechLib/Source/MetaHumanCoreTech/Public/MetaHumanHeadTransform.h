// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



// Functions to convert a transformation that is applied to a 
// standalone head skel mesh into a transformation to apply to the
// head bone of a full MetaHuman (that uses that head mesh) such
// that the head remains in a constant pose.

class METAHUMANCORETECH_API FMetaHumanHeadTransform
{
public:

	static FTransform MeshToBone(const FTransform& InTransform);
	static FTransform BoneToMesh(const FTransform& InTransform);

	static FTransform HeadToRoot(const FTransform& InTransform);
	static FTransform RootToHead(const FTransform& InTransform);
};
