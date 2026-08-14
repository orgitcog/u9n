// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterTextureSynthesis.h"

#include "Editor/EditorEngine.h"
#include "HAL/ConsoleManager.h"
#include "ImageCore.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "PixelFormat.h"
#include "Tasks/Task.h"
#include "TextureResource.h"
#include "UObject/NameTypes.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Logging/StructuredLog.h"
#include "TextureCompiler.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterEditorSettings.h"
#include "MetaHumanFaceTextureSynthesizer.h"
#include "MetaHumanCharacterEditorSubsystem.h"


extern UNREALED_API UEditorEngine* GEditor;

namespace UE::MetaHuman
{
	static FAutoConsoleCommand ResetMetaHumanCharacterTextureSynthesis(
		TEXT("mh.TextureSynthesis.ResetModel"), TEXT("Reset Texture Synthesis by re-loading the model data"),
		FConsoleCommandDelegate::CreateStatic(
			[]()
			{
				if (UMetaHumanCharacterEditorSubsystem* MetaHumanCharacterEditorSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>())
				{
					MetaHumanCharacterEditorSubsystem->ResetTextureSynthesis();
					UE_LOGFMT(LogMetaHumanCharacterEditor, Display, "Texture sythesis reset");
				}
				else
				{
					UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to reset texture synthesis");
				}
			}
		)
	);

	// Texture Type to be used if a cached image is not available by the local model
	static constexpr EFaceTextureType MapToCompatibleTextureType[] =
	{
		EFaceTextureType::Basecolor,
		EFaceTextureType::Basecolor,
		EFaceTextureType::Basecolor,
		EFaceTextureType::Basecolor,

		EFaceTextureType::Normal,
		EFaceTextureType::Normal,
		EFaceTextureType::Normal,
		EFaceTextureType::Normal,

		EFaceTextureType::Cavity,
	};
	static_assert(UE_ARRAY_COUNT(MapToCompatibleTextureType) == static_cast<int32>(EFaceTextureType::Count));

	// Set the texture properties as expected by the face material
	void SetFaceTextureProperties(EFaceTextureType InType, TNotNull<UTexture2D*> Texture)
	{
		// Order should match the one in EFaceTextureType
		static constexpr TextureCompressionSettings TextureTypeToCompressionSettings[] =
		{
			TC_Default,			// Basecolor
			TC_HDR_Compressed,	// Animated delta color
			TC_HDR_Compressed,
			TC_HDR_Compressed,

			TC_Normalmap,	// Normal
			TC_Default,		// Animated delta normal
			TC_Default,
			TC_Default,

			TC_Masks		// Cavity
		};

		static constexpr TextureGroup TextureTypeToTextureGroup[] =
		{
			TEXTUREGROUP_Character,				// Basecolor
			TEXTUREGROUP_Character,
			TEXTUREGROUP_Character,
			TEXTUREGROUP_Character,

			TEXTUREGROUP_CharacterNormalMap,	// Normal
			TEXTUREGROUP_CharacterNormalMap,
			TEXTUREGROUP_CharacterNormalMap,
			TEXTUREGROUP_CharacterNormalMap,

			TEXTUREGROUP_CharacterSpecular		// Cavity
		};

		static_assert(UE_ARRAY_COUNT(TextureTypeToCompressionSettings) == static_cast<int32>(EFaceTextureType::Count));
		static_assert(UE_ARRAY_COUNT(TextureTypeToTextureGroup) == static_cast<int32>(EFaceTextureType::Count));

		const bool bIsAlbedoTexture = InType == EFaceTextureType::Basecolor;

		// Set its properties
		Texture->CompressionSettings = TextureTypeToCompressionSettings[static_cast<int32>(InType)];
		Texture->AlphaCoverageThresholds.W = 1.0f;

		// Disable MIPs for albedo
		Texture->MipGenSettings = bIsAlbedoTexture ? TMGS_NoMipmaps : TMGS_FromTextureGroup;

		// Set texture to the "Character" texture group (rather than the default "World")
		Texture->LODGroup = TextureTypeToTextureGroup[static_cast<int32>(InType)];

		// Set sRGB for albedo textures
		Texture->SRGB = bIsAlbedoTexture;
	}

