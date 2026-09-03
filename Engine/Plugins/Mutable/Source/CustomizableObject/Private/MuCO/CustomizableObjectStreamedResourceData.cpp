// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectStreamedResourceData.h"

#include "ExternalPackageHelper.h"
#include "MuCO/CustomizableObject.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectStreamedResourceData)

#if WITH_EDITOR
FCustomizableObjectStreamedResourceData::FCustomizableObjectStreamedResourceData(
	UCustomizableObjectResourceDataContainer* InContainer)
{
	check(IsInGameThread());
	check(InContainer);

	Container = InContainer;
	ContainerPath = InContainer;
}

void FCustomizableObjectStreamedResourceData::ConvertToSoftReferenceForCooking()
{
	check(IsInGameThread());
	check(Container);

	// Update ContainerPath;
	ContainerPath = TSoftObjectPtr<UCustomizableObjectResourceDataContainer>(const_cast<UCustomizableObjectResourceDataContainer*>(Container.Get()));
	
	// Remove the hard reference to the container, so that it can be unloaded
	Container = nullptr;
}
#endif // WITH_EDITOR

bool FCustomizableObjectStreamedResourceData::IsLoaded() const
{
	check(IsInGameThread());

	return Container != nullptr;
}

const FCustomizableObjectResourceData& FCustomizableObjectStreamedResourceData::GetLoadedData() const
{
	check(IsInGameThread());
	check(Container);
	
	return Container->Data;
}

void FCustomizableObjectStreamedResourceData::Release()
{
	check(IsInGameThread());

	if (FPlatformProperties::RequiresCookedData()) // TODO GMT Remove once UE-232022. Editor will no longer use StreamedResources.
	{
		Container = nullptr;
	}
}

void FCustomizableObjectStreamedResourceData::Hold()
{
	check(IsInGameThread());

	Container = GetPath().Get();
}
