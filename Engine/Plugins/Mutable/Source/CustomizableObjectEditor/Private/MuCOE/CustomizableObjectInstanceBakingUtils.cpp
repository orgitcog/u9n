// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectInstanceBakingUtils.h"

#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"

#include "Misc/MessageDialog.h"
#include "FileHelpers.h"
#include "ObjectTools.h"
#include "UnrealBakeHelpers.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAssetCommon.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "PhysicsEngine/PhysicsAsset.h"

#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectInstanceAssetUserData.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectMipDataProvider.h"
#include "MuT/UnrealPixelFormatOverride.h"
#include "Rendering/SkeletalMeshModel.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor" 

// Recommended prefixes for various types of UE assets
const FString SkeletalMeshAssetPrefix = TEXT("SK_");
const FString MaterialAssetPrefix = TEXT("M_");
const FString TextureAssetPrefix = TEXT("T_");
const FString MaterialInstanceAssetPrefix = TEXT("MI_");
const FString MaterialDynamicInstanceAssetPrefix = TEXT("MID_");
const FString MaterialConstantInstanceAssetPrefix = TEXT("MIC_");
const FString SkeletonAssetPrefix = TEXT("SKEL_");
const FString PhysicsAssetPrefix = TEXT("PHYS_");


/**
 * Remove the prefix from the provided string.
 * @param InOutString The string to update
 * @param ToCheckPrefixes The list of possible prefixes that, if present, will get removed from the target string
 */
void RemovePrefix(FString& InOutString, const TArray<FString>& ToCheckPrefixes)
{
	for (const FString& PrefixToRemove : ToCheckPrefixes)
	{
		if (InOutString.Find(PrefixToRemove, ESearchCase::CaseSensitive) == 0)
		{
			InOutString = InOutString.RightChop(PrefixToRemove.Len());
			return;
		}
	}
}

/**
 * Compose the resource name to be used by a resource.
 * @param InPrefix The prefix we want to use for a given resource.
 * @param InObjectName The name of the object this resource is part of.
 * @param InOutResourceName The name of the resource itself. This string will be the target of the concatenations.
 */
void ComposeResourceName( const FString& InPrefix, const FString& InObjectName, FString& InOutResourceName)
{
	InOutResourceName = InPrefix + InObjectName + InOutResourceName;
}


/**
 * Simple wrapper to be able to invoke the generation of a popup or log message depending on the execution context in which this code is being ran
 * @param InMessage The message to display
 * @param InTitle The title to be used for the popup or the log generated
 */
void ShowErrorNotification(const FText& InMessage, const FText& InTitle = LOCTEXT("CustomizableObjectInstanceBakingUtils_GenericBakingError","Baking Error") )
{
	if (!FApp::IsUnattended())
	{
		FMessageDialog::Open(EAppMsgType::Ok, InMessage, InTitle);
	}
	else
	{
		UE_LOG(LogMutable, Error, TEXT("%s - %s"), *InTitle.ToString(), *InMessage.ToString());
	}
}

/**
 * Utility functions for the baking operation.
 */

/**
 * Validates the filename chosen for the baking data
 * @param FileName The filename chosen by the user
 * @return True if validation was successful, false otherwise
 */
bool ValidateProvidedFileName(const FString& FileName)
{
	// Check for invalid characters in the name of the object to be serialized
	TCHAR InvalidCharacter = '0';
	{
		FString InvalidCharacters = FPaths::GetInvalidFileSystemChars();
		for (int32 InvalidCharIndex = 0; InvalidCharIndex < InvalidCharacters.Len(); ++InvalidCharIndex)
		{
			TCHAR Char = InvalidCharacters[InvalidCharIndex];
			FString SearchedChar = FString::Chr(Char);
			if (FileName.Contains(SearchedChar))
			{
				InvalidCharacter = InvalidCharacters[InvalidCharIndex];
				break;
			}
		}
	}

	if (InvalidCharacter != '0')
	{
		const FText InvalidCharacterText = FText::FromString(FString::Chr(InvalidCharacter));
		const FText ErrorText = FText::Format(LOCTEXT("CustomizableObjectInstanceBakingUtils_InvalidCharacter", "The selected contains an invalid character ({0})."), InvalidCharacterText);

		ShowErrorNotification(ErrorText);
		
		return false;
	}

	return true;
}


/**
 * Validates the AssetPath chosen for the baking data
 * @param FileName The filename chosen by the user
 * @param AssetPath The AssetPath chosen by the user
 * @param InstanceCO The CustomizableObject from the provided COI
 * @return True if validation was successful, false otherwise
 */
bool ValidateProvidedAssetPath(const FString& FileName, const FString& AssetPath, const UCustomizableObject* InstanceCO)
{
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogMutable, Error, TEXT("The AssetPath can not be empty!"));
		return false;
	}

	// Ensure we are not overriding the parent CO
	const FString FullAssetPath = AssetPath + FString("/") + FileName + FString(".") + FileName;		// Full asset path to the new asset we want to create
	if (const bool bWouldOverrideParentCO = InstanceCO->GetPathName() == FullAssetPath)
	{
		const FText ErrorText = LOCTEXT("CustomizableObjectInstanceBakingUtils_OverwriteCO", "The selected path would overwrite the instance's parent Customizable Object.");

		ShowErrorNotification(ErrorText);
		
		return false;
	}

	return true;
}



