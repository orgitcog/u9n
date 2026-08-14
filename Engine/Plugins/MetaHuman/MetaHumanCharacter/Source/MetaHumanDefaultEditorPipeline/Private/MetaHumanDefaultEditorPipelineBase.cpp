// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultEditorPipelineBase.h"

#include "Item/MetaHumanDefaultGroomPipeline.h"
#include "Item/MetaHumanGroomEditorPipeline.h"
#include "Item/MetaHumanGroomPipeline.h"
#include "Item/MetaHumanOutfitEditorPipeline.h"
#include "Item/MetaHumanOutfitPipeline.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteUnpackHelpers.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollection.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "MetaHumanDefaultPipelineBase.h"
#include "MetaHumanWardrobeItem.h"
#include "Subsystem/MetaHumanCharacterBuild.h"
#include "ProjectUtilities/MetaHumanProjectUtilities.h"
#include "MetaHumanTypesEditor.h"
#include "MetaHumanCharacterPalette.h"

#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosOutfitAsset/BodyUserData.h"
#include "Dataflow/DataflowObject.h"
#include "Engine/SkeletalMesh.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "GroomCreateFollicleMaskOptions.h"
#include "GroomTextureBuilder.h"

#include "Algo/Find.h"
#include "Algo/Contains.h"
#include "Algo/AnyOf.h"
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/TG_AsyncExportTask.h"
#include "Editor/EditorEngine.h"
#include "EditorAssetLibrary.h"
#include "Engine/SkeletalMeshLODSettings.h"
#include "LODUtilities.h"
#include "Logging/StructuredLog.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/UObjectToken.h"
#include "Engine/Texture2D.h"
#include "PackageTools.h"
#include "Internationalization/Regex.h"
#include "TG_Material.h"
#include "TextureGraph.h"
#include "UObject/FastReferenceCollector.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/GCObjectScopeGuard.h"
#include "BlueprintCompilationManager.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "Misc/MessageDialog.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshModel.h"

#include "MetaHumanCharacterActorInterface.h"
#include "MetaHumanRigLogicUnpackLibrary.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "MetaHumanDefaultEditorPipelineBase"

namespace UE::MetaHuman::Private
{
	static TAutoConsoleVariable<bool> CVarMHCEnableGCOnTextureBaking
	{
		TEXT("mh.Assembly.EnableGCOnTextureBaking"),
		true,
		TEXT("Set to true to run GC during the texture baking part of the assembly."),
		ECVF_Default
	};

	static constexpr TStaticArray<EFaceTextureType, 6> GetAnimatedMapTypes()
	{
		constexpr TStaticArray<EFaceTextureType, 6> AnimatedMapTypes =
		{
			EFaceTextureType::Basecolor_Animated_CM1,
			EFaceTextureType::Basecolor_Animated_CM2,
			EFaceTextureType::Basecolor_Animated_CM3,

			EFaceTextureType::Normal_Animated_WM1,
			EFaceTextureType::Normal_Animated_WM2,
			EFaceTextureType::Normal_Animated_WM3,
		};

		return AnimatedMapTypes;
	}

	/** Reparent the Skin LOD Materials so they form a hierarchy */
	static void ReparentSkinLODMaterials(const FMetaHumanCharacterGeneratedAssets& InGeneratedAssets)
	{
		const FMetaHumanCharacterFaceMaterialSet NewFaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(InGeneratedAssets.FaceMesh);

		TArray<TObjectPtr<UMaterialInstance>> SkinMaterialChain;
		NewFaceMaterialSet.Skin.GenerateValueArray(SkinMaterialChain);

		for (int32 Index = 0; Index < SkinMaterialChain.Num() - 1; ++Index)
		{
			UMaterialInstance* NewParent = SkinMaterialChain[Index];
			UMaterialInstanceConstant* Material = Cast<UMaterialInstanceConstant>(SkinMaterialChain[Index + 1]);

			if (Material && NewParent)
			{
				FMetaHumanCharacterSkinMaterials::SetMaterialInstanceParent(Material, NewParent);
			}
		}
	}

	static const FName OutfitResize_TargetBodyPropertyName("TargetBody");
	static const FName OutfitResize_ResizableOutfitPropertyName("ResizableOutfit");
}

struct FCharacterPipelineData
{
	USkeletalMesh* FaceMesh = nullptr;
	USkeletalMesh* BodyMesh = nullptr;
	TMap<FString, float> BodyMeasurements;

	TOptional<FMetaHumanCharacterGeneratedAssets> GeneratedAssets;

	TMap<FName, UMaterialInterface*> FaceRemovedMaterialSlots;
	TArray<UTexture2D*> FaceBakedNormalsTextures;
	UTexture2D* FollicleMap = nullptr;
	TArray<UTexture2D*> BodyHiddenFaceMaps;
	
	// Each time a material parameter (or set of material parameters) is changed on a face mesh LOD,
	// the entry of the index of that LOD should be incremented in this array.
	//
	// It will be used to determine which face LODs have unique materials and need to be baked 
	// separately.
	TArray<int32> FaceMaterialChangesPerLOD;

	USkeletalMesh* MergedHeadAndBody = nullptr;
	bool bSkipTransferSkinWeights = false;
	bool bStripSimMesh = false;
};

UMetaHumanDefaultEditorPipelineBase::UMetaHumanDefaultEditorPipelineBase()
{
	Specification = CreateDefaultSubobject<UMetaHumanCharacterEditorPipelineSpecification>("Specification");
	Specification->BuildInputStruct = FMetaHumanBuildInputBase::StaticStruct();

	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Hair");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}

	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Eyebrows");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}

	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Beard");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}
		
	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Mustache");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}
		
	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Eyelashes");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}
				
	{
		FMetaHumanCharacterPipelineSlotEditorData& Slot = Specification->SlotEditorData.FindOrAdd("Peachfuzz");
		Slot.BuildInputStruct = FMetaHumanGroomPipelineBuildInput::StaticStruct();
	}

	FaceSkeleton = FSoftObjectPath(TEXT("/Script/Engine.Skeleton'/" UE_PLUGIN_NAME "/Face/Face_Archetype_Skeleton.Face_Archetype_Skeleton'"));
	BodySkeleton = FSoftObjectPath(TEXT("/Script/Engine.Skeleton'/" UE_PLUGIN_NAME "/Female/Medium/NormalWeight/Body/metahuman_base_skel.metahuman_base_skel'"));
}

