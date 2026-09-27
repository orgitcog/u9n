// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterAnalytics.h"

#include "Editor/EditorEngine.h"
#include "Algo/Find.h"
#include "Algo/Transform.h"
#include "Engine/World.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorActor.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterTextureSynthesis.h"
#include "MetaHumanCharacterBodyTextureUtils.h"
#include "MetaHumanCharacterSkelMeshUtils.h"
#include "MetaHumanRigEvaluatedState.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterPaletteUnpackHelpers.h"
#include "MetaHumanCharacterThumbnailRenderer.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanWardrobeItem.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanInvisibleDrivingActor.h"
#include "MetaHumanCharacterEditorWardrobeSettings.h"
#include "MetaHumanCharacterPalette.h"
#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanDefaultPipelineBase.h"

#include "Subsystem/MetaHumanCharacterBuild.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/AssetManager.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture.h"
#include "Components/SkeletalMeshComponent.h"
#include "GroomComponent.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "MeshDescription.h"
#include "SkelMeshDNAUtils.h"
#include "DNAUtils.h"
#include "Cloud/MetaHumanTextureSynthesisServiceRequest.h"
#include "Cloud/MetaHumanARServiceRequest.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/ScopedSlowTask.h"
#include "ImageCoreUtils.h"
#include "AssetToolsModule.h"
#include "ObjectTools.h"
#include "InterchangeDnaModule.h"
#include "Logging/StructuredLog.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "StaticMeshAttributes.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Misc/ObjectThumbnail.h"
#include "UObject/ObjectSaveContext.h"
#include "RenderingThread.h"
#include "DNAUtilities.h"
#include "Tasks/Task.h"
#include "ChaosOutfitAsset/BodyUserData.h"
#include "ScopedTransaction.h"
#include "TextureResource.h"
#include "ImageUtils.h"
#include "Animation/MorphTarget.h"
#include "DNAReaderAdapter.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "AnimationRuntime.h"

extern UNREALED_API UEditorEngine* GEditor;

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

static const TCHAR* AutoriggingTransactionContext = TEXT("AutoriggingTransaction");


namespace UE::MetaHuman
{
	static TAutoConsoleVariable<bool> CVarMHCharacterSaveAutoRiggedDNA
	{
		TEXT("mh.Character.SaveAutoRiggedDNA"),
		false,
		TEXT("Set to true to save the DNA file returned by the Auto-Rigging service to a file in the Saved directory."),
		ECVF_Default
	};

	static TAutoConsoleVariable<bool> CvarUpdateAllLODsOnFaceEdit
	{
		TEXT("mh.Character.UpdateAllLODsOnFaceEdit"),
		false,
		TEXT("Set to true to update all LODs on skeletal mesh during face edit. Otherwise only LOD0 is updated."),
		ECVF_Default
	};

	static TAutoConsoleVariable<bool> CvarMHLoadMeshesFromDNA
	{
		TEXT("mh.Character.LoadFaceFromDNA"),
		false,
		TEXT("If enabled, Skeletal Meshes will be created from the DNA files when opening MHC asset editor."),
		ECVF_Default
	};

	static TAutoConsoleVariable<bool> CVarMHCharacterPreviewHiddenFaces
	{
		TEXT("mh.Character.PreviewHiddenFaces"),
		true,
		TEXT("If enabled, hidden faces will be applied to the editor preview."),
		ECVF_Default
	};

	static TSharedPtr<IDNAReader> GetDNAReader(USkeletalMesh* InSkeletalMesh)
	{
		UAssetUserData* UserData = InSkeletalMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass());
		if (!UserData)
		{
			return nullptr;
		}

		UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);
		return DNAAsset->GetGeometryReader();
	}

	static TSharedPtr<SNotificationItem> ShowNotification(const FText& InMessage, SNotificationItem::ECompletionState InState, float InExpireDuration = 3.5f)
	{
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
			Info.ExpireDuration = InExpireDuration;
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

	static int32 MapTextureHFToStateHFIndex(const FMetaHumanCharacterIdentity::FState& InFaceState, int32 InTextureHFIndex)
	{
		// Ensure index does not exceed variant count
		if (InTextureHFIndex >= InFaceState.GetNumHighFrequencyVariants())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Number of character Identity variants %i less than input texture index %i, max variant will be used"), InFaceState.GetNumHighFrequencyVariants(), InTextureHFIndex);
			return InFaceState.GetNumHighFrequencyVariants() - 1;
		}

		// Assume all variants are in the same order
		return InTextureHFIndex;
	}

	static int32 GetEyelashesVariantIndex(const FMetaHumanCharacterIdentity::FState& InFaceState, EMetaHumanCharacterEyelashesType InEyelashesType)
	{
		int32 VariantIndex = INDEX_NONE;
		// Check if eyelashes count is the same as the number of types in the enum, not counting "None".
		if (InFaceState.GetVariantsCount("eyelashes") == static_cast<int32>(EMetaHumanCharacterEyelashesType::Count) - 1 && InEyelashesType != EMetaHumanCharacterEyelashesType::Count)
		{
			VariantIndex = static_cast<int32>(InEyelashesType) - 1; // -1 when None is selected.
		}
		else
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Number of character identity eyelashes variants does not match input eyelahses Type, None will be used"));
		}
		return VariantIndex;
	}

	/** Makes a map of all Joints from DNA to Bones in Reference Skeleton. */
	static void MapJoints(const USkeletalMesh* InTargetSkelMesh, const dna::Reader* InDNAReader, TArray<int32>& OutRLJointToUEBoneIndices)
	{
		const FReferenceSkeleton* RefSkeleton = &InTargetSkelMesh->GetRefSkeleton();
		uint32 JointCount = InDNAReader->getJointCount();

		// Map Joints to Bones.
		OutRLJointToUEBoneIndices.Empty(JointCount);
		for (uint32 JntIndex = 0; JntIndex < JointCount; ++JntIndex)
		{
			const FString BoneNameFStr = InDNAReader->getJointName(JntIndex).c_str();
			const FName BoneName = FName{ *BoneNameFStr };
			const int32 BoneIndex = RefSkeleton->FindBoneIndex(BoneName);

			// BoneIndex can be INDEX_NONE;
			// We can safely put it into the map with other indices, it will be handled in the Evaluate method.
			OutRLJointToUEBoneIndices.Add(BoneIndex);
		}
	}

	// True if TryAddObjectToEdit is running
	// TODO: Make this a member of the subsystem
	static bool bIsAddingObjectToEdit = false;
}

FMetaHumanCharacterEditorData::FMetaHumanCharacterEditorData()
	: FaceDnaToSkelMeshMap(MakeShared<FDNAToSkelMeshMap>())
	, BodyDnaToSkelMeshMap(MakeShared<FDNAToSkelMeshMap>())
	, FaceState(MakeShared<FMetaHumanCharacterIdentity::FState>())
	, BodyState(MakeShared<FMetaHumanCharacterBodyIdentity::FState>())
{
}

FRemoveFaceRigCommandChange::FRemoveFaceRigCommandChange(
	const TArray<uint8>& InOldDNABuffer,
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
	TNotNull<UMetaHumanCharacter*> InCharacter)
	: OldDNABuffer{ InOldDNABuffer }
	, NewDNABuffer{ InCharacter->GetFaceDNABuffer() }
	, OldState{ InOldState }
	, NewState{ UMetaHumanCharacterEditorSubsystem::Get()->CopyFaceState(InCharacter) }
{}


void FRemoveFaceRigCommandChange::ApplyChange(UObject* InObject, const TArray<uint8>& InDNABuffer, TSharedRef<const FMetaHumanCharacterIdentity::FState> InState)
{
	UMetaHumanCharacter* Character = CastChecked<UMetaHumanCharacter>(InObject);

	// if an empty buffer, remove the rig from the character (special case)
	if (InDNABuffer.IsEmpty())
	{
		UMetaHumanCharacterEditorSubsystem::Get()->RemoveFaceRig(Character);
	}
	else
	{
		TArray<uint8> BufferCopy;
		BufferCopy.SetNumUninitialized(InDNABuffer.Num());
		FMemory::Memcpy(BufferCopy.GetData(), InDNABuffer.GetData(), InDNABuffer.Num());
		UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceDNA(Character, ReadDNAFromBuffer(&BufferCopy, EDNADataLayer::All).ToSharedRef());
	}

	// reset the face state
	UMetaHumanCharacterEditorSubsystem::Get()->CommitFaceState(Character, InState);
}

FAutoRigCommandChange::FAutoRigCommandChange(
	const TArray<uint8>& InOldDNABuffer,
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InOldState,
	TNotNull<UMetaHumanCharacter*> InCharacter)
	: FRemoveFaceRigCommandChange(InOldDNABuffer, InOldState, InCharacter)
{
}

void UMetaHumanCharacterEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	FCoreUObjectDelegates::OnObjectPreSave.AddWeakLambda(this, [this](UObject* InObject, FObjectPreSaveContext InObjectPreSaveContext)
	{
		if (UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(InObject))
		{
			SaveCharacterThumbnails(Character);
		}
	});
}

void UMetaHumanCharacterEditorSubsystem::SaveCharacterThumbnails(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	if (!CharacterDataMap.Contains(InCharacter))
	{
		return;
	}

	UMetaHumanCharacterThumbnailRenderer* ThumbnailRenderer = nullptr;

	if (FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(InCharacter))
	{
		ThumbnailRenderer = Cast<UMetaHumanCharacterThumbnailRenderer>(RenderInfo->Renderer);
	}

	if (!ThumbnailRenderer)
	{
		return;
	}

	const TArray<EMetaHumanCharacterThumbnailCameraPosition> ThumbnailPositionsToUpdate =
	{
		EMetaHumanCharacterThumbnailCameraPosition::Face,
		EMetaHumanCharacterThumbnailCameraPosition::Body,
		EMetaHumanCharacterThumbnailCameraPosition::Character_Face,
		EMetaHumanCharacterThumbnailCameraPosition::Character_Body,
	};

	UPackage* CharacterPackage = InCharacter->GetPackage();
	FThumbnailMap ThumbnailMap;
	TArray<FName> ObjectNames;
	Algo::Transform(
		ThumbnailPositionsToUpdate,
		ObjectNames,
		[ObjectPath = InCharacter->GetPathName()](EMetaHumanCharacterThumbnailCameraPosition Position)
		{
			return UMetaHumanCharacter::GetThumbnailPathInPackage(ObjectPath, Position);
		});

	for (int32 i = 0; i < ThumbnailPositionsToUpdate.Num(); ++i)
	{
		// Empty thumbnail object to write to
		FObjectThumbnail ThumbnailObject;

		ThumbnailRenderer->CameraPosition = ThumbnailPositionsToUpdate[i];

		ThumbnailTools::RenderThumbnail(
			InCharacter,
			ThumbnailTools::DefaultThumbnailSize,
			ThumbnailTools::DefaultThumbnailSize,
			ThumbnailTools::EThumbnailTextureFlushMode::NeverFlush,
			nullptr,
			&ThumbnailObject
		);

		// Save thumbnail to the package
		ThumbnailTools::CacheThumbnail(
			ObjectNames[i].ToString(),
			&ThumbnailObject,
			CharacterPackage);
	}

	// Thumbnail rendering enqueues a rendering command, wait until it's complete
	FlushRenderingCommands();
}

void UMetaHumanCharacterEditorSubsystem::RunCharacterEditorPipelineForPreview(TNotNull<UMetaHumanCharacter*> InCharacter) const
{
	if (!ensure(InCharacter->GetInternalCollection()))
	{
		// This should never be null, but avoid crashing if it is
		return;
	}

	if (!InCharacter->GetInternalCollection()->GetEditorPipeline())
	{
		return;
	}
	
	FInstancedStruct BuildInput;
	{
		const TObjectPtr<UScriptStruct> BuildInputStruct = InCharacter->GetInternalCollection()->GetEditorPipeline()->GetSpecification()->BuildInputStruct;
		if (BuildInputStruct
			&& BuildInputStruct->IsChildOf(FMetaHumanBuildInputBase::StaticStruct()))
		{
			// Initialize to the struct that the pipeline is expecting.
			//
			// Any properties defined in sub-structs of FMetaHumanBuildInputBase will be left as
			// their default values.
			BuildInput.InitializeAs(BuildInputStruct);

			FMetaHumanBuildInputBase& TypedBuildInput = BuildInput.GetMutable<FMetaHumanBuildInputBase>();
			TypedBuildInput.EditorPreviewCharacter = InCharacter->GetInternalCollectionKey();
		}
	}

	InCharacter->GetMutableInternalCollection()->Build(
		BuildInput,
		EMetaHumanCharacterPaletteBuildQuality::Preview,
		GetTargetPlatformManagerRef().GetRunningTargetPlatform(),
		UMetaHumanCollection::FOnBuildComplete(),
		InCharacter->GetInternalCollection()->GetDefaultInstance()->ToPinnedSlotSelections(EMetaHumanUnusedSlotBehavior::PinnedToEmpty));
}

const TSharedRef<FMetaHumanCharacterEditorData>* UMetaHumanCharacterEditorSubsystem::GetMetaHumanCharacterEditorData(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	return CharacterDataMap.Find(InCharacter);
}

bool UMetaHumanCharacterEditorSubsystem::IsTickable() const
{
	// Only tick if we are waiting on texture data from being loaded
	for (const TPair<TObjectKey<UMetaHumanCharacter>, TSharedRef<FMetaHumanCharacterEditorData>>& CharacterDataPair : CharacterDataMap)
	{
		TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataPair.Value;

		if (!CharacterData->SynthesizedFaceTexturesFutures.IsEmpty() || !CharacterData->HighResBodyTexturesFutures.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

void UMetaHumanCharacterEditorSubsystem::Tick(float InDeltaTime)
{
	for (TPair<TObjectKey<UMetaHumanCharacter>, TSharedRef<FMetaHumanCharacterEditorData>>& CharacterDataPair : CharacterDataMap)
	{
		TObjectKey<UMetaHumanCharacter> CharacterKey = CharacterDataPair.Key;
		TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataPair.Value;
		
		UMetaHumanCharacter* Character = CharacterKey.ResolveObjectPtr();
		if (!Character)
		{
			continue;
		}

		if (!CharacterData->SynthesizedFaceTexturesFutures.IsEmpty())
		{
			UpdatePendingSynthesizedTextures(Character, CharacterData, Character->SynthesizedFaceTextures);
		}

		if (!CharacterData->HighResBodyTexturesFutures.IsEmpty())
		{
			UpdatePendingHighResBodyTextures(Character, CharacterData, Character->BodyTextures);
		}
	}
}

void UMetaHumanCharacterEditorSubsystem::UpdatePendingSynthesizedTextures(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures)
{
	TArray<EFaceTextureType> ReadyTextures;

	for (TPair<EFaceTextureType, TSharedFuture<FSharedBuffer>>& FaceTextureFuturePair : InCharacterData->SynthesizedFaceTexturesFutures)
	{
		EFaceTextureType TextureType = FaceTextureFuturePair.Key;
		TSharedFuture<FSharedBuffer> PayloadFuture = FaceTextureFuturePair.Value;

		if (PayloadFuture.IsReady())
		{
			ReadyTextures.Add(TextureType);

			const FSharedBuffer& PayloadData = PayloadFuture.Get();

			if (InCharacter->HasHighResolutionTextures())
			{
				if (const FMetaHumanCharacterTextureInfo* TextureInfo = InCharacter->SynthesizedFaceTexturesInfo.Find(TextureType))
				{
					OutSynthesizedFaceTextures.FindOrAdd(TextureType) = 
						FMetaHumanCharacterTextureSynthesis::CreateFaceTextureFromSource(TextureType, FImageView(
							TextureInfo->ToImageInfo(), const_cast<void*>(PayloadData.GetData())));
				}
			}
			else
			{
				// Update the cached image for the texture type to keep it consistent with the data that was stored in the character.
				// This prevents uninitialized texture data from being stored in the character when applying the skin settings
				if (FImage* CachedSynthesizedImage = InCharacterData->CachedSynthesizedImages.Find(TextureType))
				{
					FMemory::Memcpy(CachedSynthesizedImage->RawData.GetData(), PayloadData.GetData(), PayloadData.GetSize());
				}

				check(OutSynthesizedFaceTextures.Contains(TextureType));
				FMetaHumanCharacterTextureSynthesis::UpdateTexture(MakeConstArrayView((const uint8*) PayloadData.GetData(), PayloadData.GetSize() / sizeof(uint8)),
																	   OutSynthesizedFaceTextures[TextureType]);
			}

		}
	}

	for (EFaceTextureType ReadyTexture : ReadyTextures)
	{
		InCharacterData->SynthesizedFaceTexturesFutures.Remove(ReadyTexture);
	}

	// Textures were updated so call PostEditChange to refresh the material
	InCharacterData->HeadMaterials.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* SkinMaterial)
		{
			SkinMaterial->PostEditChange();
		}
	);
}

void UMetaHumanCharacterEditorSubsystem::UpdatePendingHighResBodyTextures(
		TNotNull<const UMetaHumanCharacter*> InCharacter,
		TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
		TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& OutBodyTextures)
{
	TArray<EBodyTextureType> ReadyTextures;

	for (TPair<EBodyTextureType, TSharedFuture<FSharedBuffer>>& BodyTextureFuturePair : InCharacterData->HighResBodyTexturesFutures)
	{
		EBodyTextureType TextureType = BodyTextureFuturePair.Key;
		TSharedFuture<FSharedBuffer> PayloadFuture = BodyTextureFuturePair.Value;

		if (PayloadFuture.IsReady())
		{
			ReadyTextures.Add(TextureType);

			const FSharedBuffer& PayloadData = PayloadFuture.Get();
			if (!PayloadData.IsNull())
			{
				// Local textures are initialized in FMetaHumanCharacterBodyTextureUtils::InitBodyTextureData(), only high rez should be loaded here

				// Body textures can "by pass" the HasHighResolutionTextures() check and store both downloaded and local textures
				if (const FMetaHumanCharacterTextureInfo* TextureInfo = InCharacter->HighResBodyTexturesInfo.Find(TextureType))
				{
					OutBodyTextures.FindOrAdd(TextureType) =
						FMetaHumanCharacterBodyTextureUtils::CreateBodyTextureFromSource(TextureType, FImageView(
							TextureInfo->ToImageInfo(), const_cast<void*>(PayloadData.GetData())));
				}
			}
		}
	}

	for (EBodyTextureType ReadyTexture : ReadyTextures)
	{
		InCharacterData->HighResBodyTexturesFutures.Remove(ReadyTexture);
	}

	// Textures were updated so call PostEditChange to refresh the material
	InCharacterData->BodyMaterial->PostEditChange();
}

void UMetaHumanCharacterEditorSubsystem::WaitForSynthesizedTextures(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures,
	TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& OutBodyTextures)
{
	for (TPair<EFaceTextureType, TSharedFuture<FSharedBuffer>>& FaceTextureFuturePair : InCharacterData->SynthesizedFaceTexturesFutures)
	{
		TSharedFuture<FSharedBuffer> PayloadFuture = FaceTextureFuturePair.Value;

		// No timeout needed here, as the information is coming from disk and should
		// complete within a reasonable amount of time.
		PayloadFuture.Wait();
	}

	UpdatePendingSynthesizedTextures(InCharacter, InCharacterData, OutSynthesizedFaceTextures);

	for (TPair<EBodyTextureType, TSharedFuture<FSharedBuffer>>& BodyTextureFuturePair : InCharacterData->HighResBodyTexturesFutures)
	{
		TSharedFuture<FSharedBuffer> PayloadFuture = BodyTextureFuturePair.Value;

		// No timeout needed here, as the information is coming from disk and should
		// complete within a reasonable amount of time.
		PayloadFuture.Wait();
	}

	UpdatePendingHighResBodyTextures(InCharacter, InCharacterData, OutBodyTextures);
}

TStatId UMetaHumanCharacterEditorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UMetaHumanCharacterEditorSubsystem, STATGROUP_Tickables);
}

UMetaHumanCharacterEditorSubsystem* UMetaHumanCharacterEditorSubsystem::Get()
{
	check(GEditor);
	return GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();
}

bool UMetaHumanCharacterEditorSubsystem::TryAddObjectToEdit(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	// TryAddObjectToEdit should only be called once for a character if it succeeds, until RemoveObjectToEdit is called
	check(!CharacterDataMap.Contains(InCharacter));

	TGuardValue<bool> TryAddObjectGuard{ UE::MetaHuman::bIsAddingObjectToEdit, true };

	// Start loading the texture synthesis data in an async task
	UE::Tasks::FTask FaceTextureSynthesizerLoadTask = {};
	if (!FaceTextureSynthesizer.IsValid())
	{
		FaceTextureSynthesizerLoadTask = UE::Tasks::Launch(UE_SOURCE_LOCATION, [this]
			{
				FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer(FaceTextureSynthesizer);
			});
	}

	// When loading texture sources, we need to ensure everything is loaded since the texture objects of the preview actor 
	// are populated at load time only and CachedSynthesizedImages are not used
	const bool bBlockUntilComplete = InCharacter->HasHighResolutionTextures();

	const FEditorDataForCharacterCreationParams EditorDataForCharacterCreationParams =
	{
		.bBlockUntilComplete = bBlockUntilComplete,
		.bCreateMeshFromDNA = false,
		.OuterForGeneratedAssets = this,
		.PreviewMaterial = InCharacter->PreviewMaterialType,
	};
	TSharedPtr<FMetaHumanCharacterEditorData> CharacterData = CreateEditorDataForCharacter(InCharacter,
																						   EditorDataForCharacterCreationParams,
																						   InCharacter->SynthesizedFaceTextures,
																						   InCharacter->BodyTextures,
																						   FaceTextureSynthesizerLoadTask);

	if (CharacterData.IsValid())
	{
		CharacterDataMap.Add(InCharacter, CharacterData.ToSharedRef());

		OnBodyStateChanged(InCharacter).AddWeakLambda(this, [this, InCharacter]
		{
			UpdateCharacterIsFixedBodyType(InCharacter);
		});
	}
	else
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to create editing state for %s"), *InCharacter->GetFullName());
	}

	if (!InCharacter->HasSynthesizedTextures())
	{
		StoreSynthesizedTextures(InCharacter);
		ApplySkinSettings(InCharacter, InCharacter->SkinSettings);
	}

	if (InCharacter->HasHighResolutionTextures())
	{
		// High rez textures may have been loaded late so re-apply here
		ApplySkinSettings(InCharacter, InCharacter->SkinSettings);

		// Remove any texture object sources since these are preview data and will not get cooked
		for (const TPair<EFaceTextureType, TObjectPtr<UTexture2D>>& Pair : InCharacter->SynthesizedFaceTextures)
		{
			if (Pair.Value && Pair.Value->Source.IsValid())
			{
				Pair.Value->Source.Reset();
			}
		}

		for (const TPair<EBodyTextureType, TObjectPtr<UTexture2D>>& Pair : InCharacter->BodyTextures)
		{
			if (Pair.Value && Pair.Value->Source.IsValid())
			{
				Pair.Value->Source.Reset();
			}
		}
	}

	return CharacterData.IsValid();
}

bool UMetaHumanCharacterEditorSubsystem::IsObjectAddedForEditing(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	return CharacterDataMap.Contains(InCharacter);
}

void UMetaHumanCharacterEditorSubsystem::GetFaceAndBodySkeletalMeshes(TNotNull<const UMetaHumanCharacter*> InCharacter,
	const FEditorDataForCharacterCreationParams& InParams, USkeletalMesh*& OutFaceMesh, USkeletalMesh*& OutBodyMesh)
{
	const FName FaceMeshName = MakeUniqueObjectName(InParams.OuterForGeneratedAssets, USkeletalMesh::StaticClass(), TEXT("FaceMesh"), EUniqueObjectNameOptions::GloballyUnique);
	const FName BodyMeshName = MakeUniqueObjectName(InParams.OuterForGeneratedAssets, USkeletalMesh::StaticClass(), TEXT("BodyMesh"), EUniqueObjectNameOptions::GloballyUnique);
	bool bFaceMeshCreatedFromDNA = false;
	
	// Re-create Face mesh from DNA if blendshapes are present
	if (InCharacter->HasFaceDNA())
	{
		TArray<uint8> FaceDNABuffer = InCharacter->GetFaceDNABuffer();
		TSharedPtr<IDNAReader> FaceDNAReader = ReadDNAFromBuffer(&FaceDNABuffer);

		if(FaceDNAReader.IsValid() && FaceDNAReader->GetBlendShapeChannelCount() > 0)
		{
			FString FullPackagePath = InParams.OuterForGeneratedAssets->GetPackage()->GetName();			
			OutFaceMesh = FMetaHumanCharacterSkelMeshUtils::GetSkeletalMeshAssetFromDNA(FaceDNAReader, FullPackagePath, FaceMeshName.ToString(), EMetaHumanImportDNAType::Face);
			FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(OutFaceMesh, FaceDNAReader, true /*bIsFace*/);
			bFaceMeshCreatedFromDNA = true;
		}
	}

	//TODO: Optimize the loading from DNA. For now divert editor loading to use skelmesh assets
	if (UE::MetaHuman::CvarMHLoadMeshesFromDNA.GetValueOnAnyThread() || InParams.bCreateMeshFromDNA)
	{
		if (!bFaceMeshCreatedFromDNA)
		{
			OutFaceMesh = GetFaceArchetypeMesh(InCharacter->TemplateType);
		}
		
		OutBodyMesh = GetBodyArchetypeMesh(InCharacter->TemplateType);
	}
	else
	{
		if (!bFaceMeshCreatedFromDNA)
		{
			USkeletalMesh* FaceArchetypeMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Script/Engine.SkeletalMesh'/" UE_PLUGIN_NAME "/Face/SKM_Face.SKM_Face'"));
			OutFaceMesh = DuplicateObject(FaceArchetypeMesh, InParams.OuterForGeneratedAssets, FaceMeshName);
		}

		USkeletalMesh* BodyArchetypeMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Script/Engine.SkeletalMesh'/" UE_PLUGIN_NAME "/Body/IdentityTemplate/SKM_Body.SKM_Body'"));
		OutBodyMesh = DuplicateObject(BodyArchetypeMesh, InParams.OuterForGeneratedAssets, BodyMeshName);
	}

	if (bFaceMeshCreatedFromDNA)
	{
		UE::MetaHuman::Analytics::RecordCreateMeshFromDNAEvent(InCharacter);
	}

	check(OutFaceMesh && OutBodyMesh);
}

