// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BoneIndices.h"
#include "Utilities/MeshAttributesArray.h"

typedef FMeshAttributesArray<int32> FGLTFIndexArray;
typedef FMeshAttributesArray<FVector3f> FGLTFPositionArray;
typedef FMeshAttributesArray<FVector3f> FGLTFNormalArray;
typedef FMeshAttributesArray<FVector4f> FGLTFTangentArray;
typedef FMeshAttributesArray<FVector2f> FGLTFUVArray;
typedef FMeshAttributesArray<FColor> FGLTFColorArray;

typedef FMeshAttributesArray<UE::Math::TIntVector4<FBoneIndexType>> FGLTFJointInfluenceArray;
typedef FMeshAttributesArray<UE::Math::TIntVector4<uint16>> FGLTFJointWeightArray;