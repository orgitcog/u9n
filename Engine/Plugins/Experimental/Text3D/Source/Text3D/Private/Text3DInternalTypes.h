// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "UObject/NameTypes.h"

namespace UE::Text3D::Materials
{
	static const TArray<FLazyName> SlotNames =
	{
		TEXT("Front"),
		TEXT("Bevel"),
		TEXT("Extrude"),
		TEXT("Back")
	};
}