bool UMetaHumanCharacterEditorSubsystem::InitializeIdentityStateForFaceAndBody(TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedPtr<FMetaHumanCharacterIdentity::FState>& OutFaceState, TSharedPtr<FMetaHumanCharacterBodyIdentity::FState>& OutBodyState)
{
	// Initialize the states for face and body and from the values stored in the character
	const FMetaHumanCharacterIdentityModels& IdentityModels = GetOrCreateCharacterIdentity(InCharacter->TemplateType);

	OutFaceState = IdentityModels.Face->CreateState();
	OutBodyState = IdentityModels.Body->CreateState();
	
	if (!OutFaceState.IsValid())
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "{Name}: Identity model failed to create face state", *InCharacter->GetFullName());
		return false;
	}

	if (!OutBodyState.IsValid())
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "{Name}: Identity model failed to create body state", *InCharacter->GetFullName());
		return false;
	}

	// Set body delta evaluation to true
	FMetaHumanCharacterIdentity::FSettings FaceStateSettings = OutFaceState->GetSettings();
	FaceStateSettings.SetBodyDeltaInEvaluation(true);
	FaceStateSettings.SetGlobalVertexDeltaScale(InCharacter->FaceEvaluationSettings.GlobalDelta);
	FaceStateSettings.SetGlobalHighFrequencyScale(InCharacter->FaceEvaluationSettings.HighFrequencyDelta);
	OutFaceState->SetSettings(FaceStateSettings);

	// Initialize the face state
	const FSharedBuffer FaceStateData = InCharacter->GetFaceStateData();
	if (!OutFaceState->Deserialize(FaceStateData))
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "{Name}: Failed to deserialize face state stored in Character asset", *InCharacter->GetFullName());
		return false;
	}

	// Set the texture index for the face state
	OutFaceState->SetHighFrequenctVariant(InCharacter->SkinSettings.Skin.FaceTextureIndex);
	OutFaceState->SetFaceScale(InCharacter->FaceEvaluationSettings.HeadScale);

	// Apply body state
	const FSharedBuffer BodyStateData = InCharacter->GetBodyStateData();
	if (!OutBodyState->Deserialize(BodyStateData))
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "{Name}: Failed to deserialize body state stored in Character asset", *InCharacter->GetFullName());
		return false;
	}
	
	return true;
}

TSharedPtr<FMetaHumanCharacterEditorData> UMetaHumanCharacterEditorSubsystem::CreateEditorDataForCharacter(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	const FEditorDataForCharacterCreationParams& InParams,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures,
	TMap<EBodyTextureType, TObjectPtr<class UTexture2D>>& OutBodyTextures,
	UE::Tasks::FTask InFaceTextureSynthesizerLoadTask)
{
	USkeletalMesh* FaceMesh = nullptr;
	USkeletalMesh* BodyMesh = nullptr;
	GetFaceAndBodySkeletalMeshes(InCharacter, InParams, FaceMesh, BodyMesh);

	if (!FaceMesh || !BodyMesh)
	{
		return nullptr;
	}

	TSharedRef<FDNAToSkelMeshMap> FaceDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(FaceMesh));
	const TSharedRef<FDNAToSkelMeshMap> BodyDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(BodyMesh));

	if (InCharacter->HasFaceDNA())
	{
		TArray<uint8> FaceDNABuffer = InCharacter->GetFaceDNABuffer();
		TSharedPtr<IDNAReader> FaceDNAReader = ReadDNAFromBuffer(&FaceDNABuffer);
		
		// Update Face Mesh from DNA for joint only AR
		if (FaceDNAReader.IsValid() && FaceDNAReader->GetBlendShapeChannelCount() == 0)
		{
			FMetaHumanCharacterSkelMeshUtils::UpdateSkelMeshFromDNA(FaceDNAReader.ToSharedRef(),
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::All & ~FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::BaseMesh,
				FaceDnaToSkelMeshMap, EMetaHumanCharacterOrientation::Y_UP, FaceMesh);
		}
	}
	
	TSharedPtr<FMetaHumanCharacterIdentity::FState> FaceState;
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState;
	if (!InitializeIdentityStateForFaceAndBody(InCharacter, FaceState, BodyState))
	{
		return nullptr;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = MakeShared<FMetaHumanCharacterEditorData>(FaceMesh, BodyMesh,	FaceDnaToSkelMeshMap,
		BodyDnaToSkelMeshMap, FaceState.ToSharedRef(), BodyState.ToSharedRef());
	
	// This also updates the face state and applies it
	ApplyBodyState(CharacterData, CharacterData->BodyState, EBodyMeshUpdateMode::Full);

	// Currently we need to update the vertex normals from the state, as they are not stored in the DNA
	// TODO revisit the code below if/when we decide to set VertexNormals in the DNA
	const FMetaHumanRigEvaluatedState BodyVerticesAndVertexNormals = CharacterData->BodyState->GetVerticesAndVertexNormals();
	FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(BodyMesh, BodyVerticesAndVertexNormals,
		*CharacterData->BodyState, *CharacterData->BodyDnaToSkelMeshMap, ELodUpdateOption::All, EVertexPositionsAndNormals::NormalsOnly);
	USkelMeshDNAUtils::RebuildRenderData(BodyMesh);
	FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModel(BodyMesh);

	// Currently we need to update the vertex normals from the state, as they are not stored in the DNA
	// TODO revisit the code below if/when we decide to set VertexNormals in the DNA
	CharacterData->FaceState->SetBodyVertexNormals(BodyVerticesAndVertexNormals.VertexNormals, CharacterData->BodyState->GetNumVerticesPerLOD());
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(FaceMesh, FaceVerticesAndVertexNormals,
		*CharacterData->FaceState, *CharacterData->FaceDnaToSkelMeshMap, ELodUpdateOption::All, EVertexPositionsAndNormals::NormalsOnly);

	// Make sure FaceTextureSynthesizer has been initialized before setting up the face textures
	InFaceTextureSynthesizerLoadTask.Wait();
	if (!InCharacter->HasHighResolutionTextures())
	{
		// Only need to initialize data for texture synthesis when the asset does not contain downloaded textures
		FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData(FaceTextureSynthesizer,
			InCharacter->SynthesizedFaceTexturesInfo,
			OutSynthesizedFaceTextures,
			CharacterData->CachedSynthesizedImages);
	}
	FMetaHumanCharacterBodyTextureUtils::InitBodyTextureData(InCharacter->SkinSettings.Skin, InCharacter->HighResBodyTexturesInfo, OutBodyTextures);

	if (InCharacter->HasSynthesizedTextures())
	{
		// If we have synthesized textures, make an async request to load the data.
		//
		// The textures currently on the CharacterData will have their image data overwritten by
		// the async task, but they won't be replaced with different textures, so it's safe to take
		// references to them before the async work is done.
		for (const TPair<EFaceTextureType, FMetaHumanCharacterTextureInfo>& TextureInfoPair : InCharacter->SynthesizedFaceTexturesInfo)
		{
			const EFaceTextureType TextureType = TextureInfoPair.Key;

			CharacterData->SynthesizedFaceTexturesFutures.FindOrAdd(TextureType) = InCharacter->GetSynthesizedFaceTextureDataAsync(TextureType);
		}
	}

	for (const TPair<EBodyTextureType, FMetaHumanCharacterTextureInfo>& TextureInfoPair : InCharacter->HighResBodyTexturesInfo)
	{
		const EBodyTextureType TextureType = TextureInfoPair.Key;

		CharacterData->HighResBodyTexturesFutures.FindOrAdd(TextureType) = InCharacter->GetHighResBodyTextureDataAsync(TextureType);
	}

	// Apply the head model data to the character
	ApplyEyelashesAndTeethPropertiesToFaceState(CharacterData, InCharacter->HeadModelSettings.Eyelashes, InCharacter->HeadModelSettings.Teeth,
		/*bInUpdateEyelashes*/ true, /*bInUpdateTeeth*/ true, ELodUpdateOption::All);
	USkelMeshDNAUtils::RebuildRenderData(FaceMesh);
	FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModel(FaceMesh);


	// Apply the preview material type in the actor so it spawns with the correct preview materials
	// and update all the skin material parameters
	UpdateActorsSkinPreviewMaterial(CharacterData, InParams.PreviewMaterial);

	const bool bForceUseExistingTextures = InCharacter->HasSynthesizedTextures() || InCharacter->SkinSettings.bEnableTextureOverrides;
	bool bTexturesHaveBeenRegenerated = false;

	// Build a texture set considering any overrides in the skin settings
	const FMetaHumanCharacterSkinTextureSet FinalSkinTextureSet =
		InCharacter->SkinSettings.GetFinalSkinTextureSet(FMetaHumanCharacterSkinTextureSet
														 {
															 .Face = InCharacter->GetValidFaceTextures(),
															 .Body = InCharacter->BodyTextures
														 });

	// Wait for any async tasks to complete, if requested by the caller
	if (InParams.bBlockUntilComplete)
	{
		WaitForSynthesizedTextures(InCharacter, CharacterData, OutSynthesizedFaceTextures, OutBodyTextures);
	}

	ApplySkinSettings(CharacterData, InCharacter->SkinSettings, bForceUseExistingTextures, FinalSkinTextureSet, OutSynthesizedFaceTextures, OutBodyTextures, bTexturesHaveBeenRegenerated);
	ApplyMakeupSettings(CharacterData, InCharacter->MakeupSettings);
	ApplyEyesSettings(CharacterData, InCharacter->EyesSettings);
	FMetaHumanCharacterSkinMaterials::ApplyEyelashesPropertiesToMaterial(CharacterData->HeadMaterials, InCharacter->HeadModelSettings.Eyelashes);
	FMetaHumanCharacterSkinMaterials::ApplyTeethPropertiesToMaterial(CharacterData->HeadMaterials, InCharacter->HeadModelSettings.Teeth);
	// If there were existing textures, they should not have been regenerated
	ensure(!bForceUseExistingTextures || !bTexturesHaveBeenRegenerated);

	return CharacterData;
}

void UMetaHumanCharacterEditorSubsystem::UpdateCharacterFaceMeshFromDNA(TNotNull<UObject*> InGeneratedAssetsOuter, TSharedPtr<IDNAReader>& InDNAReader, TSharedRef<FMetaHumanCharacterEditorData>& OutCharacterData)
{
	const FName FaceMeshName = MakeUniqueObjectName(InGeneratedAssetsOuter, USkeletalMesh::StaticClass(), TEXT("FaceMesh"), EUniqueObjectNameOptions::GloballyUnique);
	const FString FullPackagePath = InGeneratedAssetsOuter->GetPackage()->GetName();
	
	USkeletalMesh* CreatedFaceMesh = FMetaHumanCharacterSkelMeshUtils::GetSkeletalMeshAssetFromDNA(InDNAReader, FullPackagePath, FaceMeshName.ToString(), EMetaHumanImportDNAType::Face);
	FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(CreatedFaceMesh, InDNAReader, true /*bIsFace*/);
	OutCharacterData->FaceMesh = CreatedFaceMesh;
	OutCharacterData->FaceDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(CreatedFaceMesh));
	
	FMetaHumanCharacterSkinMaterials::SetHeadMaterialsOnMesh(OutCharacterData->HeadMaterials, OutCharacterData->FaceMesh);
	ForEachCharacterActor(OutCharacterData, [OutCharacterData](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
	{
		Actor->UpdateFaceComponentMesh(OutCharacterData->FaceMesh);
		Actor->OnFaceMeshUpdated();
	});
}

void UMetaHumanCharacterEditorSubsystem::ResetTextureSynthesis()
{
	if (FaceTextureSynthesizer.IsValid())
	{
		FaceTextureSynthesizer.Clear();
	}
	FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer(FaceTextureSynthesizer);
}

void UMetaHumanCharacterEditorSubsystem::RemoveObjectToEdit(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	CharacterDataMap.Remove(InCharacter);

	// This function can has the potential to be called during the call to TryAddObjectToEdit
	// in which case CharacterDataMap may be leading leading to the destruction of the FaceTextureSynthesizer object
	// while TryAddObjectToEdit expects it to be alive. Checking bIsAddingObjectToEdit here to prevent TS from
	// being unloaded here. FaceTextureSynthesizer will eventually be released when this function is called for
	// the new character being opened to edit

	if (CharacterDataMap.IsEmpty() && !UE::MetaHuman::bIsAddingObjectToEdit)
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Display, "Finalizing Face Texture Synthesizer");
		FaceTextureSynthesizer.Clear();
	}
}

void UMetaHumanCharacterEditorSubsystem::InitializeMetaHumanCharacter(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::InitializeMetaHumanCharacter");

	const FMetaHumanCharacterIdentityModels& IdentityModels = GetOrCreateCharacterIdentity(InMetaHumanCharacter->TemplateType);

	TSharedPtr<FMetaHumanCharacterIdentity::FState> FaceState = IdentityModels.Face->CreateState();
	TSharedPtr<FMetaHumanCharacterBodyIdentity::FState> BodyState = IdentityModels.Body->CreateState();

	FSharedBuffer FaceStateData;
	FaceState->Serialize(FaceStateData);
	InMetaHumanCharacter->SetFaceStateData(FaceStateData);

	FSharedBuffer BodyStateData;
	BodyState->Serialize(BodyStateData);
	InMetaHumanCharacter->SetBodyStateData(BodyStateData);

	// Set the Character's internal collection to use the default Pipeline, so that the Character can 
	// be built without the user having to manually assign a pipeline.
	InMetaHumanCharacter->GetMutableInternalCollection()->SetDefaultPipeline();

	// Initialize the default eye parameters
	FMetaHumanCharacterSkinMaterials::GetDefaultEyeSettings(InMetaHumanCharacter->EyesSettings);

}

TScriptInterface<IMetaHumanCharacterEditorActorInterface> UMetaHumanCharacterEditorSubsystem::CreateMetaHumanCharacterEditorActor(TNotNull<UMetaHumanCharacter*> InCharacter, TNotNull<UWorld*> InWorld)
{
	FText FailureReason;
	TSubclassOf<AActor> ActorClass;
	if (!TryGetMetaHumanCharacterEditorActorClass(InCharacter, ActorClass, FailureReason))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("FailureReason"), FailureReason);

		FText Message = FText::Format(LOCTEXT("ActorSpawnFailedMessage", "Failed to spawn the Character Pipeline's specified actor. The default actor will be used instead.\n\nReason: {FailureReason}"), Args);

		// This message is quite long, so give a longer expire duration
		UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Fail, 12.0f);
		
		// Fall back to default actor
		ActorClass = AMetaHumanCharacterEditorActor::StaticClass();
	}
	
	check(ActorClass);
	check(ActorClass->ImplementsInterface(UMetaHumanCharacterEditorActorInterface::StaticClass()));

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.bDeferConstruction = true;

	AActor* SpawnedActor = InWorld->SpawnActor(ActorClass, nullptr, nullptr, SpawnParameters);
	check(SpawnedActor);

	TScriptInterface<IMetaHumanCharacterEditorActorInterface> ActorInterface = SpawnedActor;

	const UMetaHumanCollection* Collection = InCharacter->GetInternalCollection();
	// Internal collection should always exist
	check(Collection);
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	const TArray<int32> FaceLODMapping{ 0, 1, 2, 3, 4, 5, 6, 7 };
	const TArray<int32> BodyLODMapping{ 0, 0, 1, 1, 2, 2, 3, 3 };

	// Set the meshes before the construction script runs, so that it can access them
	ActorInterface->InitializeMetaHumanCharacterEditorActor(
		Collection->GetDefaultInstance(), 
		InCharacter, 
		CharacterData->FaceMesh, 
		CharacterData->BodyMesh,
		FaceLODMapping.Num(),
		FaceLODMapping,
		BodyLODMapping);

	SpawnedActor->FinishSpawning(FTransform::Identity);

	ActorInterface->SetForcedLOD((int32)InCharacter->ViewportSettings.LevelOfDetail);

	CharacterData->CharacterActorList.Add(ActorInterface);

	Collection->GetDefaultInstance()->OnInstanceUpdatedNative.Remove(CharacterInstanceUpdatedDelegateHandle);
	CharacterInstanceUpdatedDelegateHandle = Collection->GetDefaultInstance()->OnInstanceUpdatedNative.AddWeakLambda(this, [this, InCharacter]
	{
		OnCharacterInstanceUpdated(InCharacter);
	});

	return ActorInterface;
}

void UMetaHumanCharacterEditorSubsystem::CreateMetaHumanInvisibleDrivingActor(TNotNull<const UMetaHumanCharacter*> InCharacter, TScriptInterface<IMetaHumanCharacterEditorActorInterface> InEditorActorInterface, TNotNull<class UWorld*> InWorld)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags = RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	TObjectPtr<AMetaHumanInvisibleDrivingActor> InvisibleDrivingActor = InWorld->SpawnActor<AMetaHumanInvisibleDrivingActor>(SpawnParameters);
	InvisibleDrivingActor->SetDefaultBodySkeletalMesh();

	// Initialize the preview AnimBP.
	InvisibleDrivingActor->InitPreviewAnimInstance();

	USkeletalMeshComponent* SkelMeshComponent = InvisibleDrivingActor->GetSkeletalMeshComponent();
	ensureMsgf(SkelMeshComponent, TEXT("Cannot create invisible driving actor. Skeletal mesh component is invalid."));
	if (SkelMeshComponent)
	{
		InEditorActorInterface->SetDrivingSkeletalMesh(SkelMeshComponent);

		// Place the actor right next to the preview actor for debugging cases when making it visible.
		FVector DrivingActorLocation = InvisibleDrivingActor->GetActorLocation();
		const FBoxSphereBounds SkelMeshBounds = SkelMeshComponent->GetLocalBounds();
		DrivingActorLocation.X += SkelMeshBounds.GetBox().GetExtent().X * 1.5f;
		InvisibleDrivingActor->SetActorLocation(DrivingActorLocation);
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	CharacterData->InvisibleDrivingActor = InvisibleDrivingActor;
}

TObjectPtr<AMetaHumanInvisibleDrivingActor> UMetaHumanCharacterEditorSubsystem::GetInvisibleDrivingActor(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	const TSharedRef<FMetaHumanCharacterEditorData>* FoundCharacterData = CharacterDataMap.Find(InCharacter);
	EMetaHumanCharacterRigState RiggingState = GetRiggingState(InCharacter);
	if (FoundCharacterData)
	{
		return FoundCharacterData->Get().InvisibleDrivingActor;
	}

	return {};
}

bool UMetaHumanCharacterEditorSubsystem::RemoveTexturesAndRigs(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	if (CharacterDataMap.Contains(InCharacter))
	{
		return false;
	}

	// Remove the Rigs
	const bool bInHasFaceDNABlendShapes = false;
	InCharacter->SetFaceDNABuffer({}, bInHasFaceDNABlendShapes);
	InCharacter->SetBodyDNABuffer({});

	// Remove all textures
	InCharacter->RemoveAllTextures();

	if (!InCharacter->SkinSettings.bEnableTextureOverrides)
	{
		InCharacter->SkinSettings.TextureOverrides.Face.Empty();
		InCharacter->SkinSettings.TextureOverrides.Body.Empty();
	}

	InCharacter->MarkPackageDirty();

	return true;
}

bool UMetaHumanCharacterEditorSubsystem::TryGetMetaHumanCharacterEditorActorClass(TNotNull<const UMetaHumanCharacter*> InCharacter, TSubclassOf<AActor>& OutActorClass, FText& OutFailureReason) const
{
	const UMetaHumanCollection* Collection = InCharacter->GetInternalCollection();
	// Internal collection should always exist
	check(Collection);

	const UMetaHumanCollectionEditorPipeline* Pipeline = Collection->GetEditorPipeline();
	if (!Pipeline)
	{
		OutFailureReason = LOCTEXT("NoPipelineOnCollection", "The Character's Collection has no Pipeline set.");
		return false;
	}

	OutActorClass = Pipeline->GetEditorActorClass();
	if (!OutActorClass)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PipelinePath"), FText::FromString(Pipeline->GetPathName()));

		OutFailureReason = FText::Format(LOCTEXT("NoEditorActorClass", "The Character's Pipeline ({PipelinePath}) doesn't specify an editor actor."), Args);

		return false;
	}
	
	if (!OutActorClass->ImplementsInterface(UMetaHumanCharacterEditorActorInterface::StaticClass()))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PipelinePath"), FText::FromString(Pipeline->GetPathName()));

		OutFailureReason = FText::Format(
			LOCTEXT("InvalidEditorActorClass", "The editor actor specified by the Character's Pipeline ({PipelinePath}) doesn't implement IMetaHumanCharacterEditorActorInterface."),
			Args);

		return false;
	}

	return true;
}

bool UMetaHumanCharacterEditorSubsystem::TryGenerateCharacterAssets(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	UObject* InOuterForGeneratedAssets,
	FMetaHumanCharacterGeneratedAssets& OutGeneratedAssets)
{
	if (!InOuterForGeneratedAssets)
	{
		InOuterForGeneratedAssets = GetTransientPackage();
	}

	OutGeneratedAssets = FMetaHumanCharacterGeneratedAssets();

	const FEditorDataForCharacterCreationParams EditorDataForCharacterCreationParams =
	{ 
		.bBlockUntilComplete = true,
		.bCreateMeshFromDNA = true,
		.OuterForGeneratedAssets = InOuterForGeneratedAssets,
		.PreviewMaterial = EMetaHumanCharacterSkinPreviewMaterial::Editable,
	};
	TSharedPtr<FMetaHumanCharacterEditorData> CharacterData = CreateEditorDataForCharacter(InCharacter,
																						   EditorDataForCharacterCreationParams,
																						   OutGeneratedAssets.SynthesizedFaceTextures,
																						   OutGeneratedAssets.BodyTextures);

	if (!CharacterData.IsValid())
	{
		return false;
	}

	check(CharacterData->BodyMesh);
	OutGeneratedAssets.BodyMesh = CharacterData->BodyMesh;
	OutGeneratedAssets.Metadata.Emplace(CharacterData->BodyMesh, TEXT("Body"), FString::Format(TEXT("SKM_{0}_BodyMesh"), { InCharacter->GetName() }));

	check(CharacterData->FaceMesh);
	OutGeneratedAssets.FaceMesh = CharacterData->FaceMesh;
	OutGeneratedAssets.Metadata.Emplace(CharacterData->FaceMesh, TEXT("Face"), FString::Format(TEXT("SKM_{0}_FaceMesh"), { InCharacter->GetName() }));

	OutGeneratedAssets.PhysicsAsset = CreatePhysicsAssetForCharacter(InCharacter, InOuterForGeneratedAssets, CharacterData->BodyState);
	OutGeneratedAssets.Metadata.Emplace(OutGeneratedAssets.PhysicsAsset, TEXT("Body"), FString::Format(TEXT("PHYS_{0}"), { InCharacter->GetName() }));

	// Generate measurements using the model
	{
		// Cast to shared ref, as these should always be valid
		TSharedRef<IDNAReader> FaceDNA = UE::MetaHuman::GetDNAReader(CharacterData->FaceMesh).ToSharedRef();
		TSharedRef<IDNAReader> BodyDNA = UE::MetaHuman::GetDNAReader(CharacterData->BodyMesh).ToSharedRef();

		CharacterData->BodyState->GetMeasurementsForFaceAndBody(
			FaceDNA,
			BodyDNA,
			OutGeneratedAssets.BodyMeasurements
		);
	}

	// Face and body textures and materials
	{
		const FMetaHumanCharacterFaceMaterialSet& HeadMaterialSetDynamic = CharacterData->HeadMaterials;

		// Convert the Head material set from dynamic to constant instances

		// Create a new face material set to be applied in the face mesh being built
		FMetaHumanCharacterFaceMaterialSet FaceMaterialSet =
		{
			.EyeLeft = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.EyeLeft, InOuterForGeneratedAssets),
			.EyeRight = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.EyeRight, InOuterForGeneratedAssets),
			.EyeShell = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.EyeShell, InOuterForGeneratedAssets),
			.LacrimalFluid = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.LacrimalFluid, InOuterForGeneratedAssets),
			.Teeth = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.Teeth, InOuterForGeneratedAssets),
			.Eyelashes = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.Eyelashes, InOuterForGeneratedAssets),
			.EyelashesHiLods = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(HeadMaterialSetDynamic.EyelashesHiLods, InOuterForGeneratedAssets)
		};

		HeadMaterialSetDynamic.ForEachSkinMaterial<UMaterialInstance>(
			[&FaceMaterialSet, InOuterForGeneratedAssets](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstance* Material)
			{
				FaceMaterialSet.Skin.FindOrAdd(Slot) = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(Material, InOuterForGeneratedAssets);
			}
		);

		// Assign the material instance constants to the output meshes
		FMetaHumanCharacterSkinMaterials::SetHeadMaterialsOnMesh(FaceMaterialSet, OutGeneratedAssets.FaceMesh);

		{
			// Generate the metadata for each material in the set
			FaceMaterialSet.ForEachSkinMaterial<UMaterialInstance>(
				[&OutGeneratedAssets](EMetaHumanCharacterSkinMaterialSlot Slot, UMaterialInstance* Material)
				{
					const FString AssetName = FString::Format(TEXT("MI_Face_Skin_{0}"), { StaticEnum<EMetaHumanCharacterSkinMaterialSlot>()->GetAuthoredNameStringByValue((int64) Slot) });
					OutGeneratedAssets.Metadata.Emplace(Material, TEXT("Face/Materials"), AssetName);
				}
			);

			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.Eyelashes, TEXT("Face/Materials"), TEXT("MI_Face_Eyelashes"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.EyelashesHiLods, TEXT("Face/Materials"), TEXT("MI_Face_EyelashesHiLODs"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.EyeLeft, TEXT("Face/Materials"), TEXT("MI_Face_Eye_Left"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.EyeRight, TEXT("Face/Materials"), TEXT("MI_Face_Eye_Right"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.EyeShell, TEXT("Face/Materials"), TEXT("MI_Face_EyeShell"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.LacrimalFluid, TEXT("Face/Materials"), TEXT("MI_Face_LacrimalFluid"));
			OutGeneratedAssets.Metadata.Emplace(FaceMaterialSet.Teeth, TEXT("Face/Materials"), TEXT("MI_Face_Teeth"));
		}

		UMaterialInstanceDynamic* BodyPreviewMaterialInstance = CharacterData->BodyMaterial;
		check(BodyPreviewMaterialInstance);

		UMaterialInstance* BodySkinMaterial = UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(BodyPreviewMaterialInstance, InOuterForGeneratedAssets);
		OutGeneratedAssets.Metadata.Emplace(BodySkinMaterial, TEXT("Body/Materials"), TEXT("MI_Body_Skin"));

		// Assign the body material
		check(!OutGeneratedAssets.BodyMesh->GetMaterials().IsEmpty());
		OutGeneratedAssets.BodyMesh->GetMaterials()[0].MaterialInterface = BodySkinMaterial;

		for (TPair<EFaceTextureType, TObjectPtr<UTexture2D>>& Pair : OutGeneratedAssets.SynthesizedFaceTextures)
		{
			if (Pair.Value)
			{
				FString TextureName(TEXT("T_Face_"));
				TextureName += StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key));

				// TODO: If user doesn't set the preview material type to be Editable (aka still uses gray textures) then "editable"
				// materials and textures will still get exported, but the texture data buffer will be null causing the texture
				// source to be empty, effectively making textures black on reload.

				const TFuture<FSharedBuffer>& TextureDataFuture = InCharacter->GetSynthesizedFaceTextureDataAsync(Pair.Key);
				TextureDataFuture.Wait();

				// Textures generated for the assembly should contain the source data
				ensureAlwaysMsgf(Pair.Value->Source.IsValid(), TEXT("Texture generated for assembly without source data."));
				if (!Pair.Value->Source.IsValid())
				{
					// Initialize the source from the original data if needed
					Pair.Value->Source.Init(FImageView(
						const_cast<void*>(TextureDataFuture.Get().GetData()), // ImageView expects non-const, but Source.Init uses const void*
						Pair.Value->GetSizeX(),
						Pair.Value->GetSizeY(),
						FImageCoreUtils::GetRawImageFormatForPixelFormat(Pair.Value->GetPixelFormat())));
				}

				OutGeneratedAssets.Metadata.Emplace(Pair.Value, TEXT("Face/Textures"), TextureName);
			}
		}

		for (TPair<EBodyTextureType, TObjectPtr<UTexture2D>>& Pair : OutGeneratedAssets.BodyTextures)
		{
			if (Pair.Value && InCharacter->HighResBodyTexturesInfo.Contains(Pair.Key))
			{
				FString TextureName(TEXT("T_"));
				TextureName += StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(Pair.Key));

				// TODO: If user doesn't set the preview material type to be Editable (aka still uses gray textures) then "editable"
				// materials and textures will still get exported, but the texture data buffer will be null causing the texture
				// source to be empty, effectively making textures black on reload.

				const TFuture<FSharedBuffer>& PayloadData = InCharacter->GetHighResBodyTextureDataAsync(Pair.Key);
				PayloadData.Wait();
				const FSharedBuffer& Payload = PayloadData.Get();
				if (!Payload.IsNull())
				{
					ensureAlwaysMsgf(Pair.Value->Source.IsValid(), TEXT("Texture generated for assembly without source data."));
					if (!Pair.Value->Source.IsValid())
					{
						// Initialize the source from the original data if needed
						Pair.Value->Source.Init(FImageView(
							const_cast<void*>(Payload.GetData()), // ImageView expects non-const, but Source.Init uses const void*
							Pair.Value->GetSizeX(),
							Pair.Value->GetSizeY(),
							FImageCoreUtils::GetRawImageFormatForPixelFormat(Pair.Value->GetPixelFormat())));
					}

					OutGeneratedAssets.Metadata.Emplace(Pair.Value, TEXT("Body/Textures"), TextureName);
				}
			}
		}
	}

	// Clear the transient flag from all generated objects, and move them into the designated outer, 
	// so that they can be saved.
	for (FMetaHumanGeneratedAssetMetadata& Metadata : OutGeneratedAssets.Metadata)
	{
		if (Metadata.Object->GetOuter() != InOuterForGeneratedAssets)
		{
			// Try to keep the same name in the new outer package to improve debugging
			const FName NewName = MakeUniqueObjectName(InOuterForGeneratedAssets, Metadata.Object->GetClass(), Metadata.Object->GetFName());
			Metadata.Object->Rename(*NewName.ToString(), InOuterForGeneratedAssets);
		}

		Metadata.Object->ClearFlags(RF_Transient);
	}

	// Set preview mesh on physics asset after it has been moved
	OutGeneratedAssets.PhysicsAsset->SetPreviewMesh(OutGeneratedAssets.BodyMesh);
	OutGeneratedAssets.BodyMesh->SetPhysicsAsset(OutGeneratedAssets.PhysicsAsset);

	return true;
}

