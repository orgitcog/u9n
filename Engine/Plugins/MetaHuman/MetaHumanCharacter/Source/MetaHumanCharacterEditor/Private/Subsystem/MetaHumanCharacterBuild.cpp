// Copyright Epic Games, Inc. All Rights Reserved.
#include "Subsystem/MetaHumanCharacterBuild.h"

#include "Subsystem/MetaHumanCharacterSkelmeshHelper.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterTextureSynthesis.h"
#include "MetaHumanCharacterBodyTextureUtils.h"
#include "MetaHumanCharacterSkelMeshUtils.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanCharacterAnalytics.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "DNACalibDNAReader.h"
#include "Commands/DNACalibSetLODsCommand.h"
#include "Framework/Notifications/NotificationManager.h"
#include "IMessageLogListing.h"
#include "Logging/MessageLog.h"
#include "MessageLogModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/AssetManager.h"
#include "PackageTools.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Framework/Application/SlateApplication.h"
#include "SkeletalMeshTypes.h"
#include "Rendering/SkeletalMeshModel.h"
#include "TextureSourceDataUtils.h"
#include "TextureImportSettings.h"
#include "LODUtilities.h"
#include "Algo/Transform.h"
#include "Algo/AnyOf.h"
#include "UDynamicMesh.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/CreateNewAssetUtilityFunctions.h"
#include "GeometryScript/MeshBoneWeightFunctions.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "TextureCompiler.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "Misc/MessageDialog.h"
#include "Misc/EngineVersion.h"
#include "BlueprintCompilationManager.h"
#include "AssetCompilingManager.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "LevelSequence.h"
#include "MovieSceneSequencePlayer.h"


#define LOCTEXT_NAMESPACE "MetaHumanCharacterBuild"

