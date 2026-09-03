// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectCookPackageSplitter.h"

#include "Algo/Find.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/LoadUtils.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"

REGISTER_COOKPACKAGE_SPLITTER(FCustomizableObjectCookPackageSplitter, UCustomizableObject);

namespace
{
UModelResources* FindModelResources(UCustomizableObject& Object)
{
	// All platforms should have the same resources
	const TMap<FString, MutablePrivate::FMutableCachedPlatformData>& CachePlatforms = Object.GetPrivate()->CachedPlatformsData;
	for (const TPair<FString, MutablePrivate::FMutableCachedPlatformData>& PlatformData : CachePlatforms)
	{
		if (PlatformData.Value.ModelResources)
		{
			return PlatformData.Value.ModelResources.Get();
		}
	}

	return nullptr;
}

// Look up a streamed Resource Data constant by name on a Customizable Object.
//
// Returns nullptr if not found.
FCustomizableObjectStreamedResourceData* FindStreamedResourceData(
	TArray<FCustomizableObjectStreamedResourceData>& StreamedResources,
	const FString& ContainerName
	)
{
	return Algo::FindByPredicate(
		StreamedResources,
		[&ContainerName](const FCustomizableObjectStreamedResourceData& StreamedData)
		{
			const FSoftObjectPath& Path = StreamedData.GetPath().ToSoftObjectPath();

			// ContainerName should match the last element of the path, which could be the
			// sub-path string or the asset name.

			if (Path.GetSubPathString().Len() > 0)
			{
				return Path.GetSubPathString() == ContainerName;
			}

			return Path.GetAssetName() == ContainerName;
		});
}

enum class EMoveContainerError
{
	None,
	FailedToLoadContainer,
	NameCollision, // Object with that name already exists in the new outer
	RenameFailed,
};

const TCHAR* LexToString(EMoveContainerError Error)
{
	switch (Error)
	{
	case EMoveContainerError::None: return TEXT("None");
	case EMoveContainerError::FailedToLoadContainer: return TEXT("FailedToLoadContainer");
	case EMoveContainerError::NameCollision: return TEXT("NameCollision");
	case EMoveContainerError::RenameFailed: return TEXT("RenameFailed");
	default: return TEXT("Unknown");
	}
}

// Moves the StreamedResourceData's data container to the given Outer.
EMoveContainerError MoveContainerToNewOuter(
	UObject* NewOuter,
	const FCustomizableObjectStreamedResourceData* StreamedResourceData,
	UCustomizableObjectResourceDataContainer*& OutContainer
)
{
	check(StreamedResourceData);
	
	OutContainer = nullptr;

	UCustomizableObjectResourceDataContainer* Container = MutablePrivate::LoadObject(StreamedResourceData->GetPath());
	if (!Container)
	{
		return EMoveContainerError::FailedToLoadContainer;
	}

	if (Container->GetOuter() != NewOuter)
	{
		// Ensure the target object doesn't exist
		if (FindObject<UObject>(NewOuter, *Container->GetName()))
		{
			return EMoveContainerError::NameCollision;
		}

		// The Rename function moves the object into the given package
		if (!Container->Rename(nullptr, NewOuter, REN_DontCreateRedirectors))
		{
			return EMoveContainerError::RenameFailed;
		}
	}

	OutContainer = Container;
	return EMoveContainerError::None;
}


void GenerateNewPackage(const FCustomizableObjectStreamedResourceData& StreamedData,
	const UPackage* OwnerPackage,
	const UObject* OwnerObject,
	TArray<ICookPackageSplitter::FGeneratedPackage>& Result)
{
	// The StreamedData container path should be of the form
	// OwnerPackageName.OwnerObjectName:ContainerName
	const FSoftObjectPath& StreamedDataPath = StreamedData.GetPath().ToSoftObjectPath();

	// Check that the StreamedData container has the OwnerObject as its Outer
	check(StreamedDataPath.GetWithoutSubPath() == FSoftObjectPath(OwnerObject));

	// Check that the ContainerName is valid and that there isn't another Outer level between
	// the OwnerObject and the container.
	check(StreamedDataPath.GetSubPathString().Len() > 0);
	check(!StreamedDataPath.GetSubPathString().Contains(SUBOBJECT_DELIMITER));

	ICookPackageSplitter::FGeneratedPackage& Package = Result.AddDefaulted_GetRef();
	// Because of the checks above, the container name must be unique within this Customizable
	// Object, so it's safe to use as a package path.
	Package.RelativePath = StreamedDataPath.GetSubPathString();
	Package.SetCreateAsMap(false);

	// To support iterative cooking, GenerationHash should only change when OwnerPackage
	// changes.
	//
	// The simplest and fastest way to do this is to set it to OwnerPackage's PackageSavedHash.
	{
		// Zero the hash, as we won't be writing all bytes of it below
		Package.GenerationHash.Reset();

		FIoHash OwnerSavedHash = OwnerPackage->GetSavedHash();
		static_assert(sizeof(Package.GenerationHash.GetBytes()) >= sizeof(OwnerSavedHash.GetBytes()));  // -V568
		static_assert(sizeof(Package.GenerationHash.GetBytes()) > 8); // It should be a byte array, not a pointer // -V568
		static_assert(sizeof(OwnerSavedHash.GetBytes()) > 8); // It should be a byte array, not a pointer // -V568
		FMemory::Memcpy(Package.GenerationHash.GetBytes(), OwnerSavedHash.GetBytes(), sizeof(OwnerSavedHash.GetBytes())); // -V568

	}
}

}

