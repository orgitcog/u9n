// Copyright Epic Games, Inc. All Rights Reserved.
#include "Subsystem/MetaHumanCharacterService.h"

#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterSkelMeshUtils.h"
#include "MetaHumanRigEvaluatedState.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCharacterTextureSynthesis.h"
#include "MetaHumanCharacterBodyTextureUtils.h"
#include "Cloud/MetaHumanTextureSynthesisServiceRequest.h"
#include "Cloud/MetaHumanARServiceRequest.h"
#include "Misc/ScopedSlowTask.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ImageUtils.h"
#include "ImageCoreUtils.h"
#include "Engine/Texture.h"
#include "Logging/StructuredLog.h"
#include "Engine/Texture.h"

#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"

namespace UE::MetaHuman
{
	const FString MeshNameHeadLod0 = TEXT("head_lod0_mesh");
	const FString MeshNameTeethLod0 = TEXT("teeth_lod0_mesh");
	const FString MeshNameEyeLeftLod0 = TEXT("eyeLeft_lod0_mesh");
	const FString MeshNameEyeRightLod0 = TEXT("eyeRight_lod0_mesh");
	const FString MeshNameSalivaLod0 = TEXT("saliva_lod0_mesh");
	const FString MeshNameEyeShellLod0 = TEXT("eyeshell_lod0_mesh");
	const FString MeshNameEyeLashesLod0 = TEXT("eyelashes_lod0_mesh");
	const FString MeshNameEyeEdgeLod0 = TEXT("eyeEdge_lod0_mesh");
	const FString MeshNameCartilageLod0 = TEXT("cartilage_lod0_mesh");

	static void ExtractMeshVertices(TSharedRef<const IDNAReader> InArchetypeDNAReader, const int32 InMeshIndex, const FMetaHumanCharacterIdentity::FState& InState, const TArray<FVector3f>& InConformedVertices, TArray<FVector>& InOutMeshVertices)
	{
		const int32 VertexCount = InArchetypeDNAReader->GetVertexPositionCount(InMeshIndex);
		InOutMeshVertices.Reserve(VertexCount);
		for (int32 VertexIndex = 0; VertexIndex < VertexCount; VertexIndex++)
		{
			InOutMeshVertices.Add(FVector{ InState.GetRawVertex(InConformedVertices, InMeshIndex, VertexIndex) });
		}
	}

	static bool SynthesizeAlbedoWithHFMap(EFaceTextureType InTextureType, FImageView InHFAlbedoImage, const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter)
	{
		if (!InFaceTextureSynthesizer.IsValid())
		{
			return false;
		}

		// Resize the synthesized cache image so that the new higher rez albedo can be generated into it
		FImage& CachedSynthesizedImage = InCharacterData->CachedSynthesizedImages.FindOrAdd(InTextureType);
		if (CachedSynthesizedImage.GetHeight() != InHFAlbedoImage.GetHeight() ||
			CachedSynthesizedImage.GetWidth() != InHFAlbedoImage.GetWidth())
		{
			CachedSynthesizedImage.Init(
				InHFAlbedoImage.GetWidth(),
				InHFAlbedoImage.GetHeight(),
				InFaceTextureSynthesizer.GetTextureFormat(),
				InFaceTextureSynthesizer.GetTextureColorSpace());
		}

		// Store the HF map to a temp buffer in the Character Data
		const int32 AlbedoMapIndex = static_cast<int32>(InTextureType);
		TArray<uint8>& CacheHFMap = InCharacterData->CachedHFAlbedoMaps[AlbedoMapIndex];
		const int64 RequiredBufferSize = InHFAlbedoImage.GetWidth() * InHFAlbedoImage.GetHeight() * 3;
		if (CacheHFMap.Num() != RequiredBufferSize)
		{
			CacheHFMap.SetNumUninitialized(RequiredBufferSize, EAllowShrinking::Yes);
		}

		// Copy the BGR channels of the decompressed image to the storage buffer
		TArrayView64<FColor> TextureImageView = InHFAlbedoImage.AsBGRA8();
		int32 CachedHFMapIndex = 0;
		for (const FColor& Color : TextureImageView)
		{
			CacheHFMap[CachedHFMapIndex++] = Color.R;
			CacheHFMap[CachedHFMapIndex++] = Color.G;
			CacheHFMap[CachedHFMapIndex++] = Color.B;
		}

		// Synthesize the new image
		return FMetaHumanCharacterTextureSynthesis::SynthesizeFaceAlbedoWithHFMap(InTextureType,
																				InMetaHumanCharacter->SkinSettings.Skin,
																				InFaceTextureSynthesizer,
																				InCharacterData->CachedHFAlbedoMaps,
																				InCharacterData->CachedSynthesizedImages[InTextureType]);
	}
}