bool UMetaHumanCharacterEditorSubsystem::TryGetCharacterPreviewAssets(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	FMetaHumanCharacterPreviewAssets& OutPreviewAssets)
{
	TSharedRef<FMetaHumanCharacterEditorData>* CharacterDataPtr = CharacterDataMap.Find(InCharacter);
	if (!CharacterDataPtr)
	{
		return false;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = *CharacterDataPtr;

	OutPreviewAssets.FaceMesh = CharacterData->FaceMesh;
	OutPreviewAssets.BodyMesh = CharacterData->BodyMesh;

	// Generate measurements using the model
	{
		const double Start = FPlatformTime::Seconds();

		// Cast to shared ref, as these should always be valid
		TSharedRef<IDNAReader> FaceDNA = UE::MetaHuman::GetDNAReader(CharacterData->FaceMesh).ToSharedRef();
		TSharedRef<IDNAReader> BodyDNA = UE::MetaHuman::GetDNAReader(CharacterData->BodyMesh).ToSharedRef();

		CharacterData->BodyState->GetMeasurementsForFaceAndBody(
			FaceDNA,
			BodyDNA,
			OutPreviewAssets.BodyMeasurements
		);

		const double End = FPlatformTime::Seconds();
		UE_LOGFMT(LogMetaHumanCharacterEditor, Log, "Time to read character preview assets = {Time}", End - Start);
	}

	return true;
}

EMetaHumanCharacterRigState UMetaHumanCharacterEditorSubsystem::GetRiggingState(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	if (InCharacter->HasFaceDNA())
	{
		return EMetaHumanCharacterRigState::Rigged;
	}
	
	if (IsAutoRiggingFace(InCharacter))
	{
		return EMetaHumanCharacterRigState::RigPending;
	}

	return EMetaHumanCharacterRigState::Unrigged;
}


bool UMetaHumanCharacterEditorSubsystem::CanBuildMetaHuman(TNotNull<const UMetaHumanCharacter*> InCharacter, FText& OutErrorMessage)
{
	OutErrorMessage = FText::GetEmpty();

	if (!CharacterDataMap.Contains(InCharacter))
	{
		OutErrorMessage = LOCTEXT("BuildDisabled_CharacterNotLoaded", "Character data is not loaded.");
		return false;
	}

	if (IsRequestingHighResolutionTextures(InCharacter))
	{
		OutErrorMessage = LOCTEXT("BuildDisabled_RequestingHighResTextures", "Requesting HighRes texture in progress.");
		return false;
	}

	if (IsAutoRiggingFace(InCharacter))
	{
		OutErrorMessage = LOCTEXT("BuildDisabled_FaceAutoRigInProgress", "Face auto rig in progress.");
		return false;
	}

	if (!InCharacter->HasFaceDNA())
	{
		OutErrorMessage = LOCTEXT("BuildDisabled_NoFaceDNA", "Character is not rigged.");
		return false;
	}

	// TODO: ensure override textures set this property
	// This is a restriction at the moment since the assembly expects the animated maps which become available once high rez textures are downloaded
	if (!InCharacter->HasHighResolutionTextures())
	{
		OutErrorMessage = LOCTEXT("BuildDisabled_NoHighResolutionTextures", "The Character is missing textures, use Download Texture Sources to create them before assembling");
		return false;
	}

	return true;
}

void UMetaHumanCharacterEditorSubsystem::GetMaterialSetForCharacter(TNotNull<const UMetaHumanCharacter*> InCharacter, FMetaHumanCharacterFaceMaterialSet& OutFaceMaterials, UMaterialInstanceDynamic*& OutBodyMaterial)
{
	OutFaceMaterials = FMetaHumanCharacterSkinMaterials::GetHeadPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial::Editable);	
	OutBodyMaterial = FMetaHumanCharacterSkinMaterials::GetBodyPreviewMaterialInstance(EMetaHumanCharacterSkinPreviewMaterial::Editable);

	FMetaHumanCharacterSkinMaterials::ApplySynthesizedTexturesToFaceMaterial(OutFaceMaterials, InCharacter->SynthesizedFaceTextures);
	FMetaHumanCharacterSkinMaterials::ApplySkinParametersToMaterials(OutFaceMaterials, OutBodyMaterial, InCharacter->SkinSettings);
	FMetaHumanCharacterSkinMaterials::ApplyFoundationMakeupToMaterial(OutFaceMaterials, InCharacter->MakeupSettings.Foundation);
	FMetaHumanCharacterSkinMaterials::ApplyEyeMakeupToMaterial(OutFaceMaterials, InCharacter->MakeupSettings.Eyes);
	FMetaHumanCharacterSkinMaterials::ApplyBlushMakeupToMaterial(OutFaceMaterials, InCharacter->MakeupSettings.Blush);
	FMetaHumanCharacterSkinMaterials::ApplyLipsMakeupToMaterial(OutFaceMaterials, InCharacter->MakeupSettings.Lips);

	// Update the Body Material Parameters to match
	FMetaHumanCharacterBodyTextureUtils::GetSkinToneAndUpdateMaterials(InCharacter->SkinSettings.Skin,
																	   FaceTextureSynthesizer,
																	   InCharacter->BodyTextures,
																	   OutFaceMaterials,
																	   OutBodyMaterial);
}

UMaterialInterface* UMetaHumanCharacterEditorSubsystem::GetTranslucentClothingMaterial() const
{
	return LoadObject<UMaterialInterface>(nullptr, TEXT("/" UE_PLUGIN_NAME "/Clothing/MI_TranslucentClothing.MI_TranslucentClothing"));
}

bool UMetaHumanCharacterEditorSubsystem::IsCharacterOutfitSelected(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter)
{
	bool bIsOutfitSelected = false;

	if (const UMetaHumanCollection* CharacterCollection = InMetaHumanCharacter->GetInternalCollection())
	{
		// Find the slot names for any slots supporting outfits
		TArray<FName> OutfitSlots;
		if (CharacterCollection->GetPipeline())
		{
			TNotNull<const UMetaHumanCharacterPipelineSpecification*> Specification = CharacterCollection->GetPipeline()->GetSpecification();
			for (const TPair<FName, FMetaHumanCharacterPipelineSlot>& Pair : Specification->Slots)
			{
				if (Pair.Value.SupportsAssetType(UChaosOutfitAsset::StaticClass()))
				{
					OutfitSlots.Add(Pair.Key);
				}
			}
		}

		// Check if the outfit slots have any built data
		const FMetaHumanCollectionBuiltData& CollectionBuiltData = CharacterCollection->GetBuiltData(EMetaHumanCharacterPaletteBuildQuality::Preview);
		if (!OutfitSlots.IsEmpty() && CollectionBuiltData.IsValid())
		{
			for (const FMetaHumanCharacterPaletteItem& Item : CharacterCollection->GetItems())
			{
				if (Item.GetItemKey() != FMetaHumanPaletteItemKey()
					&& OutfitSlots.Contains(Item.SlotName))
				{
					if (CollectionBuiltData.PaletteBuiltData.HasBuildOutputForItem(FMetaHumanPaletteItemPath{ Item.GetItemKey() }))
					{
						bIsOutfitSelected = true;
						break;
					}
				}
			}
		}
	}

	return bIsOutfitSelected;
}

void UMetaHumanCharacterEditorSubsystem::ApplyFaceState(TNotNull<const UMetaHumanCharacter*> InCharacter, TSharedRef<const FMetaHumanCharacterIdentity::FState> InState)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	// Since this function is publicly accessible, we copy the passed-in state so that the caller
	// can't keep a mutable reference to the subsystem's edit state.
	ApplyFaceState(CharacterData, MakeShared<FMetaHumanCharacterIdentity::FState>(*InState));

}

void UMetaHumanCharacterEditorSubsystem::ApplyFaceState(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, TSharedRef<FMetaHumanCharacterIdentity::FState> InState)
{
	InCharacterData->FaceState = InState;

	FMetaHumanCharacterFaceEvaluationSettings FaceEvaluationSettings = InCharacterData->FaceEvaluationSettings.Get({});
	FaceEvaluationSettings.GlobalDelta = InState->GetSettings().GlobalVertexDeltaScale();
	FaceEvaluationSettings.HighFrequencyDelta = InState->GetSettings().GlobalHighFrequencyScale();
	FaceEvaluationSettings.HeadScale = InState->GetFaceScale();
	InCharacterData->FaceEvaluationSettings = FaceEvaluationSettings;
	
	UpdateFaceMeshInternal(InCharacterData, InState->Evaluate(), ELodUpdateOption::All);
	
	FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModelVerticesNormalsAndTangents(InCharacterData->FaceMesh);
	
	InCharacterData->OnFaceStateChangedDelegate.Broadcast();
}

TSharedRef<const FMetaHumanCharacterIdentity::FState> UMetaHumanCharacterEditorSubsystem::GetFaceState(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->FaceState;
}

TSharedRef<FMetaHumanCharacterIdentity::FState> UMetaHumanCharacterEditorSubsystem::CopyFaceState(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	return MakeShared<FMetaHumanCharacterIdentity::FState>(*GetFaceState(InCharacter));
}

void UMetaHumanCharacterEditorSubsystem::CommitFaceState(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<const FMetaHumanCharacterIdentity::FState> InState)
{
	FSharedBuffer FaceStateData;
	InState->Serialize(FaceStateData);
	
	InCharacter->SetFaceStateData(FaceStateData);
	InCharacter->FaceEvaluationSettings.GlobalDelta = InState->GetSettings().GlobalVertexDeltaScale();
	InCharacter->FaceEvaluationSettings.HighFrequencyDelta = InState->GetSettings().GlobalHighFrequencyScale();
	InCharacter->FaceEvaluationSettings.HeadScale = InState->GetFaceScale();
	InCharacter->MarkPackageDirty();

	ApplyFaceState(InCharacter, InState);
}

TSharedPtr<IDNAReader> UMetaHumanCharacterEditorSubsystem::ApplyFaceDNA(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedPtr<IDNAReader> InFaceDNAReader, ELodUpdateOption InLodUpdateOption, bool bInResettingToArchetypeDNA)
{
	bool bDnaApplied = false;

	UE::MetaHuman::ERigType RigType = UE::MetaHuman::ERigType::JointsOnly;

	if (InFaceDNAReader->GetBlendShapeChannelCount() > 0)
	{
		RigType = UE::MetaHuman::ERigType::JointsAndBlendshapes;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	TSharedPtr<IDNAReader> Dna;

	USkeletalMesh* FaceSkeletalMesh = CharacterData->FaceMesh;

	if (FMetaHumanCharacterSkelMeshUtils::CheckDNACompatibility(UE::MetaHuman::GetDNAReader(FaceSkeletalMesh).Get(), InFaceDNAReader.Get()))
	{
		if (UDNAAsset* BodyDNAAsset = CharacterData->BodyMesh->GetAssetUserData<UDNAAsset>())
		{
			TSharedPtr<IDNAReader> BodyDna = CharacterData->BodyState->StateToDna(BodyDNAAsset); // ensure body DNA is updated from body state
			TSharedPtr<FMetaHumanCharacterIdentity> FaceCharacterIdentity = GetOrCreateCharacterIdentity(InCharacter->TemplateType).Face;
			Dna = FaceCharacterIdentity->CopyBodyJointsToFace(BodyDna->Unwrap(), InFaceDNAReader->Unwrap());
			// copy the vertex influence weights from the combined model
			TArray<TPair<int32, TArray<FFloatTriplet>>>  CombinedModelVertexInfluenceWeights;
			CharacterData->BodyState->CopyCombinedModelVertexInfluenceWeights(CombinedModelVertexInfluenceWeights);
			Dna = FaceCharacterIdentity->UpdateFaceSkinWeightsFromBodyAndVertexNormals(CombinedModelVertexInfluenceWeights, Dna->Unwrap(), CharacterData->FaceState.Get());
		}

		if (RigType == UE::MetaHuman::ERigType::JointsOnly)
		{
			const FMetaHumanRigEvaluatedState VerticesAndNormals = CharacterData->FaceState->Evaluate();

			// Check that DNA vertices are at the same position as current face state.
			// we do not perform this test if we are resetting to the placeholder archetype DNA, as we are not updating the base mesh, and 
			// the state vertices will not match in this case; this does not matter
			if (!bInResettingToArchetypeDNA) 
			{
				// NB the tolerance below has to be quite high because can get slightly different numerical results from autorigging service; we do not believe this is an error
				if (!FMetaHumanCharacterSkelMeshUtils::CompareDnaToStateVerticesAndNormals(Dna, VerticesAndNormals.Vertices, VerticesAndNormals.VertexNormals, CharacterData->FaceState, /*Tolerance=*/ 0.05f)) 
				{
					UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "DNA vertices or vertex normals mismatch with the State");
				}				
				//return?
			}

			// TODO: verify whether we should actually update everything in the Skel Mesh, if not we need to combine the local archetype with the AR one
			// For now test updating only the new data since the base mesh should match and the skinning may not be correct
			const FMetaHumanCharacterSkelMeshUtils::EUpdateFlags SkelMeshUpdateFlags =
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::Joints |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::DNABehavior |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::DNAGeometry |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::SkinWeights;

			FMetaHumanCharacterSkelMeshUtils::UpdateSkelMeshFromDNA(Dna.ToSharedRef(), SkelMeshUpdateFlags, CharacterData->FaceDnaToSkelMeshMap, EMetaHumanCharacterOrientation::Y_UP, FaceSkeletalMesh);
		

			// this is needed to update the normals
			UpdateFaceMeshInternal(CharacterData, VerticesAndNormals, InLodUpdateOption);

		}
		else
		{
			UpdateCharacterFaceMeshFromDNA(GetTransientPackage(), Dna, CharacterData);
		}
		bDnaApplied = true;
	}

	if (bDnaApplied)
	{
		return Dna;
	}

	return nullptr;
}

void UMetaHumanCharacterEditorSubsystem::EnableSkeletalPostProcessing(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	if (USkeletalMesh* FaceMesh = CharacterData->FaceMesh)
	{
		FMetaHumanCharacterSkelMeshUtils::SetPostProcessAnimBP(FaceMesh, TEXT("/" UE_PLUGIN_NAME "/Face/ABP_Face_PostProcess"));
	}

	if (USkeletalMesh* BodyMesh = CharacterData->BodyMesh)
	{
		FMetaHumanCharacterSkelMeshUtils::SetPostProcessAnimBP(BodyMesh, TEXT("/" UE_PLUGIN_NAME "/Body/ABP_Body_PostProcess"));
	}
}

void UMetaHumanCharacterEditorSubsystem::DisableSkeletalPostProcessing(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	if (USkeletalMesh* FaceMesh = CharacterData->FaceMesh)
	{
		FMetaHumanCharacterSkelMeshUtils::SetPostProcessAnimBP(FaceMesh, {});
	}

	if (USkeletalMesh* BodyMesh = CharacterData->BodyMesh)
	{
		FMetaHumanCharacterSkelMeshUtils::SetPostProcessAnimBP(BodyMesh, {});
	}
}

void UMetaHumanCharacterEditorSubsystem::EnableAnimation(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterEditorData = CharacterDataMap[InCharacter];
	ForEachCharacterActor(CharacterEditorData,
		[CharacterEditorData](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
		{
			Actor->ReinitAnimation();
		});
}

void UMetaHumanCharacterEditorSubsystem::DisableAnimation(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterEditorData = CharacterDataMap[InCharacter];
	ForEachCharacterActor(CharacterEditorData,
		[CharacterEditorData](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
		{
			Actor->ResetAnimation();
		});
}

void UMetaHumanCharacterEditorSubsystem::ImportFaceDNA(TNotNull<const UMetaHumanCharacter*> InCharacter, TSharedPtr<IDNAReader> InFaceDNAReader)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	FString NewRigAssetName, NewRigPath, DefaultSuffix;

	const FString SanitizedBasePackageName = InCharacter->GetOutermost()->GetName();
	const FString PackagePath = FPackageName::GetLongPackagePath(SanitizedBasePackageName);
	const FString BaseAssetNameWithPrefix = TEXT("SK_") + FPackageName::GetLongPackageAssetName(InFaceDNAReader->GetName());
	const FString SanitizedBaseAssetNameWithPrefix = ObjectTools::SanitizeObjectName(BaseAssetNameWithPrefix);

	AssetTools.CreateUniqueAssetName(PackagePath / SanitizedBaseAssetNameWithPrefix, DefaultSuffix, NewRigPath, NewRigAssetName);
	NewRigPath = FPackageName::GetLongPackagePath(NewRigPath);

	FInterchangeDnaModule& DNAImportModule = FInterchangeDnaModule::GetModule();
	USkeletalMesh* RigSkeletalMesh = DNAImportModule.ImportSync(NewRigAssetName, NewRigPath, InFaceDNAReader);
	FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(RigSkeletalMesh, InFaceDNAReader, true /*bIsFace*/);

	UPackage* NewPackage = CreatePackage(*NewRigPath);
	USkeletalMesh* NewAsset = DuplicateObject(RigSkeletalMesh, NewPackage, FName(NewRigAssetName));

	FAssetRegistryModule::AssetCreated(NewAsset);
}

void UMetaHumanCharacterEditorSubsystem::CommitFaceDNA(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<IDNAReader> InFaceDNAReader)
{
	// Update the actor skeletal meshes
	TSharedPtr<IDNAReader> InDna(InFaceDNAReader);
	TSharedPtr<IDNAReader> OutDna = ApplyFaceDNA(InCharacter, InDna);

	if (OutDna.IsValid())
	{
		InCharacter->SetFaceDNABuffer(ReadStreamFromDNA(OutDna.Get(), EDNADataLayer::All), InFaceDNAReader->GetBlendShapeChannelCount() > 0);
		InCharacter->MarkPackageDirty();

		RunCharacterEditorPipelineForPreview(InCharacter);

	}

	EnableSkeletalPostProcessing(InCharacter);
}

void UMetaHumanCharacterEditorSubsystem::ResetCharacterFace(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState->Reset();

	UpdateFaceMeshInternal(CharacterData, CharacterData->FaceState->Evaluate(), GetUpdateOptionForEditing());
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::GetFaceGizmos(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	return CharacterData->FaceState->EvaluateGizmos(CharacterData->FaceState->Evaluate().Vertices);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::SetFaceGizmoPosition(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InState,
	int32 InGizmoIndex,
	const FVector3f& InPosition,
	bool bInSymmetric,
	bool bInEnforceBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::SetFaceGizmoPosition");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState = MakeShared<FMetaHumanCharacterIdentity::FState>(*InState);
	CharacterData->FaceState->SetGizmoPosition(InGizmoIndex, InPosition, bInSymmetric, bInEnforceBounds);

	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());

	CharacterData->OnFaceStateChangedDelegate.Broadcast();

	return CharacterData->FaceState->EvaluateGizmos(FaceVerticesAndVertexNormals.Vertices);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::SetFaceGizmoRotation(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InState,
	int32 InGizmoIndex,
	const FVector3f& InRotation,
	bool bInSymmetric,
	bool bInEnforceBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::SetFaceGizmoRotation");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState = MakeShared<FMetaHumanCharacterIdentity::FState>(*InState);
	CharacterData->FaceState->SetGizmoRotation(InGizmoIndex, InRotation, bInSymmetric, bInEnforceBounds);

	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());

	CharacterData->OnFaceStateChangedDelegate.Broadcast();

	return CharacterData->FaceState->EvaluateGizmos(FaceVerticesAndVertexNormals.Vertices);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::SetFaceGizmoScale(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InState,
	int32 InGizmoIndex,
	float InScale,
	bool bInSymmetric,
	bool bInEnforceBounds)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::SetFaceGizmoScale");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState = MakeShared<FMetaHumanCharacterIdentity::FState>(*InState);
	CharacterData->FaceState->SetGizmoScale(InGizmoIndex, InScale, bInSymmetric, bInEnforceBounds);

	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());

	CharacterData->OnFaceStateChangedDelegate.Broadcast();

	return CharacterData->FaceState->EvaluateGizmos(FaceVerticesAndVertexNormals.Vertices);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::GetFaceLandmarks(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	return CharacterData->FaceState->EvaluateLandmarks(CharacterData->FaceState->Evaluate().Vertices);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::TranslateFaceLandmark(
	TNotNull<const UMetaHumanCharacter*> InCharacter, 
	TSharedRef<const FMetaHumanCharacterIdentity::FState> InState, 
	int32 InLandmarkIndex, 
	const FVector3f& InDelta,
	bool bInTranslateSymmetrically)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::TranslateFaceLandmark");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState = MakeShared<FMetaHumanCharacterIdentity::FState>(*InState);
	CharacterData->FaceState->TranslateLandmark(InLandmarkIndex, InDelta, bInTranslateSymmetrically);
	
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());

	CharacterData->OnFaceStateChangedDelegate.Broadcast();

	return CharacterData->FaceState->EvaluateLandmarks(FaceVerticesAndVertexNormals.Vertices);
}

int32 UMetaHumanCharacterEditorSubsystem::SelectFaceVertex(TNotNull<const UMetaHumanCharacter*> InCharacter, const FRay& InRay, FVector& OutHitVertex, FVector& OutHitNormal)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::SelectFaceVertex");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	FVector3f HitVertex;
	FVector3f HitNormal;
	FVector3f RayOrigin{ (float)InRay.Origin.X, (float)InRay.Origin.Y, (float)InRay.Origin.Z };
	FVector3f RayDirection{ (float)InRay.Direction.X, (float)InRay.Direction.Y, (float)InRay.Direction.Z };
	int32 HitVertexID = CharacterData->FaceState->SelectFaceVertex(RayOrigin, RayDirection, HitVertex, HitNormal);
	if (HitVertexID != INDEX_NONE)
	{
		OutHitVertex = FVector{ HitVertex.X, HitVertex.Y, HitVertex.Z };
		OutHitNormal = FVector{ HitNormal.X, HitNormal.Y, HitNormal.Z };
	}
	return HitVertexID;
}

void UMetaHumanCharacterEditorSubsystem::AddFaceLandmark(TNotNull<const UMetaHumanCharacter*> InCharacter, int32 InMeshVertexIndex)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState->AddLandmark(InMeshVertexIndex);
}

void UMetaHumanCharacterEditorSubsystem::RemoveFaceLandmark(TNotNull<const UMetaHumanCharacter*> InCharacter, int32 InLandmarkIndex)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->FaceState->RemoveLandmark(InLandmarkIndex);
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::BlendFaceRegion(TNotNull<const UMetaHumanCharacter*> InCharacter, int32 InRegionIndex, const TSharedPtr<const FMetaHumanCharacterIdentity::FState>& InStartState, const TArray<TSharedPtr<const FMetaHumanCharacterIdentity::FState>>& InPresetStates, TConstArrayView<float> InPresetWeights, EBlendOptions InBlendOptions, bool bInBlendSymmetrically)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::BlendFaceRegion");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	if (InPresetStates.Num() <= InPresetWeights.Num() && InPresetStates.Num() > 0)
	{
		TArray<TPair<float, const FMetaHumanCharacterIdentity::FState*>> PresetStateWeights;
		for (int32 PresetIndex = 0; PresetIndex < InPresetStates.Num(); PresetIndex++)
		{
			if (InPresetStates[PresetIndex])
			{
				PresetStateWeights.Add({ InPresetWeights[PresetIndex], InPresetStates[PresetIndex].Get() });
			}
		}

		CharacterData->FaceState = MakeShared<FMetaHumanCharacterIdentity::FState>(*InStartState);
		CharacterData->FaceState->BlendPresets(InRegionIndex, PresetStateWeights, InBlendOptions, bInBlendSymmetrically);

		const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
		UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());
	}
	return CharacterData->FaceState->EvaluateGizmos(CharacterData->FaceState->Evaluate().Vertices);
}

// TODO move this to UDNAAsset
TSharedRef<IDNAReader> GetDnaReaderFromAsset(UDNAAsset* InDna)
{
	pma::ScopedPtr<dna::MemoryStream> MemoryStream = pma::makeScoped<dna::MemoryStream>();
	pma::ScopedPtr<dna::BinaryStreamWriter> DnaWriter = pma::makeScoped<dna::BinaryStreamWriter>(MemoryStream.get());

	DnaWriter->setFrom(InDna->GetBehaviorReader()->Unwrap(), dna::DataLayer::All);
#if WITH_EDITORONLY_DATA
	DnaWriter->setFrom(InDna->GetGeometryReader()->Unwrap(), dna::DataLayer::Geometry);
#endif
	DnaWriter->write();

	pma::ScopedPtr<dna::BinaryStreamReader> BinaryDnaReader = pma::makeScoped<dna::BinaryStreamReader>(MemoryStream.get());
	BinaryDnaReader->read();

	return MakeShared<FDNAReader<dna::BinaryStreamReader>>(BinaryDnaReader.release());
}