namespace UE::MetaHuman::Build
{
	static TSharedPtr<SNotificationItem> ShowNotification(const FText& InMessage, SNotificationItem::ECompletionState InState)
	{
		if (!FSlateApplication::IsInitialized())
		{
			return nullptr;
		}

		FNotificationInfo Info{ InMessage };
		Info.FadeInDuration = 0.1f;
		Info.FadeOutDuration = 3.0f;

		if (InState == SNotificationItem::CS_Pending)
		{
			Info.bFireAndForget = false;
			Info.bUseThrobber = true;
		}
		else
		{
			Info.ExpireDuration = 8.0f;
			Info.bFireAndForget = true;
			Info.bUseThrobber = false;
		}

		Info.bUseSuccessFailIcons = true;
		Info.bUseLargeFont = true;

		TSharedPtr<SNotificationItem> NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationItem->SetCompletionState(InState);

		if (InState != SNotificationItem::CS_Pending)
		{
			NotificationItem->ExpireAndFadeout();
		}

		if (InState == SNotificationItem::ECompletionState::CS_Fail)
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("%s"), *InMessage.ToString());
		}
		else
		{
			UE_LOG(LogMetaHumanCharacterEditor, Display, TEXT("%s"), *InMessage.ToString());
		}

		return NotificationItem;
	}

	static UPackage* FindOrCreatePackage(const FString& InAssetRootPath, const FString& InAssetName, UClass* InAssetClass)
	{
		const FString PackagePath = InAssetRootPath / InAssetName;
		return UPackageTools::FindOrCreatePackageForAssetType(FName(*PackagePath), InAssetClass);
	}

	static UObject* CreateNewGeneratedAsset(
		const FString& InAssetRootPath,
		const FString& InAssetName,
		UClass* InAssetClass,
		const UObject* InSourceObject,
		bool bMakePackageTransient)
	{
		UPackage* AssetPackage = bMakePackageTransient ? GetTransientPackage() : FindOrCreatePackage(InAssetRootPath, InAssetName, InAssetClass);

		// Attempt to load an object from this package to see if one already exists
		const FString AssetPath = AssetPackage->GetPathName() + TEXT(".") + InAssetName;
		UObject* ExistingAsset = LoadObject<UObject>(AssetPackage, *AssetPath, nullptr, LOAD_NoWarn);

		// Rename any existing object out of the way
		if (ExistingAsset)
		{
			if (!ExistingAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors))
			{
				return nullptr;
			}
		}

		const EObjectFlags DefaultFlags = bMakePackageTransient ? RF_Transient : (RF_Public | RF_Standalone);

		UObject* NewAsset = nullptr;
		if (InSourceObject)
		{
			check(InSourceObject->GetClass() == InAssetClass);

			NewAsset = DuplicateObject<UObject>(InSourceObject, AssetPackage, *InAssetName);

			NewAsset->ClearFlags(RF_AllFlags);
			NewAsset->SetFlags(DefaultFlags);
		}
		else
		{
			NewAsset = NewObject<UObject>(AssetPackage, InAssetClass, *FPackageName::GetShortName(AssetPackage), DefaultFlags);
		}

		if (!bMakePackageTransient)
		{
			NewAsset->MarkPackageDirty();
		}

		if (!ExistingAsset && !bMakePackageTransient)
		{
			FAssetRegistryModule::AssetCreated(NewAsset);
		}
		return NewAsset;
	}

	static USkeletalMesh* MergeHeadAndBody(
		TNotNull<USkeletalMesh*> InFaceMesh,
		TNotNull<USkeletalMesh*> InBodyMesh,
		UObject* InOuter,
		const FString& InAssetPathAndName)
	{
		const TArray<TPair<int32, int32>> BodyFaceLodPairing =
		{
			{ 0, 1 },
			{ 1, 3 },
			{ 2, 5 },
			{ 3, 7 },
		};

		const int32 FaceLODCount = InFaceMesh->GetLODNum();
		const int32 BodyLODCount = InBodyMesh->GetLODNum();

		// Quick check if LOD pairing is applicable
		for (const TPair<int32, int32>& Pair : BodyFaceLodPairing)
		{
			if (Pair.Key >= BodyLODCount || Pair.Value >= FaceLODCount)
			{
				return nullptr;
			}
		}

		// Creates dynamic from a skeletal mesh
		auto SkelMeshToDynamic = [](TNotNull<USkeletalMesh*> SkelMesh, int32 LOD) -> UDynamicMesh*
			{
				UDynamicMesh* DynamicMesh = NewObject<UDynamicMesh>();

				FGeometryScriptCopyMeshFromAssetOptions AssetOptions;

				FGeometryScriptMeshReadLOD RequestedLOD;
				RequestedLOD.LODType = EGeometryScriptLODType::SourceModel;
				RequestedLOD.LODIndex = LOD;

				EGeometryScriptOutcomePins Result;

				UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromSkeletalMesh(
					SkelMesh,
					DynamicMesh,
					AssetOptions,
					RequestedLOD,
					Result
				);

				if (Result != EGeometryScriptOutcomePins::Success)
				{
					return nullptr;
				}

				return DynamicMesh;
			};

		// Use body skeleton as target skeleton
		USkeleton* TargetSkeleton = InBodyMesh->GetSkeleton();

		// List of merged dynamic meshes per LOD
		TArray<UDynamicMesh*> MergedLODs;

		// Create merged dynamic mesh for each face - body LOD pair
		for (const TPair<int32, int32>& Pair : BodyFaceLodPairing)
		{
			// Body mesh for this LOD
			UDynamicMesh* BodyMeshLOD = SkelMeshToDynamic(InBodyMesh, Pair.Key);

			if (!BodyMeshLOD)
			{
				return nullptr;
			}
			
			// Face mesh for this LOD
			UDynamicMesh* FaceMeshLOD = SkelMeshToDynamic(InFaceMesh, Pair.Value);

			if (!FaceMeshLOD)
			{
				return nullptr;
			}

			// Find face to body mapping
			TArray<FGeometryScriptBoneInfo> FaceBonesInfo;
			UGeometryScriptLibrary_MeshBoneWeightFunctions::GetAllBonesInfo(FaceMeshLOD, FaceBonesInfo);

			TArray<FGeometryScriptBoneInfo> BodyBonesInfo;
			UGeometryScriptLibrary_MeshBoneWeightFunctions::GetAllBonesInfo(BodyMeshLOD, BodyBonesInfo);
			
			TArray<int32> FaceBonesToBodyBonesMap;
			FaceBonesToBodyBonesMap.AddUninitialized(FaceBonesInfo.Num());
			for (int32 FaceBoneIndex = 0; FaceBoneIndex < FaceBonesInfo.Num(); ++FaceBoneIndex)
			{
				const FName FaceBoneName = FaceBonesInfo[FaceBoneIndex].Name;
				const FGeometryScriptBoneInfo* BodyBoneInfo = BodyBonesInfo.FindByPredicate([FaceBoneName](const FGeometryScriptBoneInfo& BoneInfo)
					{
						return BoneInfo.Name == FaceBoneName;
					});
				if (BodyBoneInfo)
				{
					FaceBonesToBodyBonesMap[FaceBoneIndex] = BodyBoneInfo->Index;
				}
				else
				{
					FaceBonesToBodyBonesMap[FaceBoneIndex] = -1;
					if (FaceBonesInfo[FaceBoneIndex].ParentIndex < 0)
					{
						// root joint of face needs to map to the body
						return nullptr;
					}
				}
			}

			for (int32 FaceBoneIndex = 0; FaceBoneIndex < FaceBonesInfo.Num(); ++FaceBoneIndex)
			{
				int32 MappedFaceBoneIndex = FaceBoneIndex;
				while (FaceBonesToBodyBonesMap[MappedFaceBoneIndex] < 0)
				{
					MappedFaceBoneIndex = FaceBonesInfo[MappedFaceBoneIndex].ParentIndex;
				}
				FaceBonesToBodyBonesMap[FaceBoneIndex] = FaceBonesToBodyBonesMap[MappedFaceBoneIndex];
			}
			
			// set face skinning
			for (int32 VertexID = 0; VertexID < FaceMeshLOD->GetMeshRef().VertexCount(); ++VertexID)
			{
				TArray<FGeometryScriptBoneWeight> FaceBoneWeights;
				bool bHasValidBoneWeights;
				UGeometryScriptLibrary_MeshBoneWeightFunctions::GetVertexBoneWeights(FaceMeshLOD, VertexID, FaceBoneWeights, bHasValidBoneWeights);
				if (bHasValidBoneWeights)
				{
					TArray<FGeometryScriptBoneWeight> NewFaceBoneWeights;
					for (int32 Idx = 0; Idx < FaceBoneWeights.Num(); ++Idx)
					{
						const FGeometryScriptBoneWeight& FaceBoneWeight = FaceBoneWeights[Idx];
						int32 BoneIndex = FaceBonesToBodyBonesMap[FaceBoneWeight.BoneIndex];
						bool Found = false;
						for (int32 NewIdx = 0; NewIdx < NewFaceBoneWeights.Num(); ++NewIdx)
						{
							if (NewFaceBoneWeights[NewIdx].BoneIndex == BoneIndex)
							{
								NewFaceBoneWeights[NewIdx].Weight += FaceBoneWeight.Weight;
								Found = true;
							}
						}
						if (!Found)
						{
							NewFaceBoneWeights.Add(FGeometryScriptBoneWeight(BoneIndex, FaceBoneWeight.Weight));
						}
					}
					if (NewFaceBoneWeights.Num() > 0)
					{
						bool bIsValidVertexID;
						UGeometryScriptLibrary_MeshBoneWeightFunctions::SetVertexBoneWeights(FaceMeshLOD, VertexID, NewFaceBoneWeights, bIsValidVertexID);
					}
				}
			}

			// Remove joints from the face mesh
			FaceMeshLOD = UGeometryScriptLibrary_MeshBoneWeightFunctions::DiscardBonesFromMesh(FaceMeshLOD);

			FGeometryScriptAppendMeshOptions AppendOptions;

			// Combine body and face meshes
			UDynamicMesh* MergedLOD = UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
				BodyMeshLOD,
				FaceMeshLOD,
				FTransform::Identity,
				false,
				AppendOptions);

			MergedLODs.Add(MergedLOD);
		}

		// Resulting skel mesh
		USkeletalMesh* MergedSkelMesh = nullptr;

		FString AssetPathAndName = InAssetPathAndName;

		// Generate unique path for the skeletal mesh, if not already provided
		if (!InOuter && AssetPathAndName.IsEmpty())
		{
			EGeometryScriptOutcomePins Result;
			FGeometryScriptUniqueAssetNameOptions Options;
			FString UniqueAssetName;

			UGeometryScriptLibrary_CreateNewAssetFunctions::CreateUniqueNewAssetPathName(
				FPackageName::GetLongPackagePath(InBodyMesh->GetPathName()),
				TEXT("CombinedSkelMesh"),
				AssetPathAndName,
				UniqueAssetName,
				Options,
				Result);

			if (Result != EGeometryScriptOutcomePins::Success)
			{
				return nullptr;
			}
		}

		// Create skel mesh asset
		{
			FGeometryScriptCreateNewSkeletalMeshAssetOptions Options;
			Options.bUseOriginalVertexOrder = true;
			Options.bUseMeshBoneProportions = true;

			EGeometryScriptOutcomePins Result;

			if (AssetPathAndName.IsEmpty())
			{
				MergedSkelMesh = UE::MetaHuman::Build::CreateNewIncompleteSkeletalIncludingMeshDescriptions(
					InOuter,
					MergedLODs,
					TargetSkeleton,
					Options,
					Result);
			}
			else
			{
				MergedSkelMesh = UGeometryScriptLibrary_CreateNewAssetFunctions::CreateNewSkeletalMeshAssetFromMeshLODs(
					MergedLODs,
					TargetSkeleton,
					AssetPathAndName,
					Options,
					Result);
			}

			if (Result != EGeometryScriptOutcomePins::Success)
			{
				return nullptr;
			}
		}

		// TODO: We need any material on the skel mesh to avoid raising exceptions, so this will suffice for now.
		MergedSkelMesh->SetMaterials(InBodyMesh->GetMaterials());

		return MergedSkelMesh;
	}

	// Check if the pipeline is about to overwrite pre 5.6 MH Common assets in the given 
	static bool ShouldWriteInTargetFolders(const FString& InRootPath, const FString& InCommonAssetsPath)
	{
		FString PathToMHCommonSkeleton = TEXT("/Game/MetaHumans/Common/Female/Medium/NormalWeight/Body/metahuman_base_skel.metahuman_base_skel");
		const UObject* CommonSkeletonAsset = LoadObject<UObject>(nullptr, *PathToMHCommonSkeleton);
		if (!CommonSkeletonAsset)
		{
			return true;
		}
		
		if (FMetaHumanCharacterEditorBuild::MetaHumanAssetMetadataVersionIsCompatible(TNotNull<const UObject*>(CommonSkeletonAsset)))
		{
			return true;
		}
		
		// Get all MetaHumans installed in the target folders
		const TArray<FInstalledMetaHuman> DefaultInstalledMetaHumans = FMetaHumanProjectUtilities::GetInstalledMetaHumans();
		if (!DefaultInstalledMetaHumans.IsEmpty())
		{
			// Find any pre 5.6 MHs
			TArray<FInstalledMetaHuman> OldMetaHumans;
			for (const FInstalledMetaHuman& InstalledMH : DefaultInstalledMetaHumans)
			{
				if (const UObject* Asset = LoadObject<UObject>(nullptr, *InstalledMH.GetRootAsset(), nullptr, LOAD_Quiet | LOAD_EditorOnly))
				{
					if (!FMetaHumanCharacterEditorBuild::MetaHumanAssetMetadataVersionIsCompatible(TNotNull<const UObject*>(Asset)))
					{
						OldMetaHumans.Add(InstalledMH);
					}
				}
			}

			if (!OldMetaHumans.IsEmpty())
			{
				FString OldMetaHumansList;
				for (const FInstalledMetaHuman& OldMetaHuman : OldMetaHumans)
				{
					if (OldMetaHuman.GetCommonAssetPath() == InCommonAssetsPath)
					{
						OldMetaHumansList += OldMetaHuman.GetRootPackage().ToString() + TEXT("\n");
					}
				}

				if (!OldMetaHumansList.IsEmpty())
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("OldMetaHumansList"), FText::FromString(OldMetaHumansList));
					const FText Message = FText::Format(
						LOCTEXT("OldMetaHumanCommonAssetsWarning", "The assembly is about to write over MetaHuman Common assets which have been imported to the project using Quixel Bridge. "
							"Continuing may break functionality on these existing MetaHumans. Do you wish to continue?\n\n{OldMetaHumansList}"),
						Args
					);

					const EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::OkCancel, Message);
					return Result == EAppReturnType::Ok;
				}
			}
		}

		return true;
	}
}

