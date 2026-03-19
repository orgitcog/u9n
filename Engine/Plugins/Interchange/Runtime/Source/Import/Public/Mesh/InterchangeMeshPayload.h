// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"

namespace UE
{
	namespace Interchange
	{
		struct FMeshPayloadData
		{
			/* MESH */
			//Currently the skeletalmesh payload data is editor only, we have to move to something available at runtime
			FMeshDescription MeshDescription;

			/* SKELETAL */
			//This map the indice use in the meshdescription to the bone name, so we can use this information to remap properly the skinning when we merge the meshdescription
			TArray<FString> JointNames;

			/* MORPH */
			//We don't have to store GlobalTransform here anymore, since The Mesh node parent bake transform was pass to the payload request.
			//The vertex offset of the morph target in case we combine mesh node together
			int32 VertexOffset;
			//The name of the morph target
			FString MorphTargetName;
		};

		struct FMeshPayload
		{
			FString MeshName;
			TOptional<UE::Interchange::FMeshPayloadData> PayloadData;
			FTransform Transform = FTransform::Identity;
		};
	}//ns Interchange
}//ns UE