void FMetaHumanCharacterEditorCloudRequests::TextureSynthesisRequestFinished()
{
	TextureSynthesis.Reset();
	TextureSynthesisStartTime = 0.0f;
	TextureSynthesisProgressHandle.Reset();
	TextureSynthesisNotificationItem.Reset();
}

void FMetaHumanCharacterEditorCloudRequests::BodyTextureRequestFinished()
{
	BodyTextures.Reset();
	BodyTextureStartTime = 0.0f;
	BodyTextureProgressHandle.Reset();
	BodyTextureNotificationItem.Reset();
}

void FMetaHumanCharacterEditorCloudRequests::AutoRiggingRequestFinished()
{
	AutoRig.Reset();
	AutoRiggingStartTime = 0.0f;
	AutoRiggingProgressHandle.Reset();
	AutoRiggingNotificationItem.Reset();
}

bool FMetaHumanCharacterEditorCloudRequests::HasActiveRequest() const
{
	return TextureSynthesis.IsValid() || AutoRig.IsValid() || BodyTextures.IsValid();
}

void FMetaHumanCharacterEditorCloudRequests::InitFaceAutoRigParams(const FMetaHumanCharacterIdentity::FState& InFaceState, TSharedRef<const IDNAReader> InFaceDNAReader, UE::MetaHuman::FTargetSolveParameters& OutAutoRigParameters)
{
	using namespace UE::MetaHuman;

	const int32 MeshCount = InFaceDNAReader->GetMeshCount();
	const TArray<FVector3f> ConformedVertices = InFaceState.Evaluate().Vertices;

	InFaceState.GetRawBindPose(ConformedVertices, OutAutoRigParameters.BindPose);
	InFaceState.GetCoefficients(OutAutoRigParameters.Coefficients);
	InFaceState.GetModelIdentifier(OutAutoRigParameters.ModelIdentifier);
	InFaceState.GetGlobalScale(OutAutoRigParameters.Scale);
	OutAutoRigParameters.HighFrequency = InFaceState.GetHighFrequenctVariant();
	const float HighFrequencyScale = InFaceState.GetSettings().GlobalHighFrequencyScale();
	if (HighFrequencyScale <= 0)
	{
		// no high frequency if scale is 0
		OutAutoRigParameters.HighFrequency = -1;
	}

	for (int32 MeshIndex = 0; MeshIndex < MeshCount; MeshIndex++)
	{
		if (MeshNameHeadLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedFaceVertices);
		}
		else if (MeshNameTeethLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedTeethVertices);
		}
		else if (MeshNameEyeLeftLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedLeftEyeVertices);
		}
		else if (MeshNameEyeRightLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedRightEyeVertices);
		}
		else if (MeshNameSalivaLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedSalivaVertices);
		}
		else if (MeshNameEyeShellLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedEyeShellVertices);
		}
		else if (MeshNameEyeLashesLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedEyeLashesVertices);
		}
		else if (MeshNameEyeEdgeLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedEyeEdgeVertices);
		}
		else if (MeshNameCartilageLod0 == InFaceDNAReader->GetMeshName(MeshIndex))
		{
			ExtractMeshVertices(InFaceDNAReader, MeshIndex, InFaceState, ConformedVertices, OutAutoRigParameters.ConformedCartilageVertices);
		}
	}
}