	static bool CheckMatchingImageAndTextureSize(const FImageView& InImage, TNotNull<const UTexture2D*> InTexture2D)
	{
		if (const FTexturePlatformData* TexturePlatformData = InTexture2D->GetPlatformData())
		{
			const FPixelFormatInfo& FormatInfo = GPixelFormats[InTexture2D->GetPixelFormat()];

			return	InImage.SizeX == TexturePlatformData->Mips[0].SizeX ||
					InImage.SizeY == TexturePlatformData->Mips[0].SizeY ||
					InImage.GetBytesPerPixel() == FormatInfo.BlockBytes;
		}

		return false;
	}

	static void CopySynthesizedDataToTexture2D(TConstArrayView<uint8> InSynthesizedRawData, UTexture2D* InOutTexture2D)
	{
		check(InOutTexture2D);
		check(!InSynthesizedRawData.IsEmpty());

		// Get Texture2DData
		const int32 MipLevel = 0;
		FTexture2DMipMap& Mip = InOutTexture2D->GetPlatformData()->Mips[MipLevel];

		if (Mip.BulkData.IsLocked())
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to update texture '{Texture}' because mip bulk data is locked", InOutTexture2D->GetName());
			ensure(false);
			return;
		}

		uint8* Texture2DData = (uint8*)Mip.BulkData.Lock(LOCK_READ_WRITE);
		if (!Texture2DData
			|| Mip.BulkData.GetBulkDataSize() != InSynthesizedRawData.Num())
		{
			ensure(false);
			return;
		}

		// Copy the data into the final UTexture2D
		FMemory::Memcpy(Texture2DData, InSynthesizedRawData.GetData(), InSynthesizedRawData.Num());

		// Unlock source data
		Mip.BulkData.Unlock();

		// Refresh rendering thread
		InOutTexture2D->UpdateResource();
	}

	static FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams SkinPropertiesToSynthesizerParams(const FMetaHumanCharacterSkinProperties& SkinProperties, int32 MaxHFIndex)
	{
		return FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams{
			.SkinUVFromUI = FVector2f{ SkinProperties.U, SkinProperties.V },
			.HighFrequencyIndex = FMath::Clamp(SkinProperties.FaceTextureIndex, 0, MaxHFIndex - 1),
			.MapType = FMetaHumanFaceTextureSynthesizer::EMapType::Base
		};
	}

	static TArray<EFaceTextureType> GetSupportedTextureTypes(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer)
	{
		// Ensure that EFaceTextureType and FMetaHumanFaceTextureSynthesizer::EMapType are in sync
		static_assert(static_cast<int32>(EFaceTextureType::Basecolor) == 0);
		static_assert(static_cast<int32>(EFaceTextureType::Normal) == static_cast<int32>(FMetaHumanFaceTextureSynthesizer::EMapType::Animated2) + 1);

		const int32 BaseNormalIndex = static_cast<int32>(EFaceTextureType::Normal);
		TArray<EFaceTextureType> OutTextureTypes{};

		// No supported images when there is no texture synthesis loaded
		if (InFaceTextureSynthesizer.IsValid())
		{
			const TArray<FMetaHumanFaceTextureSynthesizer::EMapType> SupportedAlbedoTypes = InFaceTextureSynthesizer.GetSupportedAlbedoMapTypes();
			for (FMetaHumanFaceTextureSynthesizer::EMapType MapType : SupportedAlbedoTypes)
			{
				const int32 MapIndex = static_cast<int32>(MapType);
				OutTextureTypes.Add(static_cast<EFaceTextureType>(MapIndex));
			}

			const TArray<FMetaHumanFaceTextureSynthesizer::EMapType> SupportedNormalTypes = InFaceTextureSynthesizer.GetSupportedNormalMapTypes();
			for (FMetaHumanFaceTextureSynthesizer::EMapType MapType : SupportedNormalTypes)
			{
				const int32 MapIndex = static_cast<int32>(MapType);
				OutTextureTypes.Add(static_cast<EFaceTextureType>(BaseNormalIndex + MapIndex));
			}

			// Cavity should always be supported
			OutTextureTypes.Add(EFaceTextureType::Cavity);
		}

		return OutTextureTypes;
	}
} // namespace UE::MetaHuman

void FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer(FMetaHumanFaceTextureSynthesizer& OutFaceTextureSynthesizer)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FMetaHumanCharacterTextureSynthesis::InitFaceTextureSynthesizer");
	
	// First try to initialize the face synthesizer with the model path from the plugin Settings 
	const UMetaHumanCharacterEditorSettings* Settings = GetDefault<UMetaHumanCharacterEditorSettings>();
	check(Settings);
	const FString TextureSynthesisModelPath = Settings->TextureSynthesisModelDir.Path;
 
	if (!TextureSynthesisModelPath.IsEmpty())
	{
		// Assume it is a valid model directory
		if (FPaths::DirectoryExists(TextureSynthesisModelPath) && OutFaceTextureSynthesizer.Init(TextureSynthesisModelPath, Settings->TextureSynthesisThreadCount))
		{
			return;
		}
		else
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Warning, "Failed to initialize texture synthesis model from: {TextureSynthesisModelPath}, will try to load the default models", TextureSynthesisModelPath);
		}
	}

	// Try to load the test model from the Plugin Content
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME);
	// Paths to find model data in order of priority
	const FString DefaultModelPaths[] =
	{
		Plugin->GetContentDir() + TEXT("/Optional/TextureSynthesis/TS-1.3-D_UE_res-1024_nchr-153"),
	};

	bool bIsModelLoaded = false;
	for (const FString& ModelPath : DefaultModelPaths)
	{
		if (OutFaceTextureSynthesizer.Init(ModelPath, Settings->TextureSynthesisThreadCount))
		{
			bIsModelLoaded = true;
			break;
		}
	}

	if (!bIsModelLoaded)
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Warning, "Failed to initialize texture synthesis with default models, skin editing will be disabled");
	}
}

void FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData(const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
																  const TMap<EFaceTextureType, FMetaHumanCharacterTextureInfo>& InTextureInfo,
																  TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures,
																  TMap<EFaceTextureType, FImage>& OutSynthesizedFaceImages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FMetaHumanCharacterTextureSynthesis::InitSynthesizedFaceData");

	// Set defaults for when texture synthesis is disabled
	const int32 DefaultTextureSizeX					= InFaceTextureSynthesizer.IsValid() ? InFaceTextureSynthesizer.GetTextureSizeX() : 128;
	const int32 DefaultTextureSizeY					= InFaceTextureSynthesizer.IsValid() ? InFaceTextureSynthesizer.GetTextureSizeY() : 128;
	const ERawImageFormat::Type DefaultImageFormat	= InFaceTextureSynthesizer.IsValid() ? InFaceTextureSynthesizer.GetTextureFormat() : ERawImageFormat::BGRA8;
	const EGammaSpace DefaultGammaSpace				= InFaceTextureSynthesizer.IsValid() ? InFaceTextureSynthesizer.GetTextureColorSpace() : EGammaSpace::sRGB;

	if (OutSynthesizedFaceTextures.IsEmpty())
	{
		if (InTextureInfo.IsEmpty())
		{
			FMetaHumanCharacterTextureSynthesis::CreateSynthesizedFaceTextures(DefaultTextureSizeX, OutSynthesizedFaceTextures);
		}
		else
		{
			// Synthesized Face Textures need to match the ones expected by the preview material, so always create one for all types
			for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
			{
				// Get a compatible texture type if there is no info for this texture
				const EFaceTextureType MatchedTextureType = InTextureInfo.Contains(TextureType) ? TextureType : UE::MetaHuman::MapToCompatibleTextureType[static_cast<int32>(TextureType)];

				// Get the texture size
				int32 TextureSizeX = DefaultTextureSizeX;
				int32 TextureSizeY = DefaultTextureSizeY;
				if (const FMetaHumanCharacterTextureInfo* TextureInfo = InTextureInfo.Find(MatchedTextureType))
				{
					TextureSizeX = TextureInfo->SizeX;
					TextureSizeY = TextureInfo->SizeY;
				}
				else
				{
					const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureType));
					UE_LOGFMT(LogMetaHumanCharacterEditor, Warning, "No compatible texture info for {TextureTypeName}, using the default size", *TextureTypeName);
				}

				OutSynthesizedFaceTextures.FindOrAdd(TextureType) = CreateFaceTextureEditable(TextureType, TextureSizeX, TextureSizeY);
			}
		}
	}

	if (OutSynthesizedFaceImages.IsEmpty())
	{
		if (InTextureInfo.IsEmpty())
		{
			// Create cached images for all types of maps that the local model supports
			const TArray<EFaceTextureType> SupportedTextureTypes = UE::MetaHuman::GetSupportedTextureTypes(InFaceTextureSynthesizer);
			for (EFaceTextureType TextureType : SupportedTextureTypes)
			{
				FImage& NewSynthesizedFaceTexture = OutSynthesizedFaceImages.Add(TextureType);
				NewSynthesizedFaceTexture.Init(DefaultTextureSizeX, DefaultTextureSizeY, DefaultImageFormat, DefaultGammaSpace);
			}
		}
		else
		{
			for (const TPair<EFaceTextureType, FMetaHumanCharacterTextureInfo>& TextureInfoPair : InTextureInfo)
			{
				const FMetaHumanCharacterTextureInfo& TextureInfo = TextureInfoPair.Value;

				OutSynthesizedFaceImages.Add(TextureInfoPair.Key, TextureInfo.GetBlankImage());
			}
		}
	}
}