/**
 * Outputs a string that we know it is unique.
 * @param ResourceName The name of the resource we have provided. This should have the name of the current resource and will have the unique name for the resource once the method exits
 * @param InCachedResourceNames Collection with all the already processed resources name's
 */
void MakeResourceNameUnique(FString& ResourceName, const TArray<FString>& InCachedResourceNames)
{
	// Look for the resource name provided to see if we have already worked with it.
	int32 FindResult = InCachedResourceNames.Find(ResourceName);
	if (FindResult != INDEX_NONE)
	{
		// Add an integer suffix to create the unique name
		uint32 Count = 0;
		while (FindResult != INDEX_NONE)
		{
			FindResult = InCachedResourceNames.Find(ResourceName + "_" + FString::FromInt(Count));
			Count++;
		}

		ResourceName += "_" + FString::FromInt(--Count);
	}
}


/**
 * Ensures the resource we want to save is ready to be saved. It handles closing it's editor and warning the user about possible overriding of resources.
 * @param InAssetSavePath The directory path where to save the baked object
 * @param InObjName The name of the object to be baked
 * @param bOverridePermissionGranted Control flag that determines if the user has given or not permission to override resources already in disk
 * @param bIsUnattended
 * @param OutSaveResolution
 * @return True if the operation was successful, false otherwise
 */
bool CreateAssetAtPath(const FString& InAssetSavePath, const FString& InObjName, bool& bOverridePermissionGranted, const bool& bIsUnattended, EPackageSaveResolutionType& OutSaveResolution)
{
	// Before the value provided by "bOverridePermissionGranted" was being updated due to user request but not it is not. It will stay as is if unattended an
	// will get updated if this gets to be an attended execution.
	
	const FString PackagePath = InAssetSavePath + "/" + InObjName;
	UPackage* ExistingPackage = FindPackage(nullptr, *PackagePath);

	if (!ExistingPackage)
	{
		const FString PackageFilePath = PackagePath + "." + InObjName;

		FString PackageFileName;
		if (FPackageName::DoesPackageExist(PackageFilePath, &PackageFileName))
		{
			ExistingPackage = LoadPackage(nullptr, *PackageFileName, LOAD_EditorOnly);
		}
		else
		{
			// if package does not exist
			
			if (!bIsUnattended)
			{
				// If the run is attended (the user is participating in it) then we will take care in consideration his decision what he wants.
				bOverridePermissionGranted = false;
			}

			OutSaveResolution = EPackageSaveResolutionType::NewFile;
			return true;
		}
	}

	if (ExistingPackage)
	{
		// Checking if the asset is open in an editor
		TArray<IAssetEditorInstance*> ObjectEditors = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->FindEditorsForAssetAndSubObjects(ExistingPackage);
		if (ObjectEditors.Num())
		{
			for (IAssetEditorInstance* ObjectEditorInstance : ObjectEditors)
			{
				// Close the editors that contains this asset
				if (!ObjectEditorInstance->CloseWindow(EAssetEditorCloseReason::AssetEditorHostClosed))
				{
					const FText Caption = LOCTEXT("CustomizableObjectInstanceBakingUtils_OpenExistingFile", "Open File");
					const FText Message = FText::Format(LOCTEXT("CustomizableObjectInstanceBakingUtils_CantCloseAsset", "This Object \"{0}\" is open in an editor and can't be closed automatically. Please close the editor and try to bake it again"), FText::FromString(InObjName));

					ShowErrorNotification(Message, Caption);
					
					return false;
				}
			}
		}

		// If the execution requires user interaction, and we have no permission to override the existing file ask him if he wants or not to override data
		if (!bIsUnattended && !bOverridePermissionGranted)
		{
			check (!FApp::IsUnattended())
			const FText Caption = LOCTEXT("CustomizableObjectInstanceBakingUtils_AlreadyExistingBakedFiles", "Already existing baked files");
			const FText Message = FText::Format(LOCTEXT("CustomizableObjectInstanceBakingUtils_OverwriteBakedInstance", "Instance baked files already exist in selected destination \"{0}\", this action will overwrite them."), FText::AsCultureInvariant(InAssetSavePath));

			if (FMessageDialog::Open(EAppMsgType::OkCancel, Message, Caption) == EAppReturnType::Cancel)
			{
				return false;		// if the user cancels then we will still have no rights for overriding data
			}

			UE_LOG(LogMutable, Error, TEXT("%s - %s"), *Caption.ToString(), *Message.ToString());
			
			// If the user accepts the prompt then we will consider we have a green light to override the asset
			bOverridePermissionGranted = true;
		}
		
		// At this point we may or may not have permission to delete the existing asset
		
		// Delete the old asset if we have permission to do so
		if (UObject* ExistingObject = StaticFindObject(UObject::StaticClass(), ExistingPackage, *InObjName))
		{
			// Based on if we have or not permission to override the file do or do not so
			if (bOverridePermissionGranted)
			{
				ExistingPackage->FullyLoad();

				TArray<UObject*> ObjectsToDelete;
				ObjectsToDelete.Add(ExistingObject);
				
				const FText Message = FText::Format(LOCTEXT("CustomizableObjectInstanceBakingUtils_AssetOverriden", "The COI asset \"{0}\" already exists and will be overriden due to user demand."), FText::FromString(ExistingPackage->GetName()));
				UE_LOG(LogMutable,Warning,TEXT("%s"), *Message.ToString());

				// Notify the caller we did proceed with the override (performed later)
				OutSaveResolution = EPackageSaveResolutionType::Overriden;
				
				// Delete objects in the package with the same name as the one we want to create
				const uint32 NumObjectsDeleted = ObjectTools::ForceDeleteObjects(ObjectsToDelete, false);
				return NumObjectsDeleted == ObjectsToDelete.Num();
			}
			else
			{
				// Notify the caller that the override will not be performed
				OutSaveResolution = EPackageSaveResolutionType::UnableToOverride;
				
				// Report that the file will not get overriden since we have no permission to do so
				const FText UnableToOverrideMessage = FText::Format(LOCTEXT("CustomizableObjectInstanceBakingUtils_AssetCanNotBeOverriden", "Could not replace the COI asset \"{0}\" as it already exists."), FText::FromString(ExistingPackage->GetName()));
				UE_LOG(LogMutable,Error,TEXT("%s"), *UnableToOverrideMessage.ToString());

				return false;
			}
		}
		else
		{
			// Notify the caller that no override was required
			OutSaveResolution = EPackageSaveResolutionType::NewFile;
		}
	}

	return true;
}