void UMetaHumanDefaultEditorPipelineBase::BuildCollection(
	TNotNull<const UMetaHumanCollection*> CharacterPalette,
	TNotNull<UObject*> OuterForGeneratedAssets,
	const TArray<FMetaHumanPinnedSlotSelection>& SortedPinnedSlotSelections,
	const TArray<FMetaHumanPaletteItemPath>& SortedItemsToExclude,
	const FInstancedStruct& BuildInput,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	const FOnBuildComplete& OnComplete) const
{
	const FText SlowTaskMessage = Quality == EMetaHumanCharacterPaletteBuildQuality::Preview
		? LOCTEXT("BuildSlowTaskMessage_Preview", "Assembling Character for preview...")
		: LOCTEXT("BuildSlowTaskMessage_Production", "Assembling Character...");
	FScopedSlowTask SlowTask(1, SlowTaskMessage);
	SlowTask.MakeDialog();

	check(CharacterPalette->GetEditorPipeline() == this);

	TSharedRef<FMetaHumanCollectionBuiltData> BuiltData = MakeShared<FMetaHumanCollectionBuiltData>();
	BuiltData->Quality = Quality;

	const UMetaHumanDefaultPipelineBase* RuntimePipeline = Cast<UMetaHumanDefaultPipelineBase>(GetRuntimePipeline());
	if (!RuntimePipeline)
	{
		// Runtime pipeline must inherit from UMetaHumanDefaultPipelineBase
		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
		return;
	}

	FMetaHumanBuildInputBase Input;
	{
		const FMetaHumanBuildInputBase* InputPtr = BuildInput.GetPtr<FMetaHumanBuildInputBase>();
		if (InputPtr)
		{
			Input = *InputPtr;
		}
	}

	if (!CharacterPalette->GetPipeline()->GetSpecification()->IsValid())
	{
		FMessageLog(UE::MetaHuman::MessageLogName).Error()
			->AddToken(FTextToken::Create(LOCTEXT("PipelineSpecInvalid", "The MetaHuman Character Pipeline's specification is invalid. This usually means there's an issue with the configuration of the pipeline slots.")));

		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
		return;
	}

	// TODO: More validations on the skeletons?
	if (FaceSkeleton.IsNull())
	{
		FMessageLog(UE::MetaHuman::MessageLogName).Error()
			->AddToken(FTextToken::Create(LOCTEXT("InvalidFaceSkeleton", "A valid Face Skeleton is required to run the pipeline")));

		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
		return;
	}

	if (BodySkeleton.IsNull())
	{
		FMessageLog(UE::MetaHuman::MessageLogName).Error()
			->AddToken(FTextToken::Create(LOCTEXT("InvalidBodySkeleton", "A valid Body Skeleton is required to run the pipeline")));

		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
		return;
	}

	TMap<FMetaHumanPaletteItemKey, FCharacterPipelineData> CharacterPipelineData;
	const bool bCanResizeOutfits = CanResizeOutfits();

	auto GenerateMergedMesh = [bCanResizeOutfits, &CharacterPipelineData, OuterForGeneratedAssets](FCharacterPipelineData& PipelineData)
		{
			if (bCanResizeOutfits)
			{
				if (PipelineData.FaceMesh && PipelineData.BodyMesh)
				{
					// Note that this creates a mesh that has no render data and can't safely be 
					// passed to other engine systems, other than Outfit resizing.
					PipelineData.MergedHeadAndBody = FMetaHumanCharacterEditorBuild::MergeHeadAndBody_CreateTransient(
						PipelineData.FaceMesh,
						PipelineData.BodyMesh,
						OuterForGeneratedAssets);

					// Add body user data for measurements
					if (PipelineData.MergedHeadAndBody
						&& PipelineData.BodyMeasurements.Num() > 0)
					{
						UChaosOutfitAssetBodyUserData* BodyUserData = PipelineData.MergedHeadAndBody->GetAssetUserData<UChaosOutfitAssetBodyUserData>();

						if (!BodyUserData)
						{
							BodyUserData = NewObject<UChaosOutfitAssetBodyUserData>(PipelineData.MergedHeadAndBody);
							PipelineData.MergedHeadAndBody->AddAssetUserData(BodyUserData);
						}

						BodyUserData->Measurements = PipelineData.BodyMeasurements;
					}
				}
			}
		};
	
	for (const FMetaHumanCharacterPaletteItem& Item : CharacterPalette->GetItems())
	{
		UObject* PrincipalAsset = Item.LoadPrincipalAssetSynchronous();
		const FMetaHumanPaletteItemKey ItemKey(Item.GetItemKey());
		const FMetaHumanPaletteItemPath ItemPath(ItemKey);

		const UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(PrincipalAsset);

		if (Item.SlotName != UE::MetaHuman::CharacterPipelineSlots::Character
			|| !Character
			|| SortedItemsToExclude.Contains(ItemPath))
		{
			continue;
		}

		UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = UMetaHumanCharacterEditorSubsystem::Get();

		if (Quality == EMetaHumanCharacterPaletteBuildQuality::Preview
			&& Input.EditorPreviewCharacter == ItemKey)
		{
			FMetaHumanCharacterPreviewAssets PreviewAssets;
			if (MetaHumanCharacterEditorSubsystem->TryGetCharacterPreviewAssets(Character, PreviewAssets))
			{
				FCharacterPipelineData& PipelineData = CharacterPipelineData.Add(ItemKey);
				PipelineData.FaceMesh = PreviewAssets.FaceMesh;
				PipelineData.BodyMesh = PreviewAssets.BodyMesh;
				PipelineData.BodyMeasurements = PreviewAssets.BodyMeasurements;
				// no skin transfer is required when no rig is available as animation is disabled anyhow
				PipelineData.bSkipTransferSkinWeights = !Character->HasFaceDNA();
				PipelineData.bStripSimMesh = !Character->HasFaceDNA();

				PipelineData.FaceMaterialChangesPerLOD.AddZeroed(PipelineData.FaceMesh->GetLODNum());

				GenerateMergedMesh(PipelineData);
			}
		}
		else
		{
			FMetaHumanCharacterGeneratedAssets GeneratedAssets;
			if (MetaHumanCharacterEditorSubsystem->TryGenerateCharacterAssets(Character, OuterForGeneratedAssets, GeneratedAssets))
			{
				FCharacterPipelineData& PipelineData = CharacterPipelineData.Add(ItemKey);
				PipelineData.FaceMesh = GeneratedAssets.FaceMesh;
				PipelineData.BodyMesh = GeneratedAssets.BodyMesh;
				PipelineData.BodyMeasurements = GeneratedAssets.BodyMeasurements;
				PipelineData.GeneratedAssets = GeneratedAssets;
				PipelineData.bSkipTransferSkinWeights = false;
				PipelineData.bStripSimMesh = false;

				check(GeneratedAssets.FaceMesh);
				check(GeneratedAssets.BodyMesh);

				PipelineData.FaceMaterialChangesPerLOD.AddZeroed(PipelineData.FaceMesh->GetLODNum());

				TNotNull<USkeleton*> GeneratedFaceSkeleton = GenerateSkeleton(GeneratedAssets, FaceSkeleton.LoadSynchronous(), TEXT("Face"), OuterForGeneratedAssets);
				TNotNull<USkeleton*> GeneratedBodySkeleton = GenerateSkeleton(GeneratedAssets, BodySkeleton.LoadSynchronous(), TEXT("Body"), OuterForGeneratedAssets);

				GeneratedAssets.FaceMesh->SetSkeleton(GeneratedFaceSkeleton);
				GeneratedAssets.BodyMesh->SetSkeleton(GeneratedBodySkeleton);

				// Set the MH asset version to the assets that will be exported as is
				FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(GeneratedAssets.FaceMesh);
				FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(GeneratedAssets.BodyMesh);
				FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(GeneratedAssets.PhysicsAsset);

				// This must be done before the LODs are removed
				GenerateMergedMesh(PipelineData);

				const UMetaHumanMaterialBakingSettings* BakingSettings = FaceMaterialBakingOptions.BakingSettings.LoadSynchronous();

				if (bBakeMaterials
					&& BakingSettings != nullptr
					&& BakingSettings->LODBakingUtilityClass != nullptr)
				{
					ULODBakingUtility* LODBaking = NewObject<ULODBakingUtility>(GetTransientPackage(), BakingSettings->LODBakingUtilityClass);
					UGeometryScriptDebug* DebugObject = NewObject<UGeometryScriptDebug>(LODBaking);
					PipelineData.FaceBakedNormalsTextures = LODBaking->BakeTangentNormals(GeneratedAssets.FaceMesh, DebugObject);

					if (PipelineData.FaceBakedNormalsTextures.Num() < 3)
					{
						// TODO: Log the messages from the Debug Object, if any

						OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
						return;
					}
					else
					{
						PipelineData.GeneratedAssets->Metadata.Emplace(PipelineData.FaceBakedNormalsTextures[0], TEXT("Face/Baked"), TEXT("T_BakedNormal_LOD3"));
						PipelineData.GeneratedAssets->Metadata.Emplace(PipelineData.FaceBakedNormalsTextures[1], TEXT("Face/Baked"), TEXT("T_BakedNormal_LOD4"));
						PipelineData.GeneratedAssets->Metadata.Emplace(PipelineData.FaceBakedNormalsTextures[2], TEXT("Face/Baked"), TEXT("T_BakedNormal_LOD5"));
					}
				}

				RemoveLODsIfNeeded(*PipelineData.GeneratedAssets, PipelineData.FaceRemovedMaterialSlots);

				for (int32& ChangeCount : PipelineData.FaceMaterialChangesPerLOD)
				{
					ChangeCount = INDEX_NONE;
				}
				
				const FMetaHumanCharacterFaceMaterialSet FaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(GeneratedAssets.FaceMesh);
				FaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceConstant>(
					[&PipelineData](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstanceConstant* Material)
					{
						switch (Slot)
						{
							case EMetaHumanCharacterSkinMaterialSlot::LOD0:
								PipelineData.FaceMaterialChangesPerLOD[0] = 0;
								break;

							case EMetaHumanCharacterSkinMaterialSlot::LOD1:
								PipelineData.FaceMaterialChangesPerLOD[1] = 0;
								break;

							case EMetaHumanCharacterSkinMaterialSlot::LOD2:
								PipelineData.FaceMaterialChangesPerLOD[2] = 0;
								break;

							case EMetaHumanCharacterSkinMaterialSlot::LOD3:
								PipelineData.FaceMaterialChangesPerLOD[3] = 0;
								break;

							case EMetaHumanCharacterSkinMaterialSlot::LOD4:
								PipelineData.FaceMaterialChangesPerLOD[4] = 0;
								break;

							case EMetaHumanCharacterSkinMaterialSlot::LOD5to7:
								PipelineData.FaceMaterialChangesPerLOD[5] = 0;
								PipelineData.FaceMaterialChangesPerLOD[6] = 0;
								PipelineData.FaceMaterialChangesPerLOD[7] = 0;
								break;

							default:
								checkNoEntry();
						}
					});
			}
		}
	}
	
	ProcessGroomAndClothSlots(
		CharacterPalette,
		BuiltData,
		SortedPinnedSlotSelections,
		SortedItemsToExclude,
		Quality,
		TargetPlatform,
		OuterForGeneratedAssets,
		CharacterPipelineData);

	// Process Character and skel mesh slots
	for (const FMetaHumanCharacterPaletteItem& Item : CharacterPalette->GetItems())
	{
		UObject* PrincipalAsset = Item.LoadPrincipalAssetSynchronous();
		const FMetaHumanPaletteItemKey ItemKey(Item.GetItemKey());
		const FMetaHumanPaletteItemPath ItemPath(ItemKey);

		if (Item.SlotName == NAME_None
			|| !PrincipalAsset
			|| SortedItemsToExclude.Contains(ItemPath))
		{
			continue;
		}

		const TOptional<FName> RealSlotName = GetRuntimePipeline()->GetSpecification()->ResolveRealSlotName(Item.SlotName);
		if (!ensure(RealSlotName.IsSet()))
		{
			// Since the spec was validated above, this shouldn't happen.
			//
			// Handle gracefully anyway by skipping this item.
			continue;
		}

		FMetaHumanPipelineBuiltData ItemBuiltData;
		FInstancedStruct& BuildOutput = ItemBuiltData.BuildOutput;
		ItemBuiltData.SlotName = RealSlotName.GetValue();

		if (Item.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			FCharacterPipelineData& PipelineData = CharacterPipelineData[ItemKey];
			if (!PipelineData.GeneratedAssets.IsSet())
			{
				FMetaHumanCharacterPartOutput& OutputStruct = BuildOutput.InitializeAs<FMetaHumanCharacterPartOutput>();
				OutputStruct.GeneratedAssets.FaceMesh = PipelineData.FaceMesh;
				OutputStruct.GeneratedAssets.BodyMesh = PipelineData.BodyMesh;
				OutputStruct.GeneratedAssets.BodyMeasurements = PipelineData.BodyMeasurements;
			}
			else
			{
				FMetaHumanCharacterGeneratedAssets& GeneratedAssets = PipelineData.GeneratedAssets.GetValue();

				if (GeneratedAssets.FaceMesh
					&& (PipelineData.FollicleMap || HairProperties.UseFollicleMapMaterialParameterName != NAME_None))
				{
					// Either there's a follicle map, or a parameter we need to set to enable/disable the follicle map

					const bool bShouldSetFollicleMap = PipelineData.FollicleMap != nullptr && HairProperties.FollicleMapMaterialParameterName != NAME_None;

					TArray<FSkeletalMaterial>& MeshMaterials = GeneratedAssets.FaceMesh->GetMaterials();

					for (FName MaterialSlotName : HairProperties.FollicleMapMaterialSlotNames)
					{
						if (PipelineData.FaceRemovedMaterialSlots.Contains(MaterialSlotName))
						{
							// This slot has been intentionally removed, so don't search for it
							continue;
						}

						FSkeletalMaterial* FoundMaterial = Algo::FindBy(MeshMaterials, MaterialSlotName, &FSkeletalMaterial::MaterialSlotName);
						if (FoundMaterial)
						{
							if (Algo::FindBy(GeneratedAssets.Metadata, FoundMaterial->MaterialInterface, &FMetaHumanGeneratedAssetMetadata::Object))
							{
								if (UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(FoundMaterial->MaterialInterface))
								{
									if (HairProperties.UseFollicleMapMaterialParameterName != NAME_None)
									{
										MIC->SetStaticSwitchParameterValueEditorOnly(HairProperties.UseFollicleMapMaterialParameterName, bShouldSetFollicleMap);
									}

									if (bShouldSetFollicleMap)
									{
										MIC->SetTextureParameterValueEditorOnly(HairProperties.FollicleMapMaterialParameterName, PipelineData.FollicleMap);
									}
								}
								else
								{
									UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Can't set follicle map on material {Material}: Must be MaterialInstanceConstant", GetFullNameSafe(FoundMaterial->MaterialInterface));
								}
							}
							else
							{
								UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Can't set follicle map on material {Material}: Must be part of generated character assets", GetFullNameSafe(FoundMaterial->MaterialInterface));
							}
						}
						else
						{
							UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Can't set follicle map on material slot {MaterialSlot}: Slot not found on face mesh", MaterialSlotName);
						}
					}
				}

				if (GeneratedAssets.BodyMesh
					&& PipelineData.BodyHiddenFaceMaps.Num() > 0)
				{
					FScopedSkeletalMeshPostEditChange ScopedPostEditChange(GeneratedAssets.BodyMesh);

					const int32 NumLODs = GeneratedAssets.BodyMesh->GetLODNum();

					for (UTexture2D* HiddenFaceMap : PipelineData.BodyHiddenFaceMaps)
					{
						check(HiddenFaceMap);

						for (int32 LODIndex = 0; LODIndex < NumLODs; LODIndex++)
						{
							FLODUtilities::StripLODGeometry(GeneratedAssets.BodyMesh, LODIndex, HiddenFaceMap, 0.1f);
						}
					}
				}

				if (bBakeMaterials)
				{
					const FString TextureOutputFolder = FPackageName::GetLongPackagePath(CharacterPalette->GetUnpackFolder() / TEXT("Textures"));

					if (!ProcessBakedMaterials(TextureOutputFolder, BuiltData, GeneratedAssets, OuterForGeneratedAssets, PipelineData))
					{
						OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed, nullptr);
						return;
					}
				}

				UE::MetaHuman::Private::ReparentSkinLODMaterials(GeneratedAssets);

				// Downsize any textures if specified in the pipeline
				if (!MaxTextureResolutions.Face.IsEmpty())
				{
					for (const TPair<EFaceTextureType, EMetaHumanBuildTextureResolution>& TargetResolutionPair : MaxTextureResolutions.Face)
					{
						EFaceTextureType TextureType = TargetResolutionPair.Key;
						EMetaHumanBuildTextureResolution TargetResolution = TargetResolutionPair.Value;

						if (const TObjectPtr<UTexture2D>* FoundTexture = GeneratedAssets.SynthesizedFaceTextures.Find(TextureType))
						{
							FMetaHumanCharacterEditorBuild::DownsizeTexture(*FoundTexture, static_cast<int32>(TargetResolution), TargetPlatform);
						}
					}
				}

				// Set the post process anim blueprint
				if (IsValid(BodyProperties.PostProcessAnimBp.Get()))
				{
					GeneratedAssets.BodyMesh->SetPostProcessAnimBlueprint(BodyProperties.PostProcessAnimBp.Get());
				}

				if (BodyProperties.bUnpackRigLogic)
				{
					UAnimBlueprint* BodyPostProcessAnimBP = nullptr;
					if (IsValid(BodyProperties.PostProcessAnimBp.Get()))
					{
						BodyPostProcessAnimBP = Cast<UAnimBlueprint>(UEditorAssetLibrary::LoadAsset(BodyProperties.PostProcessAnimBp->GetPackage()->GetName()));
					}
						
					TArray<uint16> HalfRotationSolvers;
					if (BodyProperties.BodyRigLogicUnpackProperties.bUnpackRbfToPoseAssets)
					{
						TArray<FMetaHumanBodyRigLogicGeneratedAsset> OutGeneratedRigLogicAssets;
						UMetaHumanRigLogicUnpackLibrary::UnpackRBFEvaluation(
							BodyPostProcessAnimBP,
							GeneratedAssets.BodyMesh,
							OuterForGeneratedAssets,
							BodyProperties.BodyRigLogicUnpackProperties.bUnpackFingerHalfRotationsToControlRig,
							HalfRotationSolvers,
							OutGeneratedRigLogicAssets
							);

						for (const FMetaHumanBodyRigLogicGeneratedAsset& GeneratedRiglogicAsset: OutGeneratedRigLogicAssets)
						{
							GeneratedAssets.Metadata.Emplace(GeneratedRiglogicAsset.AnimSequence, "Body/RBF", "AS_"+GeneratedRiglogicAsset.SolverName);
							GeneratedAssets.Metadata.Emplace(GeneratedRiglogicAsset.PoseAsset, "Body/RBF", "PA_"+GeneratedRiglogicAsset.SolverName);
								
						}
						GeneratedAssets.BodyRigLogicAssets = OutGeneratedRigLogicAssets;
					}
						
					if (BodyProperties.BodyRigLogicUnpackProperties.bUnpackSwingTwistToControlRig || (BodyProperties.BodyRigLogicUnpackProperties.bUnpackRbfToPoseAssets && BodyProperties.BodyRigLogicUnpackProperties.bUnpackFingerHalfRotationsToControlRig))
					{
						TObjectPtr<UControlRigBlueprint> BodyControlRig = UMetaHumanRigLogicUnpackLibrary::UnpackControlRigEvaluation(
							BodyPostProcessAnimBP,
							GeneratedAssets.BodyMesh,
							BodyProperties.BodyRigLogicUnpackProperties.ControlRig,
							OuterForGeneratedAssets,
							BodyProperties.BodyRigLogicUnpackProperties.bUnpackFingerHalfRotationsToControlRig,
							HalfRotationSolvers
						);
						if (IsValid(BodyControlRig))
						{
							GeneratedAssets.Metadata.Emplace(BodyControlRig, "Body/Controls", "CR_Body_Procedural");
						}
					}
					// Update the body DNA user asset data to enable/disable rbf and swing twist evaluation
					if (UAssetUserData* UserData = GeneratedAssets.BodyMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass()))
					{
						UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);
						if (IsValid(BodyProperties.PostProcessAnimBp.Get()))
						{
							DNAAsset->RigLogicConfiguration.LoadRBFBehavior = !BodyProperties.BodyRigLogicUnpackProperties.bUnpackRbfToPoseAssets;
						}
						DNAAsset->RigLogicConfiguration.LoadTwistSwingBehavior = !BodyProperties.BodyRigLogicUnpackProperties.bUnpackSwingTwistToControlRig;
					}
				}

				FMetaHumanCharacterPartOutput& OutputStruct = BuildOutput.InitializeAs<FMetaHumanCharacterPartOutput>();
				OutputStruct.GeneratedAssets = GeneratedAssets;
			}
		}
		else if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(PrincipalAsset))
		{
			FMetaHumanMeshPartOutput& OutputStruct = BuildOutput.InitializeAs<FMetaHumanMeshPartOutput>();
			OutputStruct.Mesh = Mesh;
		}

		if (BuildOutput.IsValid())
		{
			BuiltData->PaletteBuiltData.ItemBuiltData.Add(FMetaHumanPaletteItemPath(Item.GetItemKey()), MoveTemp(ItemBuiltData));
		}
	}

	OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Succeeded, BuiltData);
}

