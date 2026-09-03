// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacter.h"

#include "MetaHumanCharacterCustomVersion.h"
#include "MetaHumanCharacterInstance.h"
#include "MetaHumanCharacterLog.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCollectionEditorPipeline.h"
#include "MetaHumanWardrobeItem.h"

#include "AssetRegistry/AssetData.h"
#include "Components/SkeletalMeshComponent.h"
#include "Misc/TransactionObjectEvent.h"
#include "Logging/StructuredLog.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"
#include "Templates/SharedPointer.h"
#include "HAL/IConsoleManager.h"

namespace UE::MetaHuman
{
	static TAutoConsoleVariable<bool> CVarMHCUseTextureCompression
	{
		TEXT("mh.Character.UseTextureCompression"),
		true,
		TEXT("Set to true to store the Character face and body textures in a compressed format."),
		ECVF_Default
	};

#if WITH_EDITOR
	namespace ThumbnailObjectName
	{
		static constexpr FStringView CharacterBody = TEXTVIEW("ThumbnailAux_CharacterBody");
		static constexpr FStringView Face = TEXTVIEW("ThumbnailAux_Face");
		static constexpr FStringView Body = TEXTVIEW("ThumbnailAux_Body");
	}
#endif

	/**
	* Utility function to read and write compressed editor bulk data
	* TODO: this not working as intended since UpdatePayload() uncompress the input compressed buffer before serializing
	*/
	static void CompressAndUpdateBulkData(UE::Serialization::FEditorBulkData& InBulkData, const FSharedBuffer& InBuffer, UObject* InOwner)
	{
		FCompressedBuffer CompressedPayLoad = FCompressedBuffer::Compress(InBuffer);
		InBulkData.UpdatePayload(CompressedPayLoad, InOwner);
	}

	static void CompressAndUpdateBulkData(UE::Serialization::FEditorBulkData& InBulkData, FMemoryView InData, UObject* InOwner)
	{
		FSharedBuffer Payload = FSharedBuffer::Clone(InData);
		CompressAndUpdateBulkData(InBulkData, Payload, InOwner);
	}

	[[nodiscard]] static FSharedBuffer GetBufferBulkData(const UE::Serialization::FEditorBulkData& InBulkData)
	{
		FSharedBuffer PayloadData;

		if (InBulkData.HasPayloadData())
		{
			TSharedFuture<FSharedBuffer> PayloadFuture = InBulkData.GetPayload();
			PayloadData = PayloadFuture.Get();
		}

		return PayloadData;
	}

	template<typename TEnum>
	static TMap<TEnum, TObjectPtr<UTexture2D>> LoadTextures(const TMap<TEnum, TSoftObjectPtr<UTexture2D>>& InSoftTextures)
	{
		TMap<TEnum, TObjectPtr<UTexture2D>> LoadedTextures;
		for (const TPair<TEnum, TSoftObjectPtr<UTexture2D>>& SoftTexturePair : InSoftTextures)
		{
			const TEnum TextureType = SoftTexturePair.Key;
			const TSoftObjectPtr<UTexture2D> Texture = SoftTexturePair.Value;

			if (!Texture.IsNull())
			{
				LoadedTextures.Add(TextureType, Texture.LoadSynchronous());
			}
		}
		return LoadedTextures;
	}
}

bool FMetaHumanCharacterGeneratedAssets::RemoveAssetMetadata(TNotNull<const UObject*> InAsset)
{
	const int NumRemoved = Metadata.RemoveAll(
		[InAsset](const FMetaHumanGeneratedAssetMetadata& Candidate)
		{
			return Candidate.Object == InAsset;
		}
	);
	return NumRemoved > 0;
}

void FMetaHumanCharacterSkinTextureSet::Append(const FMetaHumanCharacterSkinTextureSet& InOther)
{
	Face.Append(InOther.Face);
	Body.Append(InOther.Body);
}

FMetaHumanCharacterSkinTextureSet FMetaHumanCharacterSkinTextureSoftSet::LoadTextureSet() const
{
	return FMetaHumanCharacterSkinTextureSet
	{
		.Face = UE::MetaHuman::LoadTextures(Face),
		.Body = UE::MetaHuman::LoadTextures(Body)
	};
}