UTexture2D* FMetaHumanCharacterTextureSynthesis::CreateFaceTextureEditable(EFaceTextureType InTextureType, int32 InSizeX, int32 InSizeY)
{
	// Sanity check
	if (!(InSizeX >= 0 && (InSizeX == InSizeY)))
	{
		return nullptr;
	}

	// Create a sensible unique name for the texture to allow easy identification when debugging
	const FString TextureName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue((int64)InTextureType);
	const FString CandidateName = FString::Format(TEXT("T_Face_Editable_{0}"), { TextureName });
	const FName AssetName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName{ CandidateName }, EUniqueObjectNameOptions::GloballyUnique);

	// Create a transient texture with a single uncompressed mip and no resource
	UTexture2D* Texture = UTexture2D::CreateTransient(InSizeX, InSizeY, PF_B8G8R8A8, AssetName);
	if (Texture)
	{
		UE::MetaHuman::SetFaceTextureProperties(InTextureType, Texture);
	}

	return Texture;
}

UTexture2D* FMetaHumanCharacterTextureSynthesis::CreateFaceTextureFromSource(EFaceTextureType InTextureType, FImageView InTextureImage)
{
	if (InTextureImage.SizeX == 0 || InTextureImage.SizeY == 0)
	{
		return nullptr;
	}

	// Create a sensible unique name for the texture to allow easy identification when debugging
	const FString TextureName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue((int64)InTextureType);
	const FString CandidateName = FString::Format(TEXT("T_Face_{0}"), { TextureName });
	const FName AssetName = MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), FName{ CandidateName }, EUniqueObjectNameOptions::GloballyUnique);

	// Create a transient texture object
	UTexture2D* Texture = NewObject<UTexture2D>(
		GetTransientPackage(),
		AssetName,
		RF_Transient
	);

	// Set the texture source from the image data and then compile the texture object so that any platform data are optimized
	if (Texture)
	{
		Texture->PreEditChange(nullptr);

		Texture->Source.Init(InTextureImage);

		UE::MetaHuman::SetFaceTextureProperties(InTextureType, Texture);
		// TODO: Revisit mips generation, storing mips creates issues with textures in the MHC asset editor
		Texture->MipGenSettings = TMGS_NoMipmaps;

		Texture->UpdateResource();
		Texture->PostEditChange();
		FTextureCompilingManager::Get().FinishCompilation({ Texture });
	}

	return Texture;
}

void FMetaHumanCharacterTextureSynthesis::CreateSynthesizedFaceTextures(int32 InResolution,
	TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures)
{
	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		OutSynthesizedFaceTextures.Emplace(TextureType, CreateFaceTextureEditable(TextureType, InResolution, InResolution));
	}
}

