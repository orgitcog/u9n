// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/PCGPointArrayData.h"

#include "PCGContext.h"

#include "Data/PCGPointData.h"

TAutoConsoleVariable<bool> CVarPCGEnablePointArrayDataParenting(
	TEXT("pcg.EnablePointArrayDataParenting"),
	true,
	TEXT("Whether to enable inheritance of data on PointArrayData (memory savings)"));

void UPCGPointArrayData::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);

	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(PointArray.GetSizeBytes());
}

void UPCGPointArrayData::VisitDataNetwork(TFunctionRef<void(const UPCGData*)> Action) const
{
	Super::VisitDataNetwork(Action);
	if (ParentData)
	{
		ParentData->VisitDataNetwork(Action);
	}
}

const UPCGPointData* UPCGPointArrayData::ToPointData(FPCGContext* Context, const FBox& InBounds) const
{
	UPCGPointData* PointData = FPCGContext::NewObject_AnyThread<UPCGPointData>(Context);
	PointData->InitializeFromData(this);

	SetPoints(this, PointData, {}, /*bCopyAll=*/true);

	return PointData;
}

UPCGSpatialData* UPCGPointArrayData::CopyInternal(FPCGContext* Context) const
{
	UPCGPointArrayData* NewPointData = FPCGContext::NewObject_AnyThread<UPCGPointArrayData>(Context);
	
	// If inheritance is supported we are going to inherit from this data in InitializeSpatialDataInternal
	if (!SupportsSpatialDataInheritance())
	{
		NewPointData->PointArray = PointArray;
	}

	return NewPointData;
}

void UPCGPointArrayData::CopyPropertiesTo(UPCGBasePointData* To, int32 ReadStartIndex, int32 WriteStartIndex, int32 Count, EPCGPointNativeProperties Properties) const
{
	if (Count <= 0)
	{
		return;
	}

	if (UPCGPointArrayData* PointArrayData = Cast<UPCGPointArrayData>(To))
	{
		PointArrayData->AllocateProperties(GetAllocatedProperties());

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::Transform))
		{
			GetProperty<FTransform>(EPCGPointNativeProperties::Transform)->CopyTo(*PointArrayData->GetProperty<FTransform>(EPCGPointNativeProperties::Transform, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::Density))
		{
			GetProperty<float>(EPCGPointNativeProperties::Density)->CopyTo(*PointArrayData->GetProperty<float>(EPCGPointNativeProperties::Density, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::BoundsMin))
		{
			GetProperty<FVector>(EPCGPointNativeProperties::BoundsMin)->CopyTo(*PointArrayData->GetProperty<FVector>(EPCGPointNativeProperties::BoundsMin, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::BoundsMax))
		{
			GetProperty<FVector>(EPCGPointNativeProperties::BoundsMax)->CopyTo(*PointArrayData->GetProperty<FVector>(EPCGPointNativeProperties::BoundsMax, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::Color))
		{
			GetProperty<FVector4>(EPCGPointNativeProperties::Color)->CopyTo(*PointArrayData->GetProperty<FVector4>(EPCGPointNativeProperties::Color, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::Steepness))
		{
			GetProperty<float>(EPCGPointNativeProperties::Steepness)->CopyTo(*PointArrayData->GetProperty<float>(EPCGPointNativeProperties::Steepness, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::Seed))
		{
			GetProperty<int32>(EPCGPointNativeProperties::Seed)->CopyTo(*PointArrayData->GetProperty<int32>(EPCGPointNativeProperties::Seed, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}

		if (EnumHasAllFlags(Properties, EPCGPointNativeProperties::MetadataEntry))
		{
			GetProperty<int64>(EPCGPointNativeProperties::MetadataEntry)->CopyTo(*PointArrayData->GetProperty<int64>(EPCGPointNativeProperties::MetadataEntry, /*bWithInheritance=*/false), ReadStartIndex, WriteStartIndex, Count);
		}
	}
	else
	{
		Super::CopyPropertiesTo(To, ReadStartIndex, WriteStartIndex, Count, Properties);
	}
}

void UPCGPointArrayData::InitializeSpatialDataInternal(const FPCGInitializeFromDataParams& InParams)
{
	Super::InitializeSpatialDataInternal(InParams);
		
	if (const UPCGPointArrayData* SourceParentData = Cast<UPCGPointArrayData>(InParams.Source); InParams.bInheritSpatialData && SourceParentData && SupportsSpatialDataInheritance())
	{
		// Some nodes will call DuplicateData and then call InitializeFromData so it is possible we already have set the parent
		check(ParentData == nullptr || ParentData == InParams.Source);

		if (ParentData != InParams.Source)
		{
			SetNumPoints(SourceParentData->GetNumPoints());
			InheritedProperties = EPCGPointNativeProperties::All;
			ParentData = const_cast<UPCGPointArrayData*>(SourceParentData);
		}
	}
}

EPCGPointNativeProperties UPCGPointArrayData::GetAllocatedProperties(bool bWithInheritance) const
{
	EPCGPointNativeProperties AllocatedProperties = PointArray.GetAllocatedProperties();
	if (bWithInheritance && ParentData)
	{
		AllocatedProperties |= ParentData->GetAllocatedProperties(bWithInheritance);
	}
	return AllocatedProperties;
}

bool UPCGPointArrayData::SupportsSpatialDataInheritance() const
{
	return CVarPCGEnablePointArrayDataParenting.GetValueOnAnyThread();
}

void UPCGPointArrayData::Flatten()
{
	Super::Flatten();

	FlattenPropertiesIfNeeded();
	
	check(!ParentData);
}

void UPCGPointArrayData::FlattenPropertiesIfNeeded(EPCGPointNativeProperties Properties)
{	
	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::Transform))
	{
		FlattenPropertyIfNeeded<FTransform>(EPCGPointNativeProperties::Transform);
	}
	
	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::Steepness))
	{
		FlattenPropertyIfNeeded<float>(EPCGPointNativeProperties::Steepness);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::BoundsMin))
	{
		FlattenPropertyIfNeeded<FVector>(EPCGPointNativeProperties::BoundsMin);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::BoundsMax))
	{
		FlattenPropertyIfNeeded<FVector>(EPCGPointNativeProperties::BoundsMax);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::Color))
	{
		FlattenPropertyIfNeeded<FVector4>(EPCGPointNativeProperties::Color);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::Density))
	{
		FlattenPropertyIfNeeded<float>(EPCGPointNativeProperties::Density);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::Seed))
	{
		FlattenPropertyIfNeeded<int32>(EPCGPointNativeProperties::Seed);
	}

	if (EnumHasAnyFlags(Properties, EPCGPointNativeProperties::MetadataEntry))
	{
		FlattenPropertyIfNeeded<int64>(EPCGPointNativeProperties::MetadataEntry);
	}
}

