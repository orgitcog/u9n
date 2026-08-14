// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanIdentityThumbnailRenderer.h"
#include "MetaHumanIdentity.h"
#include "MetaHumanIdentityParts.h"
#include "MetaHumanIdentityPose.h"
#include "MetaHumanIdentityPromotedFrames.h"
#include "MetaHumanFootageComponent.h"
#include "MetaHumanViewportModes.h"
#include "ImageSequenceUtils.h"
#include "CaptureData.h"
#include "CameraCalibration.h"

#include "Components/StaticMeshComponent.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Texture2D.h"
#include "Animation/SkeletalMeshActor.h"
#include "SceneView.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "ImgMediaSource.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanIdentityThumbnailRenderer)

static USceneComponent* GetNeutralPoseMeshComponentFromMetaHumanIdentity(UMetaHumanIdentity* InIdentity)
{
	if (InIdentity != nullptr)
	{
		if (UMetaHumanIdentityFace* FacePart = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = FacePart->FindPoseByType(EIdentityPoseType::Neutral))
			{
				return NeutralPose->CaptureDataSceneComponent;
			}
		}
	}

	return nullptr;
}

static FImage GetFrontalFootageFrameAsRGBImage(UMetaHumanIdentity* InIdentity)
{
	if (InIdentity != nullptr)
	{
		if (UMetaHumanIdentityFace* Face = InIdentity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = Face->FindPoseByType(EIdentityPoseType::Neutral))
			{
				if (UFootageCaptureData* FootageCaptureData = Cast<UFootageCaptureData>(NeutralPose->GetCaptureData()))
				{
					int32 ViewIndex = FootageCaptureData->GetViewIndexByCameraName(NeutralPose->Camera);

					if (FootageCaptureData->ImageSequences.IsValidIndex(ViewIndex))
					{
						if (UMetaHumanIdentityFootageFrame* FrontalFootageFrame = Cast<UMetaHumanIdentityFootageFrame>(NeutralPose->GetFrontalViewPromotedFrame()))
						{
							UImgMediaSource* ImageSequence = FootageCaptureData->ImageSequences[ViewIndex];
							TArray<FString> FrameImageNames;
							if (FImageSequenceUtils::GetImageSequenceFilesFromPath(ImageSequence->GetFullPath(), FrameImageNames))
							{
								if (FrameImageNames.IsValidIndex(FrontalFootageFrame->FrameNumber))
								{
									const FString CurrentImagePath = ImageSequence->GetFullPath() / FrameImageNames[FrontalFootageFrame->FrameNumber];

									FImage Image;
									if (FImageUtils::LoadImage(*CurrentImagePath, Image))
									{
										FImage ImageRGB;
										Image.CopyTo(ImageRGB, ERawImageFormat::BGRA8, EGammaSpace::sRGB);

										return ImageRGB;
									}
								}
							}
						}
					}
				}
			}
		}
	}

	return FImage{};
}

/////////////////////////////////////////////////////
// FMetaHumanIdentityThumbnailScene

FMetaHumanIdentityThumbnailScene::FMetaHumanIdentityThumbnailScene()
	: FThumbnailPreviewScene{}
{
	bForceAllUsedMipsResident = false;
}