bool FMetaHumanCharacterTextureSynthesis::AreTexturesAndImagesSuitableForSynthesis(
	const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer,
	const TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& InSynthesizedFaceTextures,
	const TMap<EFaceTextureType, FImage>& InSynthesizedFaceImages)
{
	if (!InFaceTextureSynthesizer.IsValid())
	{
		return false;
	}

	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		const TObjectPtr<UTexture2D>* TexturePtr = InSynthesizedFaceTextures.Find(TextureType);
		if (!TexturePtr)
		{
			// Expected to find a texture of this type
			return false;
		}

		const TObjectPtr<UTexture2D> Texture = *TexturePtr;

		if (Texture && (Texture->GetSizeX() != InFaceTextureSynthesizer.GetTextureSizeX()
			|| Texture->GetSizeY() != InFaceTextureSynthesizer.GetTextureSizeY()))
		{
			// TODO: Check texture is of the correct type for completeness
			return false;
		}
	}

	for (const TPair<EFaceTextureType, FImage>& Kvp : InSynthesizedFaceImages)
	{
		const FImage& Image = Kvp.Value;
		if (Image.SizeX != InFaceTextureSynthesizer.GetTextureSizeX()
			|| Image.SizeY != InFaceTextureSynthesizer.GetTextureSizeY()
			|| Image.Format != InFaceTextureSynthesizer.GetTextureFormat()
			|| Image.GammaSpace != InFaceTextureSynthesizer.GetTextureColorSpace())
		{
			return false;
		}
	}

	return true;
}

FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams FMetaHumanCharacterTextureSynthesis::SkinPropertiesToSynthesizerParams(const FMetaHumanCharacterSkinProperties& InSkinProperties, const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer)
{
	return UE::MetaHuman::SkinPropertiesToSynthesizerParams(InSkinProperties, InFaceTextureSynthesizer.GetMaxHighFrequencyIndex());
}

bool FMetaHumanCharacterTextureSynthesis::SynthesizeFaceTextures(const FMetaHumanCharacterSkinProperties& InSkinProperties, const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, TMap<EFaceTextureType, FImage>& OutCachedImages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FMetaHumanCharacterTextureSynthesis::SynthesizeFaceTextures");

	if (!InFaceTextureSynthesizer.IsValid())
	{
		return false;
	}

	FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams Params = UE::MetaHuman::SkinPropertiesToSynthesizerParams(InSkinProperties, InFaceTextureSynthesizer.GetMaxHighFrequencyIndex());

	// Synthesize albedo maps
	for (EFaceTextureType TextureType :
		{
			EFaceTextureType::Basecolor,
			EFaceTextureType::Basecolor_Animated_CM1,
			EFaceTextureType::Basecolor_Animated_CM2,
			EFaceTextureType::Basecolor_Animated_CM3,
		})
	{
		Params.MapType = static_cast<FMetaHumanFaceTextureSynthesizer::EMapType>(TextureType);

		if (OutCachedImages.Contains(TextureType))
		{
			if (!InFaceTextureSynthesizer.SynthesizeAlbedo(Params, OutCachedImages[TextureType]))
			{
				const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureType));
				UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to synthesize albedo map: {AlbedoMapTypeName}", *TextureTypeName);
				return false;
			}
		}
	}

	return true;
}

bool FMetaHumanCharacterTextureSynthesis::SynthesizeFaceAlbedoWithHFMap(EFaceTextureType InTextureType, const FMetaHumanCharacterSkinProperties& InSkinProperties, 
	const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, const TStaticArray<TArray<uint8>, 4>& InHFMaps, FImageView OutImage)
{
	if (!InFaceTextureSynthesizer.IsValid())
	{
		return false;
	}

	const int32 TextureIndex = static_cast<int32>(InTextureType);
	
	if (InTextureType >= EFaceTextureType::Normal)
	{
		const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureIndex));
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Invalid texture type [{TextureTypeName}] passed to SynthesizeFaceAlbedoWithHFMap, only base color types are supported", *TextureTypeName);
		return false;
	}

	FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams Params = UE::MetaHuman::SkinPropertiesToSynthesizerParams(InSkinProperties, InFaceTextureSynthesizer.GetMaxHighFrequencyIndex());
	Params.MapType = static_cast<FMetaHumanFaceTextureSynthesizer::EMapType>(TextureIndex);

	if (!InFaceTextureSynthesizer.SynthesizeAlbedoWithHF(Params, InHFMaps, OutImage))
	{
		const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureIndex));
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to synthesize albedo map: {TextureTypeName}", *TextureTypeName);
		return false;
	}

	return true;
}