void UMetaHumanCharacterEditorSubsystem::RemoveFaceRig(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	// stop any animation if we have any
	AMetaHumanInvisibleDrivingActor* DrivingActor = GetInvisibleDrivingActor(InCharacter);
	if (DrivingActor)
	{
		DrivingActor->StopAnimation();
	}

	// reset the bulk data
	InCharacter->SetFaceDNABuffer({}, /*bInHasFaceDNABlendshapes*/ false);
	InCharacter->MarkPackageDirty();

	// delete morph targets if there are any
	if (!CharacterData->FaceMesh->GetMorphTargets().IsEmpty())
	{
		CharacterData->FaceMesh->UnregisterAllMorphTarget();
	}

	// set the face mesh DNA back to the archetype
	UDNAAsset* FaceArchetypeDNA = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAAseet(EMetaHumanImportDNAType::Face, GetTransientPackage());
	check(FaceArchetypeDNA);
	// Revert the map to match archetype DNA.
	TSharedPtr<IDNAReader> ArchetypeDNAReader = GetDnaReaderFromAsset(FaceArchetypeDNA);
	CharacterData->FaceDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(ArchetypeDNAReader.Get(), CharacterData->FaceMesh));
	ApplyFaceDNA(InCharacter, ArchetypeDNAReader, ELodUpdateOption::All, /*bInResettingToArchetypeDNA*/ true);


	CharacterData->FaceMesh->PostEditChange();
	CharacterData->FaceMesh->MarkPackageDirty();	
	
	// set LOD to LOD0
	UpdateCharacterLOD(InCharacter, EMetaHumanCharacterLOD::LOD0);
}


void UMetaHumanCharacterEditorSubsystem::RemoveBodyRig(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	// stop any animation if we have any
	AMetaHumanInvisibleDrivingActor* DrivingActor = GetInvisibleDrivingActor(InCharacter);
	if (DrivingActor)
	{
		DrivingActor->StopAnimation();
	}

	// reset the bulk data
	InCharacter->SetBodyDNABuffer({});
	UpdateCharacterIsFixedBodyType(InCharacter);
	InCharacter->MarkPackageDirty();

	// delete morph targets if there are any
	if (!CharacterData->BodyMesh->GetMorphTargets().IsEmpty())
	{
		CharacterData->BodyMesh->UnregisterAllMorphTarget();
	}

	// set the body mesh DNA back to the archetype
	UDNAAsset* BodyArchetypeDNA = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAAseet(EMetaHumanImportDNAType::Body, GetTransientPackage());
	check(BodyArchetypeDNA);
	ApplyBodyDNA(InCharacter, GetDnaReaderFromAsset(BodyArchetypeDNA));


	CharacterData->BodyMesh->PostEditChange();
	CharacterData->BodyMesh->MarkPackageDirty();

	// set LOD to LOD0
	UpdateCharacterLOD(InCharacter, EMetaHumanCharacterLOD::LOD0);
}

void UMetaHumanCharacterEditorSubsystem::AutoRigFace(TNotNull<UMetaHumanCharacter*> InCharacter, const UE::MetaHuman::ERigType InRigType)
{
	check(IsInGameThread());

	if (IsAutoRiggingFace(InCharacter))
	{
		return;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	// Prepare AutoRig parameters.
	TSharedPtr<IDNAReader> FaceDNAReader = UE::MetaHuman::GetDNAReader(CharacterData->FaceMesh);
	UE::MetaHuman::FTargetSolveParameters AutoRigParameters;
	const FMetaHumanCharacterIdentity::FState& FaceState = *CharacterData->FaceState;
	FMetaHumanCharacterEditorCloudRequests::InitFaceAutoRigParams(FaceState, FaceDNAReader.ToSharedRef(), AutoRigParameters);
	AutoRigParameters.RigType = InRigType;
	if (InRigType == UE::MetaHuman::ERigType::JointsAndBlendshapes)
	{
		AutoRigParameters.RigRefinementLevel = UE::MetaHuman::ERigRefinementLevel::Medium;
	}
	else
	{
		AutoRigParameters.RigRefinementLevel = UE::MetaHuman::ERigRefinementLevel::None;
	}

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests.FindOrAdd(InCharacter);

	CloudRequests.AutoRig = UE::MetaHuman::FAutoRigServiceRequest::CreateRequest(AutoRigParameters);
	CloudRequests.AutoRig->AutorigRequestCompleteDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceRequestCompleted, TObjectKey<UMetaHumanCharacter>(InCharacter), InRigType);
	CloudRequests.AutoRig->OnMetaHumanServiceRequestFailedDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceRequestFailed, TObjectKey<UMetaHumanCharacter>(InCharacter));
	CloudRequests.AutoRig->MetaHumanServiceRequestProgressDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceProgressUpdated, TObjectKey<UMetaHumanCharacter>(InCharacter));

	CloudRequests.AutoRiggingProgressHandle = FSlateNotificationManager::Get().StartProgressNotification(LOCTEXT("AutoRiggingProgress", "Auto-Rigging"), 100);
	CloudRequests.AutoRiggingNotificationItem = UE::MetaHuman::ShowNotification(LOCTEXT("StartAutoRiggingMessage", "Auto-Rigging Face Mesh"), SNotificationItem::ECompletionState::CS_Pending);
	
	// Fire the request.
	CloudRequests.AutoRig->RequestSolveAsync();
	CloudRequests.AutoRiggingStartTime = FPlatformTime::Seconds();

	InCharacter->OnRiggingStateChanged.Broadcast();

	UE::MetaHuman::Analytics::RecordRequestAutorigEvent(InCharacter, InRigType);
}

bool UMetaHumanCharacterEditorSubsystem::IsAutoRiggingFace(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	check(IsInGameThread());

	if (const FMetaHumanCharacterEditorCloudRequests* Requests = CharacterCloudRequests.Find(InCharacter))
	{
		return Requests->AutoRig.IsValid();
	}

	return false;
}

void UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceRequestCompleted(const UE::MetaHuman::FAutorigResponse& InResponse, TObjectKey<UMetaHumanCharacter> InCharacterKey, const UE::MetaHuman::ERigType InRigType)
{
	check(IsInGameThread());
	// The cloud request should exist, even if the character is no longer valid
	check(CharacterCloudRequests.Contains(InCharacterKey));

	using namespace UE::MetaHuman;

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	ON_SCOPE_EXIT
	{
		CloudRequests.AutoRiggingRequestFinished();

		if (!CloudRequests.HasActiveRequest())
		{
			CharacterCloudRequests.Remove(InCharacterKey);
		}
	};

	UMetaHumanCharacter* Character = InCharacterKey.ResolveObjectPtr();
	if (!Character || !IsObjectAddedForEditing(Character))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacterKey];

	const double ElapsedTime = FPlatformTime::Seconds() - CloudRequests.AutoRiggingStartTime;

	bool bDnaApplied = false;

	if (InResponse.IsValid())
	{
		FScopedSlowTask ApplyDNATask{ 0.0f, LOCTEXT("ApplyDNATask", "Applying DNA from Auto-Rigging service") };
		ApplyDNATask.MakeDialog();

		TArray<uint8> DNABuffer = Character->GetFaceDNABuffer();
		TSharedRef<FMetaHumanCharacterIdentity::FState> OriginalFaceState = CopyFaceState(Character);


		TSharedPtr<IDNAReader> OutDna = ApplyFaceDNA(Character, InResponse.Dna, ELodUpdateOption::All);

		if (OutDna.IsValid())
		{
			// Scope for the undo transactions
			{
				FScopedTransaction Transaction(AutoriggingTransactionContext, LOCTEXT("CharacterAutorigSuccessTransaction", "Apply Auto-rig"), Character);
				Character->Modify();

				// Store the DNA into the character asset
				Character->SetFaceDNABuffer(ReadStreamFromDNA(OutDna.Get(), EDNADataLayer::All), OutDna->GetBlendShapeChannelCount() > 0);
				Character->MarkPackageDirty();


				TUniquePtr<FAutoRigCommandChange> Change = MakeUnique<FAutoRigCommandChange>(
					DNABuffer,
					OriginalFaceState,
					Character);

				if (GUndo != nullptr)
				{
					GUndo->StoreUndo(Character, MoveTemp(Change));
				}
			}

			RunCharacterEditorPipelineForPreview(Character);
		}
		else
		{
			UE::MetaHuman::ShowNotification(LOCTEXT("AutoRiggingCheckDNAFailed", "Auto-Rigging failed due to incompatible DNA"), SNotificationItem::CS_Fail);
		}

		if (CVarMHCharacterSaveAutoRiggedDNA.GetValueOnAnyThread())
		{
			TSharedPtr<IDNAReader> BodyDna;
			USkeletalMesh* BodySkeletalMesh = CharacterData->BodyMesh;
			if (UDNAAsset* BodyDNA = BodySkeletalMesh->GetAssetUserData<UDNAAsset>())
			{
				BodyDna = BodyDNA->GetGeometryReader();
			}

			WriteDNAToFile(InResponse.Dna.Get(), EDNADataLayer::All, *(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanCharacterAutorig.dna"))));
			WriteDNAToFile(BodyDna.Get(), EDNADataLayer::All, *(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanCharacterBody.dna"))));
			WriteDNAToFile(OutDna.Get(), EDNADataLayer::All, *(FPaths::ProjectSavedDir() + FString(TEXT("MetaHumanCharacterAutorigApplied.dna"))));

		}
	}
	else
	{
		UE::MetaHuman::ShowNotification(LOCTEXT("AutoRiggingInvalidResponse", "Auto-Rigging failed due to invalid response from the service"), SNotificationItem::CS_Fail);
	}

	if (bDnaApplied)
	{
		const FText Message = FText::Format(LOCTEXT("AutoRiggingRequestCompleted", "Auto-Rigging finished in {0} seconds"), ElapsedTime);
		UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Success);
	}

	if (TSharedPtr<SNotificationItem> AutoRiggingNotificationItem = CloudRequests.AutoRiggingNotificationItem.Pin())
	{
		AutoRiggingNotificationItem->SetCompletionState(bDnaApplied ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
		AutoRiggingNotificationItem->ExpireAndFadeout();
	}

	Character->OnRiggingStateChanged.Broadcast();
}

void UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceProgressUpdated(float InPercentage, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	// The cloud request should exist, even if the character is no longer valid
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];
	FSlateNotificationManager::Get().UpdateProgressNotification(CloudRequests.AutoRiggingProgressHandle,
																100.0f * InPercentage);
}

void UMetaHumanCharacterEditorSubsystem::OnAutoRigFaceRequestFailed(EMetaHumanServiceRequestResult InResult, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	// The cloud request should exist, even if the character is no longer valid
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	// Close the notifications
	FSlateNotificationManager::Get().CancelProgressNotification(CloudRequests.AutoRiggingProgressHandle);
	if (TSharedPtr<SNotificationItem> AutoRiggingNotificationItem = CloudRequests.AutoRiggingNotificationItem.Pin())
	{
		AutoRiggingNotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		AutoRiggingNotificationItem->ExpireAndFadeout();
	}

	const FText Message = FText::Format(LOCTEXT("AutoRigFailedMessage", "Auto-Rigging of Face failed with code '{0}'"), UEnum::GetDisplayValueAsText(InResult));
	UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Fail);

	CloudRequests.AutoRiggingRequestFinished();

	if (!CloudRequests.HasActiveRequest())
	{
		CharacterCloudRequests.Remove(InCharacterKey);
	}

	UMetaHumanCharacter* Character = InCharacterKey.ResolveObjectPtr();
	if (!Character || !IsObjectAddedForEditing(Character))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	Character->OnRiggingStateChanged.Broadcast();
}

void UMetaHumanCharacterEditorSubsystem::ApplyEyelashesAndTeethPropertiesToFaceState(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties, const FMetaHumanCharacterTeethProperties& InTeethProperties, bool bInUpdateEyelashes, bool bInUpdateTeeth, ELodUpdateOption InUpdateOption) const
{
	if (bInUpdateEyelashes)
	{
		UpdateEyelashesVariantFromProperties(InCharacterData->FaceState, InEyelashesProperties);
	}

	if (bInUpdateTeeth)
	{
		// set the expression activations (add this also as a method of the TeethProperties)
		TMap<FString, float> ExpressionActivations;
	#if WITH_EDITOR // if in the editor tool, we only want to enable the show teeth expression when the tool is enabled
		if (InTeethProperties.EnableShowTeethExpression)
	#endif
		{
			ExpressionActivations.Add("jaw_open", InTeethProperties.JawOpen);
			ExpressionActivations.Add("McornerPull_Mstretch_MupperLipRaise_MlowerLipDepress_tgt", 1.0f);
		}
	#if WITH_EDITOR
		else

		{
			ExpressionActivations.Add("jaw_open", 0.0f);
			ExpressionActivations.Add("McornerPull_Mstretch_MupperLipRaise_MlowerLipDepress_tgt", 0.0f);
		}
	#endif
		InCharacterData->FaceState->SetExpressionActivations(ExpressionActivations);
		if (!InCharacterData->HeadModelSettings.IsSet() || InTeethProperties.IsVariantUpdated(InCharacterData->HeadModelSettings.GetValue().Teeth))
		{
			UpdateTeethVariantFromProperties(InCharacterData->FaceState, InTeethProperties);
		}
	}
	
	// Update the actor face skel mesh to apply eyelashes and/or teeth geometry.
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = InCharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(InCharacterData, FaceVerticesAndVertexNormals, InUpdateOption);
}


void UMetaHumanCharacterEditorSubsystem::UpdateEyelashesVariantFromProperties(TSharedRef<FMetaHumanCharacterIdentity::FState> InOutFaceState, const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties) const
{
	// Set the Eyelashes variant to the actor face state
	
	TArray<float> EyelashesVariantsWeights = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
	int32 EyelashesIndex = UE::MetaHuman::GetEyelashesVariantIndex(*InOutFaceState, InEyelashesProperties.Type);
	if (EyelashesIndex > INDEX_NONE)
	{
		EyelashesVariantsWeights[EyelashesIndex] = 1.0f;
	}
	InOutFaceState->SetVariant("eyelashes", EyelashesVariantsWeights);
}

void UMetaHumanCharacterEditorSubsystem::UpdateTeethVariantFromProperties(TSharedRef<FMetaHumanCharacterIdentity::FState> InOutFaceState, const FMetaHumanCharacterTeethProperties& InTeethProperties, bool bInUseExpressions) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdateTeethVariantFromProperties");

	// set the variant weights
	TArray<float> TeethVariantsWeights;
	TeethVariantsWeights.SetNumZeroed(InOutFaceState->GetVariantsCount("teeth"));

	const int32 VariationsStartIndex = 15;
	const int32 TeethCharacterCount = 8;

	// We need to include the default (archetype) character, since it has valid teeth data.
	float Value = InTeethProperties.Variation * TeethCharacterCount;
	float MinValue = FMath::Floor(Value);
	float MaxValue = MinValue + 1.0f;

	float HighValue = FMath::GetRangePct(MinValue, MaxValue, Value);
	float LowValue = 1.0f - HighValue;

	// Note that HigherIndex will be out of array bounds when Variation = 1.0 and
	// LowerIndex won't be teeth variation when Variation = 0.0, so don't use them
	// for array access.
	int32 LowerIndex = (int32)MinValue + VariationsStartIndex - 1;
	int32 HigherIndex = LowerIndex + 1;

	for (int32 CharIndex = VariationsStartIndex; CharIndex < VariationsStartIndex + TeethCharacterCount; CharIndex++)
	{
		if (CharIndex == LowerIndex && !FMath::IsNearlyZero(LowValue))
		{
			TeethVariantsWeights[CharIndex] = LowValue;
		}
		else if (CharIndex == HigherIndex && !FMath::IsNearlyZero(HighValue))
		{
			TeethVariantsWeights[CharIndex] = HighValue;
		}
		else
		{
			TeethVariantsWeights[CharIndex] = 0.0f;
		}
	}

	// update the properties
	const int32 ShortOption = 0;
	const int32 LongOption = 1;
	const int32 CrowdedOption = 2;
	const int32 SpacedOption = 3;
	const int32 UpperShiftRightOption = 4;
	const int32 UpperShiftLeftOption = 5;
	const int32 LowerShiftRightOption = 6;
	const int32 LowerShiftLeftOption = 7;
	const int32 UnderbiteOption = 8;
	const int32 OverbiteOption = 9;
	const int32 OverjetOption = 10;
	const int32 WornDownOption = 11;
	const int32 PolycanineOption = 12;
	const int32 RecedingGumsOption = 13;
	const int32 NarrowOption = 14;
	TeethVariantsWeights[ShortOption] = FMath::Clamp(-InTeethProperties.ToothLength, 0.0f, 1.0f);
	TeethVariantsWeights[LongOption] = FMath::Clamp(InTeethProperties.ToothLength, 0.0f, 1.0f);
	TeethVariantsWeights[CrowdedOption] = FMath::Clamp(-InTeethProperties.ToothSpacing, 0.0f, 1.0f);
	TeethVariantsWeights[SpacedOption] = FMath::Clamp(InTeethProperties.ToothSpacing, 0.0f, 1.0f);
	TeethVariantsWeights[UpperShiftRightOption] = FMath::Clamp(-InTeethProperties.UpperShift, 0.0f, 1.0f);
	TeethVariantsWeights[UpperShiftLeftOption] = FMath::Clamp(InTeethProperties.UpperShift, 0.0f, 1.0f);
	TeethVariantsWeights[LowerShiftRightOption] = FMath::Clamp(-InTeethProperties.LowerShift, 0.0f, 1.0f);
	TeethVariantsWeights[LowerShiftLeftOption] = FMath::Clamp(InTeethProperties.LowerShift, 0.0f, 1.0f);
	TeethVariantsWeights[UnderbiteOption] = FMath::Clamp(-InTeethProperties.Overbite, 0.0f, 1.0f);
	TeethVariantsWeights[OverbiteOption] = FMath::Clamp(InTeethProperties.Overbite, 0.0f, 1.0f);
	TeethVariantsWeights[OverjetOption] = InTeethProperties.Overjet;
	TeethVariantsWeights[WornDownOption] = InTeethProperties.WornDown;
	TeethVariantsWeights[PolycanineOption] = InTeethProperties.Polycanine;
	TeethVariantsWeights[RecedingGumsOption] = InTeethProperties.RecedingGums;
	TeethVariantsWeights[NarrowOption] = InTeethProperties.Narrowness;

	InOutFaceState->SetVariant("teeth", TeethVariantsWeights);

}


void UMetaHumanCharacterEditorSubsystem::ApplySkinPropertiesToFaceState(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, const FMetaHumanCharacterSkinProperties& InSkinProperties) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplySkinPropertiesToFaceState");

	UpdateHFVariantFromSkinProperties(InCharacterData->FaceState, InSkinProperties);

	// Update the actor face skel mesh
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = InCharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(InCharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());
}

void UMetaHumanCharacterEditorSubsystem::UpdateHFVariantFromSkinProperties(TSharedRef<FMetaHumanCharacterIdentity::FState> InOutFaceState, const FMetaHumanCharacterSkinProperties& InSkinProperties) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdateHFVariantFromSkinProperties");

	const int32 HighFrequencyIndex = FMath::Clamp(InSkinProperties.FaceTextureIndex, 0, FaceTextureSynthesizer.GetMaxHighFrequencyIndex() - 1);

	// Set the HF variant to the actor face state
	InOutFaceState->SetHighFrequenctVariant(UE::MetaHuman::MapTextureHFToStateHFIndex(*InOutFaceState, HighFrequencyIndex));
}

TSharedRef<const FDNAToSkelMeshMap> UMetaHumanCharacterEditorSubsystem::GetFaceDnaToSkelMeshMap(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->FaceDnaToSkelMeshMap;
}

FSimpleMulticastDelegate& UMetaHumanCharacterEditorSubsystem::OnFaceStateChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->OnFaceStateChangedDelegate;
}

TWeakObjectPtr<UTexture2D> UMetaHumanCharacterEditorSubsystem::GetOrCreateSkinToneTexture()
{
	if (!SkinToneTexture.IsValid())
	{
		const int32 TextureSize = 256;

		TArray<FColor> SkinToneColorData;
		SkinToneColorData.Reserve(TextureSize * TextureSize);

		const FVector2f SkinToneTextureSize(TextureSize, TextureSize);

		// Generates the texture with the skin tones the user can use to pick up
		for (int32 Y = 0; Y < TextureSize; ++Y)
		{
			for (int32 X = 0; X < TextureSize; ++X)
			{				
				// The skin tone color is already in sRGB so don't perform the conversion here
				const bool bSRGB = false;
				const FVector2f UV = FVector2f(X, Y) / SkinToneTextureSize;
				SkinToneColorData.Add(FaceTextureSynthesizer.GetSkinTone(UV).ToFColor(bSRGB));
			}
		}

		
		const FString TextureName = MakeUniqueObjectName(this, UTexture2D::StaticClass(), TEXT("SkinToneTexture")).ToString();
		FCreateTexture2DParameters CreateTextureParams;
		CreateTextureParams.bSRGB = true;
		SkinToneTexture = FImageUtils::CreateTexture2D(TextureSize, TextureSize, SkinToneColorData, this, TextureName, RF_NoFlags, CreateTextureParams);
	}

	return SkinToneTexture.Get();
}

FVector2f UMetaHumanCharacterEditorSubsystem::EstimateSkinTone(const FLinearColor& InSkinTone) const
{
	return FaceTextureSynthesizer.ProjectSkinTone(InSkinTone);
}

int32 UMetaHumanCharacterEditorSubsystem::GetMaxHighFrequencyIndex() const
{
	return FaceTextureSynthesizer.GetMaxHighFrequencyIndex();
}

void UMetaHumanCharacterEditorSubsystem::UpdateCharacterPreviewMaterial(TNotNull<UMetaHumanCharacter*> InCharacter, EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterial)
{
	InCharacter->PreviewMaterialType = InPreviewMaterial;
	InCharacter->MarkPackageDirty();

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	UpdateActorsSkinPreviewMaterial(CharacterData, InPreviewMaterial);

	// When switching to editable, ensure that all textures are updated for the material
	// Also consider the clay material to apply the normal maps in the material
	if (InPreviewMaterial == EMetaHumanCharacterSkinPreviewMaterial::Editable ||
		InPreviewMaterial == EMetaHumanCharacterSkinPreviewMaterial::Clay)
	{
		const FMetaHumanCharacterSkinTextureSet FinalSkinTextureSet =
			InCharacter->SkinSettings.GetFinalSkinTextureSet(FMetaHumanCharacterSkinTextureSet
				{
					.Face = InCharacter->GetValidFaceTextures(),
					.Body = InCharacter->BodyTextures
				});

		const FMetaHumanCharacterSkinSettings& SkinSettings = CharacterData->SkinSettings.IsSet() ? CharacterData->SkinSettings.GetValue() : InCharacter->SkinSettings;
		UpdateSkinTextures(CharacterData, SkinSettings.Skin, FinalSkinTextureSet);

		ApplyMakeupSettings(CharacterData, InCharacter->MakeupSettings);
		ApplyEyesSettings(CharacterData, InCharacter->EyesSettings);
		FMetaHumanCharacterSkinMaterials::ApplySkinParametersToMaterials(CharacterData->HeadMaterials, CharacterData->BodyMaterial, SkinSettings);
		FMetaHumanCharacterSkinMaterials::ApplyEyelashesPropertiesToMaterial(CharacterData->HeadMaterials, InCharacter->HeadModelSettings.Eyelashes);
		FMetaHumanCharacterSkinMaterials::ApplyTeethPropertiesToMaterial(CharacterData->HeadMaterials, InCharacter->HeadModelSettings.Teeth);
	}

	UpdateCharacterPreviewMaterialBodyHiddenFacesMask(InCharacter);
}

void UMetaHumanCharacterEditorSubsystem::StoreSynthesizedTextures(TNotNull<UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	if (!InCharacter->HasHighResolutionTextures())
	{
		// Store the textures as compressed data in the character asset
		for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
		{
			if (CharacterData->CachedSynthesizedImages.Contains(TextureType))
			{
				InCharacter->StoreSynthesizedFaceTexture(TextureType, CharacterData->CachedSynthesizedImages[TextureType]);
			}
		}
	}
}

void UMetaHumanCharacterEditorSubsystem::UpdateActorsSkinPreviewMaterial(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, EMetaHumanCharacterSkinPreviewMaterial InPreviewMaterialType)
{
	FMetaHumanCharacterFaceMaterialSet HeadMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadPreviewMaterialInstance(InPreviewMaterialType);

	UMaterialInstanceDynamic* BodyPreviewMaterialInstance = FMetaHumanCharacterSkinMaterials::GetBodyPreviewMaterialInstance(InPreviewMaterialType);
	check(BodyPreviewMaterialInstance);

	if (InPreviewMaterialType == EMetaHumanCharacterSkinPreviewMaterial::Clay)
	{
		// The Clay material is just a parameter of the full editable material, so just enable it here
		HeadMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
			[](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
			{
				Material->SetScalarParameterValue(TEXT("ClayMaterial"), 1.0f);
			}
		);

		BodyPreviewMaterialInstance->SetScalarParameterValue(TEXT("ClayMaterial"), 1.0f);
	}

	InCharacterData->HeadMaterials = HeadMaterialSet;
	InCharacterData->BodyMaterial = BodyPreviewMaterialInstance;

	FMetaHumanCharacterSkinMaterials::SetHeadMaterialsOnMesh(InCharacterData->HeadMaterials, InCharacterData->FaceMesh);
	FMetaHumanCharacterSkinMaterials::SetBodyMaterialOnMesh(InCharacterData->BodyMaterial, InCharacterData->BodyMesh);

	ForEachCharacterActor(InCharacterData, [](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
		{
			Actor->OnFaceMeshUpdated();
			Actor->OnBodyMeshUpdated();
		});
}

void UMetaHumanCharacterEditorSubsystem::CommitSkinSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterSkinSettings& InSkinSettings)
{
	InCharacter->SkinSettings = InSkinSettings;
	FMetaHumanCharacterBodyTextureUtils::UpdateBodySkinBiasGain(FaceTextureSynthesizer, InCharacter->SkinSettings.Skin);
	InCharacter->MarkPackageDirty();

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	ApplySkinSettings(InCharacter, InCharacter->SkinSettings);
	StoreSynthesizedTextures(InCharacter);
}

