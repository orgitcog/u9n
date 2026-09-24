// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ChaosClothAssetThumbnailRenderer.h"

#include "SceneView.h"
#include "ShowFlags.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"

AChaosClothPreviewActor::AChaosClothPreviewActor()
{
	ClothComponent = CreateDefaultSubobject<UChaosClothComponent>(TEXT("ClothComponent0"));
	RootComponent = ClothComponent;
}

namespace UE::Chaos::ClothAsset
{
	FThumbnailScene::FThumbnailScene() : FThumbnailPreviewScene()
	{
		bForceAllUsedMipsResident = false;

		FActorSpawnParameters SpawnInfo;
		SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnInfo.bNoFail = true;
		SpawnInfo.ObjectFlags = RF_Transient;

		PreviewActor = GetWorld()->SpawnActor<AChaosClothPreviewActor>( SpawnInfo );
		PreviewActor->SetActorEnableCollision(false);

		check(PreviewActor);
	}

	void FThumbnailScene::SetClothAsset(UChaosClothAssetBase* InClothAsset)
	{
		UChaosClothComponent* ClothComponent = PreviewActor->GetClothComponent();
		check(ClothComponent);
		
		ClothComponent->SetAsset(InClothAsset);
	}

	void FThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw,
		float& OutOrbitZoom) const
	{	
		UChaosClothComponent* ClothComponent = PreviewActor->GetClothComponent();
		check(ClothComponent);

		FBoxSphereBounds Bounds = ClothComponent->Bounds;
		Bounds = Bounds.ExpandBy(2.0f);
		
		const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
		const float HalfMeshSize = static_cast<float>(Bounds.SphereRadius); 
		const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

		USceneThumbnailInfo* ThumbnailInfo = Cast<USceneThumbnailInfo>(ClothComponent->GetThumbnailInfo());
		if (ThumbnailInfo)
		{
			if (TargetDistance + ThumbnailInfo->OrbitZoom < 0 )
			{
				ThumbnailInfo->OrbitZoom = -TargetDistance;
			}
		}
		else
		{
			ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
		}

		OutOrigin = -Bounds.Origin;
		OutOrbitPitch = ThumbnailInfo->OrbitPitch;
		OutOrbitYaw = ThumbnailInfo->OrbitYaw;
		OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
	}
}

void UChaosClothAssetThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* Viewport, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UChaosClothAssetBase* AsClothAsset = Cast<UChaosClothAssetBase>(Object);
	if (!AsClothAsset)
	{
		return;
	}
	
	TSharedRef<UE::Chaos::ClothAsset::FThumbnailScene> ThumbnailScene = ClothThumbnailSceneCache.EnsureThumbnailScene(Object);

	ThumbnailScene->SetClothAsset(AsClothAsset);

	FSceneViewFamilyContext ViewFamily( FSceneViewFamily::ConstructionValues( Viewport, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game) )
		.SetTime(UThumbnailRenderer::GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));

	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
	ThumbnailScene->SetClothAsset(nullptr);
}

bool UChaosClothAssetThumbnailRenderer::CanVisualizeAsset(UObject* Object)
{
	return Cast<UChaosClothAssetBase>(Object) != nullptr;
}

EThumbnailRenderFrequency UChaosClothAssetThumbnailRenderer::GetThumbnailRenderFrequency(UObject* Object) const
{
	UChaosClothAssetBase* AsClothAsset = Cast<UChaosClothAssetBase>(Object);
	return AsClothAsset && AsClothAsset->GetResourceForRendering() ? EThumbnailRenderFrequency::Realtime : EThumbnailRenderFrequency::OnPropertyChange;
}

void UChaosClothAssetThumbnailRenderer::BeginDestroy()
{
	ClothThumbnailSceneCache.Clear();
	Super::BeginDestroy();
}