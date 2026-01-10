// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HierarchicalHashGrid2D.h"
#include "SmartObjectTypes.h"
#include "SmartObjectHashGrid.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

struct FInstancedStruct;
struct FStructView;

struct FSmartObjectHandle;

typedef THierarchicalHashGrid2D<2, 4, FSmartObjectHandle> FSmartObjectHashGrid2D;

USTRUCT()
struct FSmartObjectHashGridEntryData : public FSmartObjectSpatialEntryData
{
	GENERATED_BODY()

	FSmartObjectHashGrid2D::FCellLocation CellLoc;
};

UCLASS(MinimalAPI)
class USmartObjectHashGrid : public USmartObjectSpacePartition
{
	GENERATED_BODY()

protected:
	UE_API virtual void Add(const FSmartObjectHandle Handle, const FBox& Bounds, FInstancedStruct& OutHandle) override;
	UE_API virtual void Remove(const FSmartObjectHandle Handle, FStructView EntryData) override;
	UE_API virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) override;

#if UE_ENABLE_DEBUG_DRAWING
	UE_API virtual void Draw(FDebugRenderSceneProxy* DebugProxy) override;
#endif
private:
	FSmartObjectHashGrid2D HashGrid;
};

#undef UE_API
