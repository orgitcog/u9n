// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGCommon.h"
#include "PCGSpatialData.h"

#include "Utils/PCGPointOctree.h"
#include "Utils/PCGValueRange.h"

#include "PCGBasePointData.generated.h"

#define UE_API PCG_API

namespace PCGPointDataConstants
{
	const FName ActorReferenceAttribute = TEXT("ActorReference");
	const FName ElementsDomainName = "Points";
}

UCLASS(MinimalAPI, Abstract, BlueprintType, ClassGroup = (Procedural))
class UPCGBasePointData : public UPCGSpatialData
{
	GENERATED_BODY()

public:
	UE_API UPCGBasePointData(const FObjectInitializer& ObjectInitializer);

	// Get the functions to the accessor factory
	static UE_API FPCGAttributeAccessorMethods GetPointAccessorMethods();

	//~Begin UObject Interface
	UE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~End UObject Interface

	// ~Begin UPCGSpatialData interface
	virtual int GetDimension() const override { return 0; }
	UE_API virtual bool SamplePoint(const FTransform& Transform, const FBox& Bounds, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	UE_API virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata) const override;
	// ~End UPCGSpatialData interface
		
	// ~Begin UPCGData Interface
	
	/** Make a pass on Metadata to flatten parenting and only keep entries used by points. */
	UE_API virtual void Flatten() override;
	virtual bool SupportsFullDataCrc() const override { return true; }
	UE_API virtual void AddToCrc(FArchiveCrc32& Ar, bool bFullDataCrc) const override;

	/** Metadata specific */
	virtual FPCGMetadataDomainID GetDefaultMetadataDomainID() const override { return PCGMetadataDomainID::Elements; }
	virtual TArray<FPCGMetadataDomainID> GetAllSupportedMetadataDomainIDs() const override { return {PCGMetadataDomainID::Data, PCGMetadataDomainID::Elements}; }
	UE_API virtual FPCGMetadataDomainID GetMetadataDomainIDFromSelector(const FPCGAttributePropertySelector& InSelector) const override;
	UE_API virtual bool SetDomainFromDomainID(const FPCGMetadataDomainID& InDomainID, FPCGAttributePropertySelector& InOutSelector) const;
	// ~End UPCGData Interface

	UE_API virtual bool ProjectPoint(const FTransform& InTransform, const FBox& InBounds, const FPCGProjectionParams& InParams, FPCGPoint& OutPoint, UPCGMetadata* OutMetadata, bool bUseBounds) const;

	/** Initializes a single point based on the given actor */
	UE_API void InitializeFromActor(AActor* InActor, bool* bOutOptionalSanitizedTagAttributeName = nullptr);

	/** Adds a single point based on the given actor */
	UE_API void AddSinglePointFromActor(AActor* InActor, bool* bOutOptionalSanitizedTagAttributeName = nullptr);

	UE_API virtual bool IsValidRef(const PCGPointOctree::FPointRef& InPointRef) const PURE_VIRTUAL(UPCGBasePointData::IsValidRef, return false;);
	
	virtual EPCGDataType GetDataType() const override { return EPCGDataType::Point; }

	UE_API virtual int32 GetNumPoints() const PURE_VIRTUAL(UPCGBasePointData::GetNumPoints, return 0;);
	bool IsEmpty() const { return GetNumPoints() == 0; }
	UE_API virtual void SetNumPoints(int32 InNumPoints, bool bInitializeValues = true) PURE_VIRTUAL(UPCGBasePointData::SetNumPoints, return;);

	virtual void AllocateProperties(EPCGPointNativeProperties Properties) {}
	virtual void FreeProperties(EPCGPointNativeProperties Properties) {}
	UE_API virtual void MoveRange(int32 RangeStartIndex, int32 MoveToIndex, int32 NumElements) PURE_VIRTUAL(UPCGBasePointData::MoveRange, return;);
	virtual void CopyUnallocatedPropertiesFrom(const UPCGBasePointData* InPointData) {}