void UPCGPointArrayData::SetNumPoints(int32 InNumPoints, bool bInitializeValues)
{
	if (ParentData && ParentData->GetNumPoints() != InNumPoints)
	{
		FlattenPropertiesIfNeeded();
	}

	if (InNumPoints != PointArray.GetNumPoints())
	{
		PointArray.SetNumPoints(InNumPoints, bInitializeValues);
		DirtyCache();
	}
}

void UPCGPointArrayData::AllocateProperties(EPCGPointNativeProperties Properties)
{
	FlattenPropertiesIfNeeded(Properties);
	PointArray.Allocate(Properties);
}

void UPCGPointArrayData::FreeProperties(EPCGPointNativeProperties Properties)
{
	FlattenPropertiesIfNeeded(Properties);
	PointArray.Free(Properties);
}

void UPCGPointArrayData::MoveRange(int32 RangeStartIndex, int32 MoveToIndex, int32 NumElements)
{
	FlattenPropertiesIfNeeded();
	PointArray.MoveRange(RangeStartIndex, MoveToIndex, NumElements);
}

void UPCGPointArrayData::CopyUnallocatedPropertiesFrom(const UPCGBasePointData* InPointData)
{
	if (HasSpatialDataParent())
	{
		return;
	}

	if (const UPCGPointArrayData* InPointArrayData = Cast<UPCGPointArrayData>(InPointData))
	{
		InPointArrayData->GetProperty<FTransform>(EPCGPointNativeProperties::Transform)->CopyUnallocatedProperty(*GetProperty<FTransform>(EPCGPointNativeProperties::Transform, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<float>(EPCGPointNativeProperties::Density)->CopyUnallocatedProperty(*GetProperty<float>(EPCGPointNativeProperties::Density, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<FVector>(EPCGPointNativeProperties::BoundsMin)->CopyUnallocatedProperty(*GetProperty<FVector>(EPCGPointNativeProperties::BoundsMin, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<FVector>(EPCGPointNativeProperties::BoundsMax)->CopyUnallocatedProperty(*GetProperty<FVector>(EPCGPointNativeProperties::BoundsMax, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<FVector4>(EPCGPointNativeProperties::Color)->CopyUnallocatedProperty(*GetProperty<FVector4>(EPCGPointNativeProperties::Color, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<float>(EPCGPointNativeProperties::Steepness)->CopyUnallocatedProperty(*GetProperty<float>(EPCGPointNativeProperties::Steepness, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<int32>(EPCGPointNativeProperties::Seed)->CopyUnallocatedProperty(*GetProperty<int32>(EPCGPointNativeProperties::Seed, /*bWithInheritance=*/false));
		InPointArrayData->GetProperty<int64>(EPCGPointNativeProperties::MetadataEntry)->CopyUnallocatedProperty(*GetProperty<int64>(EPCGPointNativeProperties::MetadataEntry, /*bWithInheritance=*/false));
	}
}

TPCGValueRange<FTransform> UPCGPointArrayData::GetTransformValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::Transform);
	return PointArray.GetTransformValueRange(bAllocate); 
}

TPCGValueRange<float> UPCGPointArrayData::GetDensityValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::Density);
	return PointArray.GetDensityValueRange(bAllocate); 
}

TPCGValueRange<FVector> UPCGPointArrayData::GetBoundsMinValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::BoundsMin);
	return PointArray.GetBoundsMinValueRange(bAllocate); 
}

TPCGValueRange<FVector> UPCGPointArrayData::GetBoundsMaxValueRange(bool bAllocate)
{
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::BoundsMax);
	return PointArray.GetBoundsMaxValueRange(bAllocate); 
}

TPCGValueRange<FVector4> UPCGPointArrayData::GetColorValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::Color);
	return PointArray.GetColorValueRange(bAllocate); 
}

TPCGValueRange<float> UPCGPointArrayData::GetSteepnessValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::Steepness);
	return PointArray.GetSteepnessValueRange(bAllocate); 
}

TPCGValueRange<int32> UPCGPointArrayData::GetSeedValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::Seed);
	return PointArray.GetSeedValueRange(bAllocate); 
}
TPCGValueRange<int64> UPCGPointArrayData::GetMetadataEntryValueRange(bool bAllocate)
{ 
	FlattenPropertiesIfNeeded(EPCGPointNativeProperties::MetadataEntry);
	return PointArray.GetMetadataEntryValueRange(bAllocate); 
}

