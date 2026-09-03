// Copyright Epic Games, Inc. All Rights Reserved.

#include "LandscapeEditLayerMergeContext.h"
#include "Algo/AllOf.h"
#include "Algo/Transform.h"
#include "Landscape.h"
#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"
#include "Materials/MaterialExpressionLandscapeVisibilityMask.h"

namespace UE::Landscape::EditLayers
{

#if WITH_EDITOR

FMergeContext::FMergeContext(ALandscape* InLandscape, bool bInIsHeightmapMerge, bool bInSkipProceduralRenderers)
	: bIsHeightmapMerge(bInIsHeightmapMerge)
	, bSkipProceduralRenderers(bInSkipProceduralRenderers)
	, Landscape(InLandscape)
	, LandscapeInfo(InLandscape->GetLandscapeInfo())
{
	check(LandscapeInfo != nullptr);

	// Start by gathering all possible target layer names on this landscape: 
	//  This list of all unique target layer names will help accelerate the gathering of output layers on each component (using bit arrays) as well as the target layers intersection tests:
	if (bInIsHeightmapMerge)
	{
		// Only one target layer in the case of heightmap: 
		const FName TargetLayerName = FName("Height");
		AllTargetLayerNames = { TargetLayerName };
		// And it's valid :
		ValidTargetLayerBitIndices = TBitArray<>(true, 1);
		VisibilityTargetLayerMask = TBitArray<>(false, 1);
		AllWeightmapLayerInfos = { nullptr };
	}
	else
	{
		// Gather all target layer names and mark those that are valid layers :
		for (const FLandscapeInfoLayerSettings& LayerSettings : LandscapeInfo->Layers)
		{
			check(!LayerSettings.LayerName.IsNone());
			ULandscapeLayerInfoObject*& LayerInfo = AllWeightmapLayerInfos.Add_GetRef(LayerSettings.LayerInfoObj);
			AllTargetLayerNames.Add(LayerSettings.LayerName);
			ValidTargetLayerBitIndices.Add(LayerInfo != nullptr);
		}

		// Visibility is always a valid layer : 
		VisibilityTargetLayerIndex = AllTargetLayerNames.Find(UMaterialExpressionLandscapeVisibilityMask::ParameterName);
		if (VisibilityTargetLayerIndex == INDEX_NONE)
		{
			VisibilityTargetLayerIndex = AllTargetLayerNames.Num();
			AllTargetLayerNames.Add(UMaterialExpressionLandscapeVisibilityMask::ParameterName);
			check(!AllWeightmapLayerInfos.Contains(ALandscape::VisibilityLayer));
			AllWeightmapLayerInfos.Add(ALandscape::VisibilityLayer);
			check(ALandscape::VisibilityLayer != nullptr);
			ValidTargetLayerBitIndices.Add(true);
		}
		check(VisibilityTargetLayerIndex != INDEX_NONE);

		VisibilityTargetLayerMask = TBitArray<>(false, AllTargetLayerNames.Num());
		VisibilityTargetLayerMask[VisibilityTargetLayerIndex] = true;
	}

	NegatedVisibilityTargetLayerMask = VisibilityTargetLayerMask;
	NegatedVisibilityTargetLayerMask.BitwiseNOT();
}

TArray<FName> FMergeContext::GetValidTargetLayerNames() const
{
	return bIsHeightmapMerge ? AllTargetLayerNames : ConvertTargetLayerBitIndicesToNames(ValidTargetLayerBitIndices);
}

int32 FMergeContext::IsValidTargetLayerName(const FName& InName) const
{
	int32 Index = GetTargetLayerIndexForName(InName);
	return (Index != INDEX_NONE) ? ValidTargetLayerBitIndices[Index] : false;
}

int32 FMergeContext::IsValidTargetLayerNameChecked(const FName& InName) const
{
	int32 Index = AllTargetLayerNames.Find(InName);
	check(Index != INDEX_NONE);
	return ValidTargetLayerBitIndices[Index];
}

int32 FMergeContext::IsTargetLayerIndexValid(int32 InIndex) const
{
	return AllTargetLayerNames.IsValidIndex(InIndex);
}

int32 FMergeContext::GetTargetLayerIndexForName(const FName& InName) const
{
	return AllTargetLayerNames.Find(InName);
}

int32 FMergeContext::GetTargetLayerIndexForNameChecked(const FName& InName) const
{
	int32 Index = AllTargetLayerNames.Find(InName);
	check(Index != INDEX_NONE);
	return Index;
}

FName FMergeContext::GetTargetLayerNameForIndex(int32 InIndex) const
{
	return AllTargetLayerNames.IsValidIndex(InIndex) ? AllTargetLayerNames[InIndex] : NAME_None;
}

FName FMergeContext::GetTargetLayerNameForIndexChecked(int32 InIndex) const
{
	check(AllTargetLayerNames.IsValidIndex(InIndex));
	return AllTargetLayerNames[InIndex];
}

int32 FMergeContext::GetTargetLayerIndexForLayerInfo(ULandscapeLayerInfoObject* InLayerInfo) const
{
	return AllWeightmapLayerInfos.Find(InLayerInfo);
}

int32 FMergeContext::GetTargetLayerIndexForLayerInfoChecked(ULandscapeLayerInfoObject* InLayerInfo) const
{
	int32 Index = AllWeightmapLayerInfos.Find(InLayerInfo);;
	check(Index != INDEX_NONE);
	return Index;
}

ULandscapeLayerInfoObject* FMergeContext::GetTargetLayerInfoForName(const FName& InName) const
{
	int32 Index = GetTargetLayerIndexForName(InName);
	return (Index != INDEX_NONE) ? AllWeightmapLayerInfos[Index] : nullptr;
}

ULandscapeLayerInfoObject* FMergeContext::GetTargetLayerInfoForNameChecked(const FName& InName) const
{
	int32 Index = GetTargetLayerIndexForNameChecked(InName);
	return AllWeightmapLayerInfos[Index];
}

ULandscapeLayerInfoObject* FMergeContext::GetTargetLayerInfoForIndex(int32 InIndex) const
{
	check(AllTargetLayerNames.IsValidIndex(InIndex));
	return AllWeightmapLayerInfos[InIndex];
}

TBitArray<> FMergeContext::ConvertTargetLayerNamesToBitIndices(TConstArrayView<FName> InTargetLayerNames) const
{
	TBitArray<> Result(false, AllTargetLayerNames.Num());
	for (FName Name : InTargetLayerNames)
	{
		if (int32 Index = GetTargetLayerIndexForName(Name); Index != INDEX_NONE)
		{
			Result[Index] = true;
		}
	}
	return Result;
}

TBitArray<> FMergeContext::ConvertTargetLayerNamesToBitIndicesChecked(TConstArrayView<FName> InTargetLayerNames) const
{
	TBitArray<> Result(false, AllTargetLayerNames.Num());
	for (FName Name : InTargetLayerNames)
	{
		int32 Index = GetTargetLayerIndexForNameChecked(Name);
		Result[Index] = true;
	}
	return Result;
}

TArray<FName> FMergeContext::ConvertTargetLayerBitIndicesToNames(const TBitArray<>& InTargetLayerBitIndices) const
{
	const int32 NumNames = AllTargetLayerNames.Num();
	check(InTargetLayerBitIndices.Num() == NumNames);
	TArray<FName> Names;
	Names.Reserve(NumNames);
	for (TConstSetBitIterator It(InTargetLayerBitIndices); It; ++It)
	{
		Names.Add(AllTargetLayerNames[It.GetIndex()]);
	}
	return Names;
}

TArray<ULandscapeLayerInfoObject*> FMergeContext::ConvertTargetLayerBitIndicesToLayerInfos(const TBitArray<>& InTargetLayerBitIndices) const
{
	const int32 NumTargetLayerInfos = AllTargetLayerNames.Num();
	check(InTargetLayerBitIndices.Num() == NumTargetLayerInfos);
	TArray<ULandscapeLayerInfoObject*> LayerInfos;
	LayerInfos.Reserve(NumTargetLayerInfos);
	for (TConstSetBitIterator It(InTargetLayerBitIndices); It; ++It)
	{
		LayerInfos.Add(AllWeightmapLayerInfos[It.GetIndex()]);
	}
	return LayerInfos;
}

void FMergeContext::ForEachTargetLayer(const TBitArray<>& InTargetLayerBitIndices, TFunctionRef<bool(int32 /*InTargetLayerIndex*/, const FName& /*InTargetLayerName*/, ULandscapeLayerInfoObject* /*InWeightmapLayerInfo*/)> Fn) const
{
	check(InTargetLayerBitIndices.Num() == AllTargetLayerNames.Num());
	for (TConstSetBitIterator It(InTargetLayerBitIndices); It; ++It)
	{
		const int32 TargetLayerIndex = It.GetIndex();
		if (!AllTargetLayerNames.IsValidIndex(TargetLayerIndex))
		{
			return;
		}

		if (!Fn(TargetLayerIndex, AllTargetLayerNames[TargetLayerIndex], AllWeightmapLayerInfos[TargetLayerIndex]))
		{
			return;
		}
	}
}

void FMergeContext::ForEachValidTargetLayer(TFunctionRef<bool(int32 /*InTargetLayerIndex*/, const FName& /*InTargetLayerName*/, ULandscapeLayerInfoObject* /*InWeightmapLayerInfo*/)> Fn) const
{
	return ForEachTargetLayer(ValidTargetLayerBitIndices, Fn);
}

TBitArray<> FMergeContext::BuildTargetLayerBitIndices(bool bInBitValue) const
{
	return TBitArray<>(bInBitValue, AllTargetLayerNames.Num());
}

#endif // WITH_EDITOR

} // namespace UE::Landscape::EditLayers
