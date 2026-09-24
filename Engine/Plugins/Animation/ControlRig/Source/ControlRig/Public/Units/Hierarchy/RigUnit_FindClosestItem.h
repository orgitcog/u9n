// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/RigUnit.h"
#include "RigUnit_FindClosestItem.generated.h"

#define UE_API CONTROLRIG_API

USTRUCT(meta = (DisplayName = "Find Closest Item", Category = "Hierarchy", Keywords = "Find,Closest,Item,Transform,Bone,Joint", NodeColor = "0.3 0.1 0.1"))
struct FRigUnit_FindClosestItem : public FRigUnitMutable
{
	GENERATED_BODY()

	FRigUnit_FindClosestItem()
	{
		Point = FVector::ZeroVector;
	}

	RIGVM_METHOD()
		UE_API virtual void Execute() override;

	UPROPERTY(meta = (Input))
	TArray<FRigElementKey> Items;

	UPROPERTY(meta = (Input))
	FVector Point;

	UPROPERTY(meta = (Output, ExpandByDefault))
	FRigElementKey Item;

	// Used to cache the internally used bone index
	UPROPERTY(transient)
	TArray<FCachedRigElement> CachedItems;

};

#undef UE_API
