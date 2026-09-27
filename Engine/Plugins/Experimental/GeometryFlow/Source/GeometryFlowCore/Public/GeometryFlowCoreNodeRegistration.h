// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE
{
namespace GeometryFlow
{

	struct FCoreNodeRegistration
	{
		// function that registers the core nodes defined in GeometryFlowCore (mostly POD source nodes)
		GEOMETRYFLOWCORE_API static void RegisterNodes();
	};
}
}