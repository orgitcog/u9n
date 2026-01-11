// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchyTableType.h"

#include "SkeletonHierarchyTableType.generated.h"

class USkeleton;

USTRUCT(DisplayName = "Skeleton")
struct FHierarchyTable_TableType_Skeleton final : public FHierarchyTable_TableType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Default)
	TObjectPtr<USkeleton> Skeleton;
};

UENUM()
enum class ESkeletonHierarchyTable_TablePayloadEntryType
{
	Bone,
	Curve,
	Attribute
};

USTRUCT()
struct FHierarchyTable_TablePayloadType_Skeleton final : public FHierarchyTable_TablePayloadType
{
	GENERATED_BODY()

	UPROPERTY()
	ESkeletonHierarchyTable_TablePayloadEntryType EntryType = ESkeletonHierarchyTable_TablePayloadEntryType::Bone;
};