FMetaHumanCharacterSkinTextureSet FMetaHumanCharacterSkinSettings::GetFinalSkinTextureSet(const FMetaHumanCharacterSkinTextureSet& InSkinTextureSet) const
{
	FMetaHumanCharacterSkinTextureSet FinalSkinTextureSet = InSkinTextureSet;

	if (bEnableTextureOverrides)
	{
		FMetaHumanCharacterSkinTextureSet LoadedOverrides = TextureOverrides.LoadTextureSet();

		FinalSkinTextureSet.Append(LoadedOverrides);
	}

	return FinalSkinTextureSet;
}

UMetaHumanCharacter::UMetaHumanCharacter()
{
	//Need to make sure the map with bulk data doesn't change in order to avoid data from being moved
	// when adding new entries to it, so allocate all entries here
	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		SynthesizedFaceTexturesData.Add(TextureType);
	}

	for (EBodyTextureType TextureType : TEnumRange<EBodyTextureType>())
	{
		HighResBodyTexturesData.Add(TextureType);
	}

	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		InternalCollection = CreateDefaultSubobject<UMetaHumanCollection>(TEXT("InternalCollection"));
		InternalCollection->SetFlags(RF_Public);

#if WITH_EDITORONLY_DATA
		ThumbnailAux_CharacterBody = CreateDefaultSubobject<UMetaHumanCharacterThumbnailAux>(UE::MetaHuman::ThumbnailObjectName::CharacterBody.GetData());
		ThumbnailAux_Face = CreateDefaultSubobject<UMetaHumanCharacterThumbnailAux>(UE::MetaHuman::ThumbnailObjectName::Face.GetData());
		ThumbnailAux_Body = CreateDefaultSubobject<UMetaHumanCharacterThumbnailAux>(UE::MetaHuman::ThumbnailObjectName::Body.GetData());
#endif

#if WITH_EDITOR
		// If the palette's pipeline changes, that could cause this Character to be removed from its slot, so
		// make sure it's still set up correctly.
		InternalCollection->OnPipelineChanged.AddUObject(this, &UMetaHumanCharacter::ConfigureCollection);
#endif
	}
}

#if WITH_EDITOR
void UMetaHumanCharacter::PostInitProperties()
{
	Super::PostInitProperties();

	ConfigureCollection();
}

void UMetaHumanCharacter::PostLoad()
{
	Super::PostLoad();

	ConfigureCollection();
}

void UMetaHumanCharacter::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	Super::PostEditChangeProperty(InPropertyChangedEvent);

	const FName PropertyName = InPropertyChangedEvent.GetPropertyName();
	const FName MemberPropertyName = InPropertyChangedEvent.GetMemberPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacter, WardrobePaths)
		|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(UMetaHumanCharacter, WardrobePaths))
	{
		OnWardrobePathsChanged.Broadcast();
	}
}

void UMetaHumanCharacter::PostTransacted(const FTransactionObjectEvent& InTransactionEvent)
{
	Super::PostTransacted(InTransactionEvent);

	if (InTransactionEvent.GetEventType() == ETransactionObjectEventType::UndoRedo)
	{
		TArray<FName> PropertiesChanged = InTransactionEvent.GetChangedProperties();

		if (PropertiesChanged.Contains(GET_MEMBER_NAME_CHECKED(UMetaHumanCharacter, WardrobePaths)))
		{
			OnWardrobePathsChanged.Broadcast();
		}
	}
}
#endif // WITH_EDITOR

void UMetaHumanCharacter::Serialize(FArchive& InAr)
{
	Super::Serialize(InAr);

	InAr.UsingCustomVersion(FMetaHumanCharacterCustomVersion::GUID);

	FaceStateBulkData.Serialize(InAr, this);
	FaceDNABulkData.Serialize(InAr, this);
	BodyStateBulkData.Serialize(InAr, this);
	BodyDNABulkData.Serialize(InAr, this);

	for (TPair<EFaceTextureType, UE::Serialization::FEditorBulkData>& TextureBulkDataPair : SynthesizedFaceTexturesData)
	{
		TextureBulkDataPair.Value.Serialize(InAr, this);
	}

	bool bSerializeHighResBodyTextures = true;
	if (InAr.IsLoading() && InAr.CustomVer(FMetaHumanCharacterCustomVersion::GUID) < FMetaHumanCharacterCustomVersion::BodyTexturesSerialized)
	{
		bSerializeHighResBodyTextures = false;
	}

	if (bSerializeHighResBodyTextures)
	{
		for (TPair<EBodyTextureType, UE::Serialization::FEditorBulkData>& TextureBulkDataPair : HighResBodyTexturesData)
		{
			TextureBulkDataPair.Value.Serialize(InAr, this);
		}
	}
}