void UMetaHumanCharacterEditorSubsystem::RequestHighResolutionTextures(TNotNull<UMetaHumanCharacter*> InCharacter, ERequestTextureResolution InResolution)
{
	check(IsInGameThread());

	if (IsRequestingHighResolutionTextures(InCharacter))
	{
		return;
	}

	FMetaHumanCharacterSkinProperties& SkinProperties = InCharacter->SkinSettings.Skin;

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests.FindOrAdd(InCharacter);

	// Set up face request
	const FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams TextureSynthesisParams = FMetaHumanCharacterTextureSynthesis::SkinPropertiesToSynthesizerParams(SkinProperties, FaceTextureSynthesizer);
	UE::MetaHuman::FFaceTextureRequestCreateParams FaceTextureRequestCreateParams;
	FaceTextureRequestCreateParams.HighFrequency = TextureSynthesisParams.HighFrequencyIndex;
	CloudRequests.TextureSynthesis = UE::MetaHuman::FFaceTextureSynthesisServiceRequest::CreateRequest(FaceTextureRequestCreateParams);

	CloudRequests.TextureSynthesis->FaceTextureSynthesisRequestCompleteDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesRequestCompleted, TObjectKey<UMetaHumanCharacter>(InCharacter));
	CloudRequests.TextureSynthesis->OnMetaHumanServiceRequestFailedDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesRequestFailed, TObjectKey<UMetaHumanCharacter>(InCharacter));
	CloudRequests.TextureSynthesis->MetaHumanServiceRequestProgressDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesProgressUpdated, TObjectKey<UMetaHumanCharacter>(InCharacter));

	// The request completion delegates will be called when *all* textures are downloaded
	const TArray<UE::MetaHuman::FFaceTextureRequestParams, TInlineAllocator<static_cast<int32>(EFaceTextureType::Count)>> FaceTextureTypesToRequest =
	{
		{ EFaceTextureType::Basecolor, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Basecolor_Animated_CM1, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Basecolor_Animated_CM2, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Basecolor_Animated_CM3, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Normal, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Normal_Animated_WM1, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Normal_Animated_WM2, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Normal_Animated_WM3, static_cast<int32>(InResolution) },
		{ EFaceTextureType::Cavity, static_cast<int32>(InResolution) },
	};

	const int32 NumSteps = 100;
	CloudRequests.TextureSynthesisProgressHandle = FSlateNotificationManager::Get().StartProgressNotification(LOCTEXT("DownloadSourceFaceTextures", "Downloading source face textures"), NumSteps);
	CloudRequests.TextureSynthesisNotificationItem = UE::MetaHuman::ShowNotification(LOCTEXT("StartSourceFaceTexturesDownload", "Downloading source face textures"), SNotificationItem::ECompletionState::CS_Pending);

	// Set up body request
	UE::MetaHuman::FBodyTextureRequestCreateParams BodyTextureRequestCreateParams;
	BodyTextureRequestCreateParams.Tone = FMetaHumanCharacterBodyTextureUtils::GetSkinToneIndex(SkinProperties);
	BodyTextureRequestCreateParams.SurfaceMap = FMetaHumanCharacterBodyTextureUtils::GetBodySurfaceMapId(SkinProperties);
	CloudRequests.BodyTextures = UE::MetaHuman::FBodyTextureSynthesisServiceRequest::CreateRequest(BodyTextureRequestCreateParams);

	CloudRequests.BodyTextures->BodyTextureSynthesisRequestCompleteDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesRequestCompleted, TObjectKey<UMetaHumanCharacter>(InCharacter));
	CloudRequests.BodyTextures->OnMetaHumanServiceRequestFailedDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesRequestFailed, TObjectKey<UMetaHumanCharacter>(InCharacter));
	CloudRequests.BodyTextures->MetaHumanServiceRequestProgressDelegate.BindUObject(this, &UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesProgressUpdated, TObjectKey<UMetaHumanCharacter>(InCharacter));

	// The request completion delegates will be called when *all* textures are downloaded
	const TArray<UE::MetaHuman::FBodyTextureRequestParams, TInlineAllocator<static_cast<int32>(EBodyTextureType::Count)>> BodyTextureTypesToRequest =
		{
		{ EBodyTextureType::Body_Basecolor, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Body_Normal, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Body_Cavity, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Body_Underwear_Basecolor, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Body_Underwear_Normal, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Body_Underwear_Mask, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Chest_Basecolor, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Chest_Normal, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Chest_Cavity, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Chest_Underwear_Basecolor, static_cast<int32>(InResolution) },
		{ EBodyTextureType::Chest_Underwear_Normal, static_cast<int32>(InResolution) },
	};

	CloudRequests.BodyTextureProgressHandle = FSlateNotificationManager::Get().StartProgressNotification(LOCTEXT("DownloadSourceBodyTextures", "Downloading source body textures"), NumSteps);
	CloudRequests.BodyTextureNotificationItem = UE::MetaHuman::ShowNotification(LOCTEXT("StartSourceBodyTexturesDownload", "Downloading source body textures"), SNotificationItem::ECompletionState::CS_Pending);

	// Make requests
	OnDownloadingTexturesStateChanged.Broadcast(InCharacter);
	CloudRequests.TextureSynthesisStartTime = FPlatformTime::Seconds();	
	CloudRequests.TextureSynthesis->RequestTexturesAsync(MakeConstArrayView<UE::MetaHuman::FFaceTextureRequestParams>(FaceTextureTypesToRequest.GetData(), FaceTextureTypesToRequest.Num()));

	CloudRequests.BodyTextureStartTime = FPlatformTime::Seconds();
	CloudRequests.BodyTextures->RequestTexturesAsync(MakeConstArrayView<UE::MetaHuman::FBodyTextureRequestParams>(BodyTextureTypesToRequest.GetData(), BodyTextureTypesToRequest.Num()));

	UE::MetaHuman::Analytics::RecordRequestHighResolutionTexturesEvent(InCharacter, InResolution);
}

bool UMetaHumanCharacterEditorSubsystem::IsRequestingHighResolutionTextures(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	check(IsInGameThread());

	if (const FMetaHumanCharacterEditorCloudRequests* Requests = CharacterCloudRequests.Find(InCharacter))
	{
		return Requests->TextureSynthesis.IsValid() || Requests->BodyTextures.IsValid();
	}

	return false;
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesRequestCompleted(TSharedPtr<UE::MetaHuman::FFaceHighFrequencyData> InResponse, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	//NOTE: This delegate is *only* invoked if the TS download is complete and all the images have been recieved. Hence it can assert that the image data is present and valid
	//		If any of the downloaded images are found to be incorrect (for whatever reason) this delegate is never invoked
	
	check(IsInGameThread());
	// The cloud request should exist, even if the character is no longer valid
	check(CharacterCloudRequests.Contains(InCharacterKey));

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	ON_SCOPE_EXIT
	{
		CloudRequests.TextureSynthesisRequestFinished();

		if (!CloudRequests.HasActiveRequest())
		{
			OnDownloadingTexturesStateChanged.Broadcast(InCharacterKey.ResolveObjectPtr());
			CharacterCloudRequests.Remove(InCharacterKey);
		}
	};

	UMetaHumanCharacter* InMetaHumanCharacter = InCharacterKey.ResolveObjectPtr();
	if (!InMetaHumanCharacter || !IsObjectAddedForEditing(InMetaHumanCharacter))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];

	// Calculate how much time it took to get the textures
	const double ElapsedTime = FPlatformTime::Seconds() - CloudRequests.TextureSynthesisStartTime;
	
	if (FMetaHumanCharacterEditorCloudRequests::GenerateTexturesFromResponse(InResponse, FaceTextureSynthesizer, CharacterData, InMetaHumanCharacter))
	{
		// Build a texture set with only the face textures considering their overrides
		const FMetaHumanCharacterSkinTextureSet FinalFaceTextureSet =
			InMetaHumanCharacter->SkinSettings.GetFinalSkinTextureSet(FMetaHumanCharacterSkinTextureSet
				{
					.Face = InMetaHumanCharacter->GetValidFaceTextures()
				});

		UpdateSkinTextures(CharacterData, InMetaHumanCharacter->SkinSettings.Skin, FinalFaceTextureSet);
	}

	const FText Message = FText::Format(LOCTEXT("DownloadSourceFaceTexturesCompleted", "Download of source face textures finished in {0} seconds"), ElapsedTime);
	UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Success);

	if (TSharedPtr<SNotificationItem> TextureSynthesisNotificationItem = CloudRequests.TextureSynthesisNotificationItem.Pin())
	{
		TextureSynthesisNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		TextureSynthesisNotificationItem->ExpireAndFadeout();
	}
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesRequestFailed(EMetaHumanServiceRequestResult InResult, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	FText Message;
	if(InResult != EMetaHumanServiceRequestResult::Unauthorized)
	{
		Message = FText::Format(LOCTEXT("DownloadFailedMessage", "Download of source face textures failed with code '{0}'"), UEnum::GetDisplayValueAsText(InResult));
	}
	else
	{
		Message = LOCTEXT("DownloadLoginFailedMessage", "User not logged in, please autorig before downloading source face textures");
	}
	UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Fail);
	FSlateNotificationManager::Get().CancelProgressNotification(CloudRequests.TextureSynthesisProgressHandle);

	// always try to stop the "spinner"
	if (TSharedPtr<SNotificationItem> TextureSynthesisNotificationItem = CloudRequests.TextureSynthesisNotificationItem.Pin())
	{
		TextureSynthesisNotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		TextureSynthesisNotificationItem->ExpireAndFadeout();
	}

	CloudRequests.TextureSynthesisRequestFinished();
	if (!CloudRequests.HasActiveRequest())
	{
		OnDownloadingTexturesStateChanged.Broadcast(InCharacterKey.ResolveObjectPtr());
		CharacterCloudRequests.Remove(InCharacterKey);
	}
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionTexturesProgressUpdated(float InPercentage, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	UMetaHumanCharacter* Character = InCharacterKey.ResolveObjectPtr();
	if (!Character || !IsObjectAddedForEditing(Character))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	float Percentage = InPercentage * 100.f;
	FSlateNotificationManager::Get().UpdateProgressNotification(CloudRequests.TextureSynthesisProgressHandle, Percentage);
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesRequestCompleted(TSharedPtr<UE::MetaHuman::FBodyHighFrequencyData> InResponse, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	//NOTE: This delegate is *only* invoked if the TS download is complete and all the images have been recieved. Hence it can assert that the image data is present and valid
	//		If any of the downloaded images are found to be incorrect (for whatever reason) this delegate is never invoked
	
	check(IsInGameThread());
	// The cloud request should exist, even if the character is no longer valid
	check(CharacterCloudRequests.Contains(InCharacterKey));

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	ON_SCOPE_EXIT
	{
		CloudRequests.BodyTextureRequestFinished();

		if (!CloudRequests.HasActiveRequest())
		{
			OnDownloadingTexturesStateChanged.Broadcast(InCharacterKey.ResolveObjectPtr());
			CharacterCloudRequests.Remove(InCharacterKey);
		}
	};

	UMetaHumanCharacter* InMetaHumanCharacter = InCharacterKey.ResolveObjectPtr();
	if (!InMetaHumanCharacter || !IsObjectAddedForEditing(InMetaHumanCharacter))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];

	// Calculate how much time it took to get the textures
	const double ElapsedTime = FPlatformTime::Seconds() - CloudRequests.BodyTextureStartTime;

	if (FMetaHumanCharacterEditorCloudRequests::GenerateBodyTexturesFromResponse(InResponse, InMetaHumanCharacter))
	{
		// Build a texture set with only the body textures considering their overrides
		const FMetaHumanCharacterSkinTextureSet FinalFaceTextureSet =
			InMetaHumanCharacter->SkinSettings.GetFinalSkinTextureSet(FMetaHumanCharacterSkinTextureSet
				{
					.Body = InMetaHumanCharacter->BodyTextures
				});

		// Update the Body Material Parameters
		FMetaHumanCharacterBodyTextureUtils::GetSkinToneAndUpdateMaterials( InMetaHumanCharacter->SkinSettings.Skin,
																		   FaceTextureSynthesizer,
																		   FinalFaceTextureSet.Body,
																		   CharacterData->HeadMaterials,
																		   CharacterData->BodyMaterial);
	}

	const FText Message = FText::Format(LOCTEXT("DownloadSourceBodyTexturesCompleted", "Download of source body textures finished in {0} seconds"), ElapsedTime);
	UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Success);

	if (TSharedPtr<SNotificationItem> BodyTextureNotificationItem = CloudRequests.BodyTextureNotificationItem.Pin())
	{
		BodyTextureNotificationItem->SetCompletionState(SNotificationItem::CS_Success);
		BodyTextureNotificationItem->ExpireAndFadeout();
	}
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesRequestFailed(EMetaHumanServiceRequestResult InResult, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	FText Message;
	if (InResult != EMetaHumanServiceRequestResult::Unauthorized)
	{
		Message = FText::Format(LOCTEXT("DownloadBodyFailedMessage", "Download of source body textures failed with code '{0}'"), UEnum::GetDisplayValueAsText(InResult));
	}
	else
	{
		Message = LOCTEXT("DownloadBodyLoginFailedMessage", "User not logged in, please autorig before downloading source body textures");
	}
	UE::MetaHuman::ShowNotification(Message, SNotificationItem::ECompletionState::CS_Fail);
	FSlateNotificationManager::Get().CancelProgressNotification(CloudRequests.BodyTextureProgressHandle);

	// always try to stop the "spinner"
	if (TSharedPtr<SNotificationItem> BodyTextureNotificationItem = CloudRequests.BodyTextureNotificationItem.Pin())
	{
		BodyTextureNotificationItem->SetCompletionState(SNotificationItem::CS_Fail);
		BodyTextureNotificationItem->ExpireAndFadeout();
	}

	CloudRequests.BodyTextureRequestFinished();
	if (!CloudRequests.HasActiveRequest())
	{
		OnDownloadingTexturesStateChanged.Broadcast(InCharacterKey.ResolveObjectPtr());
		CharacterCloudRequests.Remove(InCharacterKey);
	}
}

void UMetaHumanCharacterEditorSubsystem::OnHighResolutionBodyTexturesProgressUpdated(float InPercentage, TObjectKey<UMetaHumanCharacter> InCharacterKey)
{
	check(CharacterCloudRequests.Contains(InCharacterKey));
	check(IsInGameThread());

	FMetaHumanCharacterEditorCloudRequests& CloudRequests = CharacterCloudRequests[InCharacterKey];

	UMetaHumanCharacter* Character = InCharacterKey.ResolveObjectPtr();
	if (!Character || !IsObjectAddedForEditing(Character))
	{
		// Character has been forcibly deleted or the asset editor was closed
		return;
	}

	float Percentage = InPercentage * 100.f;
	FSlateNotificationManager::Get().UpdateProgressNotification(CloudRequests.BodyTextureProgressHandle, Percentage);
}

void UMetaHumanCharacterEditorSubsystem::ApplyFaceEvaluationSettings(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplyFaceEvaluationSettings");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];

	FMetaHumanCharacterIdentity::FSettings Settings = CharacterData->FaceState->GetSettings();
	Settings.SetGlobalVertexDeltaScale(InFaceEvaluationSettings.GlobalDelta);
	Settings.SetGlobalHighFrequencyScale(InFaceEvaluationSettings.HighFrequencyDelta);
	CharacterData->FaceState->SetSettings(Settings);
	CharacterData->FaceState->SetFaceScale(InFaceEvaluationSettings.HeadScale);
	
	// Update the actor face skel mesh to apply face settings
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = CharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(CharacterData, FaceVerticesAndVertexNormals, GetUpdateOptionForEditing());

	CharacterData->FaceEvaluationSettings = InFaceEvaluationSettings;
}

void UMetaHumanCharacterEditorSubsystem::CommitFaceEvaluationSettings(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterFaceEvaluationSettings& InFaceEvaluationSettings)
{
	InMetaHumanCharacter->FaceEvaluationSettings = InFaceEvaluationSettings;
	InMetaHumanCharacter->MarkPackageDirty();

	ApplyFaceEvaluationSettings(InMetaHumanCharacter, InMetaHumanCharacter->FaceEvaluationSettings);
}

const FMetaHumanFaceTextureAttributeMap& UMetaHumanCharacterEditorSubsystem::GetFaceTextureAttributeMap() const
{
	return FaceTextureSynthesizer.GetFaceTextureAttributeMap();
}

void UMetaHumanCharacterEditorSubsystem::ApplyHeadModelSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplyHeadModelSettings");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	const bool bUpdateEyelashesVariant = !CharacterData->HeadModelSettings.IsSet() || CharacterData->HeadModelSettings.GetValue().Eyelashes.Type != InHeadModelSettings.Eyelashes.Type;
	const bool bUpdateTeethVariant = !CharacterData->HeadModelSettings.IsSet() || InHeadModelSettings.Teeth != CharacterData->HeadModelSettings.GetValue().Teeth;
	const bool bUpdateEyelashesMaterials = !CharacterData->HeadModelSettings.IsSet() || InHeadModelSettings.Eyelashes.AreMaterialsUpdated(CharacterData->HeadModelSettings.GetValue().Eyelashes);
	const bool bUpdateTeethMaterials = !CharacterData->HeadModelSettings.IsSet() || InHeadModelSettings.Teeth.AreMaterialsUpdated(CharacterData->HeadModelSettings.GetValue().Teeth);
	const bool bToggleEyelashesGrooms = !CharacterData->HeadModelSettings.IsSet() || CharacterData->HeadModelSettings.GetValue().Eyelashes.bEnableGrooms != InHeadModelSettings.Eyelashes.bEnableGrooms;
	if (bUpdateEyelashesVariant || bUpdateTeethVariant)
	{
		// ensure that we only call EvaluateState once by combining eyelash and teeth updates
		ApplyEyelashesAndTeethPropertiesToFaceState(CharacterData, InHeadModelSettings.Eyelashes, InHeadModelSettings.Teeth, bUpdateEyelashesVariant, bUpdateTeethVariant, GetUpdateOptionForEditing());
	}
	if (bUpdateEyelashesMaterials || bUpdateEyelashesVariant)
	{
		FMetaHumanCharacterSkinMaterials::ApplyEyelashesPropertiesToMaterial(CharacterData->HeadMaterials, InHeadModelSettings.Eyelashes);
	}
	if ((bUpdateEyelashesVariant && InHeadModelSettings.Eyelashes.bEnableGrooms) || bToggleEyelashesGrooms)
	{
		ToggleEyelashesGrooms(InCharacter, InHeadModelSettings.Eyelashes);
	}
	if (bUpdateTeethMaterials)
	{
		FMetaHumanCharacterSkinMaterials::ApplyTeethPropertiesToMaterial(CharacterData->HeadMaterials, InHeadModelSettings.Teeth);
	}
	CharacterData->HeadModelSettings = InHeadModelSettings;
}

void UMetaHumanCharacterEditorSubsystem::CommitHeadModelSettings(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterHeadModelSettings& InHeadModelSettings)
{
	InMetaHumanCharacter->HeadModelSettings = InHeadModelSettings;
	InMetaHumanCharacter->MarkPackageDirty();
	
	ApplyHeadModelSettings(InMetaHumanCharacter, InMetaHumanCharacter->HeadModelSettings);
}

void UMetaHumanCharacterEditorSubsystem::ToggleEyelashesGrooms(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterEyelashesProperties& InEyelashesProperties) const
{
	const FName SlotName = TEXT("Eyelashes");
	TNotNull<UMetaHumanCollection*> Collection = InMetaHumanCharacter->GetMutableInternalCollection();

	// Check if we have requested slot
	const FMetaHumanCharacterPipelineSlot* Slot = Collection->GetPipeline()->GetSpecification()->Slots.Find(SlotName);
	if (!Slot)
	{
		// Slot not found.
		return;
	}

	if (InEyelashesProperties.Type != EMetaHumanCharacterEyelashesType::None && InEyelashesProperties.bEnableGrooms)
	{
		const UMetaHumanCharacterEditorWardrobeSettings* WardrobeSettings = GetDefault<UMetaHumanCharacterEditorWardrobeSettings>();
		if (const FSoftObjectPath* FoundBinding = WardrobeSettings->EyelashesTypeToAssetPath.Find(InEyelashesProperties.Type))
		{
			// First check if the asset already exists.
			const FMetaHumanCharacterPaletteItem* FoundItem = Collection->GetItems().FindByPredicate(
				[FoundBinding, SlotName](const FMetaHumanCharacterPaletteItem& Item)
				{
					return Item.SlotName == SlotName
						&& Item.WardrobeItem
						&& Item.WardrobeItem->IsExternal()
						&& FSoftObjectPath(Item.WardrobeItem) == *FoundBinding;
				});


			FMetaHumanPaletteItemKey PaletteItemKey;
			if (FoundItem)
			{
				// Eyelashes groom is already attached.
				PaletteItemKey = FoundItem->GetItemKey();
			}
			else
			{
				TSoftObjectPtr<UMetaHumanWardrobeItem> WordrobeItemRef{ *FoundBinding };
				if (UMetaHumanWardrobeItem* WardrobeItem = WordrobeItemRef.LoadSynchronous())
				{
					if (!Collection->TryAddItemFromWardrobeItem(SlotName, WardrobeItem, PaletteItemKey))
					{
						UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to add eyelashes groom {WardrobeItem}", GetFullNameSafe(WardrobeItem));
						return;
					}
				}
			}
			Collection->GetMutableDefaultInstance()->SetSingleSlotSelection(SlotName, PaletteItemKey);
		}
	}
	else
	{
		Collection->GetMutableDefaultInstance()->SetSingleSlotSelection(SlotName, FMetaHumanPaletteItemKey());
	}

	RunCharacterEditorPipelineForPreview(InMetaHumanCharacter);
}


void UMetaHumanCharacterEditorSubsystem::ApplySkinSettings(
	TNotNull<UMetaHumanCharacter*> InCharacter,
	const FMetaHumanCharacterSkinSettings& InSkinSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplySkinSettings");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	// Update texture set in character if changed
	FMetaHumanCharacterBodyTextureUtils::UpdateBodyTextureSet(CharacterData->SkinSettings, InSkinSettings.Skin, InCharacter->HighResBodyTexturesInfo, InCharacter->BodyTextures);

	// Build a texture set considering any overrides in the skin settings
	const FMetaHumanCharacterSkinTextureSet FinalSkinTextureSet =
		InSkinSettings.GetFinalSkinTextureSet(FMetaHumanCharacterSkinTextureSet
											  {
												  .Face = InCharacter->GetValidFaceTextures(),
												  .Body = InCharacter->BodyTextures
											  });

	const bool bForceUseExistingTextures = false;
	bool bTexturesHaveBeenRegenerated = false;
	ApplySkinSettings(CharacterData, InSkinSettings, bForceUseExistingTextures, FinalSkinTextureSet, InCharacter->SynthesizedFaceTextures, InCharacter->BodyTextures,bTexturesHaveBeenRegenerated);
	ApplyMakeupSettings(CharacterData, InCharacter->MakeupSettings);

	if (bTexturesHaveBeenRegenerated)
	{
		InCharacter->SetHasHighResolutionTextures(false);
		InCharacter->ResetUnreferencedHighResTextureData();
	}

	// Update the Sclera tint based on the skin tone if needed and reapply the eye settings
	FMetaHumanCharacterSkinMaterials::ApplyEyeScleraTintBasedOnSkinTone(InSkinSettings, InCharacter->EyesSettings);
	ApplyEyesSettings(CharacterData, InCharacter->EyesSettings);
}

void UMetaHumanCharacterEditorSubsystem::ApplySkinSettings(
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
	const FMetaHumanCharacterSkinSettings& InSkinSettings,
	bool bInForceUseExistingTextures,
	const FMetaHumanCharacterSkinTextureSet& InFinalSkinTextureSet,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InOutSynthesizedFaceTextures,
	TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& InOutBodyTextures,
	bool& bOutTexturesHaveBeenRegenerated) const
{
	bOutTexturesHaveBeenRegenerated = false;

	// If the properties that affect texture synthesis have changed, re-run TS now. Any high
	// res textures that have been downloaded will be discarded.
	//
	// Callers should detect when high res textures will be discarded and prompt the user to
	// confirm before calling this function.
	if (bInForceUseExistingTextures
		|| (InCharacterData->SkinSettings.IsSet()
			&& InCharacterData->SkinSettings.GetValue().Skin == InSkinSettings.Skin))
	{
		UpdateSkinTextures(InCharacterData, InSkinSettings.Skin, InFinalSkinTextureSet);
	}
	else if (FaceTextureSynthesizer.IsValid())
	{
		ApplySkinProperties(InCharacterData, InSkinSettings.Skin, InOutSynthesizedFaceTextures, InOutBodyTextures);
		bOutTexturesHaveBeenRegenerated = true;
	}

	// Apply the skin material parameters to the face and body materials
	FMetaHumanCharacterSkinMaterials::ApplySkinParametersToMaterials(InCharacterData->HeadMaterials, InCharacterData->BodyMaterial, InSkinSettings);

	InCharacterData->SkinSettings = InSkinSettings;
}

void UMetaHumanCharacterEditorSubsystem::UpdateSkinTextures(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
															const FMetaHumanCharacterSkinProperties& InSkinProperties,
															const FMetaHumanCharacterSkinTextureSet& InSkinTextureSet) const
{
	// Set the face textures to the face material
	FMetaHumanCharacterSkinMaterials::ApplySynthesizedTexturesToFaceMaterial(InCharacterData->HeadMaterials, InSkinTextureSet.Face);

	// Update the Body Material Parameters to match
	FMetaHumanCharacterBodyTextureUtils::GetSkinToneAndUpdateMaterials(InSkinProperties,
																	   FaceTextureSynthesizer,
																	   InSkinTextureSet.Body,
																	   InCharacterData->HeadMaterials,
																	   InCharacterData->BodyMaterial);
}

void UMetaHumanCharacterEditorSubsystem::ApplyEyesSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterEyesSettings& InEyesSettings) const
{
	check(CharacterDataMap.Contains(InCharacter));
	ApplyEyesSettings(CharacterDataMap[InCharacter], InEyesSettings);

	// Reapply the skin settings to update the Sclera tint based on the skin tone if needed
	ApplySkinSettings(InCharacter, InCharacter->SkinSettings);
}

void UMetaHumanCharacterEditorSubsystem::ApplyEyesSettings(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, const FMetaHumanCharacterEyesSettings& InEyesSettings)
{
	FMetaHumanCharacterSkinMaterials::ApplyEyeSettingsToMaterial(InCharacterData->HeadMaterials, InEyesSettings);
}

void UMetaHumanCharacterEditorSubsystem::CommitEyesSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterEyesSettings& InEyesSettings) const
{
	InCharacter->EyesSettings = InEyesSettings;
	InCharacter->MarkPackageDirty();

	ApplyEyesSettings(InCharacter, InCharacter->EyesSettings);
}

void UMetaHumanCharacterEditorSubsystem::ApplyMakeupSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterMakeupSettings& InMakeupSettings) const
{
	check(CharacterDataMap.Contains(InCharacter));
	ApplyMakeupSettings(CharacterDataMap[InCharacter], InMakeupSettings);
}

void UMetaHumanCharacterEditorSubsystem::ApplyMakeupSettings(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, const FMetaHumanCharacterMakeupSettings& InMakeupSettings)
{
	FMetaHumanCharacterSkinMaterials::ApplyFoundationMakeupToMaterial(InCharacterData->HeadMaterials, InMakeupSettings.Foundation);
	FMetaHumanCharacterSkinMaterials::ApplyEyeMakeupToMaterial(InCharacterData->HeadMaterials, InMakeupSettings.Eyes);
	FMetaHumanCharacterSkinMaterials::ApplyBlushMakeupToMaterial(InCharacterData->HeadMaterials, InMakeupSettings.Blush);
	FMetaHumanCharacterSkinMaterials::ApplyLipsMakeupToMaterial(InCharacterData->HeadMaterials, InMakeupSettings.Lips);
}

void UMetaHumanCharacterEditorSubsystem::CommitMakeupSettings(TNotNull<UMetaHumanCharacter*> InCharacter, const FMetaHumanCharacterMakeupSettings& InMakeupSettings) const
{
	InCharacter->MakeupSettings = InMakeupSettings;
	InCharacter->MarkPackageDirty();

	ApplyMakeupSettings(InCharacter, InCharacter->MakeupSettings);
}

