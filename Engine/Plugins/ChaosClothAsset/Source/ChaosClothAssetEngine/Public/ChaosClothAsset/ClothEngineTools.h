// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Math/Vector2D.h"

#define UE_API CHAOSCLOTHASSETENGINE_API

struct FManagedArrayCollection;
class FName;
class FSkeletalMeshLODModel;

namespace UE::Chaos::ClothAsset
{
	/**
	 *  Tools operating on cloth collections with Engine dependency
	 */
	struct FClothEngineTools
	{
		/** Generate tether data. */
		static UE_API void GenerateTethers(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FName& WeightMap, const bool bGeodesicTethers, const FVector2f& MaxDistanceValue = FVector2f(0.f, 1.f));
		static UE_API void GenerateTethersFromSelectionSet(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FName& FixedEndSet, const bool bGeodesicTethers);
		/** @param CustomTetherEndSets: First element of each pair is DynamicSet, second is FixedSet */
		static UE_API void GenerateTethersFromCustomSelectionSets(const TSharedRef<FManagedArrayCollection>& ClothCollection, const FName& FixedEndSet, const TArray<TPair<FName, FName>>& CustomTetherEndSets, const bool bGeodesicTethers);
	};
}  // End namespace UE::Chaos::ClothAsset

#undef UE_API