bool FCustomizableObjectCookPackageSplitter::ShouldSplit(UObject* SplitData)
{
	UCustomizableObject* Object = CastChecked<UCustomizableObject>(SplitData);
	
	if (!Object->IsChildObject())
	{
		if(const UModelResources* ModelResources = FindModelResources(*Object))
		{
			return ModelResources->StreamedResourceData.Num() || ModelResources->StreamedExtensionData.Num();
		}
	}

	return false;
}

TArray<ICookPackageSplitter::FGeneratedPackage> FCustomizableObjectCookPackageSplitter::GetGenerateList(
	const UPackage* OwnerPackage,
	const UObject* OwnerObject)
{
	// Keep a strong reference to the CO.
	StrongObject.Reset(OwnerObject);
	
	UCustomizableObject* Object = const_cast<UCustomizableObject*>(CastChecked<UCustomizableObject>(OwnerObject));

	// All platforms should have the same resources
	const UModelResources* ModelResources = FindModelResources(*Object);
	check(ModelResources);

	TArray<ICookPackageSplitter::FGeneratedPackage> Result;

	// Generate a new package for each streamed Resource Data
	for (const FCustomizableObjectStreamedResourceData& StreamedData : ModelResources->StreamedResourceData)
	{
		GenerateNewPackage(StreamedData, OwnerPackage, OwnerObject, Result);
	}

	// Generate a new package for each streamed Extension Data
	for (const FCustomizableObjectStreamedResourceData& StreamedData : ModelResources->StreamedExtensionData)
	{
		GenerateNewPackage(StreamedData, OwnerPackage, OwnerObject, Result);
	}

	return Result;
}