void UMetaHumanCharacterEditorSubsystem::ApplySkinProperties(
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData,
	const FMetaHumanCharacterSkinProperties& InSkinProperties,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InOutSynthesizedFaceTextures,
	TMap<EBodyTextureType, TObjectPtr<UTexture2D>>& InOutBodyTextures) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplySkinSynthesizeProperties");

	const bool bNeedToRecreateTextures = !FMetaHumanCharacterTextureSynthesis::AreTexturesAndImagesSuitableForSynthesis(FaceTextureSynthesizer,
																													InOutSynthesizedFaceTextures,
																													InCharacterData->CachedSynthesizedImages);
	if (bNeedToRecreateTextures)
	{
		// Recreate the textures so that they match the size and format generated by the TS model
		// Note that this can cause a "de-sync" between the face texture info and the texture objects but 
		// they will be updated on the next call to commit the skin settings
		// TODO: should/can we clear up some texture memory here?

		InOutSynthesizedFaceTextures.Reset();
		InCharacterData->CachedSynthesizedImages.Reset();

		FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData(FaceTextureSynthesizer,
																	 TMap<EFaceTextureType, FMetaHumanCharacterTextureInfo>{},
																	 InOutSynthesizedFaceTextures,
																	 InCharacterData->CachedSynthesizedImages);

		FMetaHumanCharacterTextureSynthesis::CreateSynthesizedFaceTextures(FaceTextureSynthesizer.GetTextureSizeX(),
																		   InOutSynthesizedFaceTextures);
	}

	// TS data should have been initialized by this point
	if (!FMetaHumanCharacterTextureSynthesis::SynthesizeFaceTextures(
		InSkinProperties,
		FaceTextureSynthesizer,
		InCharacterData->CachedSynthesizedImages))
	{
		// TODO: Should we clear any synthesized textures here to get back to a consistent state?
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to synthesize face textures");
	}

	if (!InCharacterData->SkinSettings.IsSet() || bNeedToRecreateTextures 
		|| InCharacterData->SkinSettings.GetValue().Skin.FaceTextureIndex != InSkinProperties.FaceTextureIndex)
	{
		// Only need to update face state if texture has changed
		if (!FMetaHumanCharacterTextureSynthesis::SelectFaceTextures(
			InSkinProperties,
			FaceTextureSynthesizer,
			InCharacterData->CachedSynthesizedImages))
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to select face textures");
		}

		ApplySkinPropertiesToFaceState(InCharacterData, InSkinProperties);
	}

	if (!FMetaHumanCharacterTextureSynthesis::UpdateFaceTextures(InCharacterData->CachedSynthesizedImages, InOutSynthesizedFaceTextures))
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to update face textures");
	}

	// Update the Body Material Parameters to match
	FMetaHumanCharacterBodyTextureUtils::GetSkinToneAndUpdateMaterials(InSkinProperties,
																	   FaceTextureSynthesizer,
																	   InOutBodyTextures,
																	   InCharacterData->HeadMaterials,
																	   InCharacterData->BodyMaterial);
}

void UMetaHumanCharacterEditorSubsystem::UpdateFaceMeshInternal(
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, 
	const FMetaHumanRigEvaluatedState& InVerticesAndNormals, 
	ELodUpdateOption InUpdateOption)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdateFaceMeshInternal");

	FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(
		InCharacterData->FaceMesh,
		InVerticesAndNormals,
		*InCharacterData->FaceState,
		*InCharacterData->FaceDnaToSkelMeshMap,
		InUpdateOption,
		EVertexPositionsAndNormals::Both);

	const bool bRebuildTangents = true;
	USkelMeshDNAUtils::RebuildRenderData_VertexPosition(InCharacterData->FaceMesh, bRebuildTangents);

	ForEachCharacterActor(InCharacterData,
		[](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
		{
			Actor->OnFaceMeshUpdated();
		});
}

const UMetaHumanCharacterEditorSubsystem::FMetaHumanCharacterIdentityModels& UMetaHumanCharacterEditorSubsystem::GetOrCreateCharacterIdentity(EMetaHumanCharacterTemplateType InTemplateType)
{
	FMetaHumanCharacterIdentityModels& IdentityModels = CharacterIdentities.FindOrAdd(InTemplateType);

	const FString BodyModelPath = GetBodyIdentityModelPath();

	if (!IdentityModels.Face.IsValid())
	{
		const FString ModelPath = GetFaceIdentityTemplateModelPath(InTemplateType);
		UDNAAsset* FaceArchetypeDNA = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAAseet(EMetaHumanImportDNAType::Face, GetTransientPackage());
		check(FaceArchetypeDNA);

		const EMetaHumanCharacterOrientation HeadOrientation = EMetaHumanCharacterOrientation::Y_UP;

		IdentityModels.Face = MakeShared<FMetaHumanCharacterIdentity>();
		const bool bIsInitialized = IdentityModels.Face->Init(ModelPath, BodyModelPath, FaceArchetypeDNA, HeadOrientation);
		check(bIsInitialized);
	}

	if (!IdentityModels.Body.IsValid())
	{
		const FString LegacyBodiesPath = GetLegacyBodiesPath();

		IdentityModels.Body = MakeShared<FMetaHumanCharacterBodyIdentity>();
		const bool bIsInitialized = IdentityModels.Body->Init(BodyModelPath, LegacyBodiesPath);
		check(bIsInitialized);
	}

	return IdentityModels;
}

USkeletalMesh* UMetaHumanCharacterEditorSubsystem::GetFaceArchetypeMesh(EMetaHumanCharacterTemplateType InTemplateType)
{
	USkeletalMesh* FaceArchetypeMesh = nullptr;

	if (InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman)
	{
		TSharedPtr<IDNAReader> ArchetypeDnaReader = nullptr;
		FaceArchetypeMesh = FMetaHumanCharacterSkelMeshUtils::CreateArchetypeSkelMeshFromDNA(EMetaHumanImportDNAType::Face, ArchetypeDnaReader);
		if (FaceArchetypeMesh)
		{
			const bool bIsFace = true;
			FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(FaceArchetypeMesh, ArchetypeDnaReader, bIsFace);
		}
		else
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to load the face archetype DNA file from plugin content");
		}
	}

	return FaceArchetypeMesh;
}

USkeletalMesh* UMetaHumanCharacterEditorSubsystem::GetBodyArchetypeMesh(EMetaHumanCharacterTemplateType InTemplateType)
{
	USkeletalMesh* BodyArchetypeMesh = nullptr;

	if (ensureAlways(InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman))
	{
		TSharedPtr<IDNAReader> ArchetypeDnaReader = nullptr;
		BodyArchetypeMesh = FMetaHumanCharacterSkelMeshUtils::CreateArchetypeSkelMeshFromDNA(EMetaHumanImportDNAType::Body, ArchetypeDnaReader);
		if (BodyArchetypeMesh)
		{
			FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(BodyArchetypeMesh, ArchetypeDnaReader, false /*bIsFace*/);
		}
		else
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to load the body archetype DNA file from plugin content");
		}
	}

	return BodyArchetypeMesh;
}

USkeletalMesh* UMetaHumanCharacterEditorSubsystem::CreateCombinedFaceAndBodyMesh(TNotNull<const UMetaHumanCharacter*> InCharacter, const FString& InAssetPathAndName)
{
	check(CharacterDataMap.Contains(InCharacter));
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	USkeletalMesh* CombinedSkelMesh = FMetaHumanCharacterEditorBuild::MergeHeadAndBody_CreateAsset(
		CharacterData->FaceMesh,
		CharacterData->BodyMesh,
		InAssetPathAndName
	);

	if (CombinedSkelMesh)
	{
		// Body data that we want to record as asset user data
		UChaosOutfitAssetBodyUserData* BodyUserData = CombinedSkelMesh->GetAssetUserData<UChaosOutfitAssetBodyUserData>();

		if (!BodyUserData)
		{
			BodyUserData = NewObject<UChaosOutfitAssetBodyUserData>(CombinedSkelMesh);
			CombinedSkelMesh->AddAssetUserData(BodyUserData);
		}

		TSharedPtr<IDNAReader> FaceDNA = UE::MetaHuman::GetDNAReader(CharacterData->FaceMesh);
		check(FaceDNA.IsValid());

		UDNAAsset* BodyDNAAsset = CharacterData->BodyMesh->GetAssetUserData<UDNAAsset>();
		check(BodyDNAAsset);
		TSharedRef<IDNAReader> BodyDNA = CharacterData->BodyState->StateToDna(BodyDNAAsset);

		CharacterData->BodyState->GetMeasurementsForFaceAndBody(
			FaceDNA.ToSharedRef(),
			BodyDNA,
			BodyUserData->Measurements
		);
	}

	return CombinedSkelMesh;
}

bool UMetaHumanCharacterEditorSubsystem::IsTextureSynthesisEnabled() const
{
	return FaceTextureSynthesizer.IsValid();
}

FString UMetaHumanCharacterEditorSubsystem::GetFaceIdentityTemplateModelPath(EMetaHumanCharacterTemplateType InTemplateType)
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());
	const FString PluginContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());

	FString ModelPath;

	if (InTemplateType == EMetaHumanCharacterTemplateType::MetaHuman)
	{
		ModelPath = PluginContentDir / TEXT("Face/IdentityTemplate");
	}
	else
	{
		check(false);
	}

	return ModelPath;
}

FString UMetaHumanCharacterEditorSubsystem::GetBodyIdentityModelPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());
	const FString PluginContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());

	return PluginContentDir / TEXT("Body/IdentityTemplate");
}

FString UMetaHumanCharacterEditorSubsystem::GetLegacyBodiesPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	check(Plugin.IsValid());
	const FString PluginContentDir = FPaths::ConvertRelativePathToFull(Plugin->GetContentDir());

	return PluginContentDir / TEXT("Optional/Body/FixedCompatibility");
}

void UMetaHumanCharacterEditorSubsystem::ApplyBodyState(
	TNotNull<const UMetaHumanCharacter*> InCharacter,
	TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InState,
	EBodyMeshUpdateMode InUpdateMode)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	// Take a copy of the passed-in state so that the caller can't retain a non-const reference to it
	ApplyBodyState(CharacterData, MakeShared<FMetaHumanCharacterBodyIdentity::FState>(*InState), InUpdateMode);
}

void UMetaHumanCharacterEditorSubsystem::ApplyBodyState(
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, 
	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> InState,
	EBodyMeshUpdateMode InUpdateMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::ApplyBodyState");

	InCharacterData->BodyState = InState;

	const FMetaHumanRigEvaluatedState VerticesAndVertexNormals = InCharacterData->BodyState->GetVerticesAndVertexNormals();

	if (InUpdateMode == EBodyMeshUpdateMode::Minimal)
	{
		UpdateBodyMeshInternal(InCharacterData, VerticesAndVertexNormals, ELodUpdateOption::LOD0Only, /*bInUpdateFromStateDna*/ false);
		UpdateFaceFromBodyInternal(InCharacterData, ELodUpdateOption::LOD0Only, /*bInUpdateNeutral*/ false);
	}
	else
	{
		check(InUpdateMode == EBodyMeshUpdateMode::Full);

		UpdateBodyMeshInternal(InCharacterData, VerticesAndVertexNormals, ELodUpdateOption::All, /*bInUpdateFromStateDna*/ true);
		UpdateFaceFromBodyInternal(InCharacterData, ELodUpdateOption::All, /*bInUpdateNeutral*/ true);
		FMetaHumanCharacterSkelMeshUtils::UpdateMeshDescriptionFromLODModelVerticesNormalsAndTangents(InCharacterData->FaceMesh);
	}

	InCharacterData->OnBodyStateChangedDelegate.Broadcast();
}

void UMetaHumanCharacterEditorSubsystem::CommitBodyState(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InState, EBodyMeshUpdateMode InUpdateMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::CommitBodyState");

	FSharedBuffer BodyStateData;
	InState->Serialize(BodyStateData);
	
	InCharacter->SetBodyStateData(BodyStateData);
	InCharacter->MarkPackageDirty();

	// If the character has any outfit, we need to run the preview pipeline when committing body changes 
	// so that the outfit is refitted to the new body shape
	if (UMetaHumanCharacterEditorSubsystem::IsCharacterOutfitSelected(InCharacter))
	{
		FScopedSlowTask RefitClothingSlowTask{ 2.0f, LOCTEXT("RefitClothingSlowTask", "Fitting outfit to body mesh") };

		// Outfit refit requires a full body skel mesh update
		ApplyBodyState(InCharacter, InState, UMetaHumanCharacterEditorSubsystem::EBodyMeshUpdateMode::Full);

		RefitClothingSlowTask.EnterProgressFrame();

		RunCharacterEditorPipelineForPreview(InCharacter);
	}
	else
	{
		ApplyBodyState(InCharacter, InState, InUpdateMode);
	}
}


FSimpleMulticastDelegate& UMetaHumanCharacterEditorSubsystem::OnBodyStateChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->OnBodyStateChangedDelegate;
}

TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> UMetaHumanCharacterEditorSubsystem::GetBodyState(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	return CharacterData->BodyState;
}

TSharedRef<FMetaHumanCharacterBodyIdentity::FState> UMetaHumanCharacterEditorSubsystem::CopyBodyState(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	return MakeShared<FMetaHumanCharacterBodyIdentity::FState>(*GetBodyState(InCharacter));
}

void UMetaHumanCharacterEditorSubsystem::SetBodyGlobalDeltaScale(TNotNull<UMetaHumanCharacter*> InCharacter, float InBodyGlobalDelta) const
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	CharacterData->BodyState->SetGlobalDeltaScale(InBodyGlobalDelta);

	const FMetaHumanRigEvaluatedState VerticesAndVertexNormals = CharacterData->BodyState->GetVerticesAndVertexNormals();
	UpdateBodyMeshInternal(CharacterData, VerticesAndVertexNormals, ELodUpdateOption::All, /*bInUpdateFromStateDna*/ false);
	UpdateFaceFromBodyInternal(CharacterData, ELodUpdateOption::All, /*bInUpdateNeutral*/ false);
}

float UMetaHumanCharacterEditorSubsystem::GetBodyGlobalDeltaScale(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->BodyState->GetGlobalDeltaScale();
}

TSharedPtr<IDNAReader> UMetaHumanCharacterEditorSubsystem::ApplyBodyDNA(TNotNull<const UMetaHumanCharacter*> InCharacter, TSharedRef<IDNAReader> InBodyDNAReader)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	bool bDnaApplied = false;
	if (FMetaHumanCharacterSkelMeshUtils::CheckDNACompatibility(UE::MetaHuman::GetDNAReader(CharacterData->BodyMesh).Get(), InBodyDNAReader.ToSharedPtr().Get()))
	{
		 const FString PackagePath = InCharacter->GetOutermost()->GetName() / TEXT("Body");
		 const FString AssetName = FString::Format(TEXT("SKM_0}_BodyMesh"), { InCharacter->GetName() });
		 const FString FullPath = FPackageName::GetLongPackagePath(PackagePath / AssetName);
		
		 FInterchangeDnaModule& DNAImportModule = FInterchangeDnaModule::GetModule();
		 USkeletalMesh* BodyMesh = DNAImportModule.ImportSync(AssetName, FullPath, InBodyDNAReader, EMetaHumanImportDNAType::Body);
		 FMetaHumanCharacterSkelMeshUtils::PopulateSkelMeshData(BodyMesh, InBodyDNAReader, false /*bIsFace*/);

		 CharacterData->BodyMesh = BodyMesh;
		 CharacterData->BodyDnaToSkelMeshMap = MakeShareable(USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(BodyMesh));
		
		 FMetaHumanCharacterSkinMaterials::SetBodyMaterialOnMesh(CharacterData->BodyMaterial, CharacterData->BodyMesh);

		 ForEachCharacterActor(CharacterData,
			[CharacterData](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
			{
				Actor->UpdateBodyComponentMesh(CharacterData->BodyMesh);
				Actor->OnBodyMeshUpdated();
			});

		bDnaApplied = true;
	}

	if (bDnaApplied)
	{
		return InBodyDNAReader;
	}

	return nullptr;
}

void UMetaHumanCharacterEditorSubsystem::CommitBodyDNA(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<IDNAReader> InBodyDNAReader)
{
	TSharedPtr<IDNAReader> OutDna = ApplyBodyDNA(InCharacter, InBodyDNAReader);

	if (OutDna.IsValid())
	{
		InCharacter->SetBodyDNABuffer(ReadStreamFromDNA(OutDna.Get(), EDNADataLayer::All));
		UpdateCharacterIsFixedBodyType(InCharacter);
		InCharacter->MarkPackageDirty();
	}
}

bool UMetaHumanCharacterEditorSubsystem::ParametricFitToDnaBody(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter)
{
	if (InMetaHumanCharacter->HasBodyDNA())
	{
		TArray<uint8> DNABuffer = InMetaHumanCharacter->GetBodyDNABuffer();
		TSharedPtr<IDNAReader> DnaReader = ReadDNAFromBuffer(&DNABuffer, EDNADataLayer::All);

		if (FitToBodyDna(InMetaHumanCharacter, DnaReader.ToSharedRef(), EMetaHumanCharacterBodyFitOptions::FitFromMeshAndSkeleton))
		{
			TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];
			CharacterData->BodyState->SetMetaHumanBodyType(EMetaHumanBodyType::BlendableBody);
			InMetaHumanCharacter->SetBodyDNABuffer({});
			UpdateCharacterIsFixedBodyType(InMetaHumanCharacter);

			return true;
		}
	}

	return false;
}

bool UMetaHumanCharacterEditorSubsystem::ParametricFitToCompatibilityBody(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];
	if (CharacterData->BodyState->GetMetaHumanBodyType() != EMetaHumanBodyType::BlendableBody)
	{
		const bool bFitFromCompatibilityBody = true;
		CharacterData->BodyState->SetMetaHumanBodyType(EMetaHumanBodyType::BlendableBody, bFitFromCompatibilityBody);
		CharacterData->OnBodyStateChangedDelegate.Broadcast();
		return true;
	}

	return false;
}

EImportErrorCode UMetaHumanCharacterEditorSubsystem::ImportFromBodyDna(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TSharedRef<class IDNAReader> InBodyDna, const FImportBodyFromDNAParams& InImportOptions)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];
	EImportErrorCode ErrorCode = EImportErrorCode::Success;

	if (InImportOptions.bImportWholeRig)
	{
		if (FMetaHumanCharacterSkelMeshUtils::CheckDNACompatibility(UE::MetaHuman::GetDNAReader(CharacterData->BodyMesh).Get(), &InBodyDna.Get()))
		{
			if (FitToBodyDna(InMetaHumanCharacter, InBodyDna, EMetaHumanCharacterBodyFitOptions::FitFromMeshAndSkeleton))
			{
				CommitBodyState(InMetaHumanCharacter, GetBodyState(InMetaHumanCharacter));
				CommitBodyDNA(InMetaHumanCharacter, InBodyDna);
			}
			else
			{
				ErrorCode = EImportErrorCode::InvalidInputData;
			}
		}
		else
		{
			FString CombinedBodyModelPath = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAPath(EMetaHumanImportDNAType::Combined);
			TSharedPtr<IDNAReader> CombinedArchetypeDnaReader = ReadDNAFromFile(CombinedBodyModelPath);
			if (FMetaHumanCharacterSkelMeshUtils::CheckDNACompatibility(CombinedArchetypeDnaReader.Get(), &InBodyDna.Get()))
			{
				ErrorCode = EImportErrorCode::CombinedBodyCannotBeImportedAsWholeRig;
			}
			else
			{
				ErrorCode = EImportErrorCode::InvalidInputData;
			}
		}

	}
	else
	{
		if (FitToBodyDna(InMetaHumanCharacter, InBodyDna, InImportOptions.BodyFitOptions))
		{
			CommitBodyState(InMetaHumanCharacter, GetBodyState(InMetaHumanCharacter));
		}
		else
		{
			ErrorCode = EImportErrorCode::FittingError;
		}
	}

	return ErrorCode;
}

EImportErrorCode UMetaHumanCharacterEditorSubsystem::ImportFromBodyTemplate(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TNotNull<UObject*> InTemplateMesh, EMetaHumanCharacterBodyFitOptions InBodyFitOptions)
{
	TArray<FVector3f> ConformalVertices;
	EImportErrorCode ErrorCode = GetDataForBodyConforming(InTemplateMesh, ConformalVertices);

	if (ErrorCode != EImportErrorCode::Success)
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected asset must be a SkelMesh or Static Mesh consistent with MetaHuman topology to be imported into MetaHumanCharacter asset"));
		return ErrorCode;
	}

	TArray<FVector3f> ComponentJointTranslations;
	if (InBodyFitOptions == EMetaHumanCharacterBodyFitOptions::FitFromMeshAndSkeleton)
	{
		if (USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(InTemplateMesh))
		{
			TArray<FVector3f> TemplateMeshComponentJointTranslations = FMetaHumanCharacterSkelMeshUtils::GetComponentSpaceJointTranslations(SkelMesh);

			if (UDNAAsset* ArchetypeDna = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAAseet(EMetaHumanImportDNAType::Body, SkelMesh))
			{
				TArray<int32> RLJointToUEBoneIndices;
				const dna::Reader* DNAReader = ArchetypeDna->GetBehaviorReader()->Unwrap();
				UE::MetaHuman::MapJoints(SkelMesh, DNAReader, RLJointToUEBoneIndices);

				ComponentJointTranslations.AddUninitialized(DNAReader->getJointCount());
				for (uint16_t JointIndex = 0; JointIndex < DNAReader->getJointCount(); JointIndex++)
				{
					int32 BoneIndex = RLJointToUEBoneIndices[JointIndex];
					if (BoneIndex == INDEX_NONE)
					{
						FString BoneName = FString(ANSI_TO_TCHAR(DNAReader->getJointName(JointIndex).data()));
						UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected skel mesh must be consistent with MetaHuman topology to be imported into MetaHumanCharacter asset. Bone: %s not found in template mesh."), *BoneName);
						return EImportErrorCode::InvalidInputBones;
					}
					ComponentJointTranslations[JointIndex] = TemplateMeshComponentJointTranslations[BoneIndex];
				}
			}
		}
	}

	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> BodyState = CopyBodyState(InMetaHumanCharacter);

	if (BodyState->FitToTarget(ConformalVertices, ComponentJointTranslations, InBodyFitOptions))
	{
		// commit the body state and update the body mesh
		CommitBodyState(InMetaHumanCharacter, BodyState);
		return EImportErrorCode::Success;
	}

	return EImportErrorCode::FittingError;
}

bool UMetaHumanCharacterEditorSubsystem::FitToBodyDna(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<IDNAReader> InBodyDna, EMetaHumanCharacterBodyFitOptions InBodyFitOptions)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	TSharedRef<FMetaHumanCharacterBodyIdentity::FState> BodyState = CopyBodyState(InCharacter);

	if (BodyState->FitToBodyDna(InBodyDna, InBodyFitOptions))
	{
		ApplyBodyState(CharacterData, BodyState, EBodyMeshUpdateMode::Full);
		return true;
	}

	return false;
}

void UMetaHumanCharacterEditorSubsystem::SetBodyConstraints(TNotNull<const UMetaHumanCharacter*> InCharacter, const TArray<FMetaHumanCharacterBodyConstraint>& InBodyConstraints)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	CharacterData->BodyState->EvaluateBodyConstraints(InBodyConstraints);

	// Update mesh
	const FMetaHumanRigEvaluatedState VerticesAndVertexNormals = CharacterData->BodyState->GetVerticesAndVertexNormals();
	UpdateBodyMeshInternal(CharacterData, VerticesAndVertexNormals, ELodUpdateOption::All, /*bInUpdateFromStateDna*/ false);
	UpdateFaceFromBodyInternal(CharacterData, ELodUpdateOption::All, /*bInUpdateNeutral*/ false);
}

void UMetaHumanCharacterEditorSubsystem::ResetParametricBody(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	CharacterData->BodyState->Reset();

	CharacterData->OnBodyStateChangedDelegate.Broadcast();
}

void UMetaHumanCharacterEditorSubsystem::SetMetaHumanBodyType(TNotNull<const UMetaHumanCharacter*> InCharacter, EMetaHumanBodyType InBodyType, EBodyMeshUpdateMode InUpdateMode)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::SetMetaHumanBodyType");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

	CharacterData->BodyState->SetMetaHumanBodyType(InBodyType);

	// Update mesh
	const FMetaHumanRigEvaluatedState VerticesAndVertexNormals = CharacterData->BodyState->GetVerticesAndVertexNormals();
	if (InUpdateMode == EBodyMeshUpdateMode::Minimal)
	{
		UpdateBodyMeshInternal(CharacterData, VerticesAndVertexNormals, ELodUpdateOption::LOD0Only, /*bInUpdateFromStateDna*/ false);
		UpdateFaceFromBodyInternal(CharacterData, ELodUpdateOption::LOD0Only, /*bInUpdateNeutral*/ false);
	}
	else
	{
		UpdateBodyMeshInternal(CharacterData, VerticesAndVertexNormals, ELodUpdateOption::All,  /*bInUpdateFromStateDna*/ true);
		UpdateFaceFromBodyInternal(CharacterData, ELodUpdateOption::All, /*bInUpdateNeutral*/ true);
	}

	CharacterData->OnBodyStateChangedDelegate.Broadcast();
}

void UMetaHumanCharacterEditorSubsystem::UpdateCharacterIsFixedBodyType(TNotNull<UMetaHumanCharacter*> InCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	InCharacter->bFixedBodyType = InCharacter->HasBodyDNA() || CharacterData->BodyState->GetMetaHumanBodyType() != EMetaHumanBodyType::BlendableBody;
}

