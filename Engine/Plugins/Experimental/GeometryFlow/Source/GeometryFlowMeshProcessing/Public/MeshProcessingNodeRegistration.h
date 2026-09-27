// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
namespace GeometryFlow
{

	struct FMeshProcessingNodeRegistration
	{
		// Required registration of the geometry flow graph nodes in the MeshProcessing Module.
		GEOMETRYFLOWMESHPROCESSING_API static void RegisterNodes();
	};


}
}