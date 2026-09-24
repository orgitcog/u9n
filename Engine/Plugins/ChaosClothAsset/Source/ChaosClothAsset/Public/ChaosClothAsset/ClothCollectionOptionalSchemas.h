// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Misc/CoreMiscDefines.h"

UE_DEPRECATED_HEADER(5.6, "Use ClothCollectionExtendedSchemas.h instead of ClothCollectionOptionalSchemas.h")

#include "ChaosClothAsset/ClothCollectionExtendedSchemas.h"

namespace UE::Chaos::ClothAsset
{
	UE_DEPRECATED(5.6, "Use EClothCollectionExtendedSchemas instead") typedef enum EClothCollectionExtendedSchemas EClothCollectionOptionalSchemas;
}  // End namespace UE::Chaos::ClothAsset