bool UMetaHumanDefaultEditorPipelineBase::CanBuild() const
{
	return true;
}

bool UMetaHumanDefaultEditorPipelineBase::ProcessBakedMaterials(
	const FString& TextureOutputFolder, 
	TSharedRef<struct FMetaHumanCollectionBuiltData> BuiltData,
	FMetaHumanCharacterGeneratedAssets& GeneratedAssets,
	TNotNull<UObject*> OuterForGeneratedAssets,
	FCharacterPipelineData& PipelineData)  const
{
	// Run all the TG baking in a separate scope in order to be able to guard from GC all the build generated assets up to this point
	// Leaving the scope, some of the textures in the generated assets may be removed, depending on the baking output
	{
		TArray<UObject*> GCGuardObjects;
		if (UE::MetaHuman::Private::CVarMHCEnableGCOnTextureBaking.GetValueOnAnyThread())
		{
			// Add the assets generated by the pipeline
			Algo::Transform(GeneratedAssets.Metadata, GCGuardObjects,
				[](const FMetaHumanGeneratedAssetMetadata& Metadata)
				{
					return Metadata.Object;
				});

			// Add any built data that have been created up to this point
			for (const TPair<FMetaHumanPaletteItemPath, FMetaHumanPipelineBuiltData>& ItemBuildDataPair : BuiltData->PaletteBuiltData.ItemBuiltData)
			{
				if (const FMetaHumanOutfitPipelineBuildOutput* OutfitOutputStruct = ItemBuildDataPair.Value.BuildOutput.GetPtr<FMetaHumanOutfitPipelineBuildOutput>())
				{
					// Separate handling of the outfit pipeline since it does not add the built objects to the metadata
					for (const TPair<FMetaHumanPaletteItemKey, FMetaHumanOutfitGeneratedAssets>& Pair : OutfitOutputStruct->CharacterAssets)
					{
						if (Pair.Value.Outfit)
						{
							GCGuardObjects.Add(Pair.Value.Outfit);
						}
						if (Pair.Value.OutfitMesh)
						{
							GCGuardObjects.Add(Pair.Value.OutfitMesh);
						}
						if (Pair.Value.CombinedBodyMesh)
						{
							GCGuardObjects.Add(Pair.Value.CombinedBodyMesh);
						}
					}
				}
				else
				{
					Algo::Transform(ItemBuildDataPair.Value.Metadata, GCGuardObjects,
						[](const FMetaHumanGeneratedAssetMetadata& Metadata)
						{
							return Metadata.Object;
						});
				}
			}
		}

		TGCObjectsScopeGuard<UObject> GCGuard_Textures(GCGuardObjects);

		// Bake face textures if needed
		if (!FaceMaterialBakingOptions.BakingSettings.IsNull())
		{
			// Output to the unpack folder of defined by palette
			TArray<FSkeletalMaterial> FaceMeshMaterials = GeneratedAssets.FaceMesh->GetMaterials();
			{
				if (!TryBakeMaterials(
					TextureOutputFolder,
					FaceMaterialBakingOptions,
					FaceMeshMaterials,
					PipelineData.FaceRemovedMaterialSlots,
					PipelineData.FaceMaterialChangesPerLOD,
					OuterForGeneratedAssets,
					GeneratedAssets))
				{
					return false;
				}
			}

			GeneratedAssets.FaceMesh->SetMaterials(FaceMeshMaterials);
		}

		// Bake body textures if needed
		if (!BodyMaterialBakingOptions.BakingSettings.IsNull())
		{
			TArray<FSkeletalMaterial> BodyMeshMaterials = GeneratedAssets.BodyMesh->GetMaterials();
			if (!TryBakeMaterials(TextureOutputFolder, BodyMaterialBakingOptions, BodyMeshMaterials, TMap<FName, UMaterialInterface*>(), TArray<int32>(), OuterForGeneratedAssets, GeneratedAssets))
			{
				return false;
			}
			GeneratedAssets.BodyMesh->SetMaterials(BodyMeshMaterials);
		}
	}

	// Remove any face textures if needed
	if (!FaceMaterialBakingOptions.BakingSettings.IsNull())
	{
		// Ensure remove materials are not in the generated data
		// This is safe to do only after baking since potentially all materials will be used during baking
		for (const TPair<FName, UMaterialInterface*>& Pair : PipelineData.FaceRemovedMaterialSlots)
		{
			GeneratedAssets.RemoveAssetMetadata(Pair.Value);
		}

		// If there is a follicle map, it's now baked into the face, so we can discard it
		if (PipelineData.FollicleMap)
		{
			GeneratedAssets.RemoveAssetMetadata(PipelineData.FollicleMap);
			PipelineData.FollicleMap = nullptr;
		}

		// When baking materials is enabled, Basecolor and Normal synthesized textures are not used, so remove them from the list of exported assets
		constexpr TStaticArray<EFaceTextureType, 3> TexturesToRemove =
		{
			EFaceTextureType::Basecolor,
			EFaceTextureType::Normal,
			EFaceTextureType::Cavity,
		};

		for (EFaceTextureType TextureType : TexturesToRemove)
		{
			// Maps may have been removed already
			if (GeneratedAssets.SynthesizedFaceTextures.Contains(TextureType))
			{
				UTexture2D* TextureToRemove = GeneratedAssets.SynthesizedFaceTextures[TextureType];
				GeneratedAssets.RemoveAssetMetadata(TextureToRemove);
				GeneratedAssets.SynthesizedFaceTextures.Remove(TextureType);
				TextureToRemove->MarkAsGarbage();
			}
		}

		// Apply the animated maps to the baked materials
		const FMetaHumanCharacterFaceMaterialSet BakedFaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(GeneratedAssets.FaceMesh);

		BakedFaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceConstant>(
			[&GeneratedAssets, &PipelineData](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstanceConstant* BakedSkinMaterial)
			{
				for (EFaceTextureType AnimatedMapType : UE::MetaHuman::Private::GetAnimatedMapTypes())
				{
					if (const TObjectPtr<UTexture2D>* FoundAnimatedMap = GeneratedAssets.SynthesizedFaceTextures.Find(AnimatedMapType))
					{
						const FName FaceTextureParameterName = FMetaHumanCharacterSkinMaterials::GetFaceTextureParameterName(AnimatedMapType);
						BakedSkinMaterial->SetTextureParameterValueEditorOnly(FaceTextureParameterName, *FoundAnimatedMap);
					}
				}

				UTexture2D* BakedLODNormal = nullptr;

				if (Slot == EMetaHumanCharacterSkinMaterialSlot::LOD3 && PipelineData.FaceBakedNormalsTextures.IsValidIndex(0))
				{
					BakedLODNormal = PipelineData.FaceBakedNormalsTextures[0];
				}
				else if (Slot == EMetaHumanCharacterSkinMaterialSlot::LOD4 && PipelineData.FaceBakedNormalsTextures.IsValidIndex(1))
				{
					BakedLODNormal = PipelineData.FaceBakedNormalsTextures[1];
				}
				else if (Slot == EMetaHumanCharacterSkinMaterialSlot::LOD5to7 && PipelineData.FaceBakedNormalsTextures.IsValidIndex(2))
				{
					BakedLODNormal = PipelineData.FaceBakedNormalsTextures[2];
				}

				if (IsValid(BakedLODNormal))
				{
					// TODO: Get this parameter name from FMetaHumanCharacterSkinMaterials
					BakedSkinMaterial->SetTextureParameterValueEditorOnly(TEXT("Normal LOD Baked"), BakedLODNormal);
				}
			}
		);
	}

	// Remove any body textures if needed
	if (!BodyMaterialBakingOptions.BakingSettings.IsNull())
	{
		// Remove source textures after baking
		for (EBodyTextureType TextureType : TEnumRange<EBodyTextureType>())
		{
			UTexture2D* TextureToRemove = GeneratedAssets.BodyTextures[TextureType];
			if (GeneratedAssets.RemoveAssetMetadata(TextureToRemove))
			{
				// Clean up textures that have been generated with metadata
				GeneratedAssets.BodyTextures.Remove(TextureType);
				TextureToRemove->MarkAsGarbage();
			}
		}
	}

	return true;
}