void FMetaHumanIdentityThumbnailScene::SetMetaHumanIdentity(UMetaHumanIdentity* InIdentity)
{
	if (InIdentity != nullptr)
	{
		Identity = InIdentity;

		if (USceneComponent* PreviewComponent = GetNeutralPoseMeshComponentFromMetaHumanIdentity(InIdentity))
		{
			FActorSpawnParameters SpawnInfo;
			SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			SpawnInfo.bNoFail = true;
			SpawnInfo.ObjectFlags = RF_Transient;

			if (PreviewActor != nullptr)
			{
				// Destroy the previous preview actor as a new one will be created for this identity
				PreviewActor->Destroy();
				PreviewActor = nullptr;
			}

			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PreviewComponent))
			{
				AStaticMeshActor* StaticMeshActor = GetWorld()->SpawnActor<AStaticMeshActor>(SpawnInfo);

				StaticMeshActor->GetStaticMeshComponent()->SetStaticMesh(StaticMeshComponent->GetStaticMesh());

				// Force LOD 0, 0 means auto-select, 1 will force LOD to be 0
				StaticMeshActor->GetStaticMeshComponent()->SetForcedLodModel(1);

				PreviewActor = StaticMeshActor;
			}
			else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(PreviewComponent))
			{
				ASkeletalMeshActor* SkeletalMeshActor = GetWorld()->SpawnActor<ASkeletalMeshActor>(SpawnInfo);

				SkeletalMeshActor->GetSkeletalMeshComponent()->SetSkeletalMesh(SkeletalMeshComponent->GetSkeletalMeshAsset());

				// Force LOD 0, 0 means auto-select, 1 will force LOD to be 0
				SkeletalMeshActor->GetSkeletalMeshComponent()->SetForcedLOD(1);

				PreviewActor = SkeletalMeshActor;
			}
			else if (UMetaHumanFootageComponent* FootageComponent = Cast<UMetaHumanFootageComponent>(PreviewComponent))
			{
				const FImage FrontalFrameImage = GetFrontalFootageFrameAsRGBImage(InIdentity);

				if (FrontalFrameImage.GetWidth() != 0 && FrontalFrameImage.GetHeight() != 0)
				{
					// Recreate the texture if we didn't have one before or the size of the frame changed
					if (!FrameTexture.IsValid() ||
						(FrameTexture.IsValid() && (FrameTexture->GetSurfaceWidth() != FrontalFrameImage.GetWidth() ||
													FrameTexture->GetSurfaceHeight() != FrontalFrameImage.GetHeight())))
					{
						FrameTexture = TStrongObjectPtr<UTexture2D>(FImageUtils::CreateTexture2DFromImage(FrontalFrameImage));
					}
					else
					{
						// In this case, update the existing texture with the contents of the Frontal Footage Frame
						if (uint8* MipData = static_cast<uint8*>(FrameTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE)))
						{
							check(MipData != nullptr);
							int64 MipDataSize = FrameTexture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize();

							ERawImageFormat::Type PixelFormatRawFormat;
							FImageCoreUtils::GetPixelFormatForRawImageFormat(FrontalFrameImage.Format, &PixelFormatRawFormat);

							constexpr uint32 NumSlices = 1;
							FImageView MipImage(MipData, FrontalFrameImage.GetWidth(), FrontalFrameImage.GetHeight(), NumSlices, PixelFormatRawFormat, FrontalFrameImage.GammaSpace);

							FImageCore::CopyImage(FrontalFrameImage, MipImage);

							FrameTexture->GetPlatformData()->Mips[0].BulkData.Unlock();

							FrameTexture->UpdateResource();
						}
					}

					AStaticMeshActor* FootageComponentActor = GetWorld()->SpawnActor<AStaticMeshActor>(SpawnInfo);

					const UStaticMeshComponent* FootagePlaneComponent = FootageComponent->GetFootagePlaneComponent(EABImageViewMode::A);
					FootageComponentActor->GetStaticMeshComponent()->SetStaticMesh(FootagePlaneComponent->GetStaticMesh());
					FootageComponentActor->GetStaticMeshComponent()->SetWorldTransform(FootagePlaneComponent->GetComponentTransform());

					if (UMaterial* FootageThumbnailMaterial = LoadObject<UMaterial>(FootageComponentActor, TEXT("/Script/Engine.Material'/" UE_PLUGIN_NAME "/Exporter/M_ImagePlaneMaterial.M_ImagePlaneMaterial'")))
					{
						UMaterialInstanceDynamic* FootageThumbnailMaterialInstance = UMaterialInstanceDynamic::Create(FootageThumbnailMaterial, FootageComponentActor);
						FootageThumbnailMaterialInstance->SetTextureParameterValue(TEXT("MediaTexture"), FrameTexture.Get());
						FootageThumbnailMaterialInstance->PostEditChange();
						FootageComponentActor->GetStaticMeshComponent()->SetMaterial(0, FootageThumbnailMaterialInstance);
					}

					PreviewActor = FootageComponentActor;
				}
			}

			if (PreviewActor != nullptr && PreviewActor->GetRootComponent() != nullptr)
			{
				PreviewActor->GetRootComponent()->UpdateBounds();
				PreviewActor->SetActorEnableCollision(false);

				// Center the mesh at the world origin then offset to put it on top of the floor plane
				const float BoundsZOffset = GetBoundsZOffset(PreviewComponent->Bounds);
				PreviewActor->SetActorLocation(FVector{ 0.0f, 0.0f, BoundsZOffset });

				PreviewActor->GetRootComponent()->RecreateRenderState_Concurrent();
			}
		}
	}
}

void FMetaHumanIdentityThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	if (PreviewActor == nullptr || !Identity.IsValid())
	{
		return;
	}

	UMetaHumanIdentityThumbnailInfo* ThumbnailInfo = Cast<UMetaHumanIdentityThumbnailInfo>(Identity->ThumbnailInfo);
	if (ThumbnailInfo == nullptr)
	{
		ThumbnailInfo = UMetaHumanIdentityThumbnailInfo::StaticClass()->GetDefaultObject<UMetaHumanIdentityThumbnailInfo>();
	}

	if (UMetaHumanIdentityFace* FacePart = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
	{
		if (UMetaHumanIdentityPose* NeutralPose = FacePart->FindPoseByType(EIdentityPoseType::Neutral))
		{
			if (!NeutralPose->PromotedFrames.IsEmpty())
			{
				const int32 PromotedFrameIndex = ThumbnailInfo->OverridePromotedFrame < NeutralPose->PromotedFrames.Num() ? ThumbnailInfo->OverridePromotedFrame : 0;
				if (UMetaHumanIdentityCameraFrame* PromotedFrame = Cast<UMetaHumanIdentityCameraFrame>(NeutralPose->PromotedFrames[PromotedFrameIndex]))
				{
					const FVector& ViewDirection = PromotedFrame->ViewLocation - PromotedFrame->LookAtLocation;

					// Calculate the Yaw and Pitch by performing a conversion from Cartesian to spherical coordinates
					// By default the thumbnail renderer will rotate the view by 90 degrees so remove that rotation here as we
					// want to preserve exactly what is set in the promoted frame
					const float OrbitYaw = -HALF_PI - FMath::Atan2(ViewDirection.Y, ViewDirection.X);
					const float OrbitPitch = FMath::Atan2(FMath::Sqrt(FMath::Square(ViewDirection.X) + FMath::Square(ViewDirection.Y)), ViewDirection.Z) - HALF_PI;

					const float BoundsZOffset = GetBoundsZOffset(PreviewActor->GetRootComponent()->Bounds);

					OutOrbitYaw = FMath::RadiansToDegrees<float>(OrbitYaw);
					OutOrbitPitch = FMath::RadiansToDegrees<float>(OrbitPitch);

					// Offset the camera on Z to account for the actor offset due to the floor plane
					OutOrigin = -PromotedFrame->LookAtLocation + FVector{ 0.0f, 0.0f, -BoundsZOffset };

					const float BoundsMultiplier = 1.15f;
					OutOrbitZoom = ViewDirection.Length() * BoundsMultiplier;
				}
				else if (UMetaHumanIdentityFootageFrame* FootageFrame = Cast<UMetaHumanIdentityFootageFrame>(NeutralPose->PromotedFrames[PromotedFrameIndex]))
				{
					OutOrbitYaw = 90.0f;
					OutOrbitPitch = 0.0f;
					OutOrigin = FVector::ZeroVector;
					const FVector Extents = PreviewActor->GetRootComponent()->Bounds.GetBox().GetExtent();
					OutOrbitZoom = Extents.Z / FMath::Tan(FMath::DegreesToRadians(InFOVDegrees)) * 1.3f;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////
// UMetaHumanIdentityThumbnailRenderer

bool UMetaHumanIdentityThumbnailRenderer::CanVisualizeAsset(UObject* InObject)
{
	if (UMetaHumanIdentity* Identity = Cast<UMetaHumanIdentity>(InObject))
	{
		if (UMetaHumanIdentityFace* FacePart = Identity->FindPartOfClass<UMetaHumanIdentityFace>())
		{
			if (UMetaHumanIdentityPose* NeutralPose = FacePart->FindPoseByType(EIdentityPoseType::Neutral))
			{
				return NeutralPose->CaptureDataSceneComponent != nullptr && NeutralPose->GetFrontalViewPromotedFrame() != nullptr;
			}
		}
	}

	return false;
}

void UMetaHumanIdentityThumbnailRenderer::Draw(UObject* InObject, int32 InX, int32 InY, uint32 InWidth, uint32 InHeight, FRenderTarget* InRenderTarget, class FCanvas* InCanvas, bool bInAdditionalViewFamily)
{
	if (UMetaHumanIdentity* Identity = Cast<UMetaHumanIdentity>(InObject))
	{
		if (!ThumbnailScene.IsValid())
		{
			ThumbnailScene = MakeUnique<FMetaHumanIdentityThumbnailScene>();
		}

		ThumbnailScene->SetMetaHumanIdentity(Identity);

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(InRenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
										   .SetTime(UThumbnailRenderer::GetTime())
										   .SetAdditionalViewFamily(bInAdditionalViewFamily));

		ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
		ViewFamily.EngineShowFlags.MotionBlur = 0;
		ViewFamily.EngineShowFlags.LOD = 0;

		RenderViewFamily(InCanvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, InX, InY, InWidth, InHeight));
		ThumbnailScene->SetMetaHumanIdentity(nullptr);
	}
}

void UMetaHumanIdentityThumbnailRenderer::BeginDestroy()
{
	ThumbnailScene.Reset();

	Super::BeginDestroy();
}