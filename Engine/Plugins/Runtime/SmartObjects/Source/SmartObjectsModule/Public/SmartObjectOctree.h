// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/GenericOctreePublic.h"
#include "SmartObjectTypes.h"
#include "Math/GenericOctree.h"
#include "SmartObjectOctree.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

struct FInstancedStruct;
struct FStructView;

typedef TSharedRef<struct FSmartObjectOctreeID, ESPMode::ThreadSafe> FSmartObjectOctreeIDSharedRef;

struct FSmartObjectOctreeID : public TSharedFromThis<FSmartObjectOctreeID, ESPMode::ThreadSafe>
{
	FOctreeElementId2 ID;
};

struct FSmartObjectOctreeElement
{
	FBoxCenterAndExtent Bounds;
	FSmartObjectHandle SmartObjectHandle;
	FSmartObjectOctreeIDSharedRef SharedOctreeID;

	UE_API FSmartObjectOctreeElement(const FBoxCenterAndExtent& Bounds, const FSmartObjectHandle SmartObjectHandle, const FSmartObjectOctreeIDSharedRef& SharedOctreeID);
};

struct FSmartObjectOctreeSemantics
{
	enum { MaxElementsPerLeaf = 16 };
	enum { MinInclusiveElementsPerNode = 7 };
	enum { MaxNodeDepth = 12 };

	typedef TInlineAllocator<MaxElementsPerLeaf> ElementAllocator;

	FORCEINLINE static const FBoxCenterAndExtent& GetBoundingBox(const FSmartObjectOctreeElement& Element)
	{
		return Element.Bounds;
	}

	FORCEINLINE static bool AreElementsEqual(const FSmartObjectOctreeElement& A, const FSmartObjectOctreeElement& B)
	{
		return A.SmartObjectHandle == B.SmartObjectHandle;
	}

	static void SetElementId(const FSmartObjectOctreeElement& Element, FOctreeElementId2 Id);
};

struct FSmartObjectOctree : TOctree2<FSmartObjectOctreeElement, FSmartObjectOctreeSemantics>
{
public:
	FSmartObjectOctree();
	FSmartObjectOctree(const FVector& Origin, FVector::FReal Radius);
	virtual ~FSmartObjectOctree();

	/** Add new node and initialize using SmartObject runtime data */
	void AddNode(const FBoxCenterAndExtent& Bounds, const FSmartObjectHandle SmartObjectHandle, const FSmartObjectOctreeIDSharedRef& SharedOctreeID);
	
	/** Updates element bounds remove/add operation */
	void UpdateNode(const FOctreeElementId2& Id, const FBox& NewBounds);

	/** Remove node */
	void RemoveNode(const FOctreeElementId2& Id);
};

USTRUCT()
struct FSmartObjectOctreeEntryData : public FSmartObjectSpatialEntryData
{
	GENERATED_BODY()

	FSmartObjectOctreeEntryData() : SharedOctreeID(MakeShareable(new FSmartObjectOctreeID())) {}

	FSmartObjectOctreeIDSharedRef SharedOctreeID;
};

UCLASS(MinimalAPI)
class USmartObjectOctree : public USmartObjectSpacePartition
{
	GENERATED_BODY()

protected:
	UE_API virtual void Add(const FSmartObjectHandle Handle, const FBox& Bounds, FInstancedStruct& OutHandle) override;
	UE_API virtual void Remove(const FSmartObjectHandle Handle, FStructView EntryData) override;
	UE_API virtual void Find(const FBox& QueryBox, TArray<FSmartObjectHandle>& OutResults) override;
	UE_API virtual void SetBounds(const FBox& Bounds) override;

private:
	FSmartObjectOctree SmartObjectOctree;
};

#undef UE_API