void UMetaHumanDefaultEditorPipelineBase::ProcessGroomAndClothSlots(
	TNotNull<const UMetaHumanCollection*> CharacterCollection, 
	TSharedRef<FMetaHumanCollectionBuiltData> BuiltData,
	const TArray<FMetaHumanPinnedSlotSelection>& SortedPinnedSlotSelections,
	const TArray<FMetaHumanPaletteItemPath>& SortedItemsToExclude,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	TNotNull<UObject*> OuterForGeneratedAssets,
	TMap<FMetaHumanPaletteItemKey, FCharacterPipelineData>& CharacterPipelineData) const
{
	const bool bCanResizeOutfits = CanResizeOutfits();
	// Do some basic checks to see if a follicle map would be used
	const bool bGenerateFollicleMaps = HairProperties.FollicleMapMaterialParameterName != NAME_None && HairProperties.FollicleMapMaterialSlotNames.Num() > 0;

	// Data used to build a follicle map texture for the pinned groom selections
	TArray<FFollicleInfo> PinnedFollicleMapInfo;
	const TMap<FName, FFollicleInfo::EChannel> FollicleChannelMapping
	{
		{"Hair", FFollicleInfo::EChannel::R},
		{"Eyebrows", FFollicleInfo::EChannel::G},
		{"Beard", FFollicleInfo::EChannel::B},
		{"Mustache", FFollicleInfo::EChannel::B}
	};

	for (const FMetaHumanCharacterPaletteItem& Item : CharacterCollection->GetItems())
	{
		const FMetaHumanPaletteItemPath ItemPath(Item.GetItemKey());

		if (Item.SlotName == NAME_None
			|| !Item.WardrobeItem
			|| SortedItemsToExclude.Contains(ItemPath))
		{
			continue;
		}

		const TOptional<FName> RealSlotName = GetRuntimePipeline()->GetSpecification()->ResolveRealSlotName(Item.SlotName);
		if (!ensure(RealSlotName.IsSet())
			|| !ensure(GetRuntimePipeline()->GetSpecification()->Slots.Contains(RealSlotName.GetValue())))
		{
			// Since the spec was validated above, this shouldn't happen.
			//
			// Handle gracefully anyway by skipping this item.
			continue;
		}

		const UMetaHumanItemPipeline* ItemPipeline = nullptr;
		static_cast<void>(CharacterCollection->TryResolveItemPipeline(ItemPath, ItemPipeline));

		UObject* PrincipalAsset = Item.LoadPrincipalAssetSynchronous();

		if (UGroomBindingAsset* GroomBinding = Cast<UGroomBindingAsset>(PrincipalAsset))
		{
			if (!ItemPipeline)
			{
				ItemPipeline = GetDefault<UMetaHumanGroomPipeline>();
			}

			const UMetaHumanItemEditorPipeline* ItemEditorPipeline = ItemPipeline->GetEditorPipeline();
			if (!ItemEditorPipeline)
			{
				// Can't build this item without an editor pipeline

				// TODO: Log
				continue;
			}

			// TODO: Do this validation at the start of the build

			// Ensure the item pipeline produces build output that's compatible with the slot it's
			// assigned to.
			//
			// The item's output must be a superset of the slot's output, so that all the fields 
			// that the slot is expecting will be initialized.
			const UScriptStruct* ItemOutputStruct = ItemPipeline->GetSpecification()->BuildOutputStruct;
			const UScriptStruct* SlotOutputStruct = GetRuntimePipeline()->GetSpecification()->Slots[RealSlotName.GetValue()].BuildOutputStruct;
			if (!ItemOutputStruct)
			{
				// Items must produce build output in order to be valid

				// TODO: Log
				continue;
			}

			if (SlotOutputStruct
				&& ItemOutputStruct
				&& !ItemOutputStruct->IsChildOf(SlotOutputStruct))
			{
				// Item's output doesn't inherit from the slot's expected output, so this 
				// item isn't compatible with this slot.

				// TODO: Log
				continue;
			}

			const UScriptStruct* ItemInputStruct = ItemEditorPipeline->GetSpecification()->BuildInputStruct;
			const UScriptStruct* SlotInputStruct = Specification->SlotEditorData[RealSlotName.GetValue()].BuildInputStruct;
			check(SlotInputStruct == FMetaHumanGroomPipelineBuildInput::StaticStruct());
			if (ItemInputStruct != SlotInputStruct)
			{
				// The item and slot both need to implement the same struct, otherwise there's no
				// guarantee they will be mutually compatible.
				//
				// For example, if a groom pipeline doesn't take an input to say which meshes to 
				// bind to, how is it going to produce useful groom bindings? 
				//
				// Keeping this requirement strict should make it clearer which pipelines are
				// compatible. In future, we could allow items to declare compatibility with 
				// multiple build input structs if there's a need for more flexibility.

				// TODO: Log
				continue;
			}

			FInstancedStruct BuildInput;
			FMetaHumanGroomPipelineBuildInput& GroomBuildInput = BuildInput.InitializeAs<FMetaHumanGroomPipelineBuildInput>();
			
			for (const TPair<FMetaHumanPaletteItemKey, FCharacterPipelineData>& Pair : CharacterPipelineData)
			{
				if (Pair.Value.FaceMesh)
				{
					GroomBuildInput.BindingMeshes.Add(Pair.Value.FaceMesh);
				}
			}

			GroomBuildInput.FaceLODs = LODProperties.FaceLODs;

			FMetaHumanPaletteBuildCacheEntry BuildCache;// TODO = CharacterCollection->ItemBuildCache.FindOrAdd(ItemPath);
			FMetaHumanPaletteBuiltData ItemBuiltData;

			const TArrayView<const FMetaHumanPinnedSlotSelection> PinnedSlotSelectionsForItem = 
				UMetaHumanCharacterPipeline::FilterPinnedSlotSelectionsToItem(SortedPinnedSlotSelections, ItemPath);

			const TArrayView<const FMetaHumanPaletteItemPath> ItemsToExcludeForItem = UMetaHumanCharacterPipeline::FilterItemPaths(SortedItemsToExclude, ItemPath);

			ItemEditorPipeline->BuildItemSynchronous(
				ItemPath,
				Item.WardrobeItem,
				BuildInput,
				PinnedSlotSelectionsForItem,
				ItemsToExcludeForItem,
				BuildCache,
				Quality,
				TargetPlatform,
				OuterForGeneratedAssets,
				ItemBuiltData);

			if (ItemBuiltData.ContainsOnlyValidBuildOutputForItem(ItemPath))
			{
				if (!ItemBuiltData.ItemBuiltData[ItemPath].BuildOutput.GetScriptStruct()->IsChildOf(ItemOutputStruct))
				{
					// The item produced a struct that isn't compatible with the struct its 
					// specification said it would produce.
					//
					// This behavior is not permitted, as it can cause downstream errors that are
					// hard to diagnose, therefore we consider this a failed build.

					// TODO: Log
					continue;
				}

				BuiltData->PaletteBuiltData.IntegrateItemBuiltData(ItemPath, Item.SlotName, MoveTemp(ItemBuiltData));

				const FMetaHumanPinnedSlotSelection* PinnedItem = nullptr;
				if (FMetaHumanPinnedSlotSelection::TryGetPinnedItem(SortedPinnedSlotSelections, ItemPath, PinnedItem))
				{
					// Follicle map generation is done here for now. It should move into the groom pipeline.
					if (bGenerateFollicleMaps
						&& FollicleChannelMapping.Contains(Item.SlotName))
					{
						const FMetaHumanGroomPipelineBuildOutput& GroomBuildOutput = BuiltData->PaletteBuiltData.ItemBuiltData[ItemPath].BuildOutput.Get<FMetaHumanGroomPipelineBuildOutput>();

						UGroomAsset* Groom = nullptr;
						if (GroomBuildOutput.Bindings.Num() > 0
							&& GroomBuildOutput.Bindings[0])
						{
							Groom = GroomBuildOutput.Bindings[0]->GetGroom();
						}

						if (Groom)
						{
							FFollicleInfo& FollicleInfo = PinnedFollicleMapInfo.AddDefaulted_GetRef();
							FollicleInfo.GroomAsset = Groom;
							FollicleInfo.Channel = FollicleChannelMapping[Item.SlotName];
							FollicleInfo.KernelSizeInPixels = FMath::Max(2, HairProperties.FollicleMapRootRadius);
						}
					}

					// This is a temporary solution. This should be moved into the groom pipeline.
					if (const UMetaHumanDefaultGroomPipeline* GroomPipeline = Cast<UMetaHumanDefaultGroomPipeline>(ItemPipeline))
					{
						for (TPair<FMetaHumanPaletteItemKey, FCharacterPipelineData>& Pair : CharacterPipelineData)
						{
							if (!Pair.Value.GeneratedAssets.IsSet()
								|| !Pair.Value.FaceMesh)
							{
								// Only enabled on non-preview meshes for now
								continue;
							}

							const FMetaHumanCharacterFaceMaterialSet FaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(Pair.Value.FaceMesh);
							TArray<UMaterialInstanceConstant*> FaceMaterials;
							TArray<int32> LODToMaterial;
							const int32 NumLODs = 8;
							LODToMaterial.Reserve(NumLODs);
							for (int32 Index = 0; Index < NumLODs; Index++)
							{
								LODToMaterial.Add(INDEX_NONE);
							}

							FaceMaterialSet.ForEachSkinMaterial<UMaterialInstanceConstant>(
								[&FaceMaterials, &LODToMaterial](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstanceConstant* Material)
								{
									const int32 MaterialIndex = FaceMaterials.Add(Material);

									switch (Slot)
									{
										case EMetaHumanCharacterSkinMaterialSlot::LOD0:
											LODToMaterial[0] = MaterialIndex;
											break;

										case EMetaHumanCharacterSkinMaterialSlot::LOD1:
											LODToMaterial[1] = MaterialIndex;
											break;

										case EMetaHumanCharacterSkinMaterialSlot::LOD2:
											LODToMaterial[2] = MaterialIndex;
											break;

										case EMetaHumanCharacterSkinMaterialSlot::LOD3:
											LODToMaterial[3] = MaterialIndex;
											break;

										case EMetaHumanCharacterSkinMaterialSlot::LOD4:
											LODToMaterial[4] = MaterialIndex;
											break;

										case EMetaHumanCharacterSkinMaterialSlot::LOD5to7:
											LODToMaterial[5] = MaterialIndex;
											LODToMaterial[6] = MaterialIndex;
											LODToMaterial[7] = MaterialIndex;
											break;

										default:
											checkNoEntry();
									}
								});

							// Note that some LODs may have been removed, so LODToMaterial may 
							// still contain some entries set to INDEX_NONE.

							int32 FirstLODBaked = INDEX_NONE;

							const bool bHideHair = false;
							GroomPipeline->SetFaceMaterialParameters(
								FaceMaterials,
								LODToMaterial,
								Item.SlotName,
								PinnedItem->InstanceParameters,
								bHideHair,
								FirstLODBaked);

							// LODs from FirstLODBaked onwards have had their materials changed
							if (Pair.Value.FaceMaterialChangesPerLOD.IsValidIndex(FirstLODBaked))
							{
								for (int32 LODIndex = FirstLODBaked; LODIndex < Pair.Value.FaceMaterialChangesPerLOD.Num(); LODIndex++)
								{
									if (Pair.Value.FaceMaterialChangesPerLOD[LODIndex] != INDEX_NONE)
									{
										Pair.Value.FaceMaterialChangesPerLOD[LODIndex]++;
									}
								}
							}
						}
					}
				}
			}
		}
		else if (UChaosOutfitAsset* ClothAsset = Cast<UChaosOutfitAsset>(PrincipalAsset))
		{
			FInstancedStruct PartOutput;
			FMetaHumanOutfitPipelineBuildOutput& OutfitOutputStruct = PartOutput.InitializeAs<FMetaHumanOutfitPipelineBuildOutput>();

			if (!ItemPipeline)
			{
				ItemPipeline = GetDefault<UMetaHumanOutfitPipeline>();
			}

			const UMetaHumanItemEditorPipeline* ItemEditorPipeline = ItemPipeline->GetEditorPipeline();
			if (!ItemEditorPipeline)
			{
				// Can't build this item without an editor pipeline
				UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Failed to build item containing {PrincipalAsset}: No item editor pipeline found for {ItemPipeline}", 
					GetFullNameSafe(PrincipalAsset), GetFullNameSafe(ItemPipeline));
				continue;
			}

			// Generate a fitted version of this cloth for each Character
			for (TPair<FMetaHumanPaletteItemKey, FCharacterPipelineData>& Pair : CharacterPipelineData)
			{
				// Try to fit this cloth to the Character's body
				UChaosOutfitAsset* ClothForCharacter = nullptr;
				if (bCanResizeOutfits
					&& Pair.Value.MergedHeadAndBody)
				{
					UChaosOutfitAsset* FittedOutfit = NewObject<UChaosOutfitAsset>(OuterForGeneratedAssets);
					FittedOutfit->SetDataflow(CostumeProperties.OutfitResizeDataflowAsset);

					FDataflowVariableOverrides& FittedOutfitVariableOverrides = FittedOutfit->GetDataflowInstance().GetVariableOverrides();

					FittedOutfitVariableOverrides.OverrideVariableObject(UE::MetaHuman::Private::OutfitResize_TargetBodyPropertyName, Pair.Value.MergedHeadAndBody);
					FittedOutfitVariableOverrides.OverrideVariableObject(UE::MetaHuman::Private::OutfitResize_ResizableOutfitPropertyName, ClothAsset);

					FittedOutfitVariableOverrides.OverrideVariableBool("SkipTransferSkinWeights", Pair.Value.bSkipTransferSkinWeights);
					FittedOutfitVariableOverrides.OverrideVariableBool("StripSimMesh", Pair.Value.bStripSimMesh);
			
					
					const FMetaHumanPinnedSlotSelection* PinnedSelection = nullptr;
					if (FMetaHumanPinnedSlotSelection::TryGetPinnedItem(SortedPinnedSlotSelections, ItemPath, PinnedSelection))
					{
						auto OverrideBoolVariable = [PinnedSelection, &FittedOutfitVariableOverrides](const FName& VariableName)
						{
							const bool* VarPtr = PinnedSelection->InstanceParameters.GetValueBool(VariableName).TryGetValue();
							if(VarPtr != nullptr)
							{
								FittedOutfitVariableOverrides.OverrideVariableBool(VariableName, *VarPtr);
							}
						};

						OverrideBoolVariable(TEXT("PruneSkinWeights"));
						OverrideBoolVariable(TEXT("RelaxSkinWeights"));
						OverrideBoolVariable(TEXT("HammerSkinWeights"));
						OverrideBoolVariable(TEXT("ClampSkinWeights"));
						OverrideBoolVariable(TEXT("NormalizeSkinWeights"));
						OverrideBoolVariable(TEXT("ResizeUVs"));
						OverrideBoolVariable(TEXT("CustomRegionResizing"));
					}

					FittedOutfit->GetDataflowInstance().UpdateOwnerAsset(true);

					ClothForCharacter = FittedOutfit;
				}

				if (!ClothForCharacter)
				{
					// Failed to fit the cloth, so pass through the original cloth -- it may not need fitting
					ClothForCharacter = ClothAsset;
				}

				FMetaHumanOutfitGeneratedAssets* OutfitGeneratedAssets = nullptr;

				if (Quality == EMetaHumanCharacterPaletteBuildQuality::Production)
				{
					USkeletalMesh* SkeletalMesh = NewObject<USkeletalMesh>(OuterForGeneratedAssets);

					// For Production quality, we bake to meshes, because Outfits can't yet be cooked
					if (ClothForCharacter->ExportToSkeletalMesh(*SkeletalMesh))
					{
						SkeletalMesh->SetSkeleton(Pair.Value.BodyMesh->GetSkeleton());

						OutfitGeneratedAssets = &OutfitOutputStruct.CharacterAssets.Add(Pair.Key);
						OutfitGeneratedAssets->OutfitMesh = SkeletalMesh;
					}
				}
				else
				{
					OutfitGeneratedAssets = &OutfitOutputStruct.CharacterAssets.Add(Pair.Key);
					OutfitGeneratedAssets->Outfit = ClothForCharacter;
					OutfitGeneratedAssets->CombinedBodyMesh = Pair.Value.MergedHeadAndBody;
				}

				if (OutfitGeneratedAssets)
				{
					if (const UMetaHumanOutfitEditorPipeline* OutfitEditorPipeline = Cast<UMetaHumanOutfitEditorPipeline>(ItemEditorPipeline))
					{
						// When we move this code to the outfit editor pipeline, BodyHiddenFaceMap should come from the build output,
						// and could be different per character, as it will eventually depend on the body measurements.
						
						if (OutfitEditorPipeline->BodyHiddenFaceMap
							&& FMetaHumanPinnedSlotSelection::IsItemPinned(SortedPinnedSlotSelections, ItemPath))
						{
							// TODO: Limiting BodyHiddenFaceMaps to one item since StripLODGeometry will crash when we use
							// multiple face removal maps that applies to overlapping indices.
							if (Pair.Value.BodyHiddenFaceMaps.IsEmpty())
							{
								Pair.Value.BodyHiddenFaceMaps.Add(OutfitEditorPipeline->BodyHiddenFaceMap);
								if (Quality == EMetaHumanCharacterPaletteBuildQuality::Preview)
								{
									// Set the hidden face mask for the preview only
									OutfitGeneratedAssets->BodyHiddenFaceMap = OutfitEditorPipeline->BodyHiddenFaceMap;
								}
							}
							else
							{
								UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Warning, "Currently only one body hidden face map is allowed, ignoring {WardrobeItem}", GetFullNameSafe(Item.WardrobeItem));
							}
						}
					}
				}
			}

			FMetaHumanPipelineBuiltData& ItemBuildOutput = BuiltData->PaletteBuiltData.ItemBuiltData.Add(FMetaHumanPaletteItemPath(Item.GetItemKey()));
			ItemBuildOutput.SlotName = Item.SlotName;
			ItemBuildOutput.BuildOutput = MoveTemp(PartOutput);
		}
	}

	if (PinnedFollicleMapInfo.Num() > 0)
	{
		const int32 Resolution = static_cast<int32>(HairProperties.FollicleMapResolution);
		const int32 MipCount = FMath::FloorLog2(Resolution) + 1;

		UTexture2D* FollicleMap = NewObject<UTexture2D>(OuterForGeneratedAssets);
		FGroomTextureBuilder::AllocateFollicleTextureResources(FollicleMap, FIntPoint(Resolution), MipCount);

		// Blur the lower mips, as this looks better
		FollicleMap->MipGenSettings = TMGS_Blur5;
		
		// Need Pre/PostEditChange around updating the texture's image data
		{
			FollicleMap->PreEditChange(nullptr);

			FGroomTextureBuilder::BuildFollicleTexture(PinnedFollicleMapInfo, FollicleMap, false);

			FollicleMap->PostEditChange();
		}

		// The follicle map is character-independent, so set the same on all characters
		for (TPair<FMetaHumanPaletteItemKey, FCharacterPipelineData>& Pair : CharacterPipelineData)
		{
			Pair.Value.FollicleMap = FollicleMap;

			// Add the follicle map to the generated assets metadata so that it gets unpacked with the character's assets
			if (Pair.Value.GeneratedAssets.IsSet())
			{
				Pair.Value.GeneratedAssets.GetValue().Metadata.Emplace(FollicleMap, TEXT("Grooms"), TEXT("T_FollicleMap"));
			}
		}
	}
}

