// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/ManagedArrayCollection.h"
#include "Styling/SlateColor.h"

class SWidget;

namespace UE::Dataflow::CollectionSpreadSheetHelpers
{
	struct FAttrInfo
	{
		FName Name;
		FString Type;
	};

	static TMap<FString, int32> AttrTypeWidthMap =
	{
		{ TEXT("Transform"),	600 },
		{ TEXT("Transform3f"),	600 },
		{ TEXT("String"),		200 },
		{ TEXT("LinearColor"),	80 },
		{ TEXT("int32"),		100 },
		{ TEXT("IntArray"),		200 },
		{ TEXT("Vector"),		250 },
		{ TEXT("Vector2D"),		160 },
		{ TEXT("Float"),		150 },
		{ TEXT("IntVector"),	220 },
		{ TEXT("Bool"),			75 },
		{ TEXT("Box"),			550 },
		{ TEXT("MeshSection"),	100 },
		{ TEXT("UInt8"),		100 },
		{ TEXT("Guid"),			350 },
	};

	FString AttributeValueToString(float Value);

	FString AttributeValueToString(int32 Value);

	FString AttributeValueToString(FString Value);

	FString AttributeValueToString(FLinearColor Value);

	FString AttributeValueToString(FVector Value);

	FString AttributeValueToString(bool Value);

	FString AttributeValueToString(const FConstBitReference& Value);

	FString AttributeValueToString(TSet<int32> Value);

	FString AttributeValueToString(FTransform3f Value);

	FString AttributeValueToString(FTransform Value);

	FString AttributeValueToString(FBox Value);

	FString AttributeValueToString(FIntVector Value);

	FString AttributeValueToString(FIntVector4 Value);

	FString AttributeValueToString(FGuid Value);

	FString AttributeValueToString(Chaos::FConvexPtr Value);

	FString AttributeValueToString(const FManagedArrayCollection& InCollection, const FName& InAttributeName, const FName& InGroupName, int32 InIdxColumn);

	inline FName GetArrayTypeString(FManagedArrayCollection::EArrayType ArrayType)
	{
		switch (ArrayType)
		{
#define MANAGED_ARRAY_TYPE(a,A)	case EManagedArrayType::F##A##Type:\
		return FName(#A);
#include "GeometryCollection/ManagedArrayTypeValues.inl"
#undef MANAGED_ARRAY_TYPE
		}
		return FName();
	}

	FColor GetColorPerDepth(uint32 Depth);

	FSlateColor UpdateItemColorFromCollection(const TSharedPtr<FManagedArrayCollection> InCollection, const FName InGroup, const int32 InItemIndex);

	TSharedRef<SWidget> MakeColumnWidget(const TSharedPtr<FManagedArrayCollection> InCollection,
		const FName InGroup,
		const FName InAttr,
		const int32 InItemIndex,
		const FSlateColor InItemColor);
}