bool FMetaHumanCharacterEditorCloudRequests::GenerateTexturesFromResponse(TSharedPtr<UE::MetaHuman::FFaceHighFrequencyData> Data,
																			const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
																			TSharedRef<FMetaHumanCharacterEditorData> InCharacterData, 
																			TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter)
{
	check(Data.IsValid());

	FScopedSlowTask UpdateTexturesTask(static_cast<int32>(EFaceTextureType::Count), LOCTEXT("ApplyingFaceTexturesMessage", "Applying source face textures"));
	UpdateTexturesTask.MakeDialog();

	bool bNeedToUpdateFaceMaterial = false;

	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		UpdateTexturesTask.EnterProgressFrame();

		TConstArrayView<uint8> PngData = (*Data)[TextureType];
		
		// We might have unused slots in the image array so we can skip them here
		if (!PngData.IsEmpty())
		{
			// Returned image is expected to be a compressed png
			FImage TextureImage;
			if (FImageUtils::DecompressImage(PngData.GetData(), PngData.Num(), TextureImage))
			{
				const bool bNeedsSynthesize = TextureType < EFaceTextureType::Normal;

				// In the case of albedo maps, we need to synthesize the final image locally
				if (bNeedsSynthesize)
				{
					if (!UE::MetaHuman::SynthesizeAlbedoWithHFMap(TextureType, TextureImage, InFaceTextureSynthesizer, InCharacterData, InMetaHumanCharacter))
					{
						const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int32>(TextureType));
						UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to synthesize high rez base color texture {TextureTypeName}", *TextureTypeName);
						continue;
					}

					// The final face texture is stored in the cached synthesized images
					TextureImage = InCharacterData->CachedSynthesizedImages[TextureType];
				}

				// Store the new map in the MetaHuman asset
				InMetaHumanCharacter->StoreSynthesizedFaceTexture(TextureType, TextureImage);
				InMetaHumanCharacter->SetHasHighResolutionTextures(true);

				// Update the respective Texture Object if necessary
				if (TObjectPtr<UTexture2D>* FoundTexture = InMetaHumanCharacter->SynthesizedFaceTextures.Find(TextureType))
				{
					UTexture2D* Texture = *FoundTexture;

					// Clear the existing texture data for transient textures
					if (Texture->HasAnyFlags(RF_Transient))
					{
						Texture->SetPlatformData(nullptr);
					}

					// Create a new texture from the image
					Texture = FMetaHumanCharacterTextureSynthesis::CreateFaceTextureFromSource(TextureType, TextureImage);
					// This a preview texture so we can clear its resource
					Texture->Source.Reset();

					InMetaHumanCharacter->SynthesizedFaceTextures[TextureType] = Texture;
					bNeedToUpdateFaceMaterial = true;

					if (bNeedsSynthesize)
					{
						// If synthesized, release the larger temp buffer used to synthesize
						InCharacterData->CachedSynthesizedImages[TextureType].Reset();
					}
				}			
			}
			else
			{
				const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int32>(TextureType));
				UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to decompress face textures {TextureTypeName} from service", *TextureTypeName);
			}
		}
	}

	// Clear any temp used maps
	for (TArray<uint8>& CachedHFAlbedoMap : InCharacterData->CachedHFAlbedoMaps)
	{
		CachedHFAlbedoMap.Empty();
	}

	return bNeedToUpdateFaceMaterial;
}

bool FMetaHumanCharacterEditorCloudRequests::GenerateBodyTexturesFromResponse(TSharedPtr<UE::MetaHuman::FBodyHighFrequencyData> Data,
																				TNotNull<class UMetaHumanCharacter*> InMetaHumanCharacter)
{
	check(Data.IsValid());

	FScopedSlowTask UpdateTexturesTask(static_cast<int32>(EBodyTextureType::Count), LOCTEXT("ApplyingBodyTexturesMessage", "Applying source body textures"));
	UpdateTexturesTask.MakeDialog();

	bool bNeedToUpdateBodyMaterial = false;

	for (EBodyTextureType TextureType : TEnumRange<EBodyTextureType>())
	{
		UpdateTexturesTask.EnterProgressFrame();

		TConstArrayView<uint8> PngData = (*Data)[TextureType];
		
		// We might have unused slots in the image array so we can skip them here
		if (!PngData.IsEmpty())
		{
			// Returned image is expected to be a compressed png
			FImage TextureImage;
			if (FImageUtils::DecompressImage(PngData.GetData(), PngData.Num(), TextureImage))
			{
				// Store the new map in the MetaHuman asset
				InMetaHumanCharacter->StoreHighResBodyTexture(TextureType, TextureImage);
				InMetaHumanCharacter->SetHasHighResolutionTextures(true);

				// Update the respective Texture Object if necessary
				if (TObjectPtr<UTexture2D>* FoundTexture = InMetaHumanCharacter->BodyTextures.Find(TextureType))
				{
					// See FMetaHumanCharacterEditorCloudRequests::GenerateTexturesFromResponse() for detailed comments

					UTexture2D* Texture = *FoundTexture;

					// Clear the existing texture data for transient textures
					if (Texture->HasAnyFlags(RF_Transient))
					{
						Texture->SetPlatformData(nullptr);
					}

					Texture = FMetaHumanCharacterBodyTextureUtils::CreateBodyTextureFromSource(TextureType, TextureImage);
					Texture->Source.Reset();

					InMetaHumanCharacter->BodyTextures[TextureType] = Texture;
					bNeedToUpdateBodyMaterial = true;
				}
			}
			else
			{
				const FString TextureTypeName = StaticEnum<EBodyTextureType>()->GetAuthoredNameStringByIndex(static_cast<int32>(TextureType));
				UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to decompress body textures {TextureTypeName} from service", *TextureTypeName);
			}
		}
	}
	return bNeedToUpdateBodyMaterial;
}

#undef LOCTEXT_NAMESPACE