bool UMetaHumanDefaultEditorPipelineBase::CanResizeOutfits() const
{
	if (!CostumeProperties.OutfitResizeDataflowAsset)
	{
		return false;
	}

	const FInstancedPropertyBag& SourceVariables = CostumeProperties.OutfitResizeDataflowAsset->Variables;
	const FPropertyBagPropertyDesc* TargetBodyProperty = SourceVariables.FindPropertyDescByName(UE::MetaHuman::Private::OutfitResize_TargetBodyPropertyName);
	const FPropertyBagPropertyDesc* ResizableOutfitProperty = SourceVariables.FindPropertyDescByName(UE::MetaHuman::Private::OutfitResize_ResizableOutfitPropertyName);

	return TargetBodyProperty
		&& TargetBodyProperty->IsObjectType()
		&& USkeletalMesh::StaticClass()->IsChildOf(Cast<UClass>(TargetBodyProperty->ValueTypeObject))
		&& ResizableOutfitProperty
		&& ResizableOutfitProperty->IsObjectType()
		&& UChaosOutfitAsset::StaticClass()->IsChildOf(Cast<UClass>(ResizableOutfitProperty->ValueTypeObject));
}

void UMetaHumanDefaultEditorPipelineBase::UnpackCollectionAssets(
	TNotNull<UMetaHumanCollection*> Collection,
	FMetaHumanCollectionBuiltData& CollectionBuiltData,
	const FOnUnpackComplete& OnComplete) const
{
	// TODO: UnpackCollectionAssets should use the existing built data instead of doing its own build

	Collection->Build(
		FInstancedStruct(),
		EMetaHumanCharacterPaletteBuildQuality::Production,
		GetTargetPlatformManagerRef().GetRunningTargetPlatform(),
		UMetaHumanCollection::FOnBuildComplete::CreateUObject(
			this,
			&UMetaHumanDefaultEditorPipelineBase::OnCharacterPaletteAssetsUnpacked,
			TWeakObjectPtr<UMetaHumanCollection>(Collection),
			TNotNull<FMetaHumanCollectionBuiltData*>(&CollectionBuiltData),
			OnComplete),
		Collection->GetDefaultInstance()->ToPinnedSlotSelections(EMetaHumanUnusedSlotBehavior::PinnedToEmpty));
}

void UMetaHumanDefaultEditorPipelineBase::OnCharacterPaletteAssetsUnpacked(
	EMetaHumanBuildStatus Result,
	TWeakObjectPtr<UMetaHumanCollection> WeakCollection,
	TNotNull<FMetaHumanCollectionBuiltData*> CollectionBuiltData,
	FOnUnpackComplete OnComplete) const
{
	TStrongObjectPtr<UMetaHumanCollection> StrongCollection = WeakCollection.Pin();
	UMetaHumanCollection* Collection = StrongCollection.Get();

	if (!Collection
		|| Result == EMetaHumanBuildStatus::Failed)
	{
		OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
		return;
	}

	const FString UnpackFolder = Collection->GetUnpackFolder();

	// The paths of all unpacked assets, so that we can ensure we don't unpack two different 
	// assets to the same path.
	TSet<FString> UnpackedAssetPaths;

	for (TPair<FMetaHumanPaletteItemPath, FMetaHumanPipelineBuiltData>& Item : CollectionBuiltData->PaletteBuiltData.ItemBuiltData)
	{
		// Only process items directly owned by the collection, i.e. not sub-items
		if (!Item.Key.IsDirectChildPathOf(FMetaHumanPaletteItemPath::Collection))
		{
			continue;
		}

		if (const FMetaHumanCharacterPartOutput* CharacterPart = Item.Value.BuildOutput.GetPtr<FMetaHumanCharacterPartOutput>())
		{
			// TODO: It seems that reporting progress is causing a crash when exporting to UEFN since it causes a redraw
			// FScopedSlowTask UnpackSlowTask(CharacterPart->GeneratedAssets.Metadata.Num(), LOCTEXT("UnpackingCharacterItemsTask", "Unpacking Character Assets"));
			// UnpackSlowTask.MakeDialog();

			for (const FMetaHumanGeneratedAssetMetadata& AssetMetadata : CharacterPart->GeneratedAssets.Metadata)
			{
				// UnpackSlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("UnpackingItem", "Unpacking '{0}'"), FText::FromName(GetFNameSafe(AssetMetadata.Object))));

				if (!AssetMetadata.Object)
				{
					continue;
				}

				FString AssetPackagePath = UnpackFolder;

				if (!AssetMetadata.PreferredSubfolderPath.IsEmpty())
				{
					if (AssetMetadata.bSubfolderIsAbsolute)
					{
						AssetPackagePath = AssetMetadata.PreferredSubfolderPath;
					}
					else
					{
						AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredSubfolderPath;
					}
				}

				if (!AssetMetadata.PreferredName.IsEmpty())
				{
					AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredName;
				}
				else
				{
					AssetPackagePath = AssetPackagePath / AssetMetadata.Object->GetName();
				}

				if (!TryUnpackObject(AssetMetadata.Object, Collection, AssetPackagePath, UnpackedAssetPaths))
				{
					OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
					return;
				}
			}
		}
		else if (const FMetaHumanOutfitPipelineBuildOutput* OutfitPart = Item.Value.BuildOutput.GetPtr<FMetaHumanOutfitPipelineBuildOutput>())
		{
			FScopedSlowTask UnpackSlowTask(OutfitPart->CharacterAssets.Num(), LOCTEXT("UnpackingClothAssets", "Unpacking Clothing Assets"));
			UnpackSlowTask.MakeDialog();

			for (const TPair<FMetaHumanPaletteItemKey, FMetaHumanOutfitGeneratedAssets>& Pair : OutfitPart->CharacterAssets)
			{
				{
					const FString AssetName = FString::Format(TEXT("{0}_{1}"), { Pair.Key.ToAssetNameString(), Item.Value.SlotName.ToString() }).Replace(TEXT(" "), TEXT(""));
					FString AssetPackagePath = UnpackFolder / TEXT("Clothing") / AssetName;

					UnpackSlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("UnpackingCloshAsset", "Unpacking Clothing Asset '{0}'"), FText::FromString(AssetName)));

					// There will either be an Outfit or a mesh baked from an Outfit, but not both
					UObject* AssetToUnpack = Pair.Value.Outfit ? static_cast<UObject*>(Pair.Value.Outfit) : static_cast<UObject*>(Pair.Value.OutfitMesh);
					if (AssetToUnpack)
					{
						if (!TryUnpackObject(AssetToUnpack, Collection, AssetPackagePath, UnpackedAssetPaths))
						{
							OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
							return;
						}
					}
				}

				if (Pair.Value.CombinedBodyMesh)
				{
					// This mesh has no render data, which can cause crashes if the engine tries to
					// render it or save it to disk.
					//
					// Now that it's being unpacked, it's going to be visible to other systems and 
					// therefore its render data needs to be built.
					Pair.Value.CombinedBodyMesh->PostEditChange();

					// There should only be one combined head/body mesh per Character, so the name 
					// only needs to reference the Character name in order to be unique.
					const FString AssetName = FString::Format(TEXT("{0}_CombinedBody"), { Pair.Key.ToAssetNameString() });
					FString AssetPackagePath = UnpackFolder / TEXT("Cloth") / AssetName;

					// If the mesh has already been unpacked for another outfit, this will silently
					// succeed, so there's no need to check this before calling.
					if (!TryUnpackObject(Pair.Value.CombinedBodyMesh, Collection, AssetPackagePath, UnpackedAssetPaths))
					{
						OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
						return;
					}
				}
			}
		}
		else
		{
			const UMetaHumanItemPipeline* ItemPipeline = nullptr;
			if (Collection->TryResolveItemPipeline(Item.Key, ItemPipeline))
			{
				const UMetaHumanCharacterPalette* ContainingPalette = nullptr;
				FMetaHumanCharacterPaletteItem ResolvedItem;
				verify(Collection->TryResolveItem(Item.Key, ContainingPalette, ResolvedItem));

				if (!ResolvedItem.WardrobeItem)
				{
					OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
					return;
				}

				if (!ItemPipeline->GetEditorPipeline()->TryUnpackItemAssets(
					ResolvedItem.WardrobeItem,
					Item.Key,
					// TODO: Filter this to just the built data belonging to this item and its sub-items
					CollectionBuiltData->PaletteBuiltData.ItemBuiltData,
					UnpackFolder,
					FTryUnpackObjectDelegate::CreateWeakLambda(this, 
						[this, Collection, &UnpackedAssetPaths](TNotNull<UObject*> Object, FString& InOutAssetPath)
						{
							return TryUnpackObject(Object, Collection, InOutAssetPath, UnpackedAssetPaths);
						})))
				{
					OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Failed);
					return;
				}
			}
		}
	}

	OnComplete.ExecuteIfBound(EMetaHumanBuildStatus::Succeeded);
}

