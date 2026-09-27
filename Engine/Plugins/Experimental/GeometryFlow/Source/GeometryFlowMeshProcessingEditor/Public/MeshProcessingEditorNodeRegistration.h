// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
	namespace GeometryFlow
	{

		struct FMeshProcessingEditorNodeRegistration
		{
			// Registration is required for serialization.
			GEOMETRYFLOWMESHPROCESSINGEDITOR_API static void  RegisterNodes();
		};
	}
}