void UMetaHumanCharacterEditorSubsystem::UpdateBodyMeshInternal(
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, 
	const FMetaHumanRigEvaluatedState& InVerticesAndNormals, 
	ELodUpdateOption InUpdateOption, 
	bool bInUpdateFromStateDna)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdateBodyMeshInternal");

	FMetaHumanCharacterSkelMeshUtils::UpdateLODModelVertexPositions(
		InCharacterData->BodyMesh,
		InVerticesAndNormals,
		*InCharacterData->BodyState,
		*InCharacterData->BodyDnaToSkelMeshMap,
		InUpdateOption,
		EVertexPositionsAndNormals::Both);

	const bool bRebuildTangents = true;
	USkelMeshDNAUtils::RebuildRenderData_VertexPosition(InCharacterData->BodyMesh, bRebuildTangents);

	if (bInUpdateFromStateDna)
	{
		// Get dna from state and update skel mesh
		if (UDNAAsset* BodyDNA = InCharacterData->BodyMesh->GetAssetUserData<UDNAAsset>())
		{
			TSharedRef<IDNAReader> StateDnaReader = InCharacterData->BodyState->StateToDna(BodyDNA);

			// Already updated vertex positions and don't need to rebuild skel mesh
			const FMetaHumanCharacterSkelMeshUtils::EUpdateFlags UpdateFlags =
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::Joints |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::SkinWeights |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::DNABehavior |
				FMetaHumanCharacterSkelMeshUtils::EUpdateFlags::DNAGeometry;

			FMetaHumanCharacterSkelMeshUtils::UpdateSkelMeshFromDNA(
				StateDnaReader,
				UpdateFlags,
				InCharacterData->BodyDnaToSkelMeshMap,
				EMetaHumanCharacterOrientation::Y_UP,
				InCharacterData->BodyMesh);
		}
		else
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Unable to update body DNA. Body skeletal mesh does not contain DNA Asset User Data."));
		}
	}
	else
	{
		// Update the face joints to ensure groom physics are affected correctly
		// 
		// Mostly copied from UpdateJoints in MetaHumanCharacterSkelMeshUtils.cpp

		if (UDNAAsset* BodyDNA = InCharacterData->BodyMesh->GetAssetUserData<UDNAAsset>())
		{
			USkeletalMesh* InSkelMesh = InCharacterData->BodyMesh;
			TArray<int32> RLJointToUEBoneIndices;
			const dna::Reader* DNAReader = BodyDNA->GetBehaviorReader()->Unwrap();
			UE::MetaHuman::MapJoints(InSkelMesh, DNAReader, RLJointToUEBoneIndices);

			TArray<FTransform> RawBonePose;

			const EMetaHumanCharacterOrientation InCharacterOrientation = EMetaHumanCharacterOrientation::Y_UP;

			{	// Scoping of RefSkelModifier
				FReferenceSkeletonModifier RefSkelModifier(InSkelMesh->GetRefSkeleton(), InSkelMesh->GetSkeleton());

				// copy here
				RawBonePose = InSkelMesh->GetRefSkeleton().GetRawRefBonePose();

				// calculate component space ahead of current transform
				TArray<FTransform> ComponentTransforms;
				FAnimationRuntime::FillUpComponentSpaceTransforms(InSkelMesh->GetRefSkeleton(), RawBonePose, ComponentTransforms);

				const TArray<FMeshBoneInfo>& RawBoneInfo = InSkelMesh->GetRefSkeleton().GetRawRefBoneInfo();

				// Skipping root joint (index 0) to avoid blinking of the mesh due to bounding box issue
				for (uint16 JointIndex = 0; JointIndex < InCharacterData->BodyState->GetNumberOfJoints(); JointIndex++)
				{
					int32 BoneIndex = RLJointToUEBoneIndices[JointIndex];

					FTransform DNATransform = FTransform::Identity;

					FVector3f FloatTranslation;
					FRotator3f FloatRotation;
					InCharacterData->BodyState->GetNeutralJointTransform(JointIndex, FloatTranslation, FloatRotation);

					// Mappings from FDNAReader<TWrappedReader>::GetNeutralJointTranslation and GetNeutralJointRotation
					//
					// Would be neater to move this to FMetaHumanCharacterBodyIdentity::FState::GetNeutralJointTransform
					const FVector Translation(FloatTranslation.X, -FloatTranslation.Y, FloatTranslation.Z);
					const FRotator Rotation(-FloatRotation.Yaw, -FloatRotation.Roll, FloatRotation.Pitch);

					if (DNAReader->getJointParentIndex(JointIndex) == JointIndex) // This is the highest joint of the dna - not necessarily the UE root bone  
					{
						if (InCharacterOrientation == EMetaHumanCharacterOrientation::Y_UP)
						{
							FQuat YUpToZUpRotation = FQuat(FRotator(0, 0, 90));
							FQuat ComponentRotation = YUpToZUpRotation * FQuat(Rotation);

							DNATransform.SetTranslation(FVector(Translation.X, Translation.Z, -Translation.Y));
							DNATransform.SetRotation(ComponentRotation);
						}
						else if (InCharacterOrientation == EMetaHumanCharacterOrientation::Z_UP)
						{
							DNATransform.SetTranslation(Translation);
							DNATransform.SetRotation(Rotation.Quaternion());
						}
						else
						{
							check(false);
						}

						ComponentTransforms[BoneIndex] = DNATransform;
					}
					else
					{
						DNATransform.SetTranslation(Translation);
						DNATransform.SetRotation(Rotation.Quaternion());

						if (ensure(RawBoneInfo[BoneIndex].ParentIndex != INDEX_NONE))
						{
							ComponentTransforms[BoneIndex] = DNATransform * ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex];
						}
					}

					ComponentTransforms[BoneIndex].NormalizeRotation();
				}

				for (uint16 BoneIndex = 0; BoneIndex < RawBoneInfo.Num(); BoneIndex++)
				{
					FTransform LocalTransform;

					if (BoneIndex == 0)
					{
						LocalTransform = ComponentTransforms[BoneIndex];
					}
					else
					{
						LocalTransform = ComponentTransforms[BoneIndex].GetRelativeTransform(ComponentTransforms[RawBoneInfo[BoneIndex].ParentIndex]);
					}

					LocalTransform.NormalizeRotation();

					RefSkelModifier.UpdateRefPoseTransform(BoneIndex, LocalTransform);
				}

				// given that we revert the bones further below, we do not need to update the ref matrices
				// but keep it here for consistency in case we decide to update the body joints
				//InSkelMesh->GetRefBasesInvMatrix().Reset();
				//InSkelMesh->CalculateInvRefMatrices(); // Needs to be called after RefSkelModifier is destroyed
			}

			FMetaHumanCharacterSkelMeshUtils::UpdateBindPoseFromSource(InCharacterData->BodyMesh, InCharacterData->FaceMesh);

			// revert back the body, only the head is required for grooms
			{
				// Scoping of RefSkelModifier
				FReferenceSkeletonModifier RefSkelModifier(InSkelMesh->GetRefSkeleton(), InSkelMesh->GetSkeleton());

				for (uint16 BoneIndex = 0; BoneIndex < RawBonePose.Num(); BoneIndex++)
				{
					RefSkelModifier.UpdateRefPoseTransform(BoneIndex, RawBonePose[BoneIndex]);
				}
			}

		}
	}
}

void UMetaHumanCharacterEditorSubsystem::UpdateFaceFromBodyInternal(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, ELodUpdateOption InUpdateOption, bool bInUpdateNeutral)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdateFaceFromBodyInternal");

	const FMetaHumanRigEvaluatedState VerticesAndVertexNormals = InCharacterData->BodyState->GetVerticesAndVertexNormals();
	// Update face state from body
	InCharacterData->FaceState->SetBodyJointsAndBodyFaceVertices(InCharacterData->BodyState->CopyBindPose(), VerticesAndVertexNormals.Vertices);
	// set the body vertex normals into the face state
	InCharacterData->FaceState->SetBodyVertexNormals(VerticesAndVertexNormals.VertexNormals, InCharacterData->BodyState->GetNumVerticesPerLOD());

	// Update face mesh
	const FMetaHumanRigEvaluatedState FaceVerticesAndVertexNormals = InCharacterData->FaceState->Evaluate();
	UpdateFaceMeshInternal(InCharacterData, FaceVerticesAndVertexNormals, InUpdateOption);

	// Update face mesh neutral
	if (bInUpdateNeutral)
	{
		FMetaHumanCharacterSkelMeshUtils::UpdateBindPoseFromSource(InCharacterData->BodyMesh, InCharacterData->FaceMesh);
	}
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::GetBodyGizmos(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->BodyState->GetRegionGizmos();
}

TArray<FVector3f> UMetaHumanCharacterEditorSubsystem::BlendBodyRegion(TNotNull<const UMetaHumanCharacter*> InCharacter, int32 InRegionIndex, EBodyBlendOptions InBodyBlendOptions, const TSharedPtr<const FMetaHumanCharacterBodyIdentity::FState>& InStartState, const TArray<TSharedPtr<const FMetaHumanCharacterBodyIdentity::FState>>& InPresetStates, TConstArrayView<float> InPresetWeights)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::BlendBodyRegion");

	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	if (InPresetStates.Num() <= InPresetWeights.Num() && InPresetStates.Num() > 0)
	{
		TArray<TPair<float, const FMetaHumanCharacterBodyIdentity::FState*>> PresetStateWeights;
		for (int32 PresetIndex = 0; PresetIndex < InPresetStates.Num(); PresetIndex++)
		{
			if (InPresetStates[PresetIndex].Get())
			{
				PresetStateWeights.Add({ InPresetWeights[PresetIndex], InPresetStates[PresetIndex].Get() });
			}
		}

		CharacterData->BodyState = MakeShared<FMetaHumanCharacterBodyIdentity::FState>(*InStartState);
		CharacterData->BodyState->BlendPresets(InRegionIndex, PresetStateWeights, InBodyBlendOptions);

		const FMetaHumanRigEvaluatedState BodyVerticesAndVertexNormals = CharacterData->BodyState->GetVerticesAndVertexNormals();

		UpdateBodyMeshInternal(CharacterData, BodyVerticesAndVertexNormals, ELodUpdateOption::LOD0Only, /*bInUpdateFromStateDna*/ false);
		UpdateFaceFromBodyInternal(CharacterData, ELodUpdateOption::LOD0Only, /*bInUpdateNeutral*/ false);
	}

	return CharacterData->BodyState->GetRegionGizmos();
}

TNotNull<const USkeletalMesh*> UMetaHumanCharacterEditorSubsystem::Debug_GetFaceEditMesh(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->FaceMesh;
}

TNotNull<const USkeletalMesh*> UMetaHumanCharacterEditorSubsystem::Debug_GetBodyEditMesh(TNotNull<const UMetaHumanCharacter*> InCharacter) const
{
	TSharedRef<const FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];
	return CharacterData->BodyMesh;
}

TObjectPtr<UPhysicsAsset> UMetaHumanCharacterEditorSubsystem::CreatePhysicsAssetForCharacter(TNotNull<const UMetaHumanCharacter*> InCharacter,
																							 TNotNull<UObject*> InOuter,
																							 TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InBodyState)
{
	const FString CandidateName = FString::Format(TEXT("{0}_Physics"), { InCharacter->GetName() });
	const FName AssetName = MakeUniqueObjectName(InOuter, UPhysicsAsset::StaticClass(), FName{ CandidateName }, EUniqueObjectNameOptions::GloballyUnique);

	TObjectPtr<UPhysicsAsset> PhysicsArchetype = FMetaHumanCharacterSkelMeshUtils::GetBodyArchetypePhysicsAsset(InCharacter->TemplateType);
	TObjectPtr<UPhysicsAsset> PhysicsAsset = DuplicateObject(PhysicsArchetype, InOuter, AssetName);

	UpdatePhysicsAssetFromBodyState(PhysicsAsset, InBodyState);
	return PhysicsAsset;
}

void UMetaHumanCharacterEditorSubsystem::UpdatePhysicsAssetFromBodyState(TNotNull<UPhysicsAsset*> InPhysicsAsset, TSharedRef<const FMetaHumanCharacterBodyIdentity::FState> InBodyState)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("UMetaHumanCharacterEditorSubsystem::UpdatePhysicsAssetFromBodyState");

	// Update collision shapes
	for (TObjectPtr<USkeletalBodySetup> BodySetup : InPhysicsAsset->SkeletalBodySetups)
	{
		TArray<PhysicsBodyVolume> PhysicsBodyVolumes = InBodyState->GetPhysicsBodyVolumes(BodySetup->BoneName);
		
		for (int32 PhysicsBodyIndex = 0; PhysicsBodyIndex < PhysicsBodyVolumes.Num(); PhysicsBodyIndex++)
		{
			if (PhysicsBodyIndex < BodySetup->AggGeom.SphylElems.Num())
			{
				FKSphylElem& SphylElem = BodySetup->AggGeom.SphylElems[PhysicsBodyIndex];
				FTransform BodyTransform = SphylElem.GetTransform();
				BodyTransform.SetTranslation(PhysicsBodyVolumes[PhysicsBodyIndex].Center);
				SphylElem.SetTransform(BodyTransform);

				FVector BoxExtentsVector = PhysicsBodyVolumes[PhysicsBodyIndex].Extent;
				
				// Use rotation of archetype capsule to determine dominant axis
				FVector AxisZ = BodyTransform.GetRotation().GetAxisZ().GetAbs();
				if (AxisZ.Z > AxisZ.Y && AxisZ.Z > AxisZ.X)
				{
					SphylElem.Radius = FMath::Max(FMath::Abs(BoxExtentsVector[0]), FMath::Abs(BoxExtentsVector[1])) * 0.5;
					SphylElem.Length = FMath::Max(FMath::Abs(BoxExtentsVector[2]) - (2 * SphylElem.Radius), 0);
				}
				else if (AxisZ.Y > AxisZ.X && AxisZ.Y > AxisZ.Z)
				{
					SphylElem.Radius = FMath::Max(FMath::Abs(BoxExtentsVector[0]), FMath::Abs(BoxExtentsVector[2])) * 0.5;
					SphylElem.Length = FMath::Max(FMath::Abs(BoxExtentsVector[1]) - (2 * SphylElem.Radius), 0);
				}
				else
				{
					SphylElem.Radius = FMath::Max(FMath::Abs(BoxExtentsVector[1]), FMath::Abs(BoxExtentsVector[2])) * 0.5;
					SphylElem.Length = FMath::Max(FMath::Abs(BoxExtentsVector[0]) - (2 * SphylElem.Radius), 0);
				}
			}
			else if (PhysicsBodyIndex < BodySetup->AggGeom.BoxElems.Num())
			{
				FKBoxElem& BoxElem = BodySetup->AggGeom.BoxElems[PhysicsBodyIndex];
				FTransform BodyTransform;
				BodyTransform.SetTranslation(PhysicsBodyVolumes[PhysicsBodyIndex].Center);
				BoxElem.SetTransform(BodyTransform);

				BoxElem.X = PhysicsBodyVolumes[PhysicsBodyIndex].Extent.X;
				BoxElem.Y = PhysicsBodyVolumes[PhysicsBodyIndex].Extent.Y;
				BoxElem.Z = PhysicsBodyVolumes[PhysicsBodyIndex].Extent.Z;
			}
		}
	}

	// Update constraint positions
	for (UPhysicsConstraintTemplate* ConstraintSetup : InPhysicsAsset->ConstraintSetup)
	{
		bool bIsUserConstraint = (ConstraintSetup->DefaultInstance.JointName == TEXT("UserConstraint")) || (ConstraintSetup->DefaultInstance.JointName == TEXT("UserConstraint_0"));
		if (!bIsUserConstraint)
		{
			ConstraintSetup->Modify();
			ConstraintSetup->DefaultInstance.SnapTransformsToDefault(EConstraintTransformComponentFlags::AllPosition, InPhysicsAsset);
		}
	}
}

void UMetaHumanCharacterEditorSubsystem::SetClothingVisibilityState(TNotNull<UMetaHumanCharacter*> InCharacter, EMetaHumanClothingVisibilityState InState, bool bUpdateMaterialHiddenFaces)
{
	ForEachCharacterActor(InCharacter,
		[InState](TScriptInterface<IMetaHumanCharacterEditorActorInterface> Actor)
		{
			Actor->SetClothingVisibilityState(InState);
		});

	CharacterDataMap[InCharacter]->bClothingVisible = (InState == EMetaHumanClothingVisibilityState::Shown);

	if (bUpdateMaterialHiddenFaces)
	{
		UpdateCharacterPreviewMaterialBodyHiddenFacesMask(InCharacter);
	}
}

void UMetaHumanCharacterEditorSubsystem::OnCharacterInstanceUpdated(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	const FMetaHumanDefaultAssemblyOutput* AssemblyStruct = InCharacter->GetInternalCollection()->GetDefaultInstance()->GetAssemblyOutput().GetPtr<FMetaHumanDefaultAssemblyOutput>();

	TObjectPtr<UTexture2D> BodyHiddenFacesMask = nullptr;
	if (AssemblyStruct->ClothData.Num() > 0)
	{
		BodyHiddenFacesMask = AssemblyStruct->ClothData[0].BodyHiddenFaceMap;
	}
	CharacterDataMap[InCharacter]->BodyHiddenFaceMap = BodyHiddenFacesMask;

	UpdateCharacterPreviewMaterialBodyHiddenFacesMask(InCharacter);
}

void UMetaHumanCharacterEditorSubsystem::UpdateCharacterPreviewMaterialBodyHiddenFacesMask(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	const TObjectPtr<UTexture2D>& BodyHiddenFaceMap = CharacterDataMap[InCharacter]->BodyHiddenFaceMap;
	bool bApplyHiddenFaces = UE::MetaHuman::CVarMHCharacterPreviewHiddenFaces.GetValueOnAnyThread();
	if (bApplyHiddenFaces && (BodyHiddenFaceMap != nullptr) && CharacterDataMap[InCharacter]->bClothingVisible)
	{
		FMetaHumanCharacterBodyTextureUtils::SetMaterialHiddenFacesTexture(CharacterDataMap[InCharacter]->BodyMaterial, BodyHiddenFaceMap);
	}
	else
	{
		FMetaHumanCharacterBodyTextureUtils::SetMaterialHiddenFacesTextureNoOp(CharacterDataMap[InCharacter]->BodyMaterial);
	}
}

void UMetaHumanCharacterEditorSubsystem::ForEachCharacterActor(TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, TFunction<void(TScriptInterface<IMetaHumanCharacterEditorActorInterface>)> InFunc)
{
	for (TWeakInterfacePtr<IMetaHumanCharacterEditorActorInterface> CharacterActor : InCharacterData->CharacterActorList)
	{
		if (CharacterActor.IsValid())
		{
			InFunc(CharacterActor.ToScriptInterface());
		}
	}
}

ELodUpdateOption UMetaHumanCharacterEditorSubsystem::GetUpdateOptionForEditing()
{
	ELodUpdateOption UpdateOption = ELodUpdateOption::LOD0Only;
	if (UE::MetaHuman::CvarUpdateAllLODsOnFaceEdit.GetValueOnAnyThread())
	{
		UpdateOption = ELodUpdateOption::All;
	}

	return UpdateOption;
}

void UMetaHumanCharacterEditorSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	UMetaHumanCharacterEditorSubsystem* This = CastChecked<UMetaHumanCharacterEditorSubsystem>(InThis);

	for (TPair<TObjectKey<UMetaHumanCharacter>, TSharedRef<FMetaHumanCharacterEditorData>>& Pair : This->CharacterDataMap)
	{
		Collector.AddPropertyReferences(FMetaHumanCharacterEditorData::StaticStruct(), &Pair.Value.Get(), This);
	}
}

void UMetaHumanCharacterEditorSubsystem::ForEachCharacterActor(TNotNull<const UMetaHumanCharacter*> InCharacter, TFunction<void(TScriptInterface<class IMetaHumanCharacterEditorActorInterface>)> InFunc)
{
	check(CharacterDataMap.Contains(InCharacter));
	TSharedRef<FMetaHumanCharacterEditorData> InCharacterData = CharacterDataMap[InCharacter];
	for (TWeakInterfacePtr<IMetaHumanCharacterEditorActorInterface> CharacterActor : InCharacterData->CharacterActorList)
	{
		if (CharacterActor.IsValid())
		{
			InFunc(CharacterActor.ToScriptInterface());
		}
	}
}

FOnStudioEnvironmentChanged& UMetaHumanCharacterEditorSubsystem::OnLightEnvironmentChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	check(CharacterDataMap.Contains(InCharacter));
	return CharacterDataMap[InCharacter]->EnvironmentUpdateDelegate;
}

FOnStudioLightRotationChanged& UMetaHumanCharacterEditorSubsystem::OnLightRotationChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	check(CharacterDataMap.Contains(InCharacter));
	return CharacterDataMap[InCharacter]->EnvironmentLightRotationChangedDelegate;
}

FOnStudioBackgroundColorChanged& UMetaHumanCharacterEditorSubsystem::OnBackgroundColorChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	check(CharacterDataMap.Contains(InCharacter));
	return CharacterDataMap[InCharacter]->EnvironmentBackgroundColorChangedDelegate;
}

FOnStudioTonemapperOptionChanged& UMetaHumanCharacterEditorSubsystem::OnLightTonemapperChanged(TNotNull<const UMetaHumanCharacter*> InCharacter)
{
	check(CharacterDataMap.Contains(InCharacter));
	return CharacterDataMap[InCharacter]->EnvironmentTonemapperOptionChangedDelegate;
}

void UMetaHumanCharacterEditorSubsystem::UpdateLightingEnvironment(TNotNull<UMetaHumanCharacter*> InCharacter, EMetaHumanCharacterEnvironment InLightingEnvironment) const
{
	check(CharacterDataMap.Contains(InCharacter));

	InCharacter->ViewportSettings.CharacterEnvironment = InLightingEnvironment;
	InCharacter->MarkPackageDirty();
	CharacterDataMap[InCharacter]->EnvironmentUpdateDelegate.ExecuteIfBound(InLightingEnvironment);
}

void UMetaHumanCharacterEditorSubsystem::UpdateTonemapperOption(TNotNull<UMetaHumanCharacter*> InCharacter, bool bInToneMapperEnabled) const
{
	check(CharacterDataMap.Contains(InCharacter));

	InCharacter->ViewportSettings.bTonemapperEnabled = bInToneMapperEnabled;
	InCharacter->MarkPackageDirty();
	CharacterDataMap[InCharacter]->EnvironmentTonemapperOptionChangedDelegate.ExecuteIfBound(bInToneMapperEnabled);
}

void UMetaHumanCharacterEditorSubsystem::UpdateLightRotation(TNotNull<UMetaHumanCharacter*> InCharacter, float InRotation) const
{
	check(CharacterDataMap.Contains(InCharacter));

	InCharacter->ViewportSettings.LightRotation = InRotation;
	InCharacter->MarkPackageDirty();
	CharacterDataMap[InCharacter]->EnvironmentLightRotationChangedDelegate.ExecuteIfBound(InRotation);
}

void UMetaHumanCharacterEditorSubsystem::UpdateBackgroundColor(TNotNull<UMetaHumanCharacter*> InCharacter, const FLinearColor& InBackgroundColor) const
{
	check(CharacterDataMap.Contains(InCharacter));

	InCharacter->ViewportSettings.BackgroundColor = InBackgroundColor;
	InCharacter->MarkPackageDirty();
	CharacterDataMap[InCharacter]->EnvironmentBackgroundColorChangedDelegate.ExecuteIfBound(InBackgroundColor);
}

void UMetaHumanCharacterEditorSubsystem::UpdateCharacterLOD(TNotNull<UMetaHumanCharacter*> InCharacter, const EMetaHumanCharacterLOD NewLODValue) const
{
	if (InCharacter->ViewportSettings.LevelOfDetail != NewLODValue)
	{
		InCharacter->ViewportSettings.LevelOfDetail = NewLODValue;
		InCharacter->MarkPackageDirty();
	}

	ForEachCharacterActor(CharacterDataMap[InCharacter], [this, NewLODValue](TScriptInterface<IMetaHumanCharacterEditorActorInterface> MetaHumanCharacterActor)
		{
			if (NewLODValue == EMetaHumanCharacterLOD::Auto)
			{
				MetaHumanCharacterActor->SetForcedLOD(-1);
			}
			else
			{
				MetaHumanCharacterActor->SetForcedLOD((int32)NewLODValue);
			}
		});
}

void UMetaHumanCharacterEditorSubsystem::UpdateAlwaysUseHairCardsOption(TNotNull<UMetaHumanCharacter*> InCharacter, bool bInAlwaysUseHairCards) const
{
	ForEachCharacterActor(CharacterDataMap[InCharacter], [this, bInAlwaysUseHairCards](TScriptInterface<IMetaHumanCharacterEditorActorInterface> MetaHumanCharacterActor)
		{
			// Update groom settings on each groom component for each actor.			
			AActor* CharacterActor = Cast<AActor>(MetaHumanCharacterActor.GetObject());
			TArray<UGroomComponent*> GroomComponents;
			CharacterActor->GetComponents(UGroomComponent::StaticClass(), GroomComponents);
			for (UGroomComponent* GroomComponent : GroomComponents)
			{
				GroomComponent->SetUseCards(bInAlwaysUseHairCards);
			}
		});
}

bool UMetaHumanCharacterEditorSubsystem::FitToFaceDna(TNotNull<UMetaHumanCharacter*> InCharacter, TSharedRef<IDNAReader> InFaceDna, const FFitToTargetOptions& InFitToTargetOptions)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];


	TSharedRef<FMetaHumanCharacterIdentity::FState> FaceState = CopyFaceState(InCharacter);

	const bool bFitted = FaceState->FitToFaceDna(InFaceDna, InFitToTargetOptions);

	// apply the face state and update the body from the face
	if (bFitted)
	{
		ApplyFaceState(CharacterData, FaceState);
	}

	return bFitted;
}


bool UMetaHumanCharacterEditorSubsystem::FitStateToTargetVertices(TNotNull<UMetaHumanCharacter*> InCharacter, const TMap<EHeadFitToTargetMeshes, TArray<FVector3f>>& InTargetVertices, const FFitToTargetOptions& InFitToTargetOptions)
{
	if (!InTargetVertices.Find(EHeadFitToTargetMeshes::Head))
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected identity must contain a conformed head mesh in order to be imported into MetaHumanCharacter asset"));
		return false;
	}
	else
	{
		TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InCharacter];

		const TMap<EHeadFitToTargetMeshes, int32> PartMeshMapping =
		{
			{ EHeadFitToTargetMeshes::Head, 0 },
			{ EHeadFitToTargetMeshes::LeftEye, 3 },
			{ EHeadFitToTargetMeshes::RightEye, 4 },
			{ EHeadFitToTargetMeshes::Teeth, 1 }
		};

		TMap<int32, TArray<FVector3f>> TargetMeshVertices;
		
		for (const TPair< EHeadFitToTargetMeshes, TArray<FVector3f>>& PartMesh : InTargetVertices)
		{
			TargetMeshVertices.Add(PartMeshMapping[PartMesh.Key], PartMesh.Value);
		}

		TSharedRef<FMetaHumanCharacterIdentity::FState> FaceState = CopyFaceState(InCharacter);

		const bool bFitted = FaceState->FitToTarget(TargetMeshVertices, InFitToTargetOptions);

		if (bFitted)
		{
			// apply the face state and update the body from the face
			ApplyFaceState(CharacterData, FaceState);
		}
		else
		{
			return false;
		}
	}
	
	return true;
}


EImportErrorCode UMetaHumanCharacterEditorSubsystem::ImportFromFaceDna(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TSharedRef<class IDNAReader> InFaceDna, const FImportFromDNAParams& InImportParams)
{
	bool bSuccess = true;

	// first check that the dna is consistent with MH head
	if (TSharedPtr<IDNAReader> ArchetypeDnaReader = UMetaHumanIdentityFace::GetPluginArchetypeDNAReader())
	{
		FString OutCompatibilityMsg;
		if (!FDNAUtilities::CheckCompatibility(ArchetypeDnaReader.Get(), &InFaceDna.Get(), EDNARigCompatiblityFlags::All, OutCompatibilityMsg))
		{
			return EImportErrorCode::InvalidInputData;
		}
	}


	if (InImportParams.bImportWholeRig)
	{
		FFitToTargetOptions FitToTargetOptions{ EAlignmentOptions::None, /*bAdaptNeck*/ false, /*bDisableHighFrequencyDelta*/ true };

		const bool bFitted = FitToFaceDna(InMetaHumanCharacter, InFaceDna, FitToTargetOptions);

		if (bFitted)
		{
			CommitFaceState(InMetaHumanCharacter, GetFaceState(InMetaHumanCharacter));
			CommitFaceDNA(InMetaHumanCharacter, InFaceDna);
		}
		else
		{
			bSuccess = false;
		}
	}
	else
	{
		FFitToTargetOptions FitToTargetOptions{ InImportParams.AlignmentOptions, /*bAdaptNeck*/ InImportParams.bAdaptNeck, /*bDisableHighFrequencyDelta*/ true };

		const bool bFitted = FitToFaceDna(InMetaHumanCharacter, InFaceDna, FitToTargetOptions);

		if (bFitted)
		{
			CommitFaceState(InMetaHumanCharacter, GetFaceState(InMetaHumanCharacter));
		}
		else
		{
			bSuccess = false;
		}
	}

	if (bSuccess)
	{
		UE::MetaHuman::Analytics::RecordImportFaceDNAEvent(InMetaHumanCharacter);

		return EImportErrorCode::Success;
	}
	return EImportErrorCode::FittingError;
}


EImportErrorCode UMetaHumanCharacterEditorSubsystem::ImportFromIdentity(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TNotNull<const UMetaHumanIdentity*> InMetaHumanIdentity, const FImportFromIdentityParams& InImportParams)
{
	if (UMetaHumanIdentityFace* FacePart = InMetaHumanIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		// Fit to the conformed head mesh of the Identity asset
		if (FacePart->IsConformalRigValid())
		{

			// Fit to face the head lod0 of the conformed mesh
			const TMap<EIdentityPartMeshes, TArray<FVector>> ConformalVertices = FacePart->GetConformalVerticesWorldPos(EIdentityPoseType::Neutral);
			if (!ConformalVertices.Find(EIdentityPartMeshes::Head))
			{
				UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected identity must contain a conformed head mesh in order to be imported into MetaHumanCharacter asset"));
				return EImportErrorCode::NoHeadMeshPresent;
			}
			else
			{
				auto ConvertArrayToFVector3f = [] (const TArray<FVector>& InVectorArray) -> TArray<FVector3f>
				{
					TArray<FVector3f> OutVectorArray;
					OutVectorArray.AddUninitialized(InVectorArray.Num());
					for (int32 I = 0; I < InVectorArray.Num(); I++)
					{
						OutVectorArray[I] = FVector3f(InVectorArray[I][0], InVectorArray[I][1], InVectorArray[I][2]);
					}
					return OutVectorArray;
				};
				
				TMap<EHeadFitToTargetMeshes, TArray<FVector3f>> ConformalVerticesToUse;
				ConformalVerticesToUse.Add(EHeadFitToTargetMeshes::Head, ConvertArrayToFVector3f(ConformalVertices[EIdentityPartMeshes::Head]));

				if (InImportParams.bUseEyeMeshes)
				{
					if (!ConformalVertices.Find(EIdentityPartMeshes::LeftEye) || !ConformalVertices.Find(EIdentityPartMeshes::RightEye))
					{
						UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected identity must contain conformed eye meshes in order to be imported into MetaHumanCharacter asset with the eye meshes option selected"));
						return EImportErrorCode::NoEyeMeshesPresent;
					}
					else
					{
						ConformalVerticesToUse.Add(EHeadFitToTargetMeshes::LeftEye, ConvertArrayToFVector3f(ConformalVertices[EIdentityPartMeshes::LeftEye]));
						ConformalVerticesToUse.Add(EHeadFitToTargetMeshes::RightEye, ConvertArrayToFVector3f(ConformalVertices[EIdentityPartMeshes::RightEye]));
					}
				}

				if (InImportParams.bUseTeethMesh)
				{
					if (!ConformalVertices.Find(EIdentityPartMeshes::Teeth))
					{
						UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected identity must contain conformed teeth mesh in order to be imported into MetaHumanCharacter asset with the eye meshes option selected"));
						return EImportErrorCode::NoTeethMeshPresent;
					}
					else
					{
						ConformalVerticesToUse.Add(EHeadFitToTargetMeshes::Teeth, ConvertArrayToFVector3f(ConformalVertices[EIdentityPartMeshes::Teeth]));
					}
				}

				// we always adapt the neck for Identity fitting
				FFitToTargetOptions FitToTargetOptions{ EAlignmentOptions::ScalingRotationTranslation, /*bAdaptNeck*/ true, /*bDisableHighFrequencyDelta*/ true };

				if (InImportParams.bUseMetricScale)
				{
					FitToTargetOptions.AlignmentOptions = EAlignmentOptions::RotationTranslation;
				}

				const bool bFitted = FitStateToTargetVertices(InMetaHumanCharacter, ConformalVerticesToUse, FitToTargetOptions);

				if (bFitted)
				{
					CommitFaceState(InMetaHumanCharacter, GetFaceState(InMetaHumanCharacter));
					return EImportErrorCode::Success;
				}
				else
				{
					return	EImportErrorCode::FittingError;
				}
			}
		}
		else
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Identity has not been conformed."));
			return	EImportErrorCode::IdentityNotConformed;
		}
	}

	return EImportErrorCode::IdentityNotConformed;
}

EImportErrorCode UMetaHumanCharacterEditorSubsystem::ImportFromTemplate(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TNotNull<UObject*> InTemplateMesh, UObject* InTemplateLeftEyeMesh, UObject* InTemplateRightEyeMesh, UObject* InTemplateTeethMesh, const FImportFromTemplateParams& InImportParams)
{
	TMap<EHeadFitToTargetMeshes, TArray<FVector3f>> ConformalVertices;
	EImportErrorCode ErrorCode = GetDataForConforming(InTemplateMesh, InTemplateLeftEyeMesh, InTemplateRightEyeMesh, InTemplateTeethMesh, InImportParams, ConformalVertices);

	if (ErrorCode != EImportErrorCode::Success)
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Selected asset must be a SkelMesh or Static Mesh consistent with MetaHuman topology to be imported into MetaHumanCharacter asset"));
		return ErrorCode;
	}

	FFitToTargetOptions FitToTargetOptions{ InImportParams.AlignmentOptions, InImportParams.bAdaptNeck, /*bDisableHighFrequencyDelta*/ true};

	const bool bFitted = FitStateToTargetVertices(InMetaHumanCharacter, ConformalVertices, FitToTargetOptions);

	if (bFitted)
	{
		CommitFaceState(InMetaHumanCharacter, GetFaceState(InMetaHumanCharacter));
		return EImportErrorCode::Success;
	}
	else
	{
		return EImportErrorCode::FittingError;
	}
}

void UMetaHumanCharacterEditorSubsystem::InitializeFromPreset(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, TNotNull<UMetaHumanCharacter*> InPresetCharacter)
{
	TSharedRef<FMetaHumanCharacterEditorData> CharacterData = CharacterDataMap[InMetaHumanCharacter];

	FScopedSlowTask InitFromPresetTask{ 4.0f, LOCTEXT("InitFromPresetTask", "Initializing character from a Preset") };
	InitFromPresetTask.MakeDialog();

	InitFromPresetTask.EnterProgressFrame();

	// Apply face state
	if (!CharacterData->FaceState->Deserialize(InPresetCharacter->GetFaceStateData()))
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("%s: Failed to deserialize face state stored in Preset Character asset"), *InPresetCharacter->GetFullName());
		return;
	}
	FMetaHumanCharacterIdentity::FSettings Settings = CharacterData->FaceState->GetSettings();
	Settings.SetGlobalVertexDeltaScale(InPresetCharacter->FaceEvaluationSettings.GlobalDelta);
	Settings.SetGlobalHighFrequencyScale(InPresetCharacter->FaceEvaluationSettings.HighFrequencyDelta);
	CharacterData->FaceState->SetSettings(Settings);
	CharacterData->FaceState->SetHighFrequenctVariant(InPresetCharacter->SkinSettings.Skin.FaceTextureIndex);
	CharacterData->FaceState->SetFaceScale(InPresetCharacter->FaceEvaluationSettings.HeadScale);
	CommitFaceState(InMetaHumanCharacter, CharacterData->FaceState);

	InitFromPresetTask.EnterProgressFrame();
	// Apply body state
	if (!CharacterData->BodyState->Deserialize(InPresetCharacter->GetBodyStateData()))
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("%s: Failed to deserialize body state stored in Preset Character asset"), *InPresetCharacter->GetFullName());
		return;
	}
	CommitBodyState(InMetaHumanCharacter, CharacterData->BodyState);

	if (InPresetCharacter->HasBodyDNA())
	{
		// If there is a body DNA available, then apply to actor
		TArray<uint8> BodyDNABuffer = InPresetCharacter->GetBodyDNABuffer();
		TSharedPtr<IDNAReader> BodyDNAReader = ReadDNAFromBuffer(&BodyDNABuffer);
		if (!BodyDNAReader.IsValid())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("%s: Failed to read body DNA stored in Preset Character asset"), *InPresetCharacter->GetFullName());
		}
		// Set the Behavior part of DNA in skeletal mesh AssetUserData
		if (UAssetUserData* UserData = CharacterData->BodyMesh->GetAssetUserDataOfClass(UDNAAsset::StaticClass()))
		{
			UDNAAsset* DNAAsset = CastChecked<UDNAAsset>(UserData);
			DNAAsset->SetBehaviorReader(BodyDNAReader);
			DNAAsset->SetGeometryReader(BodyDNAReader);
			InMetaHumanCharacter->SetBodyDNABuffer(ReadStreamFromDNA(BodyDNAReader.Get(), EDNADataLayer::All));
			InMetaHumanCharacter->MarkPackageDirty();
		}
	}

	if (InPresetCharacter->HasFaceDNA())
	{
		// then update the dna
		TArray<uint8> FaceDNABuffer = InPresetCharacter->GetFaceDNABuffer();
		TSharedPtr<IDNAReader> FaceDNAReader = ReadDNAFromBuffer(&FaceDNABuffer);
		if (!FaceDNAReader.IsValid())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("%s: Failed to read face DNA stored in Preset Character asset"), *InPresetCharacter->GetFullName());
		}
		else
		{
			TSharedPtr<IDNAReader> OutDna = ApplyFaceDNA(InMetaHumanCharacter, FaceDNAReader, ELodUpdateOption::All);

			if (OutDna.IsValid())
			{
				// Store the DNA into the character asset
				InMetaHumanCharacter->SetFaceDNABuffer(ReadStreamFromDNA(OutDna.Get(), EDNADataLayer::All), OutDna->GetBlendShapeChannelCount() > 0);
				InMetaHumanCharacter->MarkPackageDirty();
			}

			InMetaHumanCharacter->OnRiggingStateChanged.Broadcast();
		}
	}

	InitFromPresetTask.EnterProgressFrame();

	CharacterData->SkinSettings.Reset();
	if (InPresetCharacter->HasSynthesizedTextures())
	{
		// If the preset has synthesized texture copy the skin settings to the character data
		// and initialize the synthesized texture info in the character to match
		CharacterData->SkinSettings = InPresetCharacter->SkinSettings;
		InMetaHumanCharacter->SynthesizedFaceTexturesInfo = InPresetCharacter->SynthesizedFaceTexturesInfo;

		// Empty this arrays so they can be initialized by InitSynthesizedFaceData
		CharacterData->CachedSynthesizedImages.Empty();
		InMetaHumanCharacter->SynthesizedFaceTextures.Empty();

		// Also initialize the cached image array to match the sizes of the textures to be copied from the preset
		FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData(FaceTextureSynthesizer,
			InMetaHumanCharacter->SynthesizedFaceTexturesInfo,
			InMetaHumanCharacter->SynthesizedFaceTextures,
			CharacterData->CachedSynthesizedImages);

		// If we have synthesized textures on a preset character, make an async request to load the data.
		for (const TPair<EFaceTextureType, FMetaHumanCharacterTextureInfo>& TextureInfoPair : InPresetCharacter->SynthesizedFaceTexturesInfo)
		{
			const EFaceTextureType TextureType = TextureInfoPair.Key;

			CharacterData->SynthesizedFaceTexturesFutures.FindOrAdd(TextureType) = InPresetCharacter->GetSynthesizedFaceTextureDataAsync(TextureType);
		}
	}

	// Do the same for the body textures
	InMetaHumanCharacter->HighResBodyTexturesInfo = InPresetCharacter->HighResBodyTexturesInfo;
	InMetaHumanCharacter->BodyTextures.Empty();
	FMetaHumanCharacterBodyTextureUtils::InitBodyTextureData(InPresetCharacter->SkinSettings.Skin, InMetaHumanCharacter->HighResBodyTexturesInfo, InMetaHumanCharacter->BodyTextures);

	for (const TPair<EBodyTextureType, FMetaHumanCharacterTextureInfo>& TextureInfoPair : InPresetCharacter->HighResBodyTexturesInfo)
	{
		const EBodyTextureType TextureType = TextureInfoPair.Key;

		CharacterData->HighResBodyTexturesFutures.FindOrAdd(TextureType) = InPresetCharacter->GetHighResBodyTextureDataAsync(TextureType);
	}

	// Need to wait for textures to be loaded here since CommitSettings will store the textures from the cache into the character
	WaitForSynthesizedTextures(InMetaHumanCharacter, CharacterData, InMetaHumanCharacter->SynthesizedFaceTextures, InMetaHumanCharacter->BodyTextures);

	UpdateCharacterPreviewMaterial(InMetaHumanCharacter, InPresetCharacter->PreviewMaterialType);
	CommitSkinSettings(InMetaHumanCharacter, InPresetCharacter->SkinSettings);
	CommitMakeupSettings(InMetaHumanCharacter, InPresetCharacter->MakeupSettings);
	CommitEyesSettings(InMetaHumanCharacter, InPresetCharacter->EyesSettings);
	CommitHeadModelSettings(InMetaHumanCharacter, InPresetCharacter->HeadModelSettings);

	TNotNull<UMetaHumanCollection*> TargetCollection = InMetaHumanCharacter->GetMutableInternalCollection();
	TArray<FName> SpecificationSlotNames;
	TargetCollection->GetPipeline()->GetSpecification()->Slots.GetKeys(SpecificationSlotNames);
	// Remove all the existing palette items from the editing character.
	for (FName SlotName : SpecificationSlotNames)
	{
		if (SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			continue;
		}
		TargetCollection->RemoveAllItemsForSlot(SlotName);
	}
	// Remove all existing selections from Target instance.
	TNotNull<UMetaHumanCharacterInstance*> TargetInstance = TargetCollection->GetMutableDefaultInstance();
	TArray<FMetaHumanPipelineSlotSelectionData> ExistingSlotSelections = TargetInstance->GetSlotSelectionData();
	for (const FMetaHumanPipelineSlotSelectionData& ExistingSelection : ExistingSlotSelections)
	{
		if (ExistingSelection.Selection.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			continue;
		}
		if (!TargetInstance->TryRemoveSlotSelection(ExistingSelection.Selection))
		{
			UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Could not remove slot selection %s from the character mutable instance."), *ExistingSelection.Selection.SlotName.ToString());
		}
	}
	TNotNull<UMetaHumanCollection*> SourceCollection = InPresetCharacter->GetMutableInternalCollection();
	const TArray<FMetaHumanCharacterPaletteItem>& PresetItems = SourceCollection->GetItems();
	InitFromPresetTask.EnterProgressFrame();
	// Copy all used collection items.
	for (int32 ItemIndex = 0; ItemIndex < PresetItems.Num(); ItemIndex++)
	{
		const FMetaHumanCharacterPaletteItem& PresetItem = PresetItems[ItemIndex];
		if (PresetItem.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character
			|| PresetItem.SlotName == NAME_None
			|| !PresetItem.WardrobeItem)
		{
			continue;
		}

		FMetaHumanPaletteItemKey PaletteItemKey = PresetItem.GetItemKey();
		const FMetaHumanPipelineSlotSelection SlotSelectionItem(PresetItem.SlotName, PaletteItemKey);
		if (SourceCollection->GetMutableDefaultInstance()->ContainsSlotSelection(SlotSelectionItem))
		{
			FMetaHumanCharacterPaletteItem CopyItem;
			CopyItem.DisplayName = PresetItem.DisplayName;
			CopyItem.SlotName = PresetItem.SlotName;
			CopyItem.Variation = PresetItem.Variation;

			if (PresetItem.WardrobeItem->IsExternal())
			{
				CopyItem.WardrobeItem = PresetItem.WardrobeItem;
			}
			else
			{
				CopyItem.WardrobeItem = DuplicateObject<UMetaHumanWardrobeItem>(PresetItem.WardrobeItem, TargetCollection);
			}

			if ((!TargetCollection->TryAddItem(CopyItem)))
			{
				UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Failed to copy wardobe item %s from a preset."), *CopyItem.DisplayName.ToString());
			}
		}
	}

	// Copy over all selections from Source instance.
	TNotNull<const UMetaHumanCharacterInstance*> SourceInstance = SourceCollection->GetMutableDefaultInstance();
	for (const FMetaHumanPipelineSlotSelectionData& SourceSelectionData : SourceInstance->GetSlotSelectionData())
	{
		if (SourceSelectionData.Selection.SlotName != UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			if (!TargetInstance->TryAddSlotSelection(SourceSelectionData.Selection))
			{
				UE_LOG(LogMetaHumanCharacterEditor, Warning, TEXT("Failed to copy wardrobe %s selection from a preset."), *SourceSelectionData.Selection.SlotName.ToString());
			}
		}
	}
	// Copy parameter overrides from Source instance.
	for (const TPair<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& Pair : SourceInstance->GetOverriddenInstanceParameters())
	{
		TargetInstance->OverrideInstanceParameters(Pair.Key, Pair.Value);
	}

	RunCharacterEditorPipelineForPreview(InMetaHumanCharacter);
}

static TArray<FVector3f> GetVertices(const UStaticMesh* InTemplateStaticMesh)
{
	FMeshDescription* MeshDescription = InTemplateStaticMesh->GetMeshDescription(0); // only 1 mesh for static mesh
	check(MeshDescription);
	FStaticMeshAttributes Attributes(*MeshDescription);

	TVertexAttributesRef<FVector3f> OriginalMeshVerts = Attributes.GetVertexPositions();
	TTriangleAttributesRef<TArrayView<FVertexID>>  OriginalMeshIndices = Attributes.GetTriangleVertexIndices();

	TArray<FVector3f> CurVertices;
	CurVertices.Reset(OriginalMeshVerts.GetNumElements());

	for (int32 RenderCtr = 0; RenderCtr < OriginalMeshVerts.GetNumElements(); ++RenderCtr)
	{
		const FVector3f OriginalVertex = OriginalMeshVerts.Get(RenderCtr);
		CurVertices.Add(OriginalVertex);
	}

	return CurVertices;
}

static bool GetVertices(USkeletalMesh* InTemplateSkeletalMesh, FDNAToSkelMeshMap* DNAToSkelMeshMap, int32 InLodIndex, int32 InMeshIndex, TArray<FVector3f>& OutVertices)
{
	OutVertices.Empty();
	OutVertices.AddUninitialized(DNAToSkelMeshMap->ImportDNAVtxToUEVtxIndex[InLodIndex][InMeshIndex].Num());
	const FSkeletalMeshLODModel& LODModel = InTemplateSkeletalMesh->GetImportedModel()->LODModels[InLodIndex];

	TArray<bool> VerticesSet;
	VerticesSet.Init(false, OutVertices.Num());

	int32 TotalNumSoftVertices = 0;
	for (const FSkelMeshSection& Section : LODModel.Sections)
	{
		TotalNumSoftVertices += Section.GetNumVertices();
	}

	for (const FSkelMeshSection& Section : LODModel.Sections)
	{
		const int32& DNAMeshIndex = DNAToSkelMeshMap->ImportVtxToDNAMeshIndex[InLodIndex][Section.GetVertexBufferIndex()];
		if (DNAMeshIndex == InMeshIndex)
		{
			const int32 NumSoftVertices = Section.GetNumVertices();
			int32 VertexBufferIndex = Section.GetVertexBufferIndex();

			for (int32 VertexIndex = 0; VertexIndex < NumSoftVertices; VertexIndex++)
			{
				const int32& DNAVertexIndex = DNAToSkelMeshMap->ImportVtxToDNAVtxIndex[InLodIndex][VertexBufferIndex++];

				if (DNAVertexIndex >= 0 && DNAVertexIndex < OutVertices.Num())
				{
					const FSoftSkinVertex& Vertex = Section.SoftVertices[VertexIndex];
					OutVertices[DNAVertexIndex] = FVector3f{ Vertex.Position.X, Vertex.Position.Y, Vertex.Position.Z };
					VerticesSet[DNAVertexIndex] = true;
				}
				else
				{
					return false;
				}
			}
		}
	}

	int32 NumSetVertices = 0;
	for (int32 V = 0; V < VerticesSet.Num(); ++V)
	{
		if (VerticesSet[V])
		{
			NumSetVertices++;
		}
	}
	if (NumSetVertices != OutVertices.Num())
	{
		return false;
	}

	return true;
}

EImportErrorCode UMetaHumanCharacterEditorSubsystem::GetDataForConforming(TNotNull<UObject*> InTemplateMesh, UObject* InTemplateLeftEyeMesh, UObject* InTemplateRightEyeMesh, UObject* InTemplateTeethMesh, const FImportFromTemplateParams& InImportParams, TMap<EHeadFitToTargetMeshes, TArray<FVector3f>>& OutVertices)const
{
#if WITH_EDITOR
	const int32 Template2MHLODIndex = 0;
	const int32 Template2MHHeadMeshIndex = 0;
	const int32 Template2MHTeethIndex = 1;
	const int32 Template2MHEyeLeftIndex = 3;
	const int32 Template2MHEyeRightIndex = 4;
	// TODO hard-coded for now; move these into the API and pass them in
	const int32 NumHeadMeshVertices = 24049;
	const int32 NumEyeMeshVertices = 770;
	const int32 NumTeethMeshVertices = 4246;


	OutVertices.Empty();

	TArray<int32> MeshIndices = { Template2MHHeadMeshIndex };
	TArray<EHeadFitToTargetMeshes> MeshTypes{ EHeadFitToTargetMeshes::Head };
	if (InImportParams.bUseEyeMeshes)
	{
		MeshIndices.Add(Template2MHEyeLeftIndex);
		MeshIndices.Add(Template2MHEyeRightIndex);
		MeshTypes.Add(EHeadFitToTargetMeshes::LeftEye);
		MeshTypes.Add(EHeadFitToTargetMeshes::RightEye);
	}
	if (InImportParams.bUseTeethMesh)
	{
		MeshIndices.Add(Template2MHTeethIndex);
		MeshTypes.Add(EHeadFitToTargetMeshes::Teeth);
	}

	if (USkeletalMesh* TemplateSkeletalMesh = Cast<USkeletalMesh>(InTemplateMesh))
	{
		FSkeletalMeshModel* ImportedModel = TemplateSkeletalMesh->GetImportedModel();

		if (Template2MHLODIndex >= ImportedModel->LODModels.Num())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Skel Mesh does not match MH topology"));
			return EImportErrorCode::InvalidInputData;
		}

		if (TSharedPtr<IDNAReader> ArchetypeDnaReader = UMetaHumanIdentityFace::GetPluginArchetypeDNAReader())
		{
			uint16 NumDNALODs = ArchetypeDnaReader.Get()->GetLODCount();
			if (Template2MHLODIndex >= static_cast<int32>(NumDNALODs))
			{
				UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Skel Mesh does not match MH topology"));
				return EImportErrorCode::InvalidInputData;
			}

			FDNAToSkelMeshMap* DNAToSkelMeshMap = USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(ArchetypeDnaReader.Get(), TemplateSkeletalMesh);

			for (int32 Mesh = 0; Mesh < MeshIndices.Num(); ++Mesh)
			{
				TArray<FVector3f> CurVertices;
				if (!GetVertices(TemplateSkeletalMesh, DNAToSkelMeshMap, Template2MHLODIndex, MeshIndices[Mesh], CurVertices))
				{
					UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Skel Mesh does not match MH topology"));
					return EImportErrorCode::InvalidInputData;
				}

				OutVertices.Add(MeshTypes[Mesh], CurVertices); 
			}
		}
		else
		{
			return EImportErrorCode::GeneralError;
		}

	}
	else if (const UStaticMesh* TemplateStaticMesh = Cast<UStaticMesh>(InTemplateMesh))
	{
		OutVertices.Add(EHeadFitToTargetMeshes::Head, GetVertices(TemplateStaticMesh));
		if (OutVertices[EHeadFitToTargetMeshes::Head].Num() != NumHeadMeshVertices)
		{
			return EImportErrorCode::InvalidHeadMesh;
		}

		// add optional eye and teeth meshes
		if (const UStaticMesh* TemplateLeftEyeStaticMesh = Cast<UStaticMesh>(InTemplateLeftEyeMesh))
		{
			OutVertices.Add(EHeadFitToTargetMeshes::LeftEye, GetVertices(TemplateLeftEyeStaticMesh));
			if (OutVertices[EHeadFitToTargetMeshes::LeftEye].Num() != NumEyeMeshVertices)
			{
				return EImportErrorCode::InvalidLeftEyeMesh;
			}
		}
		if (const UStaticMesh* TemplateRightEyeStaticMesh = Cast<UStaticMesh>(InTemplateRightEyeMesh))
		{
			OutVertices.Add(EHeadFitToTargetMeshes::RightEye, GetVertices(TemplateRightEyeStaticMesh));
			if (OutVertices[EHeadFitToTargetMeshes::RightEye].Num() != NumEyeMeshVertices)
			{
				return EImportErrorCode::InvalidRightEyeMesh;
			}
		}
		if (const UStaticMesh* TemplateTeethStaticMesh = Cast<UStaticMesh>(InTemplateTeethMesh))
		{
			OutVertices.Add(EHeadFitToTargetMeshes::Teeth, GetVertices(TemplateTeethStaticMesh));
			if (OutVertices[EHeadFitToTargetMeshes::Teeth].Num() != NumTeethMeshVertices)
			{
				return EImportErrorCode::InvalidTeethMesh;
			}
		}
	}
	else
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Mesh is invalid"));
		return EImportErrorCode::InvalidInputData;
	}
#endif

	return EImportErrorCode::Success;
}

EImportErrorCode UMetaHumanCharacterEditorSubsystem::GetDataForBodyConforming(TNotNull<UObject*> InTemplateMesh, TArray<FVector3f>& OutVertices) const
{
	OutVertices.Empty();

#if WITH_EDITOR
	const int32 Template2MHLODIndex = 0;
	const int32 NumBodyMeshVertices = 30455; // TODO hardcoded for now; make this a function of the body API
	const int32 NumCombinedBodyMeshVertices = 54412; // TODO hardcoded for now; make this a function of the body API
	
	if (USkeletalMesh* TemplateSkeletalMesh = Cast<USkeletalMesh>(InTemplateMesh))
	{
		FSkeletalMeshModel* ImportedModel = TemplateSkeletalMesh->GetImportedModel();

		if (Template2MHLODIndex >= ImportedModel->LODModels.Num())
		{
			UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Mesh does not match MH topology"));
			return EImportErrorCode::InvalidInputData;
		}

		if (TSharedPtr<IDNAReader> ArchetypeDnaReader = UE::MetaHuman::GetDNAReader(GetBodyArchetypeMesh(EMetaHumanCharacterTemplateType::MetaHuman)))
		{
			uint16 NumDNALODs = ArchetypeDnaReader.Get()->GetLODCount();
			if (Template2MHLODIndex >= static_cast<int32>(NumDNALODs))
			{
				UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Skel Mesh does not match MH topology"));
				return EImportErrorCode::InvalidInputData;
			}

			FDNAToSkelMeshMap* DNAToSkelMeshMap = USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(ArchetypeDnaReader.Get(), TemplateSkeletalMesh);
			if (!GetVertices(TemplateSkeletalMesh, DNAToSkelMeshMap, Template2MHLODIndex, 0, OutVertices))
			{
				FString CombinedBodyModelPath = FMetaHumanCharacterSkelMeshUtils::GetArchetypeDNAPath(EMetaHumanImportDNAType::Combined);
				TSharedPtr<IDNAReader> CombinedArchetypeDnaReader = ReadDNAFromFile(CombinedBodyModelPath);
				FDNAToSkelMeshMap* DNAToSkelMeshMapCombined = USkelMeshDNAUtils::CreateMapForUpdatingNeutralMesh(CombinedArchetypeDnaReader.Get(), 
					TemplateSkeletalMesh);

				if (!GetVertices(TemplateSkeletalMesh, DNAToSkelMeshMapCombined, Template2MHLODIndex, 0, OutVertices))
				{
					UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Mesh does not match MH topology"));
					return EImportErrorCode::InvalidInputData;
				}
			}
		}
		else
		{
			return EImportErrorCode::GeneralError; 
		}
	}
	else if (const UStaticMesh* TemplateStaticMesh = Cast<UStaticMesh>(InTemplateMesh))
	{
		OutVertices = GetVertices(TemplateStaticMesh);
		if (OutVertices.Num() != NumBodyMeshVertices && OutVertices.Num() != NumCombinedBodyMeshVertices)
		{
			return EImportErrorCode::InvalidInputData;
		}
	}
	else
	{
		UE_LOG(LogMetaHumanCharacterEditor, Error, TEXT("Failed to get data for conforming as Template Mesh is invalid"));
		return EImportErrorCode::InvalidInputData;
	}
#endif

	return EImportErrorCode::Success;
}

#undef LOCTEXT_NAMESPACE