bool UMetaHumanDefaultEditorPipelineBase::TryUnpackObject(
	UObject* Object,
	UObject* UnpackingAsset,
	FString& InOutAssetPath,
	TSet<FString>& OutUnpackedAssetPaths) const
{
	if (Object->GetOuter()->IsA<UPackage>()
		&& Object->GetFName() == FPackageName::GetShortName(CastChecked<UPackage>(Object->GetOuter())))
	{
		// This object is already the principal asset of its package and doesn't need unpacking
		FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(Object);
		
		return true;
	}

	if (!Object->IsInPackage(UnpackingAsset->GetPackage()))
	{
		// Can't unpack this object, as the asset being unpacked doesn't own it
		return false;
	}

	if (InOutAssetPath.Len() == 0)
	{
		InOutAssetPath = Object->GetName();
	}

	bool bIsUnpackedPathAlreadyUsed = false;
	OutUnpackedAssetPaths.Add(InOutAssetPath, &bIsUnpackedPathAlreadyUsed);

	if (bIsUnpackedPathAlreadyUsed)
	{
		const FRegexPattern Pattern(TEXT("^(.*)_(\\d+)$"));

		while (bIsUnpackedPathAlreadyUsed)
		{
			FRegexMatcher Matcher(Pattern, InOutAssetPath);

			if (Matcher.FindNext())
			{
				// The asset name is already in the format Name_Index, and so we can simply increment
				// the index
				const int32 ExistingNameIndex = FCString::Atoi(*Matcher.GetCaptureGroup(2));

				InOutAssetPath = FString::Format(TEXT("{0}_{1}"), { Matcher.GetCaptureGroup(1), ExistingNameIndex + 1 });
			}
			else
			{
				// Append a new index to the name, starting at 2
				InOutAssetPath = InOutAssetPath + TEXT("_2");
			}

			// Try to add the new name to see if it's unique
			OutUnpackedAssetPaths.Add(InOutAssetPath, &bIsUnpackedPathAlreadyUsed);
		}
	}

	return TryMoveObjectToAssetPackage(Object, InOutAssetPath);
}

bool UMetaHumanDefaultEditorPipelineBase::TryMoveObjectToAssetPackage(
	UObject* Object,
	FStringView NewAssetPath) const
{
	UPackage* AssetPackage = UPackageTools::FindOrCreatePackageForAssetType(FName(NewAssetPath), Object->GetClass());
	const FString AssetName = FPackageName::GetShortName(AssetPackage);

	// Attempt to load an object from this package to see if one already exists
	const FString AssetPath = AssetPackage->GetName() + TEXT(".") + AssetName;
	UObject* ExistingAsset = LoadObject<UObject>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);

	// Rename any existing object out of the way
	if (ExistingAsset)
	{
		if (UBlueprint* ExistingBlueprintAsset = Cast<UBlueprint>(ExistingAsset))
		{
			if (!ExistingAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_SkipGeneratedClasses))
			{
				return false;
			}
			const FName UniqueName = MakeUniqueObjectName(GetTransientPackage(), ExistingBlueprintAsset->StaticClass()); 
			ExistingBlueprintAsset->RenameGeneratedClasses(*UniqueName.ToString(), GetTransientPackage(), REN_DontCreateRedirectors);
		}
		else if (!ExistingAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors))
		{
			return false;
		}
	}

	if (!Object->Rename(*AssetName, AssetPackage, REN_DontCreateRedirectors))
	{
		return false;
	}

	Object->ClearFlags(RF_Transient);
	Object->SetFlags(RF_Public | RF_Transactional | RF_Standalone);
	Object->MarkPackageDirty();

	// Notify the asset registry so that the asset appears in the Content Browser
	if (!ExistingAsset)
	{
		FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(Object);

		FAssetRegistryModule::AssetCreated(Object);
	}

	return true;
}

bool UMetaHumanDefaultEditorPipelineBase::TryUnpackInstanceAssets(
	TNotNull<UMetaHumanCharacterInstance*> Instance,
	FInstancedStruct& AssemblyOutput,
	TArray<FMetaHumanGeneratedAssetMetadata>& AssemblyAssetMetadata,
	const FString& TargetFolder) const
{
	// Since this is not shared with the build unpack, technically the assets could clash.
	//
	// The process of unpacking an instance will be reworked in future to deal with this properly.
	TSet<FString> UnpackedAssetPaths;

	for (FMetaHumanGeneratedAssetMetadata& AssetMetadata : AssemblyAssetMetadata)
	{
		if (!AssetMetadata.Object)
		{
			continue;
		}

		FString AssetPackagePath = TargetFolder;

		if (!AssetMetadata.PreferredSubfolderPath.IsEmpty())
		{
			if (AssetMetadata.bSubfolderIsAbsolute)
			{
				AssetPackagePath = AssetMetadata.PreferredSubfolderPath;
			}
			else
			{
				AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredSubfolderPath;
			}
		}

		if (!AssetMetadata.PreferredName.IsEmpty())
		{
			AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredName;
		}
		else
		{
			AssetPackagePath = AssetPackagePath / AssetMetadata.Object->GetName();
		}

		if (const UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(AssetMetadata.Object))
		{
			AssetMetadata.Object = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(MID, MID->GetOuter());

			ReplaceReferencesInAssemblyOutput(AssemblyOutput, MID, AssetMetadata.Object);
		}

		if (!TryUnpackObject(AssetMetadata.Object, Instance, AssetPackagePath, UnpackedAssetPaths))
		{
			return false;
		}
	}

	return true;
}

void UMetaHumanDefaultEditorPipelineBase::ReplaceReferencesInAssemblyOutput(
	FInstancedStruct& AssemblyOutput, 
	TNotNull<const UObject*> OriginalObject, 
	TNotNull<UObject*> ReplacementObject)
{
	FMetaHumanDefaultAssemblyOutput* DefaultOutput = AssemblyOutput.GetMutablePtr<FMetaHumanDefaultAssemblyOutput>();
	if (!ensure(DefaultOutput))
	{
		return;
	}

	// For now this is hardcoded to search properties that are known to need replacing.
	//
	// In future, it will do a generic search over all object properties in the AssemblyOutput.
	auto FindReplaceOverrideMaterials = 
		[OriginalObject, ReplacementObject](FMetaHumanGroomPipelineAssemblyOutput& GroomOutput)
		{
			for (TPair<FName, TObjectPtr<UMaterialInterface>>& Pair : GroomOutput.OverrideMaterials)
			{
				if (Pair.Value == OriginalObject)
				{
					Pair.Value = CastChecked<UMaterialInterface>(ReplacementObject);
				}
			}
		};

	FindReplaceOverrideMaterials(DefaultOutput->Hair);
	FindReplaceOverrideMaterials(DefaultOutput->Eyebrows);
	FindReplaceOverrideMaterials(DefaultOutput->Beard);
	FindReplaceOverrideMaterials(DefaultOutput->Mustache);
	FindReplaceOverrideMaterials(DefaultOutput->Eyelashes);
	FindReplaceOverrideMaterials(DefaultOutput->Peachfuzz);

	for (FMetaHumanSkeletalMeshPipelineAssemblyOutput& Data : DefaultOutput->SkeletalMeshData)
	{
		for (TPair<FName, TObjectPtr<UMaterialInterface>>& Pair : Data.OverrideMaterials)
		{
			if (Pair.Value == OriginalObject)
			{
				Pair.Value = CastChecked<UMaterialInterface>(ReplacementObject);
			}
		}
	}
	
	for (FMetaHumanOutfitPipelineAssemblyOutput& Data : DefaultOutput->ClothData)
	{
		for (TPair<FName, TObjectPtr<UMaterialInterface>>& Pair : Data.OverrideMaterials)
		{
			if (Pair.Value == OriginalObject)
			{
				Pair.Value = CastChecked<UMaterialInterface>(ReplacementObject);
			}
		}
	}
}

TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> UMetaHumanDefaultEditorPipelineBase::GetSpecification() const
{
	return Specification;
}

TSubclassOf<AActor> UMetaHumanDefaultEditorPipelineBase::GetEditorActorClass() const
{
	return EditorActorClass;
}

