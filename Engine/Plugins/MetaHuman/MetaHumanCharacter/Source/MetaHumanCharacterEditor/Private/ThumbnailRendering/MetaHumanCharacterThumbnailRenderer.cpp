// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterThumbnailRenderer.h"
#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterEditorActorInterface.h"
#include "MetaHumanCharacterEditorLog.h"
#include "Subsystem/MetaHumanCharacterSkinMaterials.h"

#include "SceneView.h"
#include "Components/SkeletalMeshComponent.h"
#include "ObjectTools.h"
#include "Engine/LevelStreamingDynamic.h"
#include "ContentStreaming.h"
#include "Components/SkinnedMeshComponent.h"
#include "SkeletalRenderPublic.h"
#include "Engine/Level.h"
#include "Materials/MaterialInstanceDynamic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanCharacterThumbnailRenderer)


namespace UE::MetaHuman::Private
{
	void LoadLevelsInWorld(TNotNull<UWorld*> InWorld, const TArray<TSoftObjectPtr<UWorld>>& InLevels)
	{
		TArray<ULevelStreaming*> LoadedLevels;

		for (const TSoftObjectPtr<UWorld>& LevelPath : InLevels)
		{
			bool bLoadedSuccesful = false;
			const bool bLoadAsTempPackage = true;

			ULevelStreamingDynamic* StreamingLevel = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
				InWorld,
				LevelPath,
				FTransform::Identity,
				bLoadedSuccesful,
				TEXT(""),
				nullptr,
				bLoadAsTempPackage);
			check(bLoadedSuccesful);
			check(StreamingLevel);

			StreamingLevel->SetShouldBeVisibleInEditor(true);
			LoadedLevels.Add(StreamingLevel);
		}

		InWorld->FlushLevelStreaming(EFlushLevelStreamingType::Full);

		for (ULevelStreaming* StreamingLevel : LoadedLevels)
		{
			if (ULevel* NewLevel = StreamingLevel->GetLoadedLevel())
			{
				NewLevel->SetLightingScenario(true);
			}
		}
	}
} // namespace UE::MetaHuman::Private

/////////////////////////////////////////////////////
// FMetaHumanCharacterThumbnailScene

FMetaHumanCharacterThumbnailScene::FMetaHumanCharacterThumbnailScene()
	: FThumbnailPreviewScene(FThumbnailPreviewScene::FConstructionValues()
		.SetCreateSkySphere(false)
		.SetDefaultLightingThumbnailScene(false)
		.SetSkyCubeMap(false)
		.SetCreateFloorPlane(false)
	)
	, CameraPosition(EMetaHumanCharacterThumbnailCameraPosition::Character_Body)
{
	SetLightBrightness(0.0f);

	bForceAllUsedMipsResident = false;

	// All thumbnails should be rendered with the same lighting scenarios
	TArray<TSoftObjectPtr<UWorld>> LevelsToLoad =
	{
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/Studio.Studio"))),
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_BaseEnvironment.L_BaseEnvironment"))),
		TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/" UE_PLUGIN_NAME "/LightingEnvironments/L_PostProcessing.L_PostProcessing"))),
	};

	UE::MetaHuman::Private::LoadLevelsInWorld(GetWorld(), LevelsToLoad);
}

