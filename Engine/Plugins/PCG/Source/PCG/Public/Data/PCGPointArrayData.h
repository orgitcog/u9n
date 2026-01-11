// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGBasePointData.h"
#include "PCGPointArray.h"

#include "PCGPointArrayData.generated.h"

#define UE_API PCG_API

extern PCG_API TAutoConsoleVariable<bool> CVarPCGEnablePointArrayDataParenting;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGPointArrayData : public UPCGBasePointData
{
	GENERATED_BODY()
public:	
	//~Begin UObject Interface
	UE_API virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~End UObject Interface

	UE_API virtual void Flatten() override;

	UE_API virtual void VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const override;
	UE_API virtual const UPCGPointData* ToPointData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const override;
	virtual const UPCGPointArrayData* ToPointArrayData(FPCGContext* Context, const FBox& InBounds = FBox(EForceInit::ForceInit)) const override { return this; }
	UE_API virtual UPCGSpatialData* CopyInternal(FPCGContext* Context) const override;
	UE_API virtual void CopyPropertiesTo(UPCGBasePointData* To, int32 ReadStartIndex, int32 WriteStartIndex, int32 Count, EPCGPointNativeProperties Properties) const override;
	UE_API virtual EPCGPointNativeProperties GetAllocatedProperties(bool bWithInheritance = true) const override;
	UE_API virtual bool SupportsSpatialDataInheritance() const override;
	virtual bool HasSpatialDataParent() const override { return ParentData != nullptr; }

	//~Begin UPCGBasePointData Interface
	virtual bool IsValidRef(const PCGPointOctree::FPointRef& InPointRef) const override { return InPointRef.Index >= 0 && InPointRef.Index < GetNumPoints(); }
	
	virtual int32 GetNumPoints() const override { return PointArray.GetNumPoints(); }
	UE_API virtual void SetNumPoints(int32 InNumPoints, bool bInitializeValues = true) override;
		
	UE_API virtual void AllocateProperties(EPCGPointNativeProperties Properties) override;
	UE_API virtual void FreeProperties(EPCGPointNativeProperties Properties) override;
	UE_API virtual void MoveRange(int32 RangeStartIndex, int32 MoveToIndex, int32 NumElements) override;
	UE_API virtual void CopyUnallocatedPropertiesFrom(const UPCGBasePointData* InPointData) override;

	virtual TArray<FTransform> GetTransformsCopy() const { return PointArray.GetTransformCopy(); }

	UE_API virtual TPCGValueRange<FTransform> GetTransformValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<float> GetDensityValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<FVector> GetBoundsMinValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<FVector> GetBoundsMaxValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<FVector4> GetColorValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<float> GetSteepnessValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<int32> GetSeedValueRange(bool bAllocate = true) override;
	UE_API virtual TPCGValueRange<int64> GetMetadataEntryValueRange(bool bAllocate = true) override;

	UE_API virtual TConstPCGValueRange<FTransform> GetConstTransformValueRange() const override;
	UE_API virtual TConstPCGValueRange<float> GetConstDensityValueRange() const override;
	UE_API virtual TConstPCGValueRange<FVector> GetConstBoundsMinValueRange() const override;
	UE_API virtual TConstPCGValueRange<FVector> GetConstBoundsMaxValueRange() const override;
	UE_API virtual TConstPCGValueRange<FVector4> GetConstColorValueRange() const override;
	UE_API virtual TConstPCGValueRange<float> GetConstSteepnessValueRange() const override;
	UE_API virtual TConstPCGValueRange<int32> GetConstSeedValueRange() const override;
	UE_API virtual TConstPCGValueRange<int64> GetConstMetadataEntryValueRange() const override;
	//~End UPCGBasePointData Interface
		
protected:
	UE_API virtual void InitializeSpatialDataInternal(const FPCGInitializeFromDataParams& InParams) override;

private:
	UE_API void FlattenPropertiesIfNeeded(EPCGPointNativeProperties Property = EPCGPointNativeProperties::All);

	template<class T>
	FPCGPointArrayProperty<T>* GetProperty(EPCGPointNativeProperties Property, bool bWithInheritance = true)
	{
		if (bWithInheritance)
		{
			if (EnumHasAnyFlags(InheritedProperties, Property))
			{
				check(ParentData);
				return ParentData->GetProperty<T>(Property, bWithInheritance);
			}
		}

		if constexpr (std::is_same_v<T, FTransform>)
		{
			if (Property == EPCGPointNativeProperties::Transform)
			{
				return &PointArray.Transform;
			}
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			if (Property == EPCGPointNativeProperties::Density)
			{
				return &PointArray.Density;
			}
			else if (Property == EPCGPointNativeProperties::Steepness)
			{
				return &PointArray.Steepness;
			}
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			if (Property == EPCGPointNativeProperties::BoundsMin)
			{
				return &PointArray.BoundsMin;
			}
			else if (Property == EPCGPointNativeProperties::BoundsMax)
			{
				return &PointArray.BoundsMax;
			}
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			if (Property == EPCGPointNativeProperties::Color)
			{
				return &PointArray.Color;
			}
		}
		else if constexpr (std::is_same_v<T, int32>)
		{
			if (Property == EPCGPointNativeProperties::Seed)
			{
				return &PointArray.Seed;
			}
		}
		else if constexpr (std::is_same_v<T, int64>)
		{
			if (Property == EPCGPointNativeProperties::MetadataEntry)
			{
				return &PointArray.MetadataEntry;
			}
		}

		check(false);
		return nullptr;
	}

	template<class T>
	const FPCGPointArrayProperty<T>* GetProperty(EPCGPointNativeProperties Property, bool bWithInheritance = true) const
	{
		if (bWithInheritance)
		{
			if (EnumHasAnyFlags(InheritedProperties, Property))
			{
				check(ParentData);
				return ParentData->GetProperty<T>(Property, bWithInheritance);
			}
		}

		if constexpr (std::is_same_v<T, FTransform>)
		{
			if (Property == EPCGPointNativeProperties::Transform)
			{
				return &PointArray.Transform;
			}
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			if (Property == EPCGPointNativeProperties::Density)
			{
				return &PointArray.Density;
			}
			else if (Property == EPCGPointNativeProperties::Steepness)
			{
				return &PointArray.Steepness;
			}
		}
		else if constexpr (std::is_same_v<T, FVector>)
		{
			if (Property == EPCGPointNativeProperties::BoundsMin)
			{
				return &PointArray.BoundsMin;
			}
			else if (Property == EPCGPointNativeProperties::BoundsMax)
			{
				return &PointArray.BoundsMax;
			}
		}
		else if constexpr (std::is_same_v<T, FVector4>)
		{
			if (Property == EPCGPointNativeProperties::Color)
			{
				return &PointArray.Color;
			}
		}
		else if constexpr (std::is_same_v<T, int32>)
		{
			if (Property == EPCGPointNativeProperties::Seed)
			{
				return &PointArray.Seed;
			}
		}
		else if constexpr (std::is_same_v<T, int64>)
		{
			if (Property == EPCGPointNativeProperties::MetadataEntry)
			{
				return &PointArray.MetadataEntry;
			}
		}

		check(false);
		return nullptr;
	}

	template<class T>
	bool FlattenPropertyIfNeeded(EPCGPointNativeProperties NativeProperty)
	{
		if (!EnumHasAnyFlags(InheritedProperties, NativeProperty))
		{
			return false;
		}

		const FPCGPointArrayProperty<T>* InheritedProperty = GetProperty<T>(NativeProperty, /*bWithInheritance=*/true);
		FPCGPointArrayProperty<T>* Property = GetProperty<T>(NativeProperty, /*bWithInheritance=*/false);

		// Should not be equal since InheritedProperties contains that property
		if (ensure(InheritedProperty != Property))
		{
			if (InheritedProperty->IsAllocated())
			{
				Property->Allocate(/*bInitializeValues=*/false);
			}

			check(InheritedProperty->Num() == Property->Num());
			InheritedProperty->CopyTo(*Property, 0, 0, Property->Num());
		}

		EnumRemoveFlags(InheritedProperties, NativeProperty);

		if (InheritedProperties == EPCGPointNativeProperties::None)
		{
			ParentData = nullptr;
		}

		return true;
	};
	
	UPROPERTY()
	FPCGPointArray PointArray;

	UPROPERTY(VisibleAnywhere, Category = ParentData)
	TObjectPtr<UPCGPointArrayData> ParentData = nullptr;

	UPROPERTY(VisibleAnywhere, Category = ParentData)
	EPCGPointNativeProperties InheritedProperties = EPCGPointNativeProperties::None;
};

#undef UE_API