bool UMetaHumanDefaultEditorPipelineBase::TryBakeMaterials(
	const FString& BaseOutputFolder,
	const FMetaHumanMaterialBakingOptions& InMaterialBakingOptions,
	TArray<FSkeletalMaterial>& InOutSkelMeshMaterials,
	const TMap<FName, UMaterialInterface*>& RemovedMaterialSlots,
	const TArray<int32>& MaterialChangesPerLOD,
	TNotNull<UObject*> GeneratedAssetOuter,
	FMetaHumanCharacterGeneratedAssets& InOutGeneratedAssets) const
{
	TNotNull<UMetaHumanMaterialBakingSettings*> BakingSettings = InMaterialBakingOptions.BakingSettings.LoadSynchronous();

	FScopedSlowTask BakeTask{ 4, LOCTEXT("BakingFaceMaterialsTaskLabel", "Baking Face Materials") };
	BakeTask.MakeDialog();

	struct FGeneratedMaterialInstance
	{
		// Ensure that new instances are not GC'ed since we may run GC in between TG export tasks
		TStrongObjectPtr<UMaterialInstanceConstant> MaterialInstance;
		TArray<FName> AdditionalMaterialSlotNames;
	};

	TMap<FName, FGeneratedMaterialInstance> NewMaterialInstances;
	NewMaterialInstances.Reserve(BakingSettings->BakedMaterials.Num());

	BakeTask.EnterProgressFrame(1.0f, LOCTEXT("CreatingMaterialInstances", "Creating Material Instances"));

	// Create a new Material Instance for each of the specified materials
	for (const FMetaHumanBakedMaterialProperties& BakedMaterial : BakingSettings->BakedMaterials)
	{
		// Do not create materials for removed slots
		if (RemovedMaterialSlots.Contains(BakedMaterial.PrimaryMaterialSlotName))
		{
			continue;
		}

		UMaterialInstance* BakedMaterialInstance = Cast<UMaterialInstance>(BakedMaterial.Material);
		if (BakedMaterialInstance == nullptr)
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error()
				->AddToken(FTextToken::Create(LOCTEXT("CreateBakedMaterial_Failed1", "Material baking: ")))
				->AddToken(FUObjectToken::Create(BakedMaterial.Material))
				->AddToken(FTextToken::Create(LOCTEXT("CreateBakedMaterial_Failed2", " must be a Material Instance")));
			return false;
		}

		FGeneratedMaterialInstance NewEntry;
		NewEntry.MaterialInstance = TStrongObjectPtr<UMaterialInstanceConstant>(UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(BakedMaterialInstance, GeneratedAssetOuter));
		NewEntry.AdditionalMaterialSlotNames = BakedMaterial.AdditionalMaterialSlotNames;

		if (const FSkeletalMaterial* FoundMaterial = Algo::FindBy(InOutSkelMeshMaterials, BakedMaterial.PrimaryMaterialSlotName, &FSkeletalMaterial::MaterialSlotName))
		{
			for (FName ParameterToCopy : BakedMaterial.ParametersToCopy)
			{
				float ScalarParam;
				FLinearColor VectorParam;
				UTexture* TextureParam;
				if (FoundMaterial->MaterialInterface->GetScalarParameterValue(ParameterToCopy, ScalarParam))
				{
					NewEntry.MaterialInstance->SetScalarParameterValueEditorOnly(ParameterToCopy, ScalarParam);
				}
				else if (FoundMaterial->MaterialInterface->GetVectorParameterValue(ParameterToCopy, VectorParam))
				{
					NewEntry.MaterialInstance->SetVectorParameterValueEditorOnly(ParameterToCopy, VectorParam);
				}
				else if (FoundMaterial->MaterialInterface->GetTextureParameterValue(ParameterToCopy, TextureParam))
				{
					NewEntry.MaterialInstance->SetTextureParameterValueEditorOnly(ParameterToCopy, TextureParam);
				}
			}

			// Remove metadata for the material we are replacing so it doesn't get unpacked
			InOutGeneratedAssets.RemoveAssetMetadata(FoundMaterial->MaterialInterface);
		}

		// Add the replacement material to the list
		InOutGeneratedAssets.Metadata.Emplace(NewEntry.MaterialInstance.Get(), BakedMaterial.OutputMaterialFolder, BakedMaterial.OutputMaterialName.ToString());

		NewMaterialInstances.Add(BakedMaterial.PrimaryMaterialSlotName, NewEntry);
	}

	struct FGeneratedTexture
	{
		UMaterialInstanceConstant* MaterialInstance = nullptr;
		FName ParameterName;
		TSoftObjectPtr<UTexture> Texture;
	};

	TArray<FGeneratedTexture> GeneratedTextures;

	BakeTask.EnterProgressFrame(1.0f, LOCTEXT("RunningTextureGraphs", "Running Texture Graphs"));

	for (const FMetaHumanTextureGraphOutputProperties& Graph : BakingSettings->TextureGraphs)
	{
		if (!Graph.TextureGraphInstance)
		{
			return false;
		}

		// Do not bake textures if all output slots are removed
		{
			bool bHasActiveSlots = false;
			for (const FMetaHumanOutputTextureProperties& OutputTexture : Graph.OutputTextures)
			{
				for (const FName& OutputMaterialSlotName : OutputTexture.OutputMaterialSlotNames)
				{
					if (!RemovedMaterialSlots.Contains(OutputMaterialSlotName))
					{
						bHasActiveSlots = true;
						break;
					}
				}

				if (bHasActiveSlots)
				{
					break;
				}
			}

			if (!bHasActiveSlots)
			{
				continue;
			}
		}

		UObject* TextureGraphOuter = BakingSettings->bGenerateTextureGraphInstanceAssets ? GeneratedAssetOuter : GetTransientPackage();
		UTextureGraphInstance* TextureGraphInstance = DuplicateObject<UTextureGraphInstance>(Graph.TextureGraphInstance, TextureGraphOuter);

		// If the user wants to keep the TGIs, generate metadata so that they get unpacked
		if (BakingSettings->bGenerateTextureGraphInstanceAssets)
		{
			FMetaHumanGeneratedAssetMetadata& Metadata = InOutGeneratedAssets.Metadata.AddDefaulted_GetRef();
			Metadata.Object = TextureGraphInstance;
			Metadata.PreferredSubfolderPath = TEXT("TextureGraphs");
			Metadata.PreferredName = TextureGraphInstance->GetName();
		}

		if (!TextureGraphInstance->Graph())
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error()
				->AddToken(FTextToken::Create(LOCTEXT("BakeFaceMatsFailure_TextureGraphInvalid1", "Material baking: ")))
				->AddToken(FUObjectToken::Create(Graph.TextureGraphInstance))
				->AddToken(FTextToken::Create(LOCTEXT("BakeFaceMatsFailure_TextureGraphInvalid2", " is in an invalid state after being duplicated")));

			return false;
		}

		bool bSkipDueToPreviousLODMatch = true;
		for (const FMetaHumanInputMaterialProperties& InputMaterial : Graph.InputMaterials)
		{
			FVarArgument* Argument = TextureGraphInstance->InputParams.VarArguments.Find(InputMaterial.InputParameterName);
			if (!Argument)
			{
				const FText Message = FText::Format(
					LOCTEXT("BakeFaceMatsFailure_InputParamNotFound", "Material baking: Failed to find input parameter {0} in "),
					FText::FromName(InputMaterial.InputParameterName));

				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message)
					->AddToken(FUObjectToken::Create(Graph.TextureGraphInstance));

				return false;
			}

			if (InputMaterial.MainSectionTopLODIndex != INDEX_NONE
				&& MaterialChangesPerLOD.IsValidIndex(InputMaterial.MainSectionTopLODIndex)
				&& MaterialChangesPerLOD[InputMaterial.MainSectionTopLODIndex] != INDEX_NONE)
			{
				int32 PreviousValidChangeNumber = INDEX_NONE;
				for (int32 MaterialChangeIndex = InputMaterial.MainSectionTopLODIndex - 1; MaterialChangeIndex >= 0; MaterialChangeIndex--)
				{
					if (MaterialChangesPerLOD[MaterialChangeIndex] != INDEX_NONE)
					{
						PreviousValidChangeNumber = MaterialChangesPerLOD[MaterialChangeIndex];
						break;
					}
				}

				if (PreviousValidChangeNumber == MaterialChangesPerLOD[InputMaterial.MainSectionTopLODIndex])
				{
					// This is the same as the previous LOD's material, so skip this bake
					continue;
				}
			}

			// At least one input material is different, so the bake should go ahead
			bSkipDueToPreviousLODMatch = false;

			UMaterialInterface* SourceMaterial = nullptr;

			const FSkeletalMaterial* MaterialSlot = Algo::FindBy(InOutSkelMeshMaterials, InputMaterial.SourceMaterialSlotName, &FSkeletalMaterial::MaterialSlotName);
			if (MaterialSlot
				&& MaterialSlot->MaterialInterface)
			{
				SourceMaterial = MaterialSlot->MaterialInterface;
			}
			else 
			{
				UMaterialInterface* const* RemovedMaterialPtr = RemovedMaterialSlots.Find(InputMaterial.SourceMaterialSlotName);
				if (RemovedMaterialPtr
					&& *RemovedMaterialPtr)
				{
					SourceMaterial = *RemovedMaterialPtr;
				}
				else
				{
					const FText Message = FText::Format(
						LOCTEXT("BakeFaceMatsFailure_MaterialSlotNotFound", "Material baking: Failed to find material slot {0} on face mesh"),
						FText::FromName(InputMaterial.SourceMaterialSlotName));

					FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);

					return false;
				}
			}

			check(SourceMaterial);

			if (SourceMaterial->GetOuter() == GeneratedAssetOuter)
			{
				if (!BakingSettings->bGenerateTextureGraphInstanceAssets)
				{
					// The user doesn't want to keep the source materials, so remove them from the list of assets to unpack
					InOutGeneratedAssets.RemoveAssetMetadata(SourceMaterial);
				}
			}

			FTG_Material MaterialValue;
			MaterialValue.AssetPath = SourceMaterial->GetPathName();
			Argument->Var.SetAs(MaterialValue);
		}

		if (bSkipDueToPreviousLODMatch)
		{
			// Skip this bake
			continue;
		}

		for (const FMetaHumanOutputTextureProperties& OutputTexture : Graph.OutputTextures)
		{
			FTG_OutputSettings* OutputSettings = nullptr;
			for (TPair<FTG_Id, FTG_OutputSettings>& Pair : TextureGraphInstance->OutputSettingsMap)
			{
				// The Texture Graph team has provided us with this temporary workaround to get the 
				// output parameter name.
				//
				// The hardcoded constant will be removed when a proper solution is available.
				const int32 PinIndex = 3;
				const FTG_Id PinId(Pair.Key.NodeIdx(),  PinIndex);

				if (TextureGraphInstance->Graph()->GetParamName(PinId) == OutputTexture.OutputTextureNameInGraph)
				{
					OutputSettings = &Pair.Value;
					break;
				}
			}

			if (!OutputSettings)
			{
				const FText Message = FText::Format(
					LOCTEXT("BakeFaceMatsFailure_OutputTextureNotFound1", "Material baking: Failed to find output texture {0} in "),
					FText::FromName(OutputTexture.OutputTextureNameInGraph));

				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message)
					->AddToken(FUObjectToken::Create(Graph.TextureGraphInstance));

				return false;
			}

			OutputSettings->FolderPath = *(BaseOutputFolder / OutputTexture.OutputTextureFolder);

			if (OutputTexture.OutputTextureName != NAME_None)
			{
				OutputSettings->BaseName = OutputTexture.OutputTextureName;
			}

			// Override the texture resolution if specified by the pipeline
			if (const EMetaHumanBuildTextureResolution* OverrideResolution = InMaterialBakingOptions.TextureResolutionsOverrides.Find(OutputTexture.OutputTextureName))
			{
				const int32 Resolution = static_cast<int32>(*OverrideResolution);

				if (Resolution <= 0 || Resolution > static_cast<int32>(EMetaHumanBuildTextureResolution::Res8192))
				{
					const FText Message = FText::Format(LOCTEXT("InvalidResolution", "Invalid texture resolution override for texture '{0}': '{1}'"),
														FText::FromName(OutputTexture.OutputTextureName),
														Resolution);
					FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				}
				else
				{
					OutputSettings->Width = static_cast<EResolution>(Resolution);
					OutputSettings->Height = static_cast<EResolution>(Resolution);
				}
			}

			for (const FName& OutputMaterialSlotName : OutputTexture.OutputMaterialSlotNames)
			{
				// Ignore removed slots
				if (RemovedMaterialSlots.Contains(OutputMaterialSlotName))
				{
					continue;
				}

				if (NewMaterialInstances.Contains(OutputMaterialSlotName))
				{
					FGeneratedTexture& GeneratedTexture = GeneratedTextures.AddDefaulted_GetRef();
					GeneratedTexture.MaterialInstance = NewMaterialInstances[OutputMaterialSlotName].MaterialInstance.Get();
					GeneratedTexture.ParameterName = OutputTexture.OutputMaterialParameterName;

					const FString PackageName = OutputSettings->FolderPath.ToString() / OutputSettings->BaseName.ToString();
					const FString AssetPath = FString::Format(TEXT("{0}.{1}"), { PackageName, OutputSettings->BaseName.ToString() });
					GeneratedTexture.Texture = TSoftObjectPtr<UTexture>(FSoftObjectPath(AssetPath));
				}
				else
				{
					const FText Message = FText::Format(
						LOCTEXT("BakeFaceMatsFailure_BakedMaterialNotFound", "Failed to find a Baked Material entry with PrimaryMaterialSlotName set to {0}. This is being referenced by an output texture."),
						FText::FromName(OutputMaterialSlotName));

					FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);

					return false;

				}
			}
		}

		const bool bOverwriteTextures = true;
		const bool bSave = false;
		const bool bExportAll = false;
		const bool bDisableCache = true; // Disable the TG cache since we only need to run the TG instances once
										 // Works around issues with memory allocated for the cache not getting released in the editor
		UTG_AsyncExportTask* Task = UTG_AsyncExportTask::TG_AsyncExportTask(TextureGraphInstance, bOverwriteTextures, bSave, bExportAll, bDisableCache);
		Task->ActivateBlocking(nullptr);
		Task->MarkAsGarbage();

		// Running GC at this point will free the memory allocated by the TG export task and not needed since the created TG instance will not be re-used
		// This helps to reduce the total memory usage spike of running all the MH TG instances one after another
		if (UE::MetaHuman::Private::CVarMHCEnableGCOnTextureBaking.GetValueOnAnyThread())
		{
			TryCollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}

	BakeTask.EnterProgressFrame(1.0f, LOCTEXT("AssigningGeneratedTextures", "Assigning baked textures"));

	for (const FGeneratedTexture& GeneratedTexture : GeneratedTextures)
	{
		UTexture* ActualTexture = GeneratedTexture.Texture.Get();
		if (ActualTexture)
		{
			// Texture Graph generated textures are created with RF_MarkAsRootSet, which means they won't be garbage collected, even when not being referenced.
			// This is important when exporting to UEFN as the project is mounted as a plugin and if there are objects that are not garbage collected the plugin
			// will fail to unload. The textures exported here are meant to be referenced by some material so its safe to remove them from root here
			ActualTexture->RemoveFromRoot();

			const bool bAlreadyIncluded = InOutGeneratedAssets.Metadata.ContainsByPredicate([ActualTexture](const FMetaHumanGeneratedAssetMetadata& CandidateMetadata)
																							{
																								return CandidateMetadata.Object == ActualTexture;
																							});

			if (!bAlreadyIncluded)
			{
				// Add the texture metadata to the list of generated assets
				FMetaHumanGeneratedAssetMetadata TextureMetadata;
				TextureMetadata.Object = ActualTexture;
				InOutGeneratedAssets.Metadata.Emplace(TextureMetadata);
			}

			GeneratedTexture.MaterialInstance->SetTextureParameterValueEditorOnly(GeneratedTexture.ParameterName, ActualTexture);
		}
		else
		{
			const FText Message = FText::Format(
				LOCTEXT("BakeFaceMatsFailure_BakedTextureNotFound", "Couldn't find baked texture {0}. This should have been produced by the texture graph."),
				FText::FromString(GeneratedTexture.Texture.ToString()));

			FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);

			return false;
		}
	}

	BakeTask.EnterProgressFrame(1.0f, LOCTEXT("AssigningMaterials", "Assigning materials to Face mesh"));

	for (const TPair<FName, FGeneratedMaterialInstance>& NewMaterialInstance : NewMaterialInstances)
	{
		// Primary slot
		{
			FSkeletalMaterial* MaterialSlot = Algo::FindBy(InOutSkelMeshMaterials, NewMaterialInstance.Key, &FSkeletalMaterial::MaterialSlotName);
			if (MaterialSlot)
			{
				MaterialSlot->MaterialInterface = NewMaterialInstance.Value.MaterialInstance.Get();
			}
			else if (!RemovedMaterialSlots.Contains(NewMaterialInstance.Key))
			{
				const FText Message = FText::Format(
					LOCTEXT("BakeFaceMatsFailure_MaterialSlotNotFoundForBakedMaterial", "Failed to find material slot {0} on face mesh. This is referenced from the Baked Materials array."),
					FText::FromName(NewMaterialInstance.Key));

				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);

				return false;
			}
		}

		// Additional slots
		for (const FName AdditionalMaterialSlotName : NewMaterialInstance.Value.AdditionalMaterialSlotNames)
		{
			FSkeletalMaterial* MaterialSlot = Algo::FindBy(InOutSkelMeshMaterials, AdditionalMaterialSlotName, &FSkeletalMaterial::MaterialSlotName);
			if (MaterialSlot)
			{
				MaterialSlot->MaterialInterface = NewMaterialInstance.Value.MaterialInstance.Get();
			}
			else if (!RemovedMaterialSlots.Contains(AdditionalMaterialSlotName))
			{
				const FText Message = FText::Format(
					LOCTEXT("BakeFaceMatsFailure_MaterialSlotNotFoundForBakedMaterial", "Failed to find material slot {0} on face mesh. This is referenced from the Baked Materials array."),
					FText::FromName(AdditionalMaterialSlotName));

				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);

				return false;
			}
		}
	}

	return true;
}