	UE_API virtual TArray<FTransform> GetTransformsCopy() const PURE_VIRTUAL(UPCGBasePointData::GetTransformsCopy, return TArray<FTransform>(););
	
	/**
	 * GetValueRange / GetConstValueRange / Get*ValueRange / GetConst*ValueRange
	 * 
	 * Returns a value range that can be iterated and read from / written to abstracting the underlying point data structure.
	 * 
	 * Calling SetNumPoints/AllocateProperties/FreeProperties can and will probably invalidate ranges so make sure that you do those operations first or that you get a new range after you do.
	 * 
	 * @param bAllocate In the case where we return a non-const range by default the memory will be allocated but in some specific case we might not want to allocate (for SingleValue() ranges)
	 */
	template <typename T>
	TPCGValueRange<T> GetValueRange(EPCGPointNativeProperties NativeProperty, bool bAllocate = true);

	UE_API virtual TPCGValueRange<FTransform> GetTransformValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetTransformValueRange, return TPCGValueRange<FTransform>(););
	UE_API virtual TPCGValueRange<float> GetDensityValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetDensityValueRange, return TPCGValueRange<float>(););
	UE_API virtual TPCGValueRange<FVector> GetBoundsMinValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetBoundsMinValueRange, return TPCGValueRange<FVector>(););
	UE_API virtual TPCGValueRange<FVector> GetBoundsMaxValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetBoundsMaxValueRange, return TPCGValueRange<FVector>(););
	UE_API virtual TPCGValueRange<FVector4> GetColorValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetColorValueRange, return TPCGValueRange<FVector4>(););
	UE_API virtual TPCGValueRange<float> GetSteepnessValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetSteepnessValueRange, return TPCGValueRange<float>(););
	UE_API virtual TPCGValueRange<int32> GetSeedValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetSeedValueRange, return TPCGValueRange<int32>(););
	UE_API virtual TPCGValueRange<int64> GetMetadataEntryValueRange(bool bAllocate = true) PURE_VIRTUAL(UPCGBasePointData::GetMetadataEntryValueRange, return TPCGValueRange<int64>(););

	template <typename T>
	TConstPCGValueRange<T> GetConstValueRange(EPCGPointNativeProperties NativeProperty) const;

	UE_API virtual TConstPCGValueRange<FTransform> GetConstTransformValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstTransformValueRange, return TConstPCGValueRange<FTransform>(););
	UE_API virtual TConstPCGValueRange<float> GetConstDensityValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstDensityValueRange, return TConstPCGValueRange<float>(););
	UE_API virtual TConstPCGValueRange<FVector> GetConstBoundsMinValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstBoundsMinValueRange, return TConstPCGValueRange<FVector>(););
	UE_API virtual TConstPCGValueRange<FVector> GetConstBoundsMaxValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstBoundsMaxValueRange, return TConstPCGValueRange<FVector>(););
	UE_API virtual TConstPCGValueRange<FVector4> GetConstColorValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstColorValueRange, return TConstPCGValueRange<FVector4>(););
	UE_API virtual TConstPCGValueRange<float> GetConstSteepnessValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstSteepnessValueRange, return TConstPCGValueRange<float>(););
	UE_API virtual TConstPCGValueRange<int32> GetConstSeedValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstSeedValueRange, return TConstPCGValueRange<int32>(););
	UE_API virtual TConstPCGValueRange<int64> GetConstMetadataEntryValueRange() const PURE_VIRTUAL(UPCGBasePointData::GetConstMetadataEntryValueRange, return TConstPCGValueRange<int64>(););
		
	UE_API virtual void SetTransform(const FTransform& InTransform);
	UE_API virtual void SetDensity(float InDensity);
	UE_API virtual void SetBoundsMin(const FVector& InBoundsMin);
	UE_API virtual void SetBoundsMax(const FVector& InBoundsMax);
	UE_API virtual void SetColor(const FVector4& InColor);
	UE_API virtual void SetSteepness(float InSteepness);
	UE_API virtual void SetSeed(int32 InSeed);
	UE_API virtual void SetMetadataEntry(int64 InMetadataEntry);

	UE_API virtual void SetExtents(const FVector& InExtents);
	UE_API virtual void SetLocalCenter(const FVector& InLocalCenter);

	UE_API virtual const FTransform& GetTransform(int32 InPointIndex) const;
	UE_API virtual float GetDensity(int32 InPointIndex) const;
	UE_API virtual const FVector& GetBoundsMin(int32 InPointIndex) const;
	UE_API virtual const FVector& GetBoundsMax(int32 InPointIndex) const;
	UE_API virtual const FVector4& GetColor(int32 InPointIndex) const;
	UE_API virtual float GetSteepness(int32 InPointIndex) const;
	UE_API virtual int32 GetSeed(int32 InPointIndex) const;
	UE_API virtual int64 GetMetadataEntry(int32 InPointIndex) const;

	UE_API virtual FBoxSphereBounds GetDensityBounds(int32 InPointIndex) const;
	UE_API virtual FBox GetLocalDensityBounds(int32 InPointIndex) const;
	UE_API virtual FBox GetLocalBounds(int32 InPointIndex) const;
	UE_API virtual FVector GetLocalCenter(int32 InPointIndex) const;
	UE_API virtual FVector GetExtents(int32 InPointIndex) const;
	UE_API virtual FVector GetScaledExtents(int32 InPointIndex) const;
	UE_API virtual FVector GetLocalSize(int32 InPointIndex) const;
	UE_API virtual FVector GetScaledLocalSize(int32 InPointIndex) const;
		
	/** Get the dirty status of the Octree. Note that the Point Octree can be rebuilt from another thread, so this info can be invalidated at anytime. */
	virtual bool IsPointOctreeDirty() const { return bOctreeIsDirty; }
	UE_API virtual const PCGPointOctree::FPointOctree& GetPointOctree() const;
	UE_API virtual FBox GetBounds() const override;
		
	UFUNCTION(BlueprintCallable, Category = SpatialData, meta = (DisplayName="Set Points From"))
	UE_API void BP_SetPointsFrom(const UPCGBasePointData* InData, const TArray<int32>& InDataIndices);
	
	UE_API void SetPointsFrom(const UPCGBasePointData* InData, const TArrayView<const int32>& InDataIndices);

	UE_API virtual void CopyPropertiesTo(UPCGBasePointData* To, int32 ReadStartIndex, int32 WriteStartIndex, int32 Count, EPCGPointNativeProperties Properties) const;
	UE_API virtual void CopyPropertiesTo(UPCGBasePointData* To, const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices, EPCGPointNativeProperties Properties) const;
	UE_API virtual void CopyPointsTo(UPCGBasePointData* To, int32 ReadStartIndex, int32 WriteStartIndex, int32 Count) const;
	UE_API virtual void CopyPointsTo(UPCGBasePointData* To, const TArrayView<const int32>& ReadIndices, const TArrayView<const int32>& WriteIndices) const;
	virtual EPCGPointNativeProperties GetAllocatedProperties(bool bWithInheritance = true) const { return EPCGPointNativeProperties::All; }

	static UE_API void SetPoints(const UPCGBasePointData* From, UPCGBasePointData* To, const TArrayView<const int32>& InDataIndices, bool bCopyAll);

	static EPCGPointNativeProperties GetPropertiesToAllocateFromPointData(const TArray<const UPCGBasePointData*>& PointDatas)
	{
		// Start by doing a union of all input allocated properties
		EPCGPointNativeProperties PropertiesToAllocate = EPCGPointNativeProperties::None;
		for (const UPCGBasePointData* PointInputData : PointDatas)
		{
			PropertiesToAllocate |= PointInputData->GetAllocatedProperties();
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::Transform) && NeedToAllocateSingleValueProperty<FTransform>(EPCGPointNativeProperties::Transform, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::Transform;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::Density) && NeedToAllocateSingleValueProperty<float>(EPCGPointNativeProperties::Density, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::Density;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::BoundsMin) && NeedToAllocateSingleValueProperty<FVector>(EPCGPointNativeProperties::BoundsMin, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::BoundsMin;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::BoundsMax) && NeedToAllocateSingleValueProperty<FVector>(EPCGPointNativeProperties::BoundsMax, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::BoundsMax;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::Color) && NeedToAllocateSingleValueProperty<FVector4>(EPCGPointNativeProperties::Color, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::Color;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::Steepness) && NeedToAllocateSingleValueProperty<float>(EPCGPointNativeProperties::Steepness, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::Steepness;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::Seed) && NeedToAllocateSingleValueProperty<int32>(EPCGPointNativeProperties::Seed, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::Seed;
		}

		if (!EnumHasAnyFlags(PropertiesToAllocate, EPCGPointNativeProperties::MetadataEntry) && NeedToAllocateSingleValueProperty<int64>(EPCGPointNativeProperties::MetadataEntry, PointDatas))
		{
			PropertiesToAllocate |= EPCGPointNativeProperties::MetadataEntry;
		}

		return PropertiesToAllocate;
	}

protected:

	void RebuildOctreeIfNeeded() const
	{
		if (bOctreeIsDirty)
		{
			RebuildOctree();
		}
	}

	UE_API void RebuildOctree() const;
	
	void RecomputeBoundsIfNeeded() const
	{
		if (bBoundsAreDirty)
		{
			RecomputeBounds();
		}
	}
	
	UE_API void RecomputeBounds() const;

	virtual void DirtyCache()
	{
		bOctreeIsDirty = true;
		bBoundsAreDirty = true;
	}

private:

	template<class T>
	static bool NeedToAllocateSingleValueProperty(EPCGPointNativeProperties InProperty, const TArray<const UPCGBasePointData*>& PointDatas)
	{
		// Here we are comparing single values (non-allocated properties) to see if we have single values that differ between inputs.
		// If they do differ, we need to allocate those properties because the output will have multiple values.
		TOptional<const T> DefaultValue;
		for (int32 i = 0; i < PointDatas.Num(); ++i)
		{
			const TConstPCGValueRange<T> ValueRange = PointDatas[i]->GetConstValueRange<T>(InProperty);
			TOptional<const T> SingleValue = ValueRange.GetSingleValue();
			if (!DefaultValue.IsSet())
			{
				DefaultValue = SingleValue;
			}
			else if (DefaultValue.IsSet() && SingleValue.IsSet() && !PCG::Private::MetadataTraits<T>::Equal(DefaultValue.GetValue(), SingleValue.GetValue()))
			{
				return true;
			}
		}

		return false;
	}