bool FCustomizableObjectCookPackageSplitter::PreSaveGeneratorPackage(FPopulateContext& PopulateContext)
{
	// The CO is just about to be saved (i.e. produce the cooked version of the asset), so this
	// function needs to:
	// 
	// 1.	Move the streamed Data out of the CO's package, so that it doesn't get saved
	//		into the cooked package.
	// 
	// 2.	Remove hard references to the streamed data, so that it doesn't get loaded as soon as
	//		the CO is loaded
	TConstArrayView<ICookPackageSplitter::FGeneratedPackageForPopulate>& PlaceholderPackages = PopulateContext.GetGeneratedPackages();
	const auto& PreSavePackage = [] (const ICookPackageSplitter::FGeneratedPackageForPopulate& GeneratedPackage,
		TArray<FCustomizableObjectStreamedResourceData>& StreamedResources
		) -> bool
	{
		FCustomizableObjectStreamedResourceData* FoundData = FindStreamedResourceData(StreamedResources, GeneratedPackage.RelativePath);
		if (!FoundData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Resource Data container with name %s in array of %d entries"),
				*GeneratedPackage.RelativePath, StreamedResources.Num());

			return false;
		}

		// Move the streamed data to the generated package
		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(GeneratedPackage.Package, FoundData, Container);
		if (Error != EMoveContainerError::None)
		{
			UE_LOG(LogMutable, Error, TEXT("Failed to move container %s to new outer %s - %s"), *FoundData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(GeneratedPackage.Package), LexToString(Error));
			return false;
		}

		return true;
	};

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(PopulateContext.GetOwnerObject());

	UModelResources* ModelResources = FindModelResources(*Object);
	if (!ModelResources)
	{
		UE_LOG(LogMutable, Warning, TEXT("Couldn't find ModelResources. CO %s"), *GetNameSafe(Object));
		return false;
	}

	// There should be one generated package per streamed Resource Data
	const int32 NumStreamedData = ModelResources->StreamedResourceData.Num();
	const int32 NumStreamedExtensionData = ModelResources->StreamedExtensionData.Num();
	
	check(NumStreamedData + NumStreamedExtensionData == PlaceholderPackages.Num());


	// After the CO has been saved, the contract for ICookPackageSplitter states that we need to
	// restore the CO back to how it was before, so we need to save some information to help with
	// this.
	SavedContainerNames.Reset();
	SavedExtensionContainerNames.Reset();

	for (int32 Index = 0; Index < NumStreamedData; ++Index)
	{
		const ICookPackageSplitter::FGeneratedPackageForPopulate& GeneratedPackage = PlaceholderPackages[Index];
		if (!PreSavePackage(GeneratedPackage, ModelResources->StreamedResourceData))
		{
			return false;
		}

		SavedContainerNames.Add(GeneratedPackage.RelativePath);
	}

	for (int32 Index = 0; Index < NumStreamedExtensionData; ++Index)
	{
		const ICookPackageSplitter::FGeneratedPackageForPopulate& GeneratedPackage = PlaceholderPackages[NumStreamedData + Index];
		if (!PreSavePackage(GeneratedPackage, ModelResources->StreamedExtensionData))
		{
			return false;
		}

		SavedExtensionContainerNames.Add(GeneratedPackage.RelativePath);
	}

	// All platforms should have the same resources
	const TMap<FString, MutablePrivate::FMutableCachedPlatformData>& CachePlatforms = Object->GetPrivate()->CachedPlatformsData;
	for (const TPair<FString, MutablePrivate::FMutableCachedPlatformData>& PlatformData : CachePlatforms)
	{
		if (PlatformData.Value.ModelResources)
		{
			for (FCustomizableObjectStreamedResourceData& StreamedResourceData : PlatformData.Value.ModelResources->StreamedResourceData)
			{
				// Remove the hard reference and set the soft reference to the streamed data's new location
				StreamedResourceData.ConvertToSoftReferenceForCooking();
			}

			for (FCustomizableObjectStreamedResourceData& StreamedExtensionData : PlatformData.Value.ModelResources->StreamedExtensionData)
			{
				// Remove the hard reference and set the soft reference to the streamed data's new location
				StreamedExtensionData.ConvertToSoftReferenceForCooking();
			}
		}
	}

	return true;
}

void FCustomizableObjectCookPackageSplitter::PostSaveGeneratorPackage(FPopulateContext& PopulateContext)
{
	// Move the streamed data back into the CO's package and restore the StreamedResourceData and StreamedExtensionData
	// array on the CO to how it was before PreSaveGeneratorPackage.

	UCustomizableObject* Object = CastChecked<UCustomizableObject>(PopulateContext.GetOwnerObject());
	UModelResources* ModelResources = FindModelResources(*Object);
	if (!ModelResources)
	{
		UE_LOG(LogMutable, Warning, TEXT("Couldn't find ModelResources. CO %s"), *GetNameSafe(Object));
		return;
	}

	TArray<FCustomizableObjectStreamedResourceData> NewArray;
	NewArray.Reset(SavedContainerNames.Num());

	for (const FString& ContainerName : SavedContainerNames)
	{
		FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(ModelResources->StreamedResourceData, ContainerName);
		if (!ResourceData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Resource Data container with name %s in array of %d entries"),
				*ContainerName, ModelResources->StreamedResourceData.Num());

			continue;
		}

		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(Object, ResourceData, Container);
		UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, TEXT("Failed to move container %s back to %s - %s"), *ContainerName, *GetPathNameSafe(Object), LexToString(Error));

		NewArray.Emplace(Container);
	}

	ModelResources->StreamedResourceData = NewArray;

	NewArray.Reset(SavedExtensionContainerNames.Num());

	for (const FString& ContainerName : SavedExtensionContainerNames)
	{
		FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(ModelResources->StreamedExtensionData, ContainerName);
		if (!ResourceData)
		{
			UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed Extension Data container with name %s in array of %d entries"),
				*ContainerName, ModelResources->StreamedExtensionData.Num());

			continue;
		}

		UCustomizableObjectResourceDataContainer* Container = nullptr;
		EMoveContainerError Error = MoveContainerToNewOuter(Object, ResourceData, Container);
		UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, TEXT("Failed to move container %s back to %s - %s"), *ContainerName, *GetPathNameSafe(Object), LexToString(Error));

		NewArray.Emplace(Container);
	}

	ModelResources->StreamedExtensionData = NewArray;
}