static bool bIsBakeOperationAlreadyScheduled = false;


void OnInstanceUpdateFinish(const FUpdateContext& Result)
{
	FCustomizableObjectEditorLogger::CreateLog(
	LOCTEXT("CustomizableObjectInstanceBakingUtils_UpdateFinished", "The COInstance Update operation for baking has finished."))
	.Category(ELoggerCategory::COInstanceBaking)
	.CustomNotification()
	.Notification(true)
	.Log();

	// Allow the baking of more instances once the bake of this one has completed (or at least until the callbacks have been broadcast)
	bIsBakeOperationAlreadyScheduled = false;
}


void ScheduleInstanceUpdateForBaking(UCustomizableObjectInstance& InInstance, FInstanceUpdateNativeDelegate& InInstanceUpdateDelegate)
{
	// This prevents the queue of updates to have more than one instance for baking.
	if (bIsBakeOperationAlreadyScheduled)
	{
		UE_LOG(LogMutable, Error, TEXT("The COInstance update for baking could not be scheduled. Another instance is being updated for baking."));
		InInstanceUpdateDelegate.Broadcast({EUpdateResult::Error});
		return;
	}

	// TODO UE-262123: Compile Instance with correct settings instead
	if (!InInstance.GetCustomizableObject() || !InInstance.GetCustomizableObject()->IsCompiled())
	{
		UE_LOG(LogMutable, Error, TEXT("The COInstance update for baking could not be scheduled. Instance not compiled."));
		InInstanceUpdateDelegate.Broadcast({ EUpdateResult::Error });
		return;
	}

	InInstanceUpdateDelegate.AddStatic(&OnInstanceUpdateFinish);
	
	// Desired System settings for the update part of a baking operation
	TSharedPtr<FMutableSystemSettingsOverrides> SystemSettingsOverride =
		MakeShared<FMutableSystemSettingsOverrides>(
			false,
			false,
			UnrealPixelFormatFunc);
	
	// Schedule the update
	check (InInstance.GetPrivate());
	InInstance.GetPrivate()->UpdateSkeletalMeshAsyncResult(InInstanceUpdateDelegate,true,true, SystemSettingsOverride);
	
	// Tell the baking system that an instance for baking will be processed
	bIsBakeOperationAlreadyScheduled = true;

	FCustomizableObjectEditorLogger::CreateLog(
	LOCTEXT("CustomizableObjectInstanceBakingUtils_UpdateScheduled", "The COInstance Update operation for baking has been scheduled. Please hold."))
	.Category(ELoggerCategory::COInstanceBaking)
	.CustomNotification()
	.Notification(true)
	.Log();
}


