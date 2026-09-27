// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableType.h"

#include "HierarchyTableDefaultTypes.generated.h"

USTRUCT(DisplayName = "Float")
struct FHierarchyTable_ElementType_Float final : public FHierarchyTable_ElementType
{
	GENERATED_BODY()

	FHierarchyTable_ElementType_Float()
		: Value(0.0)
	{
	}

	UPROPERTY()
	float Value;
};