TConstPCGValueRange<FTransform> UPCGPointArrayData::GetConstTransformValueRange() const
{ 
	return GetProperty<FTransform>(EPCGPointNativeProperties::Transform)->GetConstValueRange();
}

TConstPCGValueRange<float> UPCGPointArrayData::GetConstDensityValueRange() const
{ 
	return GetProperty<float>(EPCGPointNativeProperties::Density)->GetConstValueRange();
}

TConstPCGValueRange<FVector> UPCGPointArrayData::GetConstBoundsMinValueRange() const
{ 
	return GetProperty<FVector>(EPCGPointNativeProperties::BoundsMin)->GetConstValueRange();
}

TConstPCGValueRange<FVector> UPCGPointArrayData::GetConstBoundsMaxValueRange() const
{ 
	return GetProperty<FVector>(EPCGPointNativeProperties::BoundsMax)->GetConstValueRange();
}

TConstPCGValueRange<FVector4> UPCGPointArrayData::GetConstColorValueRange() const
{ 
	return GetProperty<FVector4>(EPCGPointNativeProperties::Color)->GetConstValueRange();
}

TConstPCGValueRange<float> UPCGPointArrayData::GetConstSteepnessValueRange() const
{ 
	return GetProperty<float>(EPCGPointNativeProperties::Steepness)->GetConstValueRange();
}

TConstPCGValueRange<int32> UPCGPointArrayData::GetConstSeedValueRange() const
{ 
	return GetProperty<int32>(EPCGPointNativeProperties::Seed)->GetConstValueRange();
}

TConstPCGValueRange<int64> UPCGPointArrayData::GetConstMetadataEntryValueRange() const
{ 
	return GetProperty<int64>(EPCGPointNativeProperties::MetadataEntry)->GetConstValueRange();
}