bool FCustomizableObjectCookPackageSplitter::PopulateGeneratedPackage(FPopulateContext& PopulateContext)
{
	// Move the container into its newly generated package

	const FGeneratedPackageForPopulate& GeneratedPackage = *PopulateContext.GetTargetGeneratedPackage();
	UCustomizableObject* Object = CastChecked<UCustomizableObject>(PopulateContext.GetOwnerObject());
	UModelResources* ModelResources = FindModelResources(*Object);

	FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(ModelResources->StreamedResourceData, GeneratedPackage.RelativePath);
	if (!ResourceData)
	{
		ResourceData = FindStreamedResourceData(ModelResources->StreamedExtensionData, GeneratedPackage.RelativePath);
	}

	if (!ResourceData)
	{
		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed resource Data container with name %s in arrays of %d and %d entries"),
			*GeneratedPackage.RelativePath, ModelResources->StreamedResourceData.Num(), ModelResources->StreamedExtensionData.Num());

		return false;
	}

	// [TEMP] Loading a package referencing the CO before PostSaveGeneratedPackage is called causes a name collision.
	// Duplicate the object with the new outer instead of moving it until it is fixed.
	UObject* Container = MutablePrivate::LoadObject(ResourceData->GetPath());
	EMoveContainerError Error = Container ? EMoveContainerError::None : EMoveContainerError::FailedToLoadContainer;
	if (Container)
	{
		Container = StaticDuplicateObject(Container, GeneratedPackage.Package);
	}

	//UCustomizableObjectResourceDataContainer* Container = nullptr;
	//EMoveContainerError Error = MoveContainerToNewOuter(GeneratedPackage.Package, ResourceData, Container);

	if (Error != EMoveContainerError::None)
	{
		UE_LOG(LogMutable, Error, TEXT("Failed to move container %s to new outer %s - %s"), *ResourceData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(GeneratedPackage.Package), LexToString(Error));
		return false;
	}

	PopulateContext.ReportObjectToMove(Container);

	return true;
}

void FCustomizableObjectCookPackageSplitter::PostSaveGeneratedPackage(FPopulateContext& PopulateContext)
{
	// Now that the generated package has been saved/cooked, move the container back to the CO, so
	// that everything is the same as it was before cooking.

	const FGeneratedPackageForPopulate& GeneratedPackage = *PopulateContext.GetTargetGeneratedPackage();
	UCustomizableObject* Object = CastChecked<UCustomizableObject>(PopulateContext.GetOwnerObject());
	UModelResources* ModelResources = FindModelResources(*Object);

	FCustomizableObjectStreamedResourceData* ResourceData = FindStreamedResourceData(ModelResources->StreamedResourceData, GeneratedPackage.RelativePath);
	if (!ResourceData)
	{
		ResourceData = FindStreamedResourceData(ModelResources->StreamedExtensionData, GeneratedPackage.RelativePath);
	}

	if (!ResourceData)
	{
		UE_LOG(LogMutable, Error, TEXT("Couldn't find streamed resource Data container with name %s in arrays of %d and %d entries"),
			*GeneratedPackage.RelativePath, ModelResources->StreamedResourceData.Num(), ModelResources->StreamedExtensionData.Num());

		return;
	}

	UCustomizableObjectResourceDataContainer* Container = nullptr;
	EMoveContainerError Error = MoveContainerToNewOuter(Object, ResourceData, Container);
	UE_CLOG(Error != EMoveContainerError::None, LogMutable, Warning, 
		TEXT("Failed to move container %s back to %s - %s"), *ResourceData->GetPath().ToSoftObjectPath().ToString(), *GetPathNameSafe(Object), LexToString(Error));
}

void FCustomizableObjectCookPackageSplitter::Teardown(ETeardown Status)
{
	StrongObject.Reset();
}