void FMetaHumanCharacterEditorBuild::CollectUObjectReferencesFromStruct(const UStruct* StructType, const void* StructPtr, TArray<UObject*>& OutObjects)
{
	if (StructType == nullptr || StructPtr == nullptr)
	{
		return;
	}

	for (TFieldIterator<FProperty> PropIt(StructType/*, EFieldIteratorFlags::IncludeSuper*/); PropIt; ++PropIt)
	{
		FProperty* Property = *PropIt;
		if (Property == nullptr)
		{
			continue;
		}

		// UObject property
		if (FObjectProperty* ObjProp = CastField<FObjectProperty>(Property))
		{
			if (UObject* Obj = ObjProp->GetObjectPropertyValue_InContainer(StructPtr))
			{
				OutObjects.Add(Obj);
			}
		}
		// Soft object ptr
		else if (FSoftObjectProperty* SoftObjectProp = CastField<FSoftObjectProperty>(Property))
		{
			const FSoftObjectPtr& SoftObjectRef = SoftObjectProp->GetPropertyValue_InContainer(StructPtr);
			if (UObject* Obj = SoftObjectRef.LoadSynchronous())
			{
				OutObjects.Add(Obj);
			}
		}
		// Inlined struct
		else if (FStructProperty* StructProp = CastField<FStructProperty>(Property))
		{
			const void* InnerStructPtr = StructProp->ContainerPtrToValuePtr<void>(StructPtr);
			CollectUObjectReferencesFromStruct(StructProp->Struct, InnerStructPtr, OutObjects);
		}
		// Array property
		else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(StructPtr));

			if (FObjectProperty* InnerObj = CastField<FObjectProperty>(ArrayProp->Inner))
			{
				for (int32 i = 0; i < Helper.Num(); ++i)
				{
					if (Helper.IsValidIndex(i))
					{
						if (UObject* Obj = InnerObj->GetObjectPropertyValue(Helper.GetRawPtr(i)))
						{
							OutObjects.Add(Obj);
						}
					}
				}
			}
			else if (FSoftObjectProperty* InnerSoft = CastField<FSoftObjectProperty>(ArrayProp->Inner))
			{
				for (int32 i = 0; i < Helper.Num(); ++i)
				{
					if (Helper.IsValidIndex(i))
					{
						const FSoftObjectPtr& SoftObjectRef = InnerSoft->GetPropertyValue(Helper.GetRawPtr(i));
						if (UObject* Obj = SoftObjectRef.LoadSynchronous())
						{
							OutObjects.Add(Obj);
						}
					}
				}
			}
			else if (FStructProperty* InnerStruct = CastField<FStructProperty>(ArrayProp->Inner))
			{
				for (int32 i = 0; i < Helper.Num(); ++i)
				{
					if (Helper.IsValidIndex(i))
					{
						void* ElemPtr = Helper.GetRawPtr(i);
						CollectUObjectReferencesFromStruct(InnerStruct->Struct, ElemPtr, OutObjects);
					}
				}
			}
		}
		// TMap property
		else if (FMapProperty* MapProp = CastField<FMapProperty>(Property))
		{
			// Only check for values of UObjects for simplicity

			FScriptMapHelper Helper(MapProp, MapProp->ContainerPtrToValuePtr<void>(StructPtr));

			//FProperty* KeyProp = MapProp->KeyProp;
			FProperty* ValueProp = MapProp->ValueProp;

			if (FObjectProperty* ObjValue = CastField<FObjectProperty>(ValueProp))
			{
				for (FScriptMapHelper::FIterator It(Helper); It; ++It)
				{
					if (UObject* ValObj = ObjValue->GetObjectPropertyValue(Helper.GetValuePtr(It)))
					{
						OutObjects.Add(ValObj);
					}
				}
			}
		}
	}
}