bool FMetaHumanCharacterTextureSynthesis::SelectFaceTextures(const FMetaHumanCharacterSkinProperties& InSkinProperties, const FMetaHumanFaceTextureSynthesizer& InFaceTextureSynthesizer, TMap<EFaceTextureType, FImage>& OutCachedImages)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FMetaHumanCharacterTextureSynthesis::SelectFaceTextures");

	if (!InFaceTextureSynthesizer.IsValid())
	{
		return false;
	}
	
	FMetaHumanFaceTextureSynthesizer::FTextureSynthesisParams Params = UE::MetaHuman::SkinPropertiesToSynthesizerParams(InSkinProperties, InFaceTextureSynthesizer.GetMaxHighFrequencyIndex());
	const int32 BaseNormalIndex = static_cast<int32>(EFaceTextureType::Normal);

	// Select normal maps
	for (EFaceTextureType TextureType :
		{
			EFaceTextureType::Normal,
			EFaceTextureType::Normal_Animated_WM1,
			EFaceTextureType::Normal_Animated_WM2,
			EFaceTextureType::Normal_Animated_WM3,
		})
	{
		Params.MapType = static_cast<FMetaHumanFaceTextureSynthesizer::EMapType>(static_cast<int32>(TextureType) - BaseNormalIndex);

		if (OutCachedImages.Contains(TextureType))
		{
			if (!InFaceTextureSynthesizer.SelectNormal(Params, OutCachedImages[TextureType]))
			{
				const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureType));
				UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to select normal map: {NormalMapTypeName}", *TextureTypeName);
				return false;
			}
		}
	}

	// Select the cavity map
	if (OutCachedImages.Contains(EFaceTextureType::Cavity))
	{
		if (!InFaceTextureSynthesizer.SelectCavity(Params.HighFrequencyIndex, OutCachedImages[EFaceTextureType::Cavity]))
		{
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Failed to select cavity map");
			return false;
		}
	}

	return true;
}

void FMetaHumanCharacterTextureSynthesis::UpdateTexture(TConstArrayView<uint8> InRawData, TNotNull<UTexture2D*> InOutTexture)
{
	UE::MetaHuman::CopySynthesizedDataToTexture2D(InRawData, InOutTexture);
}

bool FMetaHumanCharacterTextureSynthesis::UpdateFaceTextures(const TMap<EFaceTextureType, FImage>& InCachedImages, TMap<EFaceTextureType, TObjectPtr<UTexture2D>>& OutSynthesizedFaceTextures)
{
	TRACE_CPUPROFILER_EVENT_SCOPE_STR("FMetaHumanCharacterTextureSynthesis::UpdateFaceTextures");
	
	if (OutSynthesizedFaceTextures.Num() != static_cast<int32>(EFaceTextureType::Count))
	{
		UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Invalid synthesized data sizes: cached images {InCachedImages.Num}, textures {OutSynthesizedFaceTextures.Num}", InCachedImages.Num(), OutSynthesizedFaceTextures.Num());
		return false;
	}

	// Iterate through all textures and assign the best available cached image
	for (EFaceTextureType TextureType : TEnumRange<EFaceTextureType>())
	{
		const EFaceTextureType CachedImageTextureType = InCachedImages.Contains(TextureType) ? TextureType : UE::MetaHuman::MapToCompatibleTextureType[static_cast<int32>(TextureType)];

		if (!InCachedImages.Contains(CachedImageTextureType))
		{
			const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureType));
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "No compatible cached image for {TextureTypeName}", *TextureTypeName);
			return false;
		}

		check(OutSynthesizedFaceTextures.Contains(TextureType));
		if (!UE::MetaHuman::CheckMatchingImageAndTextureSize(InCachedImages[CachedImageTextureType], OutSynthesizedFaceTextures[TextureType]))
		{
			const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByIndex(static_cast<int64>(TextureType));
			UE_LOGFMT(LogMetaHumanCharacterEditor, Error, "Mismatch between synthesized albedo texture and generated image for {TextureTypeName}", *TextureTypeName);
			return false;
		}
		UpdateTexture(InCachedImages[CachedImageTextureType].RawData, OutSynthesizedFaceTextures[TextureType]);
	}

	return true;
}