protected:

	mutable FCriticalSection CachedDataLock;
	mutable PCGPointOctree::FPointOctree PCGPointOctree;
	mutable FBox Bounds;

	mutable bool bOctreeIsDirty = true;
	mutable bool bBoundsAreDirty = true;
};

template <typename T>
inline TPCGValueRange<T> UPCGBasePointData::GetValueRange(EPCGPointNativeProperties NativeProperty, bool bAllocate)
{
	if constexpr (std::is_same_v<T, FTransform>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Transform)
		{
			return GetTransformValueRange(bAllocate);
		}
	}
	else if constexpr (std::is_same_v<T, float>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Density)
		{
			return GetDensityValueRange(bAllocate);
		}
		else if (NativeProperty == EPCGPointNativeProperties::Steepness)
		{
			return GetSteepnessValueRange(bAllocate);
		}
	}
	else if constexpr (std::is_same_v<T, FVector>)
	{
		if (NativeProperty == EPCGPointNativeProperties::BoundsMin)
		{
			return GetBoundsMinValueRange(bAllocate);
		}
		else if (NativeProperty == EPCGPointNativeProperties::BoundsMax)
		{
			return GetBoundsMaxValueRange(bAllocate);
		}
	}
	else if constexpr (std::is_same_v<T, FVector4>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Color)
		{
			return GetColorValueRange(bAllocate);
		}
	}
	else if constexpr (std::is_same_v<T, int32>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Seed)
		{
			return GetSeedValueRange(bAllocate);
		}
	}
	else if constexpr (std::is_same_v<T, int64>)
	{
		if (NativeProperty == EPCGPointNativeProperties::MetadataEntry)
		{
			return GetMetadataEntryValueRange(bAllocate);
		}
	}
	
	return TPCGValueRange<T>();
}

template <typename T>
inline TConstPCGValueRange<T> UPCGBasePointData::GetConstValueRange(EPCGPointNativeProperties NativeProperty) const
{
	if constexpr (std::is_same_v<T, FTransform>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Transform)
		{
			return GetConstTransformValueRange();
		}
	}
	else if constexpr (std::is_same_v<T, float>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Density)
		{
			return GetConstDensityValueRange();
		}
		else if (NativeProperty == EPCGPointNativeProperties::Steepness)
		{
			return GetConstSteepnessValueRange();
		}
	}
	else if constexpr (std::is_same_v<T, FVector>)
	{
		if (NativeProperty == EPCGPointNativeProperties::BoundsMin)
		{
			return GetConstBoundsMinValueRange();
		}
		else if (NativeProperty == EPCGPointNativeProperties::BoundsMax)
		{
			return GetConstBoundsMaxValueRange();
		}
	}
	else if constexpr (std::is_same_v<T, FVector4>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Color)
		{
			return GetConstColorValueRange();
		}
	}
	else if constexpr (std::is_same_v<T, int32>)
	{
		if (NativeProperty == EPCGPointNativeProperties::Seed)
		{
			return GetConstSeedValueRange();
		}
	}
	else if constexpr (std::is_same_v <T, int64>)
	{
		if (NativeProperty == EPCGPointNativeProperties::MetadataEntry)
		{
			return GetConstMetadataEntryValueRange();
		}
	}
	
	return TConstPCGValueRange<T>();
}

struct FConstPCGPointValueRanges
{
	FConstPCGPointValueRanges() = default;

	FConstPCGPointValueRanges(const UPCGBasePointData* InBasePointData)
	{
		TransformRange = InBasePointData->GetConstTransformValueRange();
		DensityRange = InBasePointData->GetConstDensityValueRange();
		SteepnessRange = InBasePointData->GetConstSteepnessValueRange();
		BoundsMinRange = InBasePointData->GetConstBoundsMinValueRange();
		BoundsMaxRange = InBasePointData->GetConstBoundsMaxValueRange();
		ColorRange = InBasePointData->GetConstColorValueRange();
		SeedRange = InBasePointData->GetConstSeedValueRange();
		MetadataEntryRange = InBasePointData->GetConstMetadataEntryValueRange();
	}

	FPCGPoint GetPoint(int32 Index) const
	{
		FPCGPoint Point(TransformRange[Index], DensityRange[Index], SeedRange[Index]);
		Point.BoundsMin = BoundsMinRange[Index];
		Point.BoundsMax = BoundsMaxRange[Index];
		Point.Steepness = SteepnessRange[Index];
		Point.Color = ColorRange[Index];
		Point.MetadataEntry = MetadataEntryRange[Index];

		return Point;
	}

	TConstPCGValueRange<FTransform> TransformRange;
	TConstPCGValueRange<float> DensityRange;
	TConstPCGValueRange<float> SteepnessRange;
	TConstPCGValueRange<FVector> BoundsMinRange;
	TConstPCGValueRange<FVector> BoundsMaxRange;
	TConstPCGValueRange<FVector4> ColorRange;
	TConstPCGValueRange<int32> SeedRange;
	TConstPCGValueRange<int64> MetadataEntryRange;
};

struct FPCGPointValueRanges
{
	FPCGPointValueRanges() = default;

	FPCGPointValueRanges(UPCGBasePointData* InBasePointData, bool bAllocate = true)
	{
		EPCGPointNativeProperties AllocatedProperties = InBasePointData->GetAllocatedProperties(/*bWithInheritance=*/false);
		
		// In the very specific case where we have just a single point, that we don't inherit from a spatial parent (Orphan point data with a single point)
		// we can create a range that will allow to write into the default value of the point data.
		const bool bIsSingleOrphanPointData = !bAllocate && InBasePointData->GetNumPoints() == 1 && !InBasePointData->HasSpatialDataParent();
		const bool bMustCreateRange = bAllocate || bIsSingleOrphanPointData;

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::Transform))
		{
			TransformRange = InBasePointData->GetTransformValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::Density))
		{
			DensityRange = InBasePointData->GetDensityValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::Steepness))
		{
			SteepnessRange = InBasePointData->GetSteepnessValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::BoundsMin))
		{
			BoundsMinRange = InBasePointData->GetBoundsMinValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::BoundsMax))
		{
			BoundsMaxRange = InBasePointData->GetBoundsMaxValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::Color))
		{
			ColorRange = InBasePointData->GetColorValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::Seed))
		{
			SeedRange = InBasePointData->GetSeedValueRange(bAllocate);
		}

		if (bMustCreateRange || EnumHasAnyFlags(AllocatedProperties, EPCGPointNativeProperties::MetadataEntry))
		{
			MetadataEntryRange = InBasePointData->GetMetadataEntryValueRange(bAllocate);
		}
	}

	TPCGValueRange<FTransform> TransformRange;
	TPCGValueRange<float> DensityRange;
	TPCGValueRange<float> SteepnessRange;
	TPCGValueRange<FVector> BoundsMinRange;
	TPCGValueRange<FVector> BoundsMaxRange;
	TPCGValueRange<FVector4> ColorRange;
	TPCGValueRange<int32> SeedRange;
	TPCGValueRange<int64> MetadataEntryRange;

	void SetFromPoint(int32 Index, const FPCGPoint& Point)
	{
		if (!TransformRange.IsEmpty())
		{
			TransformRange[Index] = Point.Transform;
		}

		if (!DensityRange.IsEmpty())
		{
			DensityRange[Index] = Point.Density;
		}
		
		if (!SteepnessRange.IsEmpty())
		{
			SteepnessRange[Index] = Point.Steepness;
		}

		if (!BoundsMinRange.IsEmpty())
		{
			BoundsMinRange[Index] = Point.BoundsMin;
		}

		if (!BoundsMaxRange.IsEmpty())
		{
			BoundsMaxRange[Index] = Point.BoundsMax;
		}

		if (!ColorRange.IsEmpty())
		{
			ColorRange[Index] = Point.Color;
		}
		
		if (!SeedRange.IsEmpty())
		{
			SeedRange[Index] = Point.Seed;
		}

		if (!MetadataEntryRange.IsEmpty())
		{
			MetadataEntryRange[Index] = Point.MetadataEntry;
		}
	}
		
	void SetFromValueRanges(int32 WriteIndex, const FConstPCGPointValueRanges& ReadRange, int32 ReadIndex)
	{
		if (!TransformRange.IsEmpty())
		{
			TransformRange[WriteIndex] = ReadRange.TransformRange[ReadIndex];
		}

		if (!DensityRange.IsEmpty())
		{
			DensityRange[WriteIndex] = ReadRange.DensityRange[ReadIndex];
		}

		if (!SteepnessRange.IsEmpty())
		{
			SteepnessRange[WriteIndex] = ReadRange.SteepnessRange[ReadIndex];
		}

		if (!BoundsMinRange.IsEmpty())
		{
			BoundsMinRange[WriteIndex] = ReadRange.BoundsMinRange[ReadIndex];
		}

		if (!BoundsMaxRange.IsEmpty())
		{
			BoundsMaxRange[WriteIndex] = ReadRange.BoundsMaxRange[ReadIndex];
		}

		if (!ColorRange.IsEmpty())
		{
			ColorRange[WriteIndex] = ReadRange.ColorRange[ReadIndex];
		}

		if (!SeedRange.IsEmpty())
		{
			SeedRange[WriteIndex] = ReadRange.SeedRange[ReadIndex];
		}

		if (!MetadataEntryRange.IsEmpty())
		{
			MetadataEntryRange[WriteIndex] = ReadRange.MetadataEntryRange[ReadIndex];
		}
	}
};

#undef UE_API