void FMetaHumanCharacterEditorBuild::CollectDependencies(const TArray<UObject*>& InRootObjects, const TSet<FString>& InAllowedMountPoints, TSet<UObject*>& OutDependencies)
{
	// Helper archive to walk through all object dependencies
	// Based on the implementation FPackageReferenceFinder & FImportExportCollector
	struct FObjectDependencyFinder : public FArchiveUObject
	{
		FObjectDependencyFinder(const TSet<FString>& InAllowedMountPoints)
			: AllowedMountPoints(InAllowedMountPoints)
		{
			// Skip transient references, as these won't be duplicated.
			SetIsPersistent(true);

			// Serialization code should write to this archive rather than read from it
			SetIsSaving(true);

			// Serialize all properties, even ones that are the same as their defaults
			ArNoDelta = true;

			// Signal to custom serialize functions that we're only interested in object references.
			// This allows them to skip potentially time consuming serialization of other data.
			ArIsObjectReferenceCollector = true;

			// Bulk data never contains object references, so it can safely be skipped.
			ArShouldSkipBulkData = true;

			// We only want to find dependencies that will still be referenced after the object is 
			// duplicated.
			//
			// DuplicateTransient references are not copied during duplication, so any objects that 
			// are only referenced by DuplicateTransient properties would end up unreferenced after
			// being copied.
			//
			// Setting PPF_Duplicate here prevents the archive from following DuplicateTransient
			// references, so they won't be reported as dependencies.
			ArPortFlags = PPF_Duplicate;
		}

		virtual FArchive& operator<<(UObject*& ObjRef) override
		{
			if (IsValidObject(ObjRef))
			{
				References.Add(ObjRef);
			}

			return *this;
		}

		virtual FArchive& operator<<(FSoftObjectPath& Value) override
		{
			if (AllowedMountPoints.Contains(FPackageName::GetPackageMountPoint(Value.GetLongPackageName()).ToString()))
			{
				if (UObject* Obj = Value.TryLoad())
				{
					if (IsValidObject(Obj))
					{
						References.Add(Obj);
					}
				}
			}

			return *this;
		}

		bool IsValidObject(const UObject* InObject)
		{
			return	InObject != nullptr 
				&& InObject->GetPackage() != nullptr
				&& AllowedMountPoints.Contains(FPackageName::GetPackageMountPoint(InObject->GetPackage()->GetName()).ToString())
				//&& !InObject->HasAnyFlags(RF_Transient | RF_ClassDefaultObject)
					;
		}

		TArray<UObject*> References;
		const TSet<FString>& AllowedMountPoints;
	};

	// Do not follow references outside the following packages
	// The intention here is to identify newly assembled and plugin assets that potentially need to be duplicated
	TSet<FString> AllowedMountPoints;
	AllowedMountPoints.Add(UE_PLUGIN_NAME);
	AllowedMountPoints.Add(TEXT("Game"));
	AllowedMountPoints.Append(InAllowedMountPoints);

	// Initialize with the root object
	TArray<UObject*> PendingRefs;
	PendingRefs.Append(InRootObjects);

	// Keep track of all visited objects
	TSet<UObject*> RefsProcessed;

	// Iterate on all referenced objects recursively
	while (PendingRefs.Num())
	{
		UObject* Iter = PendingRefs.Pop();
		RefsProcessed.Add(Iter);

		FObjectDependencyFinder DependencyFinder(AllowedMountPoints);
		Iter->Serialize(DependencyFinder);
		for (UObject* Obj : DependencyFinder.References)
		{
			if (!RefsProcessed.Contains(Obj))
			{
				PendingRefs.Add(Obj);

				UObject* Outermost = Obj->GetOutermostObject();

				// Track only the outers
				//
				// BPGCs are explicitly excluded, because only the corresponding blueprints 
				// should be moved, duplicated, etc. The blueprints will handle updating their
				// generated classes themselves.
				if (Outermost 
					&& !Outermost->IsA<UBlueprintGeneratedClass>()
					&& !Outermost->HasAnyFlags(RF_ClassDefaultObject))
				{
					OutDependencies.Add(Outermost);
				}
			}
		}
	}
}

UE::MetaHuman::FMetaHumanAssetVersion FMetaHumanCharacterEditorBuild::GetMetaHumanAssetVersion()
{
	return UE::MetaHuman::FMetaHumanAssetVersion(FEngineVersion::Current().GetMajor(), FEngineVersion::Current().GetMinor());
}

bool FMetaHumanCharacterEditorBuild::MetaHumanAssetMetadataVersionIsCompatible(TNotNull<const UObject*> InAsset)
{
	const UE::MetaHuman::FMetaHumanAssetVersion CurrentMetaHumanAssetVersion = FMetaHumanCharacterEditorBuild::GetMetaHumanAssetVersion();
	const FName VersionTag("MHAssetVersion");
	if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(InAsset))
	{
		if (const FString* AssetMetaHumanVersionStr = Metadata->Find(VersionTag))
		{
			UE::MetaHuman::FMetaHumanAssetVersion AssetMetaHumanVersion(*AssetMetaHumanVersionStr);
			if (AssetMetaHumanVersion >= CurrentMetaHumanAssetVersion)
			{
				return true;
			}
		}
	}

	return false;
}

void FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(TNotNull<UObject*> InObject)
{
	TNotNull<UPackage*> DestPackage = InObject->GetPackage();
	FMetaData& DestMetadata = DestPackage->GetMetaData();

	const FName VersionTag("MHAssetVersion");
	const UE::MetaHuman::FMetaHumanAssetVersion VersionValue = FMetaHumanCharacterEditorBuild::GetMetaHumanAssetVersion();
	DestMetadata.SetValue(InObject, VersionTag, *(VersionValue.AsString()));
}