bool UMetaHumanCharacter::IsCharacterValid() const
{
	// TODO: BodyState won't have valid data at this point
	return FaceStateBulkData.HasPayloadData(); // && BodyStateBulkData.HasPayloadData();
}

void UMetaHumanCharacter::SetFaceStateData(const FSharedBuffer& InFaceStateData)
{
	UE::MetaHuman::CompressAndUpdateBulkData(FaceStateBulkData, InFaceStateData, this);
}

FSharedBuffer UMetaHumanCharacter::GetFaceStateData() const
{
	return UE::MetaHuman::GetBufferBulkData(FaceStateBulkData);
}

void UMetaHumanCharacter::SetFaceDNABuffer(TConstArrayView<uint8> InFaceDNABuffer, bool bInHasFaceDNABlendshapes)
{
	FaceDNABulkData.UpdatePayload(FSharedBuffer::MakeView(InFaceDNABuffer.GetData(), InFaceDNABuffer.Num()));
	bHasFaceDNABlendshapes = bInHasFaceDNABlendshapes;
#if WITH_EDITOR
	OnRiggingStateChanged.Broadcast();
#endif
}

bool UMetaHumanCharacter::HasFaceDNA() const
{
	return FaceDNABulkData.HasPayloadData();
}

TArray<uint8> UMetaHumanCharacter::GetFaceDNABuffer() const
{
	if (FaceDNABulkData.HasPayloadData())
	{
		TArray<uint8> DNABuffer;
		FSharedBuffer Payload = FaceDNABulkData.GetPayload().Get();
		DNABuffer.Append((const uint8*)Payload.GetData(), Payload.GetSize());
	
		return DNABuffer;
	}

	return {};
}

bool UMetaHumanCharacter::HasFaceDNABlendshapes() const
{
	return bHasFaceDNABlendshapes;
}

void UMetaHumanCharacter::SetBodyStateData(const FSharedBuffer& InBodyStateData)
{
	UE::MetaHuman::CompressAndUpdateBulkData(BodyStateBulkData, InBodyStateData, this);
}

FSharedBuffer UMetaHumanCharacter::GetBodyStateData() const
{
	return UE::MetaHuman::GetBufferBulkData(BodyStateBulkData);
}

void UMetaHumanCharacter::SetBodyDNABuffer(TConstArrayView<uint8> InBodyDNABuffer)
{
	BodyDNABulkData.UpdatePayload(FSharedBuffer::MakeView(InBodyDNABuffer.GetData(), InBodyDNABuffer.Num()));
}

bool UMetaHumanCharacter::HasBodyDNA() const
{
	return BodyDNABulkData.HasPayloadData();
}

TArray<uint8> UMetaHumanCharacter::GetBodyDNABuffer() const
{
	if (BodyDNABulkData.HasPayloadData())
	{
		TArray<uint8> DNABuffer;
		FSharedBuffer Payload = BodyDNABulkData.GetPayload().Get();
		DNABuffer.Append((const uint8*)Payload.GetData(), Payload.GetSize());
	
		return DNABuffer;
	}

	return {};
}

bool UMetaHumanCharacter::HasSynthesizedTextures() const
{
	return !SynthesizedFaceTexturesInfo.IsEmpty();
}

void UMetaHumanCharacter::SetHasHighResolutionTextures(bool bInHasHighResolutionTextures)
{
	bHasHighResolutionTextures = bInHasHighResolutionTextures;

	if (!bInHasHighResolutionTextures)
	{
		// Remove the animated maps texture infos since they are not valid anymore
		constexpr TStaticArray<EFaceTextureType, 6> AnimatedMapTypes =
		{
			EFaceTextureType::Basecolor_Animated_CM1,
			EFaceTextureType::Basecolor_Animated_CM2,
			EFaceTextureType::Basecolor_Animated_CM3,

			EFaceTextureType::Normal_Animated_WM1,
			EFaceTextureType::Normal_Animated_WM2,
			EFaceTextureType::Normal_Animated_WM3,
		};

		for (EFaceTextureType AnimatedMap : AnimatedMapTypes)
		{
			SynthesizedFaceTexturesInfo.Remove(AnimatedMap);
		}
	}
}

bool UMetaHumanCharacter::HasHighResolutionTextures() const
{
	return bHasHighResolutionTextures;
}

void UMetaHumanCharacter::StoreSynthesizedFaceTexture(EFaceTextureType InTextureType, const FImage& InTextureData)
{
	FMetaHumanCharacterTextureInfo& TextureInfo = SynthesizedFaceTexturesInfo.FindOrAdd(InTextureType);
	TextureInfo.Init(InTextureData);

	UE::Serialization::FEditorBulkData& BulkData = SynthesizedFaceTexturesData[InTextureType];
	
	if (UE::MetaHuman::CVarMHCUseTextureCompression.GetValueOnAnyThread())
	{
		// Compressing the images using png
		// Note that the asset data seem to be compressed when serialized into disk but not when loaded
		// This allows for using compressed data even when the MHC asset is loaded in memory
		TArray64<uint8> CompressedData;
		if (FImageUtils::CompressImage(CompressedData, TEXT("png"), InTextureData))
		{
			FSharedBuffer CompressedPayLoad = FSharedBuffer::MakeView(CompressedData.GetData(), CompressedData.NumBytes());
			BulkData.UpdatePayload(CompressedPayLoad, this);
		}
		else
		{
			FMemoryView TextureDataView(InTextureData.RawData.GetData(), InTextureData.GetImageSizeBytes());
			BulkData.UpdatePayload(FSharedBuffer::MakeView(TextureDataView), this);
		}
	}
	else
	{
		FMemoryView Data(InTextureData.RawData.GetData(), InTextureData.GetImageSizeBytes());
		UE::MetaHuman::CompressAndUpdateBulkData(BulkData, Data, this);
	}

	MarkPackageDirty();
}

FInt32Point UMetaHumanCharacter::GetSynthesizedFaceTexturesResolution(EFaceTextureType InFaceTextureType) const
{
	if (const FMetaHumanCharacterTextureInfo* Info = SynthesizedFaceTexturesInfo.Find(InFaceTextureType))
	{
		return FInt32Point(Info->SizeX, Info->SizeY);
	}
	return FInt32Point(0, 0);
}

TMap<EFaceTextureType, TObjectPtr<UTexture2D>> UMetaHumanCharacter::GetValidFaceTextures() const
{
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>> ValidFaceTextures;

	for (const TPair<EFaceTextureType, FMetaHumanCharacterTextureInfo>& FaceTextureInfoPair : SynthesizedFaceTexturesInfo)
	{
		EFaceTextureType TextureType = FaceTextureInfoPair.Key;

		if (const TObjectPtr<UTexture2D>* FoundTexture = SynthesizedFaceTextures.Find(TextureType))
		{
			ValidFaceTextures.Add(TextureType, *FoundTexture);
		}
	}

	return ValidFaceTextures;
}

void UMetaHumanCharacter::StoreHighResBodyTexture(EBodyTextureType InTextureType, const FImage& InTextureData)
{
	FMetaHumanCharacterTextureInfo& TextureInfo = HighResBodyTexturesInfo.FindOrAdd(InTextureType);
	TextureInfo.Init(InTextureData);

	UE::Serialization::FEditorBulkData& BulkData = HighResBodyTexturesData[InTextureType];

	if (UE::MetaHuman::CVarMHCUseTextureCompression.GetValueOnAnyThread())
	{
		TArray64<uint8> CompressedData;
		if (FImageUtils::CompressImage(CompressedData, TEXT("png"), InTextureData))
		{
			FSharedBuffer CompressedPayLoad = FSharedBuffer::MakeView(CompressedData.GetData(), CompressedData.NumBytes());
			BulkData.UpdatePayload(CompressedPayLoad, this);
		}
		else
		{
			FMemoryView TextureDataView(InTextureData.RawData.GetData(), InTextureData.GetImageSizeBytes());
			BulkData.UpdatePayload(FSharedBuffer::MakeView(TextureDataView), this);
		}
	}
	else
	{
		FMemoryView Data(InTextureData.RawData.GetData(), InTextureData.GetImageSizeBytes());
		UE::MetaHuman::CompressAndUpdateBulkData(BulkData, Data, this);
	}

	MarkPackageDirty();
}

void UMetaHumanCharacter::ResetUnreferencedHighResTextureData()
{
	for (TPair<EFaceTextureType, UE::Serialization::FEditorBulkData>& Data : SynthesizedFaceTexturesData)
	{
		if (!SynthesizedFaceTexturesInfo.Contains(Data.Key))
		{
			Data.Value.Reset();
		}
	}

	for (TPair<EBodyTextureType, UE::Serialization::FEditorBulkData>& Data : HighResBodyTexturesData)
	{
		if (!HighResBodyTexturesInfo.Contains(Data.Key))
		{
			Data.Value.Reset();
		}
	}
}

void UMetaHumanCharacter::RemoveAllTextures()
{
	SynthesizedFaceTexturesInfo.Empty();
	HighResBodyTexturesInfo.Empty();
	
	for (TPair<EFaceTextureType, UE::Serialization::FEditorBulkData>& Data : SynthesizedFaceTexturesData)
	{
		Data.Value.Reset();
	}

	for (TPair<EBodyTextureType, UE::Serialization::FEditorBulkData>& Data : HighResBodyTexturesData)
	{
		Data.Value.Reset();
	}

	SetHasHighResolutionTextures(false);
}

FInt32Point UMetaHumanCharacter::GetSynthesizedBodyTexturesResolution(EBodyTextureType InBodyTextureType) const
{
	if (const TObjectPtr<class UTexture2D>* Texture = BodyTextures.Find(InBodyTextureType))
	{
		return FInt32Point(Texture->Get()->GetSizeX(), Texture->Get()->GetSizeY());
	}
	return FInt32Point(0, 0);
}

TFuture<FSharedBuffer> UMetaHumanCharacter::GetSynthesizedFaceTextureDataAsync(EFaceTextureType InTextureType) const
{
	check(SynthesizedFaceTexturesData.Contains(InTextureType));

	TSharedRef<TPromise<FSharedBuffer>> Promise = MakeShared<TPromise<FSharedBuffer>>();

	// Add a continuation to the bulk data async load to decompress the loaded buffer if needed
	SynthesizedFaceTexturesData[InTextureType].GetPayload().Next(
		[Promise](FSharedBuffer PayloadData)
		{
			FSharedBuffer FinalBuffer = PayloadData;

			// Check if the image was compressed when stored, if not just return the loaded buffer
			FImage DecompressedImage;
			if (FImageUtils::DecompressImage(PayloadData.GetData(), PayloadData.GetSize(), DecompressedImage))
			{
				FMemoryView Data(DecompressedImage.RawData.GetData(), DecompressedImage.GetImageSizeBytes());
				FinalBuffer = FSharedBuffer::Clone(Data);
			}

			Promise.Get().SetValue(FinalBuffer);
		});

	return Promise.Get().GetFuture();
}

TFuture<FSharedBuffer> UMetaHumanCharacter::GetHighResBodyTextureDataAsync(EBodyTextureType InTextureType) const
{
	check(HighResBodyTexturesData.Contains(InTextureType));

	TSharedRef<TPromise<FSharedBuffer>> Promise = MakeShared<TPromise<FSharedBuffer>>();

	// Add a continuation to the bulk data async load to decompress the loaded buffer if needed
	HighResBodyTexturesData[InTextureType].GetPayload().Next(
		[Promise](FSharedBuffer PayloadData)
		{
			FSharedBuffer FinalBuffer = PayloadData;

			// Check if the image was compressed when stored, if not just return the loaded buffer
			FImage DecompressedImage;
			if (FImageUtils::DecompressImage(PayloadData.GetData(), PayloadData.GetSize(), DecompressedImage))
			{
				FMemoryView Data(DecompressedImage.RawData.GetData(), DecompressedImage.GetImageSizeBytes());
				FinalBuffer = FSharedBuffer::Clone(Data);
			}

			Promise.Get().SetValue(FinalBuffer);
		});

	return Promise.Get().GetFuture();
}

TObjectPtr<UMetaHumanCollection> UMetaHumanCharacter::GetMutableInternalCollection()
{
	return InternalCollection;
}

const TObjectPtr<UMetaHumanCollection> UMetaHumanCharacter::GetInternalCollection() const
{
	return InternalCollection;
}

FMetaHumanPaletteItemKey UMetaHumanCharacter::GetInternalCollectionKey() const
{
	return InternalCollectionKey;
}

#if WITH_EDITOR
void UMetaHumanCharacter::ConfigureCollection()
{
	InternalCollectionKey.Reset();

	// Ensure the Character slot has this Character as the only item in it

	if (HasAnyFlags(RF_ClassDefaultObject | RF_NeedInitialization))
	{
		// No action needed on the CDO or when the object needs initialization.
		// This function will be called again during PostLoad, when palette
		// is properly initialized
		return;
	}

	// All instances apart from the CDO should have a palette
	check(InternalCollection);

	if (!InternalCollection->GetPipeline())
	{
		return;
	}

	int32 NumCharacters = 0;
	TOptional<FMetaHumanPaletteItemKey> CharacterItemKey;
	for (const FMetaHumanCharacterPaletteItem& Item : InternalCollection->GetItems())
	{
		if (Item.SlotName == UE::MetaHuman::CharacterPipelineSlots::Character)
		{
			NumCharacters++;

			if (NumCharacters == 1
				&& Item.WardrobeItem
				&& Item.WardrobeItem->PrincipalAsset == this)
			{
				CharacterItemKey = Item.GetItemKey();
				InternalCollection->GetMutableDefaultInstance()->SetSingleSlotSelection(UE::MetaHuman::CharacterPipelineSlots::Character, CharacterItemKey.GetValue());
			}
		}
	}

	if (NumCharacters == 1
		&& CharacterItemKey.IsSet())
	{
		// The palette contains just one character and it's the right one
		InternalCollectionKey = CharacterItemKey.GetValue();
		return;
	}

	// The palette is not set up for this character, so clear any existing characters and set it 
	// up correctly.

	InternalCollection->RemoveAllItemsForSlot(UE::MetaHuman::CharacterPipelineSlots::Character);

	if (!InternalCollection->GetEditorPipeline()->IsPrincipalAssetClassCompatibleWithSlot(UE::MetaHuman::CharacterPipelineSlots::Character, GetClass()))
	{
		UE_LOGFMT(LogMetaHumanCharacter, Error, "The Character Pipeline assigned to {Character} doesn't have a compatible Character slot", GetPathName());
		return;
	}

	verify(InternalCollection->TryAddItemFromPrincipalAsset(
		UE::MetaHuman::CharacterPipelineSlots::Character,
		this,
		InternalCollectionKey));

	InternalCollection->GetMutableDefaultInstance()->SetSingleSlotSelection(UE::MetaHuman::CharacterPipelineSlots::Character, InternalCollectionKey);
}

FName UMetaHumanCharacter::GetThumbnailPathInPackage(const FString& InCharacterAssetPath, EMetaHumanCharacterThumbnailCameraPosition InThumbnailPosition)
{
	static const TMap<EMetaHumanCharacterThumbnailCameraPosition, FString> AuxSubobjectNames =
	{
		{ EMetaHumanCharacterThumbnailCameraPosition::Character_Body, UE::MetaHuman::ThumbnailObjectName::CharacterBody.GetData() },
		{ EMetaHumanCharacterThumbnailCameraPosition::Face, UE::MetaHuman::ThumbnailObjectName::Face.GetData() },
		{ EMetaHumanCharacterThumbnailCameraPosition::Body, UE::MetaHuman::ThumbnailObjectName::Body.GetData() },
	};

	if (const FString* SubobjectName = AuxSubobjectNames.Find(InThumbnailPosition))
	{
		return *FString::Format(TEXT("{0} {1}{2}{3}"), { UMetaHumanCharacterThumbnailAux::StaticClass()->GetName(), InCharacterAssetPath, SUBOBJECT_DELIMITER, *SubobjectName });
	}
	else
	{
		return *FString::Format(TEXT("{0} {1}"), { UMetaHumanCharacter::StaticClass()->GetName(), InCharacterAssetPath });
	}
}

#endif // WITH_EDITOR