void FMetaHumanCharacterThumbnailScene::CreatePreview(UMetaHumanCharacter* InCharacter, EMetaHumanCharacterThumbnailCameraPosition InCameraPosition)
{
	Character = InCharacter;
	CameraPosition = InCameraPosition;

	PreviewActor = UMetaHumanCharacterEditorSubsystem::Get()->CreateMetaHumanCharacterEditorActor(Character.Get(), GetWorld());

	// Force LOD0 for the thumbnail
	PreviewActor->SetForcedLOD(0);

	// Nothing special to setup for the character camera view, use the character actor as-is.
	if (InCameraPosition == EMetaHumanCharacterThumbnailCameraPosition::Character_Body ||
		InCameraPosition == EMetaHumanCharacterThumbnailCameraPosition::Character_Face)
	{
		return;
	}

	AActor* Actor = Cast<AActor>(PreviewActor.GetObject());
	check(Actor);

	TArray<UActorComponent*> ActorComponents;
	Actor->GetComponents(ActorComponents);

	// Hide all scene components except for the body and face
	for (UActorComponent* Component : ActorComponents)
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			if (SceneComponent != PreviewActor->GetFaceComponent() &&
				SceneComponent != PreviewActor->GetBodyComponent())
			{
				SceneComponent->SetVisibility(false);
			}
		}
	}

	// Thumbnails for face and body should be rendered with clay preview material
	const EMetaHumanCharacterSkinPreviewMaterial PreviewMode = EMetaHumanCharacterSkinPreviewMaterial::Clay;

	FMetaHumanCharacterFaceMaterialSet HeadMaterialSet = FMetaHumanCharacterSkinMaterials::GetHeadPreviewMaterialInstance(PreviewMode);

	UMaterialInstanceDynamic* BodyPreviewMaterialInstance = FMetaHumanCharacterSkinMaterials::GetBodyPreviewMaterialInstance(PreviewMode);
	check(BodyPreviewMaterialInstance);

	// Switch to clay preview material by updating the parameter on face and body skeletal meshes
	HeadMaterialSet.ForEachSkinMaterial<UMaterialInstanceDynamic>(
		[](EMetaHumanCharacterSkinMaterialSlot, UMaterialInstanceDynamic* Material)
		{
			Material->SetScalarParameterValue(TEXT("ClayMaterial"), 1.0f);
		}
	);

	BodyPreviewMaterialInstance->SetScalarParameterValue(TEXT("ClayMaterial"), 1.0f);

	// There are no utilities at the moment to update the skel mesh components with
	// the proper materials. As we're forcing LOD0, we can hard-code materials here
	USkeletalMeshComponent* FaceComp = const_cast<USkeletalMeshComponent*>(static_cast<const USkeletalMeshComponent*>(PreviewActor->GetFaceComponent()));
	FaceComp->SetMaterial( 0, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD0]);
	FaceComp->SetMaterial( 1, HeadMaterialSet.Teeth);
	FaceComp->SetMaterial( 2, nullptr);
	FaceComp->SetMaterial( 3, HeadMaterialSet.EyeRight);
	FaceComp->SetMaterial( 4, HeadMaterialSet.EyeLeft);
	FaceComp->SetMaterial( 5, nullptr);
	FaceComp->SetMaterial( 6, nullptr);
	FaceComp->SetMaterial( 7, nullptr);
	FaceComp->SetMaterial( 8, nullptr);
	FaceComp->SetMaterial( 9, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD1]);
	FaceComp->SetMaterial(10, nullptr);
	FaceComp->SetMaterial(11, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD2]);
	FaceComp->SetMaterial(12, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD3]);
	FaceComp->SetMaterial(13, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD4]);
	FaceComp->SetMaterial(14, HeadMaterialSet.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD5to7]);

	USkeletalMeshComponent* BodyComp = const_cast<USkeletalMeshComponent*>(static_cast<const USkeletalMeshComponent*>(PreviewActor->GetBodyComponent()));
	BodyComp->SetMaterial(0, BodyPreviewMaterialInstance);

	// Force render thread to pick up material changes at once - if we don't do this
	// on the first run (when the scene is initiated) we'll end up with world grid
	// materials on the skeletal meshes.
	GetWorld()->SendAllEndOfFrameUpdates();
}

void FMetaHumanCharacterThumbnailScene::DestroyPreview()
{
	if (AActor* PrevActor = Cast<AActor>(PreviewActor.GetObject()))
	{
		PrevActor->Destroy();
	}
	PreviewActor = nullptr;
}

void FMetaHumanCharacterThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	if (PreviewActor == nullptr || !Character.IsValid())
	{
		return;
	}

	const int32 LODIndex = 0;
	FBoxSphereBounds Bounds{};
	float ZoomFactor = 1.0f;
	FVector Offset{ 0, 0, 0 };

	switch (CameraPosition)
	{
	case EMetaHumanCharacterThumbnailCameraPosition::Character_Face:
	case EMetaHumanCharacterThumbnailCameraPosition::Face:
	{
		USkeletalMeshComponent* FaceComp = const_cast<USkeletalMeshComponent*>(static_cast<const USkeletalMeshComponent*>(PreviewActor->GetFaceComponent()));
		FaceComp->UpdateBounds();
		Bounds = PreviewActor->GetFaceComponent()->Bounds;
		ZoomFactor = 0.8f;
		Offset = FVector{ 0, 0, 0.4f };
		break;
	}
	case EMetaHumanCharacterThumbnailCameraPosition::Character_Body:
	case EMetaHumanCharacterThumbnailCameraPosition::Body:
	{
		USkeletalMeshComponent* BodyComp = const_cast<USkeletalMeshComponent*>(static_cast<const USkeletalMeshComponent*>(PreviewActor->GetBodyComponent()));
		BodyComp->UpdateBounds();
		Bounds = PreviewActor->GetBodyComponent()->Bounds;
		ZoomFactor = 0.95f;
		break;
	}
	default:
		checkNoEntry()
	}

	Bounds.Origin += Bounds.BoxExtent * Offset;
	Bounds = Bounds.ExpandBy((ZoomFactor - 1.0f) * Bounds.SphereRadius);
	float Radius = FMath::Max<FVector::FReal>(Bounds.GetBox().GetExtent().Size(), 10.0f);

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	const float HalfMeshSize = Radius;
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);
	
	OutOrigin = FVector(0, 0, -Bounds.Origin.Z);
	OutOrbitPitch = 0.0f;
	OutOrbitYaw = 180.0f;
	OutOrbitZoom = TargetDistance;
}

float FMetaHumanCharacterThumbnailScene::GetFOV() const
{
	// Same FOV as in the scene
	return 18.001738f;
}

/////////////////////////////////////////////////////
// UMetaHumanCharacterThumbnailRenderer

UMetaHumanCharacterThumbnailRenderer::UMetaHumanCharacterThumbnailRenderer()
	: CameraPosition(EMetaHumanCharacterThumbnailCameraPosition::Character_Body)
{
}

bool UMetaHumanCharacterThumbnailRenderer::CanVisualizeAsset(UObject* InObject)
{
	if (UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(InObject))
	{
		return Character->IsCharacterValid()
			&& UMetaHumanCharacterEditorSubsystem::Get()->IsObjectAddedForEditing(Character);
	}

	return false;
}

void UMetaHumanCharacterThumbnailRenderer::Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, FRenderTarget* InRenderTarget, FCanvas* InCanvas, bool bInAdditionalViewFamily)
{
	if (UMetaHumanCharacter* Character = Cast<UMetaHumanCharacter>(InObject))
	{
		if (!ThumbnailScene.IsValid())
		{
			ThumbnailScene = MakeUnique<FMetaHumanCharacterThumbnailScene>();
		}

		ThumbnailScene->CreatePreview(Character, CameraPosition);

		FSceneViewFamilyContext ViewFamily(
			FSceneViewFamily::ConstructionValues(InRenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
			.SetTime(UThumbnailRenderer::GetTime())
			.SetAdditionalViewFamily(bInAdditionalViewFamily));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		RenderViewFamily(InCanvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, InX, InY, InWidth, InHeight));
		
		ThumbnailScene->DestroyPreview();

		// Revert back to using character camera just so we don't accidentally render face or body for the asset thumbnail
		CameraPosition = EMetaHumanCharacterThumbnailCameraPosition::Character_Body;
	}
}

EThumbnailRenderFrequency UMetaHumanCharacterThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	return EThumbnailRenderFrequency::OnAssetSave;
}

void UMetaHumanCharacterThumbnailRenderer::BeginDestroy()
{
	ThumbnailScene.Reset();

	Super::BeginDestroy();
}