void FMetaHumanCharacterEditorBuild::DuplicateDepedenciesToNewRoot(
	const TSet<UObject*>& InDependencies, 
	const FString& InDependencyRootPath, 
	TSet<UObject*>& InOutObjectsToReplaceWithin, 
	TMap<UObject*, UObject*>& OutDuplicatedDependencies,
	TFunction<bool(const UObject*)> InIsAssetSupported)
{
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	FScopedSlowTask DuplicatingDependenciesSlowTask{ InDependencies.Num() * 2.0f, LOCTEXT("DuplicatingDependenciesTask", "Duplicating Dependencies") };
	DuplicatingDependenciesSlowTask.MakeDialog();

	TArray<TStrongObjectPtr<UObject>> NewAssets;

	// Perform the duplication of each collected dependency
	for (UObject* DependencyAsset : InDependencies)
	{
		if (!InIsAssetSupported(DependencyAsset))
		{
			// If the asset is not supported by the pipeline, set the duplicated dependency to nullptr so any references to it can be updated
			OutDuplicatedDependencies.Emplace(DependencyAsset, nullptr);

			continue;
		}

		DuplicatingDependenciesSlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("DuplicatingDependency", "Duplicating '{0}'"), FText::FromName(DependencyAsset->GetFName())));

		FString RelativePath;
		const FString PackageRoot = FPackageName::SplitPackageNameRoot(DependencyAsset->GetPackage()->GetFName(), &RelativePath);

		// Build the path to the destination package
		const FString AssetPackagePath = InDependencyRootPath / RelativePath;

		UPackage* TargetPackage = UPackageTools::FindOrCreatePackageForAssetType(FName{ AssetPackagePath }, DependencyAsset->GetClass());

		if (UObject* FoundObject = FindObject<UObject>(TargetPackage, *DependencyAsset->GetName()))
		{
			if (UBlueprint* FoundBlueprint = Cast<UBlueprint>(FoundObject))
			{
				// Skip any blueprints that fail to load
				if (FoundBlueprint->GeneratedClass == nullptr)
				{
					continue;
				}
			}

			const FName VersionTag("MHAssetVersion");
			UE::MetaHuman::FMetaHumanAssetVersion CurrentMetaHumanVersion = GetMetaHumanAssetVersion();
			if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(FoundObject))
			{
				if (const FString* AssetMetaHumanVersionStr = Metadata->Find(VersionTag))
				{
					UE::MetaHuman::FMetaHumanAssetVersion AssetMetaHumanVersion(*AssetMetaHumanVersionStr);
					if (AssetMetaHumanVersion >= CurrentMetaHumanVersion)
					{
						OutDuplicatedDependencies.Emplace(DependencyAsset, FoundObject);
						continue;
					}
				}
			}
		}

		// Duplicate the dependency to the target package so it becomes its new principal asset
		const FString AssetName = DependencyAsset->GetName();
		UObject* DuplicatedDependency = DuplicateObject(DependencyAsset, TargetPackage, *AssetName);
		NewAssets.Add(TStrongObjectPtr<UObject>(DuplicatedDependency));

		// Set the MH version for the new asset
		FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(DuplicatedDependency);

		DuplicatedDependency->MarkPackageDirty();
		AssetRegistry.AssetCreated(DuplicatedDependency);

		OutDuplicatedDependencies.Emplace(DependencyAsset, DuplicatedDependency);
	}

	TArray<UObject*> DuplicatedDependenciesArray;
	OutDuplicatedDependencies.GenerateValueArray(DuplicatedDependenciesArray);
	InOutObjectsToReplaceWithin.Append(DuplicatedDependenciesArray);

	TArray<ObjectTools::FReplaceRequest> ReplaceRequests;

	TArray<TArray<UObject*>> Storage;

	// Perform the actual reference placement
	for (const TPair<UObject*, UObject*>& DependencyPair : OutDuplicatedDependencies)
	{
		UObject* OldObject = DependencyPair.Key;
		UObject* ObjectToReplaceWith = DependencyPair.Value;

		DuplicatingDependenciesSlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("UpdatingReferences", "Updating references for '{0}'"), FText::FromName(OldObject->GetFName())));

		TArray<UObject*>& OldObjectArray = Storage.AddDefaulted_GetRef();
		OldObjectArray.Add(OldObject);

		ReplaceRequests.Emplace(ObjectTools::FReplaceRequest
			{
				.New = ObjectToReplaceWith,
				.Old = MakeArrayView(OldObjectArray)
			});

		if (UBlueprint* OldBlueprint = Cast<UBlueprint>(OldObject))
		{
			UClass* OldClass = OldBlueprint->GeneratedClass;
			UClass* ClassToReplaceWith = ObjectToReplaceWith ? CastChecked<UBlueprint>(ObjectToReplaceWith)->GeneratedClass : nullptr;

			TArray<UObject*>& OldClassArray = Storage.AddDefaulted_GetRef();
			OldClassArray.Add(OldClass);

			// Replace any references to the blueprint generated classes
			ReplaceRequests.Emplace(ObjectTools::FReplaceRequest
				{
					.New = ClassToReplaceWith,
					.Old = MakeArrayView(OldClassArray)
				});
		}
	}

	// Ensure compilation has finished for duplicated objects since replacing refs can potentially trigger further compilation requests
	FAssetCompilingManager::Get().FinishCompilationForObjects(DuplicatedDependenciesArray);

	ObjectTools::ForceReplaceReferences(ReplaceRequests, InOutObjectsToReplaceWithin);

	// Compile duplicated BPs. This enables LS animation on MetaHuman BP and only needs to happen when new asset is created
	for (const TStrongObjectPtr<UObject>& StrongNewAsset : NewAssets)
	{
		if (UBlueprint* DuplicatedBP = Cast<UBlueprint>(StrongNewAsset.Get()))
		{
			const FBPCompileRequest Request(DuplicatedBP, EBlueprintCompileOptions::SkipGarbageCollection, nullptr);
			FBlueprintCompilationManager::CompileSynchronously(Request);
			DuplicatedBP->PreEditChange(nullptr);
			DuplicatedBP->PostEditChange();
		}
	}
}

void FMetaHumanCharacterEditorBuild::ReportMessageLogErrors(bool bWasSuccessful, const FText& InSuccessMessageText, const FText& FailureMessageText)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	TSharedRef<IMessageLogListing> MetaHumanMessageLog = MessageLogModule.GetLogListing(UE::MetaHuman::MessageLogName);

	// If any errors were logged to the Message Log, consider the build a failure and pop 
	// up a notification with a button to show the Message Log.
	if (MetaHumanMessageLog->NumMessages(EMessageSeverity::Error) > 0)
	{
		MetaHumanMessageLog->NotifyIfAnyMessages(FailureMessageText, EMessageSeverity::Error, true);
		FMessageLog(UE::MetaHuman::MessageLogName).Error(FailureMessageText);
	}
	else if (!bWasSuccessful)
	{
		// The build failed but no errors were logged, so don't prompt the user to view
		// the Message Log.
		UE::MetaHuman::Build::ShowNotification(FailureMessageText, SNotificationItem::ECompletionState::CS_Fail);
		FMessageLog(UE::MetaHuman::MessageLogName).Error(FailureMessageText);
	}
	else if (!InSuccessMessageText.IsEmpty())
	{
		UE::MetaHuman::Build::ShowNotification(InSuccessMessageText, SNotificationItem::ECompletionState::CS_Success);
		FMessageLog(UE::MetaHuman::MessageLogName).Info(InSuccessMessageText);
	}

	if (!bWasSuccessful)
	{
		MetaHumanMessageLog->Open();
	}
}

void FMetaHumanCharacterEditorBuild::BuildMetaHumanCharacter(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterEditorBuildParameters& InParams)
{
	using namespace UE::MetaHuman::Build;

	// Clear the message log to avoid confusion with previous builds, and allow the below code to
	// detect if any errors were logged during this build.
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.GetLogListing(UE::MetaHuman::MessageLogName)->ClearMessages();

	// If we are using a Common path, then check if we are about to overwrite other MH assets
	if (!InParams.CommonFolderPath.IsEmpty())
	{
		if (!ShouldWriteInTargetFolders(InParams.AbsoluteBuildPath, InParams.CommonFolderPath))
		{
			const FText SuccessMessageText = FText::GetEmpty();
			const FText FailureMessageText = LOCTEXT("CanWriteInCommonFolderFailed", "Cannot write to the Common folder");
			ReportMessageLogErrors(false, SuccessMessageText, FailureMessageText);
			return;
		}
	}

	const FString CharacterName = InParams.NameOverride.IsEmpty() ? InMetaHumanCharacter->GetName() : InParams.NameOverride;
	const FString CharacterPath = InMetaHumanCharacter->GetPathName();

	FString TargetBuildPath = InParams.AbsoluteBuildPath.IsEmpty()
		? InMetaHumanCharacter->GetInternalCollection()->GetUnpackFolder()
		: InParams.AbsoluteBuildPath;

	// Path to location the MetaHuman assets will be stored
	const FString AbsBuildPath = TargetBuildPath / CharacterName;

	bool bGenerateCollectionAndInstanceAssets = true;

	if (InParams.PipelineOverride)
	{
		bGenerateCollectionAndInstanceAssets = InParams.PipelineOverride->GetEditorPipeline()->ShouldGenerateCollectionAndInstanceAssets();
	}
	else if (const UMetaHumanCollectionEditorPipeline* InternalPipeline = InMetaHumanCharacter->GetInternalCollection()->GetEditorPipeline())
	{
		bGenerateCollectionAndInstanceAssets = InternalPipeline->ShouldGenerateCollectionAndInstanceAssets();
	}

	const FString CollectionName = FString::Format(TEXT("{0}_Collection"), { CharacterName });
	// We still need to generate collection and instance, but they'll be initialized as transient if we don't want them as assets (they'll be GCed).
	const bool bMakePackageTransient = !bGenerateCollectionAndInstanceAssets;

	UMetaHumanCollection* Collection = Cast<UMetaHumanCollection>(CreateNewGeneratedAsset(
		AbsBuildPath,
		CollectionName,
		UMetaHumanCollection::StaticClass(),
		InMetaHumanCharacter->GetInternalCollection(),
		bMakePackageTransient));

	if (!Collection)
	{
		ShowNotification(LOCTEXT("ErrorCreatingCollection", "Failed to create MetaHuman Character Collection asset"), SNotificationItem::ECompletionState::CS_Fail);
		return;
	}

	if (InParams.PipelineOverride)
	{
		Collection->SetPipeline(InParams.PipelineOverride);
	}

	if (!Collection->GetPipeline())
	{
		ShowNotification(LOCTEXT("ErrorCollectionWithoutPipeline", "The generated MetaHuman Collection has no Pipeline assigned. Check the DefaultCharacterPipelineClass project setting."), SNotificationItem::ECompletionState::CS_Fail);
		return;
	}

	if (!Collection->GetEditorPipeline())
	{
		ShowNotification(LOCTEXT("ErrorCollectionWithoutEditorPipeline", "The Pipeline assigned to the generated MetaHuman Collection has no Editor Pipeline. Check the properties of the assigned Pipeline."), SNotificationItem::ECompletionState::CS_Fail);
		return;
	}

	if (!Collection->ContainsItem(InMetaHumanCharacter->GetInternalCollectionKey()))
	{
		ShowNotification(LOCTEXT("ErrorCharacterNotInCollection", "The generated MetaHuman Collection has no Character with the expected item key"), SNotificationItem::ECompletionState::CS_Fail);
		return;
	}

	// Use absolute path for every pipeline other than the default
	if (!bGenerateCollectionAndInstanceAssets)
	{
		Collection->UnpackPathMode = EMetaHumanCharacterUnpackPathMode::Absolute;
		Collection->UnpackFolderPath = AbsBuildPath;
	}

	if (Collection->GetEditorPipeline()->GetEditorActorClass())
	{
		const FString InstanceName = FString::Format(TEXT("{0}_Instance"), { CharacterName });

		// This Instance will reference the unpacked assets, so that the actor blueprint can find them
		UMetaHumanCharacterInstance* Instance = Cast<UMetaHumanCharacterInstance>(CreateNewGeneratedAsset(
			AbsBuildPath,
			InstanceName,
			UMetaHumanCharacterInstance::StaticClass(),
			Collection->GetDefaultInstance(),
			bMakePackageTransient));

		if (!Instance)
		{
			ShowNotification(LOCTEXT("ErrorCreatingInstance", "Failed to create MetaHuman Character Instance asset"), SNotificationItem::ECompletionState::CS_Fail);
			return;
		}

		Instance->SetMetaHumanCollection(Collection);

		// Select the character being built as the option for the Character slot
		Instance->SetSingleSlotSelection(UE::MetaHuman::CharacterPipelineSlots::Character, InMetaHumanCharacter->GetInternalCollectionKey());

		// Call PreBuildCollection for the pipeline to prepare the unpack operation		
		if (!Collection->GetMutablePipeline()->GetMutableEditorPipeline()->PreBuildCollection(Collection, InParams.NameOverride.IsEmpty() ? CharacterName : InParams.NameOverride))
		{
			const FText SuccessMessageText = FText::GetEmpty();
			const FText FailureMessageText = LOCTEXT("PreBuildCollectionFailed", "Pre Build Collection failed");
			ReportMessageLogErrors(false, SuccessMessageText, FailureMessageText);
			return;
		}

		// If there is a level sequence opened close it now and reopened it once assembly is complete
		// as there is potential for the assembly process to try and override assets being used by it
		// causing the engine to crash
		ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();
		FMovieSceneSequencePlaybackParams GlobalPosition;

		if (LevelSequence)
		{
			GlobalPosition = ULevelSequenceEditorBlueprintLibrary::GetGlobalPosition();
			ULevelSequenceEditorBlueprintLibrary::CloseLevelSequence();
		}

		ON_SCOPE_EXIT
		{
			if (LevelSequence)
			{
				ULevelSequenceEditorBlueprintLibrary::OpenLevelSequence(LevelSequence);
				ULevelSequenceEditorBlueprintLibrary::SetGlobalPosition(GlobalPosition);
			}
		};
		
		// For legacy export, newly created objects (Instance, Collection and Blueprint) and it's packages are marked as
		// transient as we don't want to export them but they'll be targets for the GC. Since BP compilation triggers
		// GC, we need to keep Instance and Collection in memory throughout this process. Currently the build process is
		// synchronous, so we can just reclaim the memory once we leave the scope.
		// In non-legacy export we don't care about GC as we're creating non-transient packages.
		TGCObjectsScopeGuard<UObject> GCGuard_InstanceAndPallete({ Instance, Collection });

		const FString BlueprintShortName = FString::Format(TEXT("BP_{0}"), { CharacterName });
		UBlueprint* GeneratedBlueprint = Collection->GetEditorPipeline()->WriteActorBlueprint(Collection->GetUnpackFolder() / BlueprintShortName);

		if (!GeneratedBlueprint)
		{
			const FText SuccessMessageText = FText::GetEmpty();
			const FText FailureMessageText = LOCTEXT("BlueprintGenerationFailed", "MetaHuman Blueprint generation failed");
			ReportMessageLogErrors(false, SuccessMessageText, FailureMessageText);

			return;
		}

		// Make sure that BP doesn't get GC'ed
		TGCObjectScopeGuard<UBlueprint> GCGuard_Blueprint(GeneratedBlueprint);

		bool bWasSuccessful = false;
		Collection->UnpackAssets(FOnMetaHumanCharacterAssetsUnpacked::CreateLambda(
			[&bWasSuccessful, Instance, Collection, GeneratedBlueprint, InMetaHumanCharacter, InParams](EMetaHumanCharacterAssetsUnpackResult Result)
			{
				bWasSuccessful = Result == EMetaHumanCharacterAssetsUnpackResult::Succeeded;

				Instance->OnInstanceUpdatedNative.AddLambda([Instance, Collection, bWasSuccessful, GeneratedBlueprint, InMetaHumanCharacter, InParams]()
					{
						if (!Instance)
						{
							return;
						}

						Instance->TryUnpack(Collection->GetUnpackFolder());

						// Update the BP if everything was unpacked successfully
						// NOTE: keep this block in the lambda as the UEFN pipeline requires UpdateActorBlueprint() to be called 
						// in the scope of UnpackAssets() to have a valid reference to the mounted UEFN plugin
						if (bWasSuccessful)
						{
							// Protect the objects in these TSets from being deleted in case a GC is triggered while copying
							FGCScopeGuard GCGuard;

							// Copy dependencies from the plugin assets
							// Note that we do this *before* updating the BP components since resolving references trigger multiple post edit events
							// This is particularly problematic for Groom Components who rebuild on every event and may result in crashes from race conditions
							if (Instance != nullptr && !InParams.CommonFolderPath.IsEmpty())
							{
								// Gather the root objects of the assembled output
								TArray<UObject*> RootObjects;
								const FInstancedStruct& AssemblyOutput = Instance->GetAssemblyOutput();
								CollectUObjectReferencesFromStruct(AssemblyOutput.GetScriptStruct(), AssemblyOutput.GetMemory(), RootObjects);
								RootObjects.Add(GeneratedBlueprint);

								TSet<UObject*> AllAssetDependencies;
								CollectDependencies(RootObjects, {}, AllAssetDependencies);

								TSet<UObject*> PluginDependencies;
								TSet<UObject*> UnpackedDependencies;

								// Select the packages of the objects that are in the plugin content
								Algo::CopyIf(AllAssetDependencies, PluginDependencies,
									[](const UObject* Obj) -> bool
									{
										const FName PackageRoot = FPackageName::GetPackageMountPoint(Obj->GetPackage()->GetName());
										return PackageRoot == UE_PLUGIN_NAME;
									});

								// Select the packages of the objects that are in the plugin content
								// The following is based on the assumption that unpacked assets were create in the project and do not reference any non-assembled assets
								const FString UnpackFolder = Collection->GetUnpackFolder();
								Algo::CopyIf(AllAssetDependencies, UnpackedDependencies,
									[UnpackFolder](const UObject* Obj) -> bool
									{
										const FString PackageName = Obj->GetPackage()->GetName();
										return PackageName.StartsWith(UnpackFolder);
									});

								// Add the root objects to get the full array of everything unpacked by the assembly
								UnpackedDependencies.Append(RootObjects);

								TMap<UObject*, UObject*> DuplicatedDependencies;
								DuplicateDepedenciesToNewRoot(PluginDependencies, InParams.CommonFolderPath, UnpackedDependencies, DuplicatedDependencies,
									[](const UObject* Obj) -> bool
									{
										return true;
									});
							}

							Collection->GetEditorPipeline()->UpdateActorBlueprint(Instance, GeneratedBlueprint);

							// Figure out the quality level being exported to add the MHExportQuality metadata tag in the blueprint
							// TODO: This information should come from the assembly tool but the FMetaHumanCharacterEditorBuildParameters is public and its layout can't be changed
							EMetaHumanQualityLevel QualityLevel = EMetaHumanQualityLevel::Cinematic;
							const UMetaHumanCharacterPaletteProjectSettings* BuildSettings = GetDefault<UMetaHumanCharacterPaletteProjectSettings>();
							for (const TPair<EMetaHumanQualityLevel, TSoftClassPtr<UMetaHumanCollectionPipeline>>& LegacyPipelinePair : BuildSettings->DefaultCharacterLegacyPipelines)
							{
								const TSoftClassPtr<UMetaHumanCollectionPipeline> LegacyPipeline = LegacyPipelinePair.Value;
								if (InParams.PipelineOverride->GetClass() == LegacyPipeline.Get())
								{
									QualityLevel = LegacyPipelinePair.Key;
									break;
								}
							}

							const FString QualityLevelStr = StaticEnum<EMetaHumanQualityLevel>()->GetAuthoredNameStringByValue((int64) QualityLevel);
							TNotNull<UPackage*> BlueprintPackage = GeneratedBlueprint->GetPackage();
							FMetaData& BlueprintMetadata = BlueprintPackage->GetMetaData();
							BlueprintMetadata.SetValue(GeneratedBlueprint, TEXT("MHExportQuality"), *QualityLevelStr);

							// Recompile the BP since some of its references were updated to point to the common assets
							const FBPCompileRequest Request(GeneratedBlueprint, EBlueprintCompileOptions::SkipGarbageCollection, nullptr);
							FBlueprintCompilationManager::CompileSynchronously(Request);
							GeneratedBlueprint->PreEditChange(nullptr);
							GeneratedBlueprint->PostEditChange();

							UE::MetaHuman::Analytics::RecordBuildPipelineCharacterEvent(InMetaHumanCharacter, InParams.PipelineOverride->GetClass());
						}
					});
			}));

		const FText SuccessMessageText = LOCTEXT("CharacterAssemblySucceeded", "MetaHuman Character assembly succeeded");
		const FText FailureMessageText = LOCTEXT("CharacterAssemblyFailed", "MetaHuman Character assembly failed");
		ReportMessageLogErrors(bWasSuccessful, SuccessMessageText, FailureMessageText);
	}
	else
	{
		const FText SuccessMessageText = FText::GetEmpty();
		const FText FailureMessageText = LOCTEXT("NoActorClassSupported", "No Actor class supported by the MetaHuman pipeline");
		ReportMessageLogErrors(false, SuccessMessageText, FailureMessageText);
	}
}

void FMetaHumanCharacterEditorBuild::StripLODsFromMesh(TNotNull<USkeletalMesh*> InSkeletalMesh, const TArray<int32>& InLODsToKeep)
{
	if (InLODsToKeep.IsEmpty())
	{
		return;
	}

	int32 MaxLOD = InSkeletalMesh->GetLODNum();

	// Check to see if there are any invalid LOD index to remove
	const bool bHasInvalidLOD = Algo::AnyOf(InLODsToKeep, [MaxLOD](int32 LODIndexToCheck)
	{
		return LODIndexToCheck >= MaxLOD;
	});

	if (bHasInvalidLOD)
	{
		return;
	}

	TArray<int32> LODsToRemove;
	USkeletalMeshLODSettings* LODSettings = InSkeletalMesh->GetLODSettings();

	// Find which LODs to remove
	for (int32 LODIndex = 0; LODIndex < MaxLOD; ++LODIndex)
	{
		const FSkeletalMeshLODInfo* LODInfo = InSkeletalMesh->GetLODInfo(LODIndex);
		if (!InLODsToKeep.Contains(LODIndex))
		{
			LODsToRemove.Add(LODIndex);
		}
	}

	if (LODsToRemove.IsEmpty())
	{
		return;
	}

	LODsToRemove.Sort();

	InSkeletalMesh->Modify();
	{
		// Scope the LOD removal so we can remove the materials after.
		// This forces a build of the skeletal mesh at the end of the scope
		// which allows materials to be changed without issues
		FScopedSkeletalMeshPostEditChange ScopedPostEditChange{ InSkeletalMesh };
		FSkeletalMeshUpdateContext UpdateContext;
		UpdateContext.SkeletalMesh = InSkeletalMesh;

		if (UDNAAsset* DNAAsset = InSkeletalMesh->GetAssetUserData<UDNAAsset>())
		{
			// Remove LDOs from the attached DNA using DNACalib SetLODs command

			TArray<uint16> LODsToSet;
			Algo::Transform(InLODsToKeep, LODsToSet, [](int32 LODIndex)
							{
								return static_cast<uint16>(LODIndex);
							});
			FDNACalibSetLODsCommand SetLODsCommand{ LODsToSet };

			TSharedPtr<FDNACalibDNAReader> OutputDNABehaviourReader = MakeShared<FDNACalibDNAReader>(DNAAsset->GetBehaviorReader().Get());
			SetLODsCommand.Run(OutputDNABehaviourReader.Get());
			DNAAsset->SetBehaviorReader(OutputDNABehaviourReader);

			TSharedPtr<FDNACalibDNAReader> OutputDNAGeometryReader = MakeShared<FDNACalibDNAReader>(DNAAsset->GetGeometryReader().Get());
			SetLODsCommand.Run(OutputDNAGeometryReader.Get());
			DNAAsset->SetGeometryReader(OutputDNAGeometryReader);
		}

		// Finally remove the LODs from the skeletal mesh
		for (int32 LODIndex = LODsToRemove.Num() - 1; LODIndex >= 0; LODIndex--)
		{
			const int32 LODToRemove = LODsToRemove[LODIndex];
			FLODUtilities::RemoveLOD(UpdateContext, LODToRemove);
		}
	}

	// now LODs are removed, we have to see if those materials are all used
	// max LOD has been modified, so update it
	MaxLOD = InSkeletalMesh->GetLODNum();

	const int32 MaterialCount = InSkeletalMesh->GetMaterials().Num();
	TBitArray<> UsedFlags(false, MaterialCount);

	for (int32 LODIndex = 0; LODIndex < MaxLOD; ++LODIndex)
	{
		const FSkeletalMeshLODInfo* LODInfo = InSkeletalMesh->GetLODInfo(LODIndex);

		if (LODInfo->LODMaterialMap.IsEmpty())
		{
			// If the LODMaterialMap is empty it means this LOD uses all materials from the skeletal mesh
			// Set all materials as being used and break
			UsedFlags.Init(true, MaterialCount);
			break;
		}
		else
		{
			for (int32 MaterialIndex : LODInfo->LODMaterialMap)
			{
				if (UsedFlags.IsValidIndex(MaterialIndex))
				{
					UsedFlags[MaterialIndex] = true;
				}
			}
		}
	}

	if (UsedFlags.CountSetBits() < MaterialCount)
	{
		// iterate from back and remove materials that are not used
		for (TBitArray<>::FConstReverseIterator BitIter(UsedFlags); BitIter; ++BitIter)
		{
			// if it's not used
			if (!BitIter.GetValue())
			{
				const int32 MaterialIndexToRemove = BitIter.GetIndex();

				// remove from end
				InSkeletalMesh->GetMaterials().RemoveAt(MaterialIndexToRemove);

				const int32 NumLODInfos = InSkeletalMesh->GetLODNum();

				// When we delete a material slot we need to fix all MaterialIndex after the deleted index
				for (int32 LODInfoIdx = 0; LODInfoIdx < NumLODInfos; LODInfoIdx++)
				{
					const FSkeletalMeshLODModel& LODModel = InSkeletalMesh->GetImportedModel()->LODModels[LODInfoIdx];

					TArray<int32>& LODMaterialMap = InSkeletalMesh->GetLODInfo(LODInfoIdx)->LODMaterialMap;
					for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); ++SectionIndex)
					{
						if (LODMaterialMap.IsValidIndex(SectionIndex) && LODMaterialMap[SectionIndex] != INDEX_NONE)
						{
							if (LODMaterialMap[SectionIndex] > MaterialIndexToRemove)
							{
								--LODMaterialMap[SectionIndex];
							}
						}
					}
				}
			}
		}
	}
}

