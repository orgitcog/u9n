// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterDCCExport.h"

#include "MetaHumanCharacter.h"
#include "MetaHumanCharacterEditorSubsystem.h"
#include "MetaHumanCollection.h"
#include "MetaHumanCharacterEditorLog.h"
#include "MetaHumanCharacterPaletteEditorModule.h"
#include "MetaHumanCharacterBodyTextureUtils.h"
#include "MetaHumanCharacterThumbnailRenderer.h"
#include "Subsystem/MetaHumanCharacterBuild.h"
#include "MetaHumanCharacterPaletteUnpackHelpers.h"

#include "TG_Material.h"
#include "TG_Graph.h"
#include "Blueprint/TG_AsyncExportTask.h"
#include "Editor/EditorEngine.h"
#include "Engine/Texture2D.h"
#include "ImageUtils.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Logging/StructuredLog.h"
#include "Framework/Application/SlateApplication.h"
#include "DesktopPlatformModule.h"
#include "Developer/FileUtilities/Public/FileUtilities/ZipArchiveWriter.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ScopeExit.h"
#include "DNAUtils.h"
#include "Misc/EngineVersion.h"
#include "ObjectTools.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "MetaHumanCharacterPaletteProjectSettings.h"
#include "JsonObjectConverter.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "Misc/AssertionMacros.h"
#include "IMessageLogListing.h"
#include "MessageLogModule.h"


#define LOCTEXT_NAMESPACE "MetaHumanCharacterEditor"


namespace UE::MetaHuman
{
	static bool ExportCommonSourceAssets(const FString& DCCSourceAssetsPath, TSharedPtr<FZipArchiveWriter> InArchiveWriter, const FString& OutputFolder)
	{
		bool bResult = true;
	
		auto AddFolderFilesToArchive = [DCCSourceAssetsPath, InArchiveWriter, OutputFolder, &bResult](const FString& SubFolder)
			{
				// Adding common source files.
				const FString MapsFolder = DCCSourceAssetsPath / SubFolder;
				const FString SourceMapsFolder = FPaths::ConvertRelativePathToFull(MapsFolder);

				TArray<FString> FoundFiles;
				IFileManager::Get().FindFiles(FoundFiles, *SourceMapsFolder);
				for (FString& SourceAssetFile : FoundFiles)
				{
					const FString FullAssetPath = FPaths::ConvertRelativePathToFull(SourceMapsFolder / SourceAssetFile);
					if (InArchiveWriter)
					{
						TArray<uint8> Data;
						if (FFileHelper::LoadFileToArray(Data, *FullAssetPath))
						{
							InArchiveWriter->AddFile(TEXT("SourceAssets") / SubFolder / SourceAssetFile, Data, FDateTime::Now());
						}
						else
						{
							const FText Message = FText::Format(
								LOCTEXT("DCCExportFailure_CopyCommonAsset", "Failed to copy {0}."),
								FText::FromString(FullAssetPath));

							FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
							bResult = false;
						}
					}
					else
					{
						IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
						FString DestinationFolder = OutputFolder / TEXT("SourceAssets") / SubFolder;
						PlatformFile.CreateDirectoryTree(*DestinationFolder);
						FString ToFile = DestinationFolder / SourceAssetFile;
						if (!PlatformFile.CopyFile(*ToFile, *FullAssetPath))
						{
							const FText Message = FText::Format(
								LOCTEXT("DCCExportFailure_CopyCommonAsset", "Failed to copy {0}."),
								FText::FromString(FullAssetPath));

							FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
							bResult = false;
						}
					}
				}
			};

		AddFolderFilesToArchive(TEXT("maps"));
		AddFolderFilesToArchive(TEXT("masks"));
		AddFolderFilesToArchive(TEXT("shaders"));

		return bResult;
	}

	static bool WriteImageToArchiveAsPng(FImageView InImage, const FString& InImageName, TSharedPtr<FZipArchiveWriter> InArchiveWriter, const FString& OutputFolder)
	{
		TArray64<uint8> Data;
		if (FImageUtils::CompressImage(Data, TEXT("png"), InImage))
		{
			if (InArchiveWriter)
			{
				InArchiveWriter->AddFile(InImageName + TEXT(".png"), Data, FDateTime::Now());
			}
			else
			{
				FString ImagePath = OutputFolder / InImageName + TEXT(".png");
				if (!FFileHelper::SaveArrayToFile(Data, *ImagePath))
				{
					const FText Message = FText::Format(LOCTEXT("DCCExportFailure_SaveImage", "Failed to save image {0}."), FText::FromString(InImageName));
					FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
					return false;
				}
			}
			return true;
		}

		const FText Message = FText::Format(LOCTEXT("DCCExportFailure_CompressTimage", "Failed to compress image {0}."), FText::FromString(InImageName));
		FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
		return false;
	}

	static bool WriteToArchive(const FString& Filename, const FString& RootPackagePath, TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString& OutputFolder)
	{
		FString RelativeFilename = Filename;
		FPaths::MakePathRelativeTo(RelativeFilename, *RootPackagePath);
		if (ArchiveWriter)
		{
			TArray<uint8> Data;
			if (!FFileHelper::LoadFileToArray(Data, *Filename))
			{
				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_LoadFile", "Failed to load file {0}."), FText::FromString(Filename));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				return false;
			}		
			ArchiveWriter->AddFile(RelativeFilename, Data, FDateTime::Now());
		}
		else
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			FString ToFile = OutputFolder / RelativeFilename;
			if (!PlatformFile.CopyFile(*ToFile, *Filename))
			{
				const FText Message = FText::Format(
					LOCTEXT("DCCExportFailure_CopyFileToOutputFolder", "Failed to copy {0}."),
					FText::FromString(Filename));

				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				return false;
			}
		}
		return true;
	}

	static bool ExportDNAFiles(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString& OutputFolder)
	{
		bool bResult = true;

		if (InMetaHumanCharacter->HasFaceDNA())
		{
			TArray<uint8> FaceDNABuffer = InMetaHumanCharacter->GetFaceDNABuffer();
			if (ArchiveWriter)
			{
				ArchiveWriter->AddFile(TEXT("head.dna"), FaceDNABuffer, FDateTime::Now());
			}
			else
			{
				FString FullPath = OutputFolder / TEXT("head.dna");
				if (!FFileHelper::SaveArrayToFile(FaceDNABuffer, *FullPath))
				{
					FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_FaceDNANotSaved", "Character asset face DNA could not be saved."))
						->AddToken(FUObjectToken::Create(InMetaHumanCharacter));
					bResult = false;
				}
			}
		}
		else
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_NoFaceDNA", "Character asset has no face DNA."))
				->AddToken(FUObjectToken::Create(InMetaHumanCharacter));
			bResult = false;
		}

		TNotNull<UMetaHumanCharacterEditorSubsystem*> MetaHumanCharacterSubsystem = GEditor->GetEditorSubsystem<UMetaHumanCharacterEditorSubsystem>();

		// TODO: not use actor body skel mesh?
		if (const USkeletalMesh* ConstBodySkeletalMesh = MetaHumanCharacterSubsystem->Debug_GetBodyEditMesh(InMetaHumanCharacter))
		{
			// Cast away const-ness for GetAssetUserData. The mesh will not be modified.
			USkeletalMesh* BodySkeletalMesh = const_cast<USkeletalMesh*>(ConstBodySkeletalMesh);
			if (UDNAAsset* BodyDNA = BodySkeletalMesh->GetAssetUserData<UDNAAsset>())
			{
				TSharedRef<IDNAReader> BodyDnaReader = MetaHumanCharacterSubsystem->GetBodyState(InMetaHumanCharacter)->StateToDna(BodyDNA);
				TArray<uint8> BodyDnaBuffer = ReadStreamFromDNA(&BodyDnaReader.Get(), EDNADataLayer::All);
				if (ArchiveWriter)
				{
					ArchiveWriter->AddFile(TEXT("body.dna"), BodyDnaBuffer, FDateTime::Now());
				}
				else
				{
					FString FullPath = OutputFolder / TEXT("body.dna");
					if (!FFileHelper::SaveArrayToFile(BodyDnaBuffer, *FullPath))
					{
						FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_BodyDNANotSaved", "Character asset body DNA could not be saved."))
							->AddToken(FUObjectToken::Create(InMetaHumanCharacter));
						bResult = false;
					}
				}
			}
			else
			{
				FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_NoBodyDNA", "Character asset has no body DNA."))
					->AddToken(FUObjectToken::Create(InMetaHumanCharacter)); bResult = false;
			}
		}
		else
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_NoBodySkeletalMesh", "Character asset has no body Skeletal Mesh."))
				->AddToken(FUObjectToken::Create(InMetaHumanCharacter)); 
			bResult = false;
		}

		return bResult;
	}

	static bool GetSynthesizedFaceTextureDataCopy(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, EFaceTextureType InTextureType, FImageView& OutFaceImage, TArray<uint8>& OutDataCopy)
	{
		bool bResult = true;

		const FMetaHumanCharacterTextureInfo* TextureInfo = InMetaHumanCharacter->SynthesizedFaceTexturesInfo.Find(InTextureType);

		if (!TextureInfo)
		{
			bResult = false;
		}
		else
		{
			TFuture<FSharedBuffer> SynthesizedImageBuffer = InMetaHumanCharacter->GetSynthesizedFaceTextureDataAsync(InTextureType);
			if (!SynthesizedImageBuffer.Get().IsNull())
			{
				FImageInfo ImageInfo = TextureInfo->ToImageInfo();
				int32 TotalSize = ImageInfo.GetBytesPerPixel() * ImageInfo.GetNumPixels();
				OutDataCopy.SetNumUninitialized(TotalSize);

				if (SynthesizedImageBuffer.Get().GetData() != nullptr)
				{
					FMemory::Memcpy(OutDataCopy.GetData(), SynthesizedImageBuffer.Get().GetData(), TotalSize);
				}

				OutFaceImage = FImageView(ImageInfo, reinterpret_cast<void*>(OutDataCopy.GetData()));
			}
			else
			{
				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_NoFaceTexture", "Failed to load face texture {0}."),
					FText::FromString(StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(InTextureType))));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				bResult = false;
			}
		}

		return bResult;
	}

	static bool BakeTextures(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, TMap<EFaceTextureType, FImage>& OutFaceImages, FImage& OutBodyBaseColorImage, const FString& TempAssetPath, bool bInBakeMakeup)
	{
		bool bResult = true;

		UTextureGraphInstance* TextureGraph = LoadObject<UTextureGraphInstance>(nullptr, TEXT("/Script/Engine.TextureGraphInstance'/" UE_PLUGIN_NAME "/TextureGraphs/TGI_SkinDCC.TGI_SkinDCC'"));
		if (!TextureGraph)
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_NoFaceTextureGraph", "No Texture Graph for baking the face is assigned to the pipeline"));
			return false;
		}

		UTextureGraphInstance* TextureGraphInstance = DuplicateObject<UTextureGraphInstance>(TextureGraph, nullptr);
		check(TextureGraph);

		FMetaHumanCharacterFaceMaterialSet FaceMaterials;
		UMaterialInstanceDynamic* BodyMID = nullptr;
		UMetaHumanCharacterEditorSubsystem::Get()->GetMaterialSetForCharacter(InMetaHumanCharacter, FaceMaterials, BodyMID);

		// Texture graphs only accept material instance constants
		TStrongObjectPtr<UMaterialInstanceConstant> FaceMaterial{ UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(FaceMaterials.Skin[EMetaHumanCharacterSkinMaterialSlot::LOD0], GetTransientPackage()) };
		TStrongObjectPtr<UMaterialInstanceConstant> BodyMaterial{ UE::MetaHuman::PaletteUnpackHelpers::CreateMaterialInstanceCopy(BodyMID, GetTransientPackage()) };

		FImageView BaseColorCM1, BaseColorCM2, BaseColorCM3, NormalWM1, NormalWM2, NormalWM3;
		TArray<uint8> BaseColorCM1Data, BaseColorCM2Data, BaseColorCM3Data, NormalWM1Data, NormalWM2Data, NormalWM3Data;
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Basecolor_Animated_CM1, BaseColorCM1, BaseColorCM1Data);
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Basecolor_Animated_CM2, BaseColorCM2, BaseColorCM2Data);
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Basecolor_Animated_CM3, BaseColorCM3, BaseColorCM3Data);
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Normal_Animated_WM1, NormalWM1, NormalWM1Data);
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Normal_Animated_WM2, NormalWM2, NormalWM2Data);
		bResult &= GetSynthesizedFaceTextureDataCopy(InMetaHumanCharacter, EFaceTextureType::Normal_Animated_WM3, NormalWM3, NormalWM3Data);

		auto CreateAnimatedColorMapTexture = [](FImageView ImageView, FName Name)
			{
				const bool bDoPostEditChange = false;
				UTexture* AnimatedMapTexture = FImageUtils::CreateTexture(ETextureClass::TwoD, ImageView, GetTransientPackage(), Name.ToString(), RF_Transient, bDoPostEditChange);
				AnimatedMapTexture->CompressionSettings = TC_HDR_Compressed;
				AnimatedMapTexture->LODGroup = TEXTUREGROUP_Character;
				AnimatedMapTexture->SRGB = false;
				AnimatedMapTexture->PostEditChange();
				return AnimatedMapTexture;
			};

		auto CreateAnimatedNormalMapTexture = [](FImageView ImageView, FName Name)
			{
				const bool bDoPostEditChange = false;
				UTexture* AnimatedMapTexture = FImageUtils::CreateTexture(ETextureClass::TwoD, ImageView, GetTransientPackage(), Name.ToString(), RF_Transient, bDoPostEditChange);
				AnimatedMapTexture->CompressionSettings = TC_Normalmap;
				AnimatedMapTexture->LODGroup = TEXTUREGROUP_CharacterNormalMap;
				AnimatedMapTexture->SRGB = false;
				AnimatedMapTexture->PostEditChange();
				return AnimatedMapTexture;
			};

		UTexture* BaseColorCM1Texture = CreateAnimatedColorMapTexture(BaseColorCM1, TEXT("T_BaseColor_Animated_CM1"));
		UTexture* BaseColorCM2Texture = CreateAnimatedColorMapTexture(BaseColorCM2, TEXT("T_BaseColor_Animated_CM2"));
		UTexture* BaseColorCM3Texture = CreateAnimatedColorMapTexture(BaseColorCM3, TEXT("T_BaseColor_Animated_CM3"));

		UTexture* NormalWM1Texture = CreateAnimatedNormalMapTexture(NormalWM1, TEXT("T_Normal_Animated_WM1"));
		UTexture* NormalWM2Texture = CreateAnimatedNormalMapTexture(NormalWM2, TEXT("T_Normal_Animated_WM2"));
		UTexture* NormalWM3Texture = CreateAnimatedNormalMapTexture(NormalWM3, TEXT("T_Normal_Animated_WM3"));

		auto SetMaterialInput = [TextureGraph, TextureGraphInstance](FName InputName, UMaterialInstanceConstant* Material)
			{
				if (FVarArgument* MaterialArgument = TextureGraphInstance->InputParams.VarArguments.Find(InputName))
				{
					FTG_Material MaterialValue;
					MaterialValue.SetMaterial(Material);
					MaterialArgument->Var.SetAs(MaterialValue);
				}
				else
				{
					FMessageLog(UE::MetaHuman::MessageLogName)
						.Error(FText::Format(LOCTEXT("DCCExportFailure_NoFaceMaterialInput", "Failed to find input '{0}' in Texture Graph"), FText::FromName(InputName)))
						->AddToken(FUObjectToken::Create(TextureGraph));
				}
			};

		SetMaterialInput(TEXT("Face Material sRGB"), FaceMaterial.Get());
		SetMaterialInput(TEXT("Face Material"), FaceMaterial.Get());
		SetMaterialInput(TEXT("Body Material sRGB"), BodyMaterial.Get());

		auto SetTextureInput = [TextureGraphInstance, TextureGraph](FName InputName, UTexture* Texture)
			{
				if (FVarArgument* Argument = TextureGraphInstance->InputParams.VarArguments.Find(InputName))
				{
					FTG_Texture TextureValue;
					TextureValue.Descriptor.bIsSRGB = false;
					TextureValue.Descriptor.TextureFormat = ETG_TextureFormat::BGRA8;
					TextureValue.TexturePath = Texture->GetPathName();
					Argument->Var.SetAs(TextureValue);
				}
				else
				{
					FMessageLog(UE::MetaHuman::MessageLogName)
						.Error(FText::Format(LOCTEXT("DCCExportFailure_NoArgument", "Failed to find input argument named {0} in texture graph"), FText::FromName(InputName)))
						->AddToken(FUObjectToken::Create(TextureGraph));
				}
			};

		SetTextureInput(TEXT("AnimatedMap_CM1"), BaseColorCM1Texture);
		SetTextureInput(TEXT("AnimatedMap_CM2"), BaseColorCM2Texture);
		SetTextureInput(TEXT("AnimatedMap_CM3"), BaseColorCM3Texture);

		SetTextureInput(TEXT("AnimatedMap_WM1"), NormalWM1Texture);
		SetTextureInput(TEXT("AnimatedMap_WM2"), NormalWM2Texture);
		SetTextureInput(TEXT("AnimatedMap_WM3"), NormalWM3Texture);

		// Enable or disable the baking of makeup
		if (FVarArgument* BakeMakeupArgument = TextureGraphInstance->InputParams.VarArguments.Find(TEXT("Bake Makeup")))
		{
			BakeMakeupArgument->Var.SetAs(bInBakeMakeup);
		}
		else
		{
			FMessageLog(UE::MetaHuman::MessageLogName)
				.Error(LOCTEXT("DCCExportFailure_NoBakeMakeupArgument", "Failed to find input argument named 'Bake Makeup' in texture graph"))
				->AddToken(FUObjectToken::Create(TextureGraph));
		}

		TMap<FName, TSoftObjectPtr<UTexture>> GeneratedTextures;

		// find the output of the TG
		for (TPair<FTG_Id, FTG_OutputSettings>& Pair : TextureGraphInstance->OutputSettingsMap)
		{
			// The Texture Graph team has provided us with this temporary workaround to get the 
			// output parameter name.
			//
			// The hardcoded constant will be removed when a proper solution is available.
			const int32 PinIndex = 3;
			const FTG_Id PinId(Pair.Key.NodeIdx(), PinIndex);

			FTG_OutputSettings& OutputSettings = Pair.Value;

			FName OutputName = TextureGraphInstance->Graph()->GetParamName(PinId);

			OutputSettings.FolderPath = *TempAssetPath;
			OutputSettings.BaseName = FName{ TEXT("T_") + InMetaHumanCharacter->GetName() + TEXT("_") + OutputName.ToString()};
			// Get a path to the generated texture
			const FString PackageName = OutputSettings.FolderPath.ToString() / OutputSettings.BaseName.ToString();
			const FString AssetPath = FString::Format(TEXT("{0}.{1}"), { PackageName, OutputSettings.BaseName.ToString() });
			TSoftObjectPtr<UTexture> GeneratedTexture{ FSoftObjectPath(AssetPath) };

			GeneratedTextures.Emplace(OutputName, GeneratedTexture);
		}

		// export the TG textures
		const bool bOverwriteTextures = true;
		const bool bSave = false;
		const bool bExportAll = false;
		const bool bDisableCache = true;
		UTG_AsyncExportTask* Task = UTG_AsyncExportTask::TG_AsyncExportTask(TextureGraphInstance, bOverwriteTextures, bSave, bExportAll, bDisableCache);
		Task->ActivateBlocking(nullptr);

		for (const TPair<FName, TSoftObjectPtr<UTexture>>& GeneratedTexturePair : GeneratedTextures)
		{
			FName TextureName = GeneratedTexturePair.Key;
			TSoftObjectPtr<UTexture> GeneratedTexture = GeneratedTexturePair.Value;

			if (UTexture2D* ActualTexture = CastChecked<UTexture2D>(GeneratedTexture.LoadSynchronous()))
			{
				FImage Image;
				if (FImageUtils::GetTexture2DSourceImage(ActualTexture, Image))
				{
					if (TextureName == TEXT("Out_Face_BaseColor"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Basecolor, Image);
					}
					else if (TextureName == TEXT("Out_Face_Normal"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Normal, Image);
					}
					else if (TextureName == TEXT("Out_Body_BaseColor"))
					{
						OutBodyBaseColorImage = Image;
					}
					else if (TextureName == TEXT("Out_AnimatedMap_CM1"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Basecolor_Animated_CM1, Image);
					}
					else if (TextureName == TEXT("Out_AnimatedMap_CM2"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Basecolor_Animated_CM2, Image);
					}
					else if (TextureName == TEXT("Out_AnimatedMap_CM3"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Basecolor_Animated_CM3, Image);
					}
					else if (TextureName == TEXT("Out_AnimatedMap_WM1"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Normal_Animated_WM1, Image);
					}
					else if (TextureName == TEXT("Out_AnimatedMap_WM2"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Normal_Animated_WM2, Image);
					}
					else if (TextureName == TEXT("Out_AnimatedMap_WM3"))
					{
						OutFaceImages.Emplace(EFaceTextureType::Normal_Animated_WM3, Image);
					}
				}
				else
				{
					FMessageLog(UE::MetaHuman::MessageLogName)
						.Error(LOCTEXT("DCCExportFailure_InvalidGeneratedTexture", "No source data for the generated baked texture"))
						->AddToken(FUObjectToken::Create(ActualTexture));
					bResult = false;
				}
			}
		}

		return bResult;
	}

	static bool ExportSourceTextures(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, const FString MapsFolder, TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString OutputFolder)
	{
		bool bResult = true;

		// Body textures
		{
			for (const TPair<EBodyTextureType, FMetaHumanCharacterTextureInfo>& Pair : InMetaHumanCharacter->HighResBodyTexturesInfo)
			{
				const EBodyTextureType TextureType = Pair.Key;
				const FMetaHumanCharacterTextureInfo& TextureInfo = Pair.Value;

				if (TextureType == EBodyTextureType::Body_Basecolor)
				{
					// Body color needs to be baked and is handled separately
					continue;
				}

				FImageView BodyTextureImage;
				TFuture<FSharedBuffer> BodyImageBuffer = InMetaHumanCharacter->GetHighResBodyTextureDataAsync(TextureType);
				if (!BodyImageBuffer.Get().IsNull())
				{
					BodyTextureImage = FImageView(TextureInfo.ToImageInfo(), const_cast<void*>(BodyImageBuffer.Get().GetData()));
				}
				else
				{
					const FText Message = FText::Format(LOCTEXT("DCCExportFailure_NoBodyTexture", "Failed to load body texture {0}."),
						FText::FromString(StaticEnum<EBodyTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(TextureType))));
					FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
					bResult = false;
				}

				if (bResult)
				{
					const FString TextureTypeName = StaticEnum<EBodyTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(TextureType));
					const FString OutFileName = MapsFolder / TextureTypeName;
					bResult &= UE::MetaHuman::WriteImageToArchiveAsPng(BodyTextureImage, OutFileName, ArchiveWriter, OutputFolder);
				}
			}
		}

		return bResult;
	}

	static bool ExportBakedTextures(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, TSharedPtr<FZipArchiveWriter> InArchiveWriter, const FString& InMapsFolder, const FString& InTempAssetFolderPath, bool bInBakeMakeup, const FString& OutputFolder)
	{
		TMap<EFaceTextureType, FImage> FaceTextures;
		FImage BodyBaseColorTexture;
		if (!BakeTextures(InMetaHumanCharacter, FaceTextures, BodyBaseColorTexture, InTempAssetFolderPath, bInBakeMakeup))
		{
			return false;
		}

		// Write the Face Textures
		for (const TPair<EFaceTextureType, FImage>& FaceTexturePair : FaceTextures)
		{
			const EFaceTextureType TextureType = FaceTexturePair.Key;
			const FImage& FaceTextureImage = FaceTexturePair.Value;

			const FString TextureTypeName = StaticEnum<EFaceTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(TextureType));
			const FString OutFileName = InMapsFolder / TEXT("Head_") + TextureTypeName;
			if (!WriteImageToArchiveAsPng(FaceTextureImage, OutFileName, InArchiveWriter, OutputFolder))
			{
				return false;
			}
		}

		// Write the Body texture
		const FString BodyTextureTypeName = StaticEnum<EBodyTextureType>()->GetAuthoredNameStringByValue(static_cast<int64>(EBodyTextureType::Body_Basecolor));
		const FString OutFileName = InMapsFolder / BodyTextureTypeName;
		if (!WriteImageToArchiveAsPng(BodyBaseColorTexture, OutFileName, InArchiveWriter, OutputFolder))
		{
			return false;
		}

		return true;
	}
	
	static bool ExportUnmodifiedTextures(TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString& MapsFolder, const FString& DCCRootPath, const FString& OutputFolder)
	{
		auto AddSourceTextureToArchive = [ArchiveWriter, OutputFolder](const FString& TextureTypeName, UTexture2D* Texture) -> bool
			{
				FImage TextureImage;
				if (Texture && FImageUtils::GetTexture2DSourceImage(Texture, TextureImage))
				{
					return WriteImageToArchiveAsPng(TextureImage, TextureTypeName, ArchiveWriter, OutputFolder);
				}

				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_LoadTextureSource", "Failed to load source data for texture {0}."), FText::FromString(TextureTypeName));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message); 
				return false;
			};


		// Teeth
		const FString TeethColorTexturePath = TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Teeth/Textures/T_Teeth_BaseColor.T_Teeth_BaseColor'");
		const FString TeethNormalTexturePath = TEXT("/Script/Engine.Texture2D'/" UE_PLUGIN_NAME "/Lookdev_UHM/Teeth/Textures/T_Teeth_Normal.T_Teeth_Normal'");
		bool bResult = AddSourceTextureToArchive(MapsFolder / TEXT("Teeth_Color"), LoadObject<UTexture2D>(nullptr, *TeethColorTexturePath));
		bResult &= AddSourceTextureToArchive(MapsFolder / TEXT("Teeth_Normal"), LoadObject<UTexture2D>(nullptr, *TeethNormalTexturePath));

		// Eyes, TODO: use texture graph to get the actively selected eye textures instead of the default textures
		const FString EyesColorTexturePath = DCCRootPath / TEXT("Defaults/Maps/Eyes_Color.png");
		const FString EyesNormalTexturePath = DCCRootPath / TEXT("Defaults/Maps/Eyes_Normal.png");
		bResult &= WriteToArchive(EyesColorTexturePath, DCCRootPath / TEXT("Defaults/"), ArchiveWriter, OutputFolder);
		bResult &= WriteToArchive(EyesNormalTexturePath, DCCRootPath / TEXT("Defaults/"), ArchiveWriter, OutputFolder);

		// Eyelashes, TODO: the active eyelashes texture does not seem to be correct
		//AddSourceTextureToArchive(TEXT("eyelashes_color"), FMetaHumanCharacterSkinMaterials::GetEyelashesMask(InMetaHumanCharacter->HeadModelSettings.Eyelashes));
		bResult &= WriteToArchive(DCCRootPath / TEXT("Defaults/Maps/Eyelashes_Color.png"), DCCRootPath / TEXT("Defaults/"), ArchiveWriter, OutputFolder);

		return bResult;
	}

	static bool AddManifestToArchive(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString& OutputFolder)
	{
		const FString MetaHumanAssetName = InMetaHumanCharacter->GetName();
		
		// Write the manifest file
		FMetaHumanExportDCCManifest ExportManifest;
		ExportManifest.MetaHumanName = MetaHumanAssetName;
		ExportManifest.ExportPluginVersion = IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetDescriptor().VersionName;
		ExportManifest.ExportEngineVersion = FEngineVersion::Current().ToString();
		ExportManifest.ExportedAt = FDateTime::Now();

		FString JsonString;
		if (!FJsonObjectConverter::UStructToJsonObjectString(ExportManifest, JsonString))
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_ManifestParse", "Failed to parse manifest to json."));
			return false;
		}

		auto Convert = StringCast<ANSICHAR>(*JsonString);
		TConstArrayView<uint8, int32> JsonView(reinterpret_cast<const uint8*>(Convert.Get()), Convert.Length());

		if (ArchiveWriter)
		{
			ArchiveWriter->AddFile("ExportManifest.json", JsonView, FDateTime::Now());
		}
		else
		{
			FString JsonPath = OutputFolder / TEXT("ExportManifest.json");
			if (!FFileHelper::SaveArrayToFile(JsonView, *JsonPath))
			{
				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_SaveJson", "Failed to save json file {0}."), FText::FromString(JsonPath));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				return false;
			}
		}

		return true;
	}

	// Add a face thumbnail
	static bool AddThumbnailToArchive(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, TSharedPtr<FZipArchiveWriter> ArchiveWriter, const FString& OutputFolder)
	{
		const FString MetaHumanAssetName = InMetaHumanCharacter->GetName();
		const UMetaHumanCharacter* CharacterConstPtr = InMetaHumanCharacter;

		// Render a thumbnail for the face with higher resolution than the default one
		UMetaHumanCharacterThumbnailRenderer* ThumbnailRenderer = nullptr;
		if (FThumbnailRenderingInfo* RenderInfo = UThumbnailManager::Get().GetRenderingInfo(const_cast<UMetaHumanCharacter*>(CharacterConstPtr)))
		{
			ThumbnailRenderer = Cast<UMetaHumanCharacterThumbnailRenderer>(RenderInfo->Renderer);
		}

		if (ThumbnailRenderer)
		{
			const uint32 Resolution = 1024;

			// Set the renderer camera position to 
			EMetaHumanCharacterThumbnailCameraPosition CurrentCameraPosition = ThumbnailRenderer->CameraPosition;
			ThumbnailRenderer->CameraPosition = EMetaHumanCharacterThumbnailCameraPosition::Face;

			FObjectThumbnail CharacterThumbnail;
			ThumbnailTools::RenderThumbnail(
				const_cast<UMetaHumanCharacter*>(CharacterConstPtr),
				Resolution, Resolution,
				ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush,
				nullptr,
				&CharacterThumbnail);

			// Thumbnail rendering enqueues a rendering command, wait until it's complete
			FlushRenderingCommands();

			FImageView ThumbnailImage = CharacterThumbnail.GetImage();
			WriteImageToArchiveAsPng(ThumbnailImage, MetaHumanAssetName, ArchiveWriter, OutputFolder);

			// Restore the camera position
			ThumbnailRenderer->CameraPosition = CurrentCameraPosition;

			return true;
		}

		FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_GenerateThumbnail", "Failed to generate thumbnail"));
		return false;
	}
	
	static bool ExportChracterForDCC(TNotNull<const UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterEditorDCCExportParameters& InExportParams)
	{
		if (InExportParams.OutputFolderPath.IsEmpty())
		{
			FMessageLog(UE::MetaHuman::MessageLogName).Error(LOCTEXT("DCCExportFailure_OutputFolderEmpty", "Output folder not specified."));
			return false;
		}
		const FString AbsPluginContentDir = FPaths::ConvertRelativePathToFull(IPluginManager::Get().FindPlugin(UE_PLUGIN_NAME)->GetContentDir());
		const FString DCCRootPath = AbsPluginContentDir / TEXT("Optional/DCC");
		const FString MapsFolder = TEXT("Maps");
		const FString CharacterName = InMetaHumanCharacter->GetName();
		const FString CharacterPath = InMetaHumanCharacter->GetPathName();

		// Project path for temporary assets used during he DCC export
		const FString TempAssetFolderPath = FPackageName::GetLongPackagePath(CharacterPath) / CharacterName / TEXT("DCCExportAssets");

		const FString OutputFolder = InExportParams.OutputFolderPath / CharacterName;
		TSharedPtr<FZipArchiveWriter> ArchiveWriter = nullptr;
		if (InExportParams.bExportZipFile)
		{
		// Check if the archive file path is set and valid
			FString ArchivePath = InExportParams.OutputFolderPath / (InExportParams.ArchiveName.IsEmpty() ? CharacterName : InExportParams.ArchiveName);
			if (FPaths::FileExists(ArchivePath))
			{
				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_ArchiveFileExists", "File {0} exists."), FText::FromString(ArchivePath));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				return false;
			}

			if (!FPaths::GetExtension(ArchivePath).Equals("zip", ESearchCase::IgnoreCase))
			{
				ArchivePath = FPaths::SetExtension(ArchivePath, "zip");
			}
			IFileHandle* ArchiveFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*ArchivePath);
			if (!ArchiveFile)
			{
				const FText Message = FText::Format(LOCTEXT("DCCExportFailure_CannotOpenArchive", "Failed creating archive {0}."), FText::FromString(ArchivePath));
				FMessageLog(UE::MetaHuman::MessageLogName).Error(Message);
				return false;
			}

			// The zip writer closes the file handle
			ArchiveWriter = MakeShared<FZipArchiveWriter>(ArchiveFile);
		}
		else
		{
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			// Make sure the destination folder exists.
			if (!PlatformFile.DirectoryExists(*OutputFolder))
			{
				PlatformFile.CreateDirectoryTree(*OutputFolder);
			}			
		}	

		FScopedSlowTask ExportDCCTask(5, LOCTEXT("DCCExport_ExportCharacterTaskMessage", "Exporting MetaHuman Character asset for DCC"));
		ExportDCCTask.MakeDialog();

		// Face and body DNA
		if (!UE::MetaHuman::ExportDNAFiles(InMetaHumanCharacter, ArchiveWriter, OutputFolder))
		{
			return false;
		}
		ExportDCCTask.EnterProgressFrame();

		// Face and body textures
		if (!UE::MetaHuman::ExportSourceTextures(InMetaHumanCharacter, MapsFolder, ArchiveWriter, OutputFolder))
		{
			return false;
		}
		ExportDCCTask.EnterProgressFrame();
		
		if (!UE::MetaHuman::ExportBakedTextures(InMetaHumanCharacter, ArchiveWriter, MapsFolder, TempAssetFolderPath, InExportParams.bBakeFaceMakeup, OutputFolder))
		{
			return false;
		}

		ExportDCCTask.EnterProgressFrame();

		if (!UE::MetaHuman::ExportUnmodifiedTextures(ArchiveWriter, MapsFolder, DCCRootPath, OutputFolder))
		{
			return false;
		}
		ExportDCCTask.EnterProgressFrame();

		// Copy common assets for all DCC exports
		if (!UE::MetaHuman::ExportCommonSourceAssets(DCCRootPath / TEXT("SourceAssets"), ArchiveWriter, OutputFolder))
		{
			return false;
		}
		ExportDCCTask.EnterProgressFrame();

		// Add character info
		if (!UE::MetaHuman::AddManifestToArchive(InMetaHumanCharacter, ArchiveWriter, OutputFolder))
		{
			return false;
		}

		// Add thumbnail
		if (!UE::MetaHuman::AddThumbnailToArchive(InMetaHumanCharacter, ArchiveWriter, OutputFolder))
		{
			return false;
		}

		return true;
	}
} // namespace UE::MetaHuman


void FMetaHumanCharacterEditorDCCExport::ExportCharacterForDCC(TNotNull<UMetaHumanCharacter*> InMetaHumanCharacter, const FMetaHumanCharacterEditorDCCExportParameters& InExportParams)
{
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.GetLogListing(UE::MetaHuman::MessageLogName)->ClearMessages();

	const bool bWasSuccessful = UE::MetaHuman::ExportChracterForDCC(InMetaHumanCharacter, InExportParams);
	const FText SuccessMessageText = LOCTEXT("CharacterDCCExportSucceeded", "MetaHuman Character DCC export succeeded");
	const FText FailureMessageText = LOCTEXT("CharacterDCCExportFailed", "MetaHuman Character DCC export failed");

	FMetaHumanCharacterEditorBuild::ReportMessageLogErrors(bWasSuccessful, SuccessMessageText, FailureMessageText);
}

#undef LOCTEXT_NAMESPACE
