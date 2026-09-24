// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"

namespace UE::Chaos::ClothAsset
{
	/** Cloth collection extended schemas. */
	enum class EClothCollectionExtendedSchemas : uint8
	{
		None = 0,
		RenderDeformer = 1 << 1,
		Solvers = 1 << 2,
		Fabrics = 1 << 3,
		SimMorphTargets = 1 << 4,
		CookedOnly = 1 << 5, // If this flag is found, only cooked schemas will be considered.
		Resizing = 1 << 6,
	};
	ENUM_CLASS_FLAGS(EClothCollectionExtendedSchemas)
}  // End namespace UE::Chaos::ClothAsset