void FMetaHumanCharacterEditorBuild::DownsizeTexture(TNotNull<class UTexture*> InTexture, int32 InTargetResolution, TNotNull<const ITargetPlatform*> InTargetPlatform)
{
	if (InTargetResolution <= 0)
	{
		return;
	}

	// Update the texture resources to make sure they are valid before the actual Downsize
	InTexture->UpdateResource();

	int32 BeforeSizeX;
	int32 BeforeSizeY;
	InTexture->GetBuiltTextureSize(InTargetPlatform, BeforeSizeX, BeforeSizeY);

	const FIntPoint BeforeSourceSize = InTexture->Source.GetLogicalSize();

	if (BeforeSizeX > InTargetResolution && BeforeSizeY > InTargetResolution)
	{
		const bool bWasResized = UE::TextureUtilitiesCommon::Experimental::DownsizeTextureSourceData(InTexture, InTargetResolution, InTargetPlatform);

		if (bWasResized)
		{
			// this counts as a re-import so defaults need to be applied
			const bool bIsReimport = true;
			UE::TextureUtilitiesCommon::ApplyDefaultsForNewlyImportedTextures(InTexture, bIsReimport);

			// DownsizeTextureSourceData did the PreEditChange
			InTexture->PostEditChange();

			FTextureCompilingManager::Get().FinishCompilation({ InTexture });
		}
	}
}

USkeletalMesh* FMetaHumanCharacterEditorBuild::MergeHeadAndBody_CreateAsset(
	TNotNull<class USkeletalMesh*> InFaceMesh,
	TNotNull<class USkeletalMesh*> InBodyMesh,
	const FString& InAssetPathAndName)
{
	return UE::MetaHuman::Build::MergeHeadAndBody(
		InFaceMesh,
		InBodyMesh,
		nullptr,
		InAssetPathAndName);
}

USkeletalMesh* FMetaHumanCharacterEditorBuild::MergeHeadAndBody_CreateTransient(
	TNotNull<USkeletalMesh*> InFaceMesh,
	TNotNull<USkeletalMesh*> InBodyMesh,
	UObject* InOuter)
{
	USkeletalMesh* MergedSkelMesh = UE::MetaHuman::Build::MergeHeadAndBody(
		InFaceMesh,
		InBodyMesh,
		InOuter,
		TEXT(""));

	return MergedSkelMesh;
}

#undef LOCTEXT_NAMESPACE