bool BakeCustomizableObjectInstance(
	UCustomizableObjectInstance& InInstance,
	const FString& FileName,
	const FString& AssetPath,
	const bool bExportAllResources,
	const bool bGenerateConstantMaterialInstances,
	bool bHasPermissionToOverride,
	bool bIsUnattendedExecution,
	TArray<TPair<EPackageSaveResolutionType,UPackage*>>& OutSavedPackages)
{
	OutSavedPackages.Reset();
	
	// Ensure that the state of the COI provided is valid --------------------------------------------------------------------------------------------
	UCustomizableObject* InstanceCO = InInstance.GetCustomizableObject();

	// Ensure the CO of the COI is accessible 
	if (!InstanceCO || InstanceCO->GetPrivate()->IsLocked())
	{
		FCustomizableObjectEditorLogger::CreateLog(
		LOCTEXT("CustomizableObjectInstanceBakingUtils_LockedObject", "Please wait until the Customizable Object is compiled"))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();

		return false;
	}
	
	if (InstanceCO->GetPrivate()->Status.Get() == FCustomizableObjectStatus::EState::Loading)
	{
		FCustomizableObjectEditorLogger::CreateLog(
			LOCTEXT("CustomizableObjectInstanceBakingUtils_LoadingObject","Please wait until the Customizable Object is loaded"))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();

		return false;
	}
	
	if (!ValidateProvidedFileName(FileName))
	{
		UE_LOG(LogMutable, Error, TEXT("The FileName for the instance baking is not valid."));
		return false;
	}

	if (!ValidateProvidedAssetPath(FileName,AssetPath,InstanceCO))
	{
		UE_LOG(LogMutable, Error, TEXT("The AssetPath for the instance baking is not valid."));
		return false;
	}
	
	// Exit early if the provided instance does not have a skeletal mesh
	if (!InInstance.HasAnySkeletalMesh())
	{
		UE_LOG(LogMutable, Error, TEXT("The provided instance does not have an skeletal mesh."));
		return false;
	}

	// Early out if the Instance ModelResources is not valid.
	const UModelResources* ModelResources = InstanceCO->GetPrivate()->GetModelResources();
	if (!ModelResources)
	{
		UE_LOG(LogMutable, Error, TEXT("The ModelResources from the Customizable Object is not valid."));
		return false;
	}

	// COI Validation completed : Proceed with the baking operation ----------------------------------------------------------------------------------
	
	// Notify of better configuration -> Continue operation normally
	if (ModelResources->bCompiledWithHDTextureCompression == false)
	{
		FCustomizableObjectEditorLogger::CreateLog(
		LOCTEXT("CustomizableObjectInstanceBakingUtils_LowQualityTextures", "The Customizable Object wasn't compiled with high quality textures. For the best baking results, change the Texture Compression setting and recompile it."))
		.Category(ELoggerCategory::COInstanceBaking)
		.CustomNotification()
		.Notification(true)
		.Log();
	}
	
	// Set the overriding flag to true or false:
	//	- We ask the user at least once about if he is willing to override old baked data (attended operation) and this makes the flag change 
	//	- We never ask the user (and therefore the value in bUsedGrantedOverridingRights never changes) when we work in Unattended mode.
	bool bUsedGrantedOverridingRights = bHasPermissionToOverride;
	if (FApp::IsUnattended() || GIsRunningUnattendedScript)
	{
		bIsUnattendedExecution = true;
	}

	// Prefixes used for UMaterial assets. 
	const TArray<FString> MaterialResourcePrefixes = {MaterialAssetPrefix, MaterialInstanceAssetPrefix, MaterialDynamicInstanceAssetPrefix, MaterialConstantInstanceAssetPrefix};

	// Array with the already processed resource names and resources (UObjects)
	TArray<FString> CachedResourceNames;
	TArray<UObject*> CachedObjects;
	
	const int32 NumObjectComponents = InstanceCO->GetComponentCount();
	for (int32 ObjectComponentIndex = 0; ObjectComponentIndex < NumObjectComponents; ++ObjectComponentIndex)
	{
		const FName ComponentName = InstanceCO->GetPrivate()->GetComponentName(FCustomizableObjectComponentIndex(ObjectComponentIndex));
		USkeletalMesh* Mesh = InInstance.GetComponentMeshSkeletalMesh(ComponentName);

		if (!Mesh)
		{
			continue;
		}
		
		FString ObjectBaseName = FileName;
		
		if (FileName.IsEmpty())
		{
			ObjectBaseName = ComponentName.ToString();
		}
		else
		{
			ObjectBaseName = FileName + TEXT("_") + ComponentName.ToString();
		}

		ObjectBaseName += TEXT("_");
		
		TMap<UObject*, UObject*> ReplacementMap;

		if (bExportAllResources)
		{
			UMaterialInstance* MaterialInstance;
			UMaterial* Material;
			UTexture* Texture;
			FString PackageName;
			UObject* DuplicatedObject;
			
			TArray<TMap<int, UTexture*>> TextureReplacementMaps;

			// Duplicate Textures found in the Material Instances of the SkeletalMesh so we can later assign them to the
			// duplicates of those material instances. At the end of the baking we will have a series of materials with the 
			// parameters set as the material instances they are based of.
			for (int32 m = 0; m < Mesh->GetMaterials().Num(); ++m)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[m].MaterialInterface;
				Material = Interface->GetMaterial();
				MaterialInstance = Cast<UMaterialInstance>(Interface);

				TextureReplacementMaps.AddDefaulted();
				
				if (Material != nullptr && MaterialInstance != nullptr)
				{
					TArray<FGuid> ParameterIds;
					TArray<FMaterialParameterInfo> ParameterInfoObjects;
					Material->GetAllTextureParameterInfo(ParameterInfoObjects, ParameterIds);
					
					for (int32 ParameterInfoIndex = 0; ParameterInfoIndex < ParameterInfoObjects.Num(); ParameterInfoIndex++)
					{
						const FName& ParameterName = ParameterInfoObjects[ParameterInfoIndex].Name;
						if (MaterialInstance->GetTextureParameterValue(ParameterName, Texture))
						{
							UTexture2D* SourceTexture = Cast<UTexture2D>(Texture);
							if (!SourceTexture)
							{
								continue;
							}
							
							// Check that the resource has not yet been processed, and if not, skip it
							if (CachedObjects.Contains(SourceTexture))
							{
								continue;
							}
							
							FString TextureName = SourceTexture->GetName();
							RemovePrefix(TextureName, {TextureAssetPrefix});
							ComposeResourceName(TextureAssetPrefix, ObjectBaseName, TextureName);
							MakeResourceNameUnique(TextureName, CachedResourceNames);
							
							EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
							if (!CreateAssetAtPath(AssetPath, TextureName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
							{
								return false;
							}
							
							bool bIsMutableTexture = false;
							for (UAssetUserData* UserData : *SourceTexture->GetAssetUserDataArray())
							{
								if (Cast<UMutableTextureMipDataProviderFactory>(UserData))
								{
									bIsMutableTexture = true;
									break;
								}
							}
							
							// Duplicating mutable generated textures
							if (bIsMutableTexture)
							{
								if (SourceTexture->GetPlatformData() && SourceTexture->GetPlatformData()->Mips.Num() > 0)
								{
									// Recover original name of the texture parameter value, now substituted by the generated Mutable texture
									UTexture* OriginalTexture = nullptr;
									MaterialInstance->Parent->GetTextureParameterValue(FName(*ParameterName.GetPlainNameString()), OriginalTexture);

									PackageName = AssetPath + FString("/") + TextureName;
									TMap<UObject*, UObject*> DummyReplacementMap;
									UTexture2D* DuplicatedTexture = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SourceTexture, TextureName, PackageName, OriginalTexture, true, DummyReplacementMap, bUsedGrantedOverridingRights);
									CachedResourceNames.Add(TextureName);
									CachedObjects.Add(DuplicatedTexture);
								
									TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DuplicatedTexture->GetPackage()};
									OutSavedPackages.Add(PackageToSave);

									if (OriginalTexture != nullptr)
									{
										TextureReplacementMaps[m].Add(ParameterInfoIndex, DuplicatedTexture);
									}
								}
							}
							else
							{
								// Duplicate the non-mutable textures of the Material instance (pass-through textures)
								
								PackageName = AssetPath + FString("/") + TextureName;
								TMap<UObject*, UObject*> DummyReplacementMap;
								DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Texture, TextureName, PackageName, true, DummyReplacementMap, bUsedGrantedOverridingRights, false);
								CachedResourceNames.Add(TextureName);
								CachedObjects.Add(DuplicatedObject);

								TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DuplicatedObject->GetPackage()};
								OutSavedPackages.Add(PackageToSave);

								UTexture* DupTexture = Cast<UTexture>(DuplicatedObject);
								TextureReplacementMaps[m].Add(ParameterInfoIndex, DupTexture);
							}
						}
					}
				}
			}


			// Duplicate the materials used by each material instance so that the replacement map has proper information 
			// when duplicating the material instances
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetMaterials().Num(); ++MaterialIndex)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
				Material = Interface ? Interface->GetMaterial() : nullptr;
				
				if (Material)
				{
					// Check that the resource has not yet been processed, and if not, skip it
					if (CachedObjects.Contains(Material))
					{
						continue;
					}

					FString MaterialName = Material->GetName();
					RemovePrefix(MaterialName, {MaterialAssetPrefix});
					ComposeResourceName(MaterialAssetPrefix, ObjectBaseName, MaterialName);
					MakeResourceNameUnique(MaterialName, CachedResourceNames);

					EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
					if (!CreateAssetAtPath(AssetPath, MaterialName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
					{
						return false;
					}

					PackageName = AssetPath + FString("/") + MaterialName;
					TMap<UObject*, UObject*> FakeReplacementMap;
					DuplicatedObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Material, MaterialName, PackageName, 
						false, FakeReplacementMap, bUsedGrantedOverridingRights, bGenerateConstantMaterialInstances);
					CachedResourceNames.Add(MaterialName);
					CachedObjects.Add(DuplicatedObject);
					ReplacementMap.Add(Interface, DuplicatedObject);

					TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DuplicatedObject->GetPackage()};
					OutSavedPackages.Add(PackageToSave);

					if (UMaterial* DuplicatedMaterial = Cast<UMaterial>(DuplicatedObject))
					{
						FUnrealBakeHelpers::CopyAllMaterialParameters<UMaterial>(*DuplicatedMaterial, *Interface, TextureReplacementMaps[MaterialIndex]);
					}
				}
			}
		}
		else
		{
			// Duplicate the material instances
			for (int32 MaterialIndex = 0; MaterialIndex < Mesh->GetMaterials().Num(); ++MaterialIndex)
			{
				UMaterialInterface* Interface = Mesh->GetMaterials()[MaterialIndex].MaterialInterface;
				UMaterial* ParentMaterial = Interface->GetMaterial();

				// Check that the resource has not yet been processed, and if not, skip it
				if (CachedObjects.Contains(Interface))
				{
					continue;
				}

				FString MaterialName;
				{
					MaterialName = ParentMaterial ? ParentMaterial->GetName() : MaterialAssetPrefix;

					RemovePrefix(MaterialName, MaterialResourcePrefixes);
					
					if (bGenerateConstantMaterialInstances && Interface->IsA<UMaterialInstance>())
					{
						// Change the prefix to the Material Constant Instance since in this situation the new asset based on the Interface
						// will be of the Material Constant Instance type
						ComposeResourceName(MaterialConstantInstanceAssetPrefix, ObjectBaseName, MaterialName);
					}
					else
					{
						if (Interface->IsA<UMaterial>())
						{
							ComposeResourceName(MaterialAssetPrefix, ObjectBaseName, MaterialName);
						}
						else if (Interface->IsA<UMaterialInstanceConstant>())
						{
							ComposeResourceName(MaterialConstantInstanceAssetPrefix, ObjectBaseName, MaterialName);
						}
						else if (Interface->IsA<UMaterialInstanceDynamic>())
						{
							ComposeResourceName(MaterialDynamicInstanceAssetPrefix, ObjectBaseName, MaterialName);
						}
						else
						{
							checkNoEntry();		// Invalid material type.
						}
					}
					
					MakeResourceNameUnique(MaterialName, CachedResourceNames);
				}

				EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
				if (!CreateAssetAtPath(AssetPath, MaterialName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
				{
					return false;
				}

				FString MatPkgName = AssetPath + FString("/") + MaterialName;
				UObject* DupMat = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Interface, MaterialName, 
					MatPkgName, false, ReplacementMap, bUsedGrantedOverridingRights, bGenerateConstantMaterialInstances);
				CachedObjects.Add(DupMat);
				CachedResourceNames.Add(MaterialName);

				TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DupMat->GetPackage()};
				OutSavedPackages.Add(PackageToSave);

				// Only need to duplicate the generate textures if the original material is a dynamic instance
				// If the material has Mutable textures, then it will be a dynamic material instance for sure
				if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Interface))
				{
					// Duplicate generated textures
					UMaterialInstanceDynamic* InstDynamic = Cast<UMaterialInstanceDynamic>(DupMat);
					UMaterialInstanceConstant* InstConstant = Cast<UMaterialInstanceConstant>(DupMat);

					if (InstDynamic || InstConstant)
					{
						for (int32 TextureIndex = 0; TextureIndex < MaterialInstance->TextureParameterValues.Num(); ++TextureIndex)
						{
							if (MaterialInstance->TextureParameterValues[TextureIndex].ParameterValue)
							{
								if (MaterialInstance->TextureParameterValues[TextureIndex].ParameterValue->HasAnyFlags(RF_Transient))
								{
									if (UTexture2D* SourceTexture = Cast<UTexture2D>(MaterialInstance->TextureParameterValues[TextureIndex].ParameterValue))
									{
										if (CachedObjects.Contains(SourceTexture))
										{
											UTexture* PrevTexture = Cast<UTexture>(CachedObjects[CachedResourceNames.Find(SourceTexture->GetName())]);
											check(PrevTexture);
											
											if (InstDynamic)
											{
												InstDynamic->SetTextureParameterValue(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
											}
											else if (InstConstant)
											{
												InstConstant->SetTextureParameterValueEditorOnly(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, PrevTexture);
											}
											
											continue;
										}

										// Generate a new name for the duplicated texture so it does not collide with the original one
										FString TextureName = SourceTexture->GetName();
										RemovePrefix(TextureName, {TextureAssetPrefix});
										ComposeResourceName(TextureAssetPrefix, ObjectBaseName, TextureName);
										MakeResourceNameUnique(TextureName, CachedResourceNames);
										
										EPackageSaveResolutionType TextureSaveType = EPackageSaveResolutionType::None;
										if (!CreateAssetAtPath(AssetPath, TextureName, bUsedGrantedOverridingRights, bIsUnattendedExecution, TextureSaveType))
										{
											return false;
										}

										FString TexPkgName = AssetPath + FString("/") + TextureName;
										TMap<UObject*, UObject*> FakeReplacementMap;
										UTexture2D* DupTex = FUnrealBakeHelpers::BakeHelper_CreateAssetTexture(SourceTexture, TextureName, TexPkgName, nullptr, false, FakeReplacementMap, bUsedGrantedOverridingRights);
										CachedObjects.Add(DupTex);
										CachedResourceNames.Add(TextureName);

										TPair<EPackageSaveResolutionType, UPackage*> TexturePackageToSave {TextureSaveType, DupTex->GetPackage()};
										OutSavedPackages.Add(TexturePackageToSave);
										
										if (InstDynamic)
										{
											InstDynamic->SetTextureParameterValue(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
										}
										else if(InstConstant)
										{
											InstConstant->SetTextureParameterValueEditorOnly(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, DupTex);
										}
									}
									else
									{
										UE_LOG(LogMutable, Error, TEXT("A Mutable texture that is not a Texture2D has been found while baking a CustomizableObjectInstance."));
									}
								}
								else
								{
									// If it's not transient it's not a mutable texture, it's a pass-through texture
									// Just set the original texture
									if (InstDynamic)
									{
										InstDynamic->SetTextureParameterValue(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, MaterialInstance->TextureParameterValues[TextureIndex].ParameterValue);
									}
									else if (InstConstant)
									{
										InstConstant->SetTextureParameterValueEditorOnly(MaterialInstance->TextureParameterValues[TextureIndex].ParameterInfo.Name, MaterialInstance->TextureParameterValues[TextureIndex].ParameterValue);
									}
								}
							}
						}
					}
				}
			}
		}

		// Get the clean name of the Skeletal mesh (without the embedded CO name)
		FString SkeletalMeshName = Mesh->GetName();
		if (Mesh->HasAnyFlags(RF_Transient))
		{
			const FString CustomizableObjectName = InInstance.GetCustomizableObject()->GetName() + TEXT("_");
			SkeletalMeshName = SkeletalMeshName.Replace(*CustomizableObjectName,TEXT(""));
		}
		RemovePrefix(SkeletalMeshName, {SkeletalMeshAssetPrefix});
		ComposeResourceName(SkeletalMeshAssetPrefix, ObjectBaseName, SkeletalMeshName);
		MakeResourceNameUnique(SkeletalMeshName, CachedResourceNames);
		
		// Skeletal Mesh's Skeleton
		if (USkeleton* Skeleton = Mesh->GetSkeleton())
		{
			const bool bTransient = Skeleton->GetPackage() == GetTransientPackage();

			// Duplicate only if transient or export all assets.
			if (bTransient || bExportAllResources)
			{
				FString SkeletonName = Mesh->GetName();
				RemovePrefix(SkeletonName, {SkeletalMeshAssetPrefix});
				ComposeResourceName(SkeletonAssetPrefix, ObjectBaseName, SkeletonName);
				MakeResourceNameUnique(SkeletonName, CachedResourceNames);
				
				EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
				if (!CreateAssetAtPath(AssetPath, SkeletonName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
				{
					return false;
				}

				FString SkeletonPkgName = AssetPath + FString("/") + SkeletonName;
				UObject* DuplicatedSkeleton = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Skeleton, SkeletonName, 
					SkeletonPkgName, false, ReplacementMap, bUsedGrantedOverridingRights, false);

				CachedObjects.Add(DuplicatedSkeleton);
				TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DuplicatedSkeleton->GetPackage()};
				OutSavedPackages.Add(PackageToSave);
				ReplacementMap.Add(Skeleton, DuplicatedSkeleton);
			}
		}

		// Skeletal Mesh's Physics Asset
		bool bNewPhysicsAssetCreated = false;
		if (UPhysicsAsset* PhysicsAsset = Mesh->GetPhysicsAsset())
		{
			const bool bTransient = PhysicsAsset->GetPackage() == GetTransientPackage();

			// Duplicate only if transient or export all assets.
			if (bTransient || bExportAllResources)
			{
				FString PhysicsAssetName = Mesh->GetName();
				RemovePrefix(PhysicsAssetName, {SkeletalMeshAssetPrefix});
				ComposeResourceName(PhysicsAssetPrefix, ObjectBaseName, PhysicsAssetName);
				MakeResourceNameUnique(PhysicsAssetName, CachedResourceNames);
				
				EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
				if (!CreateAssetAtPath(AssetPath, PhysicsAssetName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
				{
					return false;
				}

				FString PhysicsAssetPkgName = AssetPath + FString("/") + PhysicsAssetName;
				UObject* DuplicatedPhysicsAsset = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(PhysicsAsset, PhysicsAssetName, 
					PhysicsAssetPkgName, false, ReplacementMap, bUsedGrantedOverridingRights, false);

				CachedObjects.Add(DuplicatedPhysicsAsset);
				TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DuplicatedPhysicsAsset->GetPackage()};
				OutSavedPackages.Add(PackageToSave);
				ReplacementMap.Add(PhysicsAsset, DuplicatedPhysicsAsset);

				bNewPhysicsAssetCreated = true;
			}
		}


		// Skeletal Mesh
		EPackageSaveResolutionType SaveType = EPackageSaveResolutionType::None;
		if (!CreateAssetAtPath(AssetPath, SkeletalMeshName, bUsedGrantedOverridingRights, bIsUnattendedExecution, SaveType))
		{
			return false;
		}

		FString PkgName = AssetPath + FString("/") + SkeletalMeshName;
		UObject* DupObject = FUnrealBakeHelpers::BakeHelper_DuplicateAsset(Mesh, SkeletalMeshName, PkgName, 
			false, ReplacementMap, bUsedGrantedOverridingRights, false);
		CachedObjects.Add(DupObject);

		TPair<EPackageSaveResolutionType, UPackage*> PackageToSave {SaveType, DupObject->GetPackage()};
		OutSavedPackages.Add(PackageToSave);

		Mesh->Build();

		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(DupObject))
		{
			SkeletalMesh->ResetLODInfo();
			for (int32 LODIndex = 0; LODIndex < Mesh->GetLODNum(); ++LODIndex)
        	{
        		SkeletalMesh->AddLODInfo(*Mesh->GetLODInfo(LODIndex));
        	}
		
			SkeletalMesh->GetImportedModel()->SkeletalMeshModelGUID = FGuid::NewGuid();

			// Duplicate AssetUserData
			{
				const TArray<UAssetUserData*>* AssetUserDataArray = Mesh->GetAssetUserDataArray();
				for (const UAssetUserData* AssetUserData : *AssetUserDataArray)
				{
					if (AssetUserData)
					{
						// Duplicate to change ownership
						UAssetUserData* NewAssetUserData = Cast<UAssetUserData>(StaticDuplicateObject(AssetUserData, SkeletalMesh));
						SkeletalMesh->AddAssetUserData(NewAssetUserData);
					}
				}
			}

			// Add Instance Info in a custom AssetUserData
			{
				const FCustomizableInstanceComponentData* ComponentData = InInstance.GetPrivate()->GetComponentData(ComponentName);
				check(ComponentData);
				
				if (InInstance.GetAnimationGameplayTags().Num() ||
					ComponentData->AnimSlotToBP.Num())
				{
					UCustomizableObjectInstanceUserData* InstanceData = NewObject<UCustomizableObjectInstanceUserData>(SkeletalMesh, NAME_None, RF_Public | RF_Transactional);
					InstanceData->AnimationGameplayTag = InInstance.GetAnimationGameplayTags();

					for (const TTuple<FName, TSoftClassPtr<UAnimInstance>>& AnimSlot : ComponentData->AnimSlotToBP)
					{
						FCustomizableObjectAnimationSlot AnimationSlot;
						AnimationSlot.Name = AnimSlot.Key;
						AnimationSlot.AnimInstance = AnimSlot.Value;
				
						InstanceData->AnimationSlots.Add(AnimationSlot);
					}

					SkeletalMesh->AddAssetUserData(InstanceData);
				}
			}

			// Copy LODSettings from the Reference Skeletal Mesh
			{
				if (ModelResources->ReferenceSkeletalMeshesData.IsValidIndex(ObjectComponentIndex))
				{
					USkeletalMeshLODSettings* LODSettings = ModelResources->ReferenceSkeletalMeshesData[ObjectComponentIndex].SkeletalMeshLODSettings;
					SkeletalMesh->SetLODSettings(LODSettings);
				}
			}

			// Set the physics asset preview mesh if the SkeletalMesh physics assets has been generated as part of the bake.
			if (SkeletalMesh->GetPhysicsAsset() && bNewPhysicsAssetCreated)
			{
				SkeletalMesh->GetPhysicsAsset()->SetPreviewMesh(SkeletalMesh);
			}

			// Generate render data
			SkeletalMesh->Build();
		}

		// Remove duplicated UObjects from Root (previously added to avoid objects from being GC in the middle of the bake process)
		for (UObject* Obj : CachedObjects)
		{
			Obj->RemoveFromRoot();
		}
	}

	// Save the packages generated during the baking operation  --------------------------------------------------------------------------------------
	
	// Complete the baking by saving the packages we have cached during the baking operation
	if (OutSavedPackages.Num())
	{
		// Prepare the list of assets we want to provide to "PromptForCheckoutAndSave" for saving
		TArray<UPackage*> PackagesToSaveProxy;
		PackagesToSaveProxy.Reserve(OutSavedPackages.Num());
		for (TPair<EPackageSaveResolutionType, UPackage*> DataToSave : OutSavedPackages)
		{
			PackagesToSaveProxy.Push(DataToSave.Value);
		}

		// List of packages that could not be saved
		TArray<UPackage*> FailedToSavePackages;
		const bool bWasSavingSuccessful = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSaveProxy, false, !bIsUnattendedExecution, &FailedToSavePackages, false, false) == FEditorFileUtils::EPromptReturnCode::PR_Success;

		// Remove all packages that were going to be saved but failed to do so
		const int32 RemovedPackagesCount = OutSavedPackages.RemoveAll([&](const TPair<EPackageSaveResolutionType, UPackage*> ToSavePackage)
		{
			return FailedToSavePackages.Contains(ToSavePackage.Value);
		});
		OutSavedPackages.Shrink();

		return RemovedPackagesCount > 0 ? false : bWasSavingSuccessful;
	}
	
	// The operation will fail if no packages are there to save
	return false;
}

#undef LOCTEXT_NAMESPACE 