UBlueprint* UMetaHumanDefaultEditorPipelineBase::WriteActorBlueprintHelper(
	TSubclassOf<AActor> InBaseActorClass,
	const FString& InBlueprintPath,
	const TFunction<bool(UBlueprint*)> CanReuseBlueprintFunc,
	const TFunction<UBlueprint*(UPackage*)> GenerateBlueprintFunc) const
{
	if (!InBaseActorClass)
	{
		return nullptr;
	}

	UPackage* BPPackage = UPackageTools::FindOrCreatePackageForAssetType(FName(InBlueprintPath), UBlueprint::StaticClass());
	const FString BlueprintShortName = FPackageName::GetShortName(InBlueprintPath);

	const FString AssetPath = BPPackage->GetPathName() + TEXT(".") + FPackageName::GetShortName(BPPackage);
	UBlueprint* GeneratedBP = LoadObject<UBlueprint>(nullptr, *AssetPath, nullptr, LOAD_NoWarn);

	const bool bAssetAlreadyExisted = GeneratedBP != nullptr;

	if (GeneratedBP)
	{
		// Check BP MH version
		if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(GeneratedBP))
		{
			const FName VersionTag("MHAssetVersion");
			if (const FString* AssetMetaHumanVersionStr = Metadata->Find(VersionTag))
			{
				const UE::MetaHuman::FMetaHumanAssetVersion CurrentMetaHumanBPVersion = FMetaHumanCharacterEditorBuild::GetMetaHumanAssetVersion();
				UE::MetaHuman::FMetaHumanAssetVersion AssetMetaHumanVersion(*AssetMetaHumanVersionStr);
				if (AssetMetaHumanVersion < CurrentMetaHumanBPVersion)
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("AssetPath"), FText::FromString(AssetPath));
					Args.Add(TEXT("AssetMetaHumanVersion"), FText::FromString(*AssetMetaHumanVersionStr));
					const FText Message = FText::Format(
						LOCTEXT("AssemblyOldBPOverwriteWarning", "The assembly is about to write over a MetaHuman Actor Blueprint imported with Quixel Bridge. "
																 "Continuing may break functionality on this MetaHuman. Do you wish to continue?\n\n"
																"{AssetPath} - Version: {AssetMetaHumanVersion}"),
						Args
					);

					EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::OkCancel, Message);
					if (Result == EAppReturnType::Cancel)
					{
						return nullptr;
					}
				}
			}
		}

		if (!CanReuseBlueprintFunc(GeneratedBP))
		{
			FFormatNamedArguments FormatArguments;
			FormatArguments.Add(TEXT("TargetAssetName"), FText::FromString(BPPackage->GetPathName()));
			FormatArguments.Add(TEXT("BaseActorClass"), FText::FromString(InBaseActorClass->GetPathName()));

			const FText Message = FText::Format(
				LOCTEXT("ExistingBlueprintDifferentParentClass",
					"The generated actor blueprint can't be written to {TargetAssetName}, because the existing blueprint "
					"is not based on the actor class specified by the MetaHuman Character Pipeline, {BaseActorClass}.\n\n"
					"If you wish to overwrite the existing blueprint, delete it from the Content Browser and try again."),
				FormatArguments);

			FMessageLog(UE::MetaHuman::MessageLogName).Error(Message)
				->AddToken(FUObjectToken::Create(InBaseActorClass));

			return nullptr;
		}

		// Set the current MetaHuman version to the update BP
		FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(GeneratedBP);
	}
	else
	{
		GeneratedBP = GenerateBlueprintFunc(BPPackage);

		if (!GeneratedBP)
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("FailedToGenerateBlueprint", "Failed to generate the MetaHuman actor blueprint."));
			return nullptr;
		}

		GeneratedBP->SetFlags(RF_Public | RF_Transactional | RF_Standalone);
	}

	FMetaHumanCharacterEditorBuild::SetMetaHumanVersionMetadata(GeneratedBP);
	GeneratedBP->MarkPackageDirty();

	const FBPCompileRequest Request(GeneratedBP, EBlueprintCompileOptions::None, nullptr);
	FBlueprintCompilationManager::CompileSynchronously(Request);

	// Check if compile was successful
	if (!GeneratedBP->IsUpToDate() || !GeneratedBP->GeneratedClass)
	{
		// Warn user but continue anyway
		FMessageLog(UE::MetaHuman::MessageLogName).Warning(LOCTEXT("NewBlueprintCompileError", "Generated blueprint failed to compile"))
			->AddToken(FUObjectToken::Create(GeneratedBP));
	}

	// Notify the asset registry so that the asset appears in the Content Browser
	if (!bAssetAlreadyExisted)
	{
		FAssetRegistryModule::AssetCreated(GeneratedBP);
	}

	return GeneratedBP;
}

bool UMetaHumanDefaultEditorPipelineBase::IsPluginAsset(TNotNull<UObject*> InObject)
{
	return FPackageName::GetPackageMountPoint(InObject->GetPackage()->GetName()) == UE_PLUGIN_NAME;
}

TNotNull<USkeleton*> UMetaHumanDefaultEditorPipelineBase::GenerateSkeleton(FMetaHumanCharacterGeneratedAssets& InGeneratedAssets,
																		   TNotNull<USkeleton*> InBaseSkeleton,
																		   const FString& InTargetFolderName,
																		   TNotNull<UObject*> InOuterForGeneratedAssets) const
{
	// By default, always return the generated skeleton
	return InBaseSkeleton;
}

void UMetaHumanDefaultEditorPipelineBase::RemoveLODsIfNeeded(FMetaHumanCharacterGeneratedAssets& InGeneratedAssets, TMap<FName, UMaterialInterface*>& OutRemovedMaterialSlots) const
{
	for (const FSkeletalMaterial& Material : InGeneratedAssets.FaceMesh->GetMaterials())
	{
		OutRemovedMaterialSlots.Add(Material.MaterialSlotName, Material.MaterialInterface);
	}

	// Get the face material set before removing LODs so unused materials can be removed later
	const FMetaHumanCharacterFaceMaterialSet OldFaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(InGeneratedAssets.FaceMesh);

	bool bFaceLODsModified = false;
	bool bBodyLODsModified = false;

	const int32 NumFaceLODs = InGeneratedAssets.FaceMesh->GetLODNum();

	// Configure the LODs of the exported character
	if (!LODProperties.FaceLODs.IsEmpty() && (LODProperties.FaceLODs.Num() < NumFaceLODs))
	{
		FMetaHumanCharacterEditorBuild::StripLODsFromMesh(InGeneratedAssets.FaceMesh, LODProperties.FaceLODs);
		bFaceLODsModified = true;
	}

	if (!LODProperties.BodyLODs.IsEmpty() && (LODProperties.BodyLODs.Num() < InGeneratedAssets.BodyMesh->GetLODNum()))
	{
		FMetaHumanCharacterEditorBuild::StripLODsFromMesh(InGeneratedAssets.BodyMesh, LODProperties.BodyLODs);
		bBodyLODsModified = true;
	}

	if (LODProperties.bOverrideFaceLODSettings)
	{
		InGeneratedAssets.FaceMesh->SetLODSettings(LODProperties.FaceLODSettings.LoadSynchronous());
		bFaceLODsModified = true;
	}

	if (LODProperties.bOverrideBodyLODSettings)
	{
		InGeneratedAssets.BodyMesh->SetLODSettings(LODProperties.BodyLODSettings.LoadSynchronous());
		bBodyLODsModified = true;
	}

	// Call PostEditChange to build the skeletal mesh in case LODs were modified
	if (bFaceLODsModified)
	{
		InGeneratedAssets.FaceMesh->PostEditChange();
	}

	if (bBodyLODsModified)
	{
		InGeneratedAssets.BodyMesh->PostEditChange();
	}

	// Remove any slots that are still on the mesh from the "removed" list.
	//
	// Any slots remaining on the list must have been removed.
	for (const FSkeletalMaterial& Material : InGeneratedAssets.FaceMesh->GetMaterials())
	{
		OutRemovedMaterialSlots.Remove(Material.MaterialSlotName);
	}

	// Get the new face material set from the face mesh after removing LODs and unused materials
	const FMetaHumanCharacterFaceMaterialSet NewFaceMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadMaterialsFromMesh(InGeneratedAssets.FaceMesh);

	if (bFaceLODsModified)
	{
		// Remove all unused materials from the list of generated assets if they are no longer used by the face mesh
		OldFaceMaterialSet.ForEachSkinMaterial<UMaterialInstance>(
			[&NewFaceMaterialSet, &InGeneratedAssets](EMetaHumanCharacterSkinMaterialSlot SkinMaterialSlot, UMaterialInstance* OldMaterialInstance)
			{
				if (!NewFaceMaterialSet.Skin.Contains(SkinMaterialSlot))
				{
					InGeneratedAssets.RemoveAssetMetadata(OldMaterialInstance);
				}
			}
		);

		if (NewFaceMaterialSet.EyeLeft == nullptr && OldFaceMaterialSet.EyeLeft != nullptr)
		{
			InGeneratedAssets.RemoveAssetMetadata(OldFaceMaterialSet.EyeLeft);
		}

		if (NewFaceMaterialSet.EyeRight == nullptr && OldFaceMaterialSet.EyeRight != nullptr)
		{
			InGeneratedAssets.RemoveAssetMetadata(OldFaceMaterialSet.EyeRight);
		}

		if (NewFaceMaterialSet.Eyelashes == nullptr && OldFaceMaterialSet.Eyelashes != nullptr)
		{
			InGeneratedAssets.RemoveAssetMetadata(OldFaceMaterialSet.Eyelashes);
		}

		if (NewFaceMaterialSet.EyelashesHiLods == nullptr && OldFaceMaterialSet.EyelashesHiLods != nullptr)
		{
			InGeneratedAssets.RemoveAssetMetadata(OldFaceMaterialSet.EyelashesHiLods);
		}
	}

	// Remove Textures that are not used based on features enabled in the remaining materials
	auto IsStaticSwitchEnabled = [](const TArray<FSkeletalMaterial>& Materials, FName ParamName)
	{
		return Algo::AnyOf(Materials,
						   [ParamName](const FSkeletalMaterial& Material)
						   {
							   bool bIsEnabled = false;

							   if (UMaterialInterface* MaterialInterface = Material.MaterialInterface)
							   {
								   FGuid Guid;
								   const bool bOverridenOnly = false;
								   Material.MaterialInterface->GetStaticSwitchParameterValue(ParamName, bIsEnabled, Guid, bOverridenOnly);
							   }

							   return bIsEnabled;
						   });
	};

	if (!IsStaticSwitchEnabled(InGeneratedAssets.FaceMesh->GetMaterials(), FMetaHumanCharacterSkinMaterials::UseAnimatedMapsParamName))
	{
		// Remove the animated maps if they are not being used by any of the face materials

		for (EFaceTextureType AnimatedMap : UE::MetaHuman::Private::GetAnimatedMapTypes())
		{
			UTexture2D* AnimatedMapToRemove = InGeneratedAssets.SynthesizedFaceTextures[AnimatedMap];
			InGeneratedAssets.RemoveAssetMetadata(AnimatedMapToRemove);
			InGeneratedAssets.SynthesizedFaceTextures.Remove(AnimatedMap);
			AnimatedMapToRemove->MarkAsGarbage();
		}
	}

	// Remove the cavity map if not being used by any of the face materials
	if (!IsStaticSwitchEnabled(InGeneratedAssets.FaceMesh->GetMaterials(), FMetaHumanCharacterSkinMaterials::UseCavityParamName))
	{
		if (const TObjectPtr<UTexture2D>* FoundCavityMap = InGeneratedAssets.SynthesizedFaceTextures.Find(EFaceTextureType::Cavity))
		{
			UTexture2D* CavityMap = *FoundCavityMap;
			InGeneratedAssets.RemoveAssetMetadata(CavityMap);
			InGeneratedAssets.SynthesizedFaceTextures.Remove(EFaceTextureType::Cavity);
			CavityMap->MarkAsGarbage();
		}
	}
}

#undef LOCTEXT_NAMESPACE