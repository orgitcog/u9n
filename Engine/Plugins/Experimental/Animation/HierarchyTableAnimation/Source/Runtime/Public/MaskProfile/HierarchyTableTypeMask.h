// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableType.h"

#include "HierarchyTableTypeMask.generated.h"

USTRUCT(DisplayName = "Mask")
struct FHierarchyTable_ElementType_Mask final : public FHierarchyTable_ElementType
{
	GENERATED_BODY()

	FHierarchyTable_ElementType_Mask()
		: Value(1.0f)
	{
	}

	UPROPERTY()
	float Value;
};