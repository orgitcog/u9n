// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectUtilities/MetaHumanAssetManager.h"

#include "Import/MetaHumanAssetUpdateHandler.h"
#include "Import/MetaHumanImport.h"
#include "MetaHumanAssetReport.h"
#include "MetaHumanSDKEditor.h"
#include "MetaHumanSDKSettings.h"
#include "ProjectUtilities/MetaHumanProjectUtilities.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Developer/FileUtilities/Public/FileUtilities/ZipArchiveReader.h"
#include "Developer/FileUtilities/Public/FileUtilities/ZipArchiveWriter.h"
#include "EditorAssetLibrary.h"
#include "EngineAnalytics.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GroomAsset.h"
#include "GroomBindingAsset.h"
#include "HAL/PlatformFileManager.h"
#include "IO/PackageStore.h"
#include "JsonObjectConverter.h"
#include "K2Node_Variable.h"
#include "Materials/Material.h"
#include "Misc/FileHelper.h"

// Skeleton definitions extracted to avoid clutter
#include "MetaHumanSkeletonDefinitions.inl"
#include "Verification/MetaHumanCharacterTypesVerificationExtensionBase.h"
#include "Verification/MetaHumanCharacterVerification.h"

using namespace UE::MetaHuman;
#define LOCTEXT_NAMESPACE "MetaHumanAssetManager"

FMetaHumanAssetDescription::FMetaHumanAssetDescription(const FAssetData& InAssetData, EMetaHumanAssetType InAssetType, const FName& DisplayName) :
	Name(DisplayName.IsNone() ? InAssetData.AssetName : DisplayName),
	AssetData(InAssetData),
	AssetType(InAssetType)
{
	UMetaHumanAssetManager::UpdateAssetDependencies(*this);
}

FString GetFilename(const FString& PackageName)
{
	FString Filename;
	FPackageName::TryConvertLongPackageNameToFilename(PackageName, Filename, FPackageName::GetAssetPackageExtension());
	return Filename;
}

FMetaHumanArchiveEntry WriteToArchive(const FString& Filename, const FString& RootPackagePath, FZipArchiveWriter& ArchiveWriter, FString Version, FString ArchiveSubFolder)
{
	TArray<uint8> Data;
	FFileHelper::LoadFileToArray(Data, *Filename);
	FString RelativeFilename = Filename;
	FPaths::MakePathRelativeTo(RelativeFilename, *RootPackagePath);
	ArchiveWriter.AddFile(ArchiveSubFolder / RelativeFilename, Data, FDateTime::Now());
	return {RelativeFilename, Version};
}

void WriteUAssetToArchive(const FString& Package, const FString& RootPackagePath, FZipArchiveWriter& ArchiveWriter, FMetaHumanArchiveContents& Contents, FString ArchiveSubFolder)
{
	FString Version = TEXT("0.0");
	FString MainAssetName = FPaths::GetPath(Package) / FString::Format(TEXT("{0}.{0}"), {FPaths::GetBaseFilename(Package)});
	if (const UObject* Asset = LoadObject<UObject>(nullptr, *MainAssetName))
	{
		if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(Asset))
		{
			if (const FString* VersionMetaData = Metadata->Find(TEXT("MHAssetVersion")))
			{
				Version = *VersionMetaData;
			}
		}
	}

	FString PackageFilename = GetFilename(Package);
	// Don't add files that don't exist. These will cause crashes.
	if (IFileManager::Get().FileExists(*PackageFilename))
	{
		Contents.Files.Add(WriteToArchive(PackageFilename, RootPackagePath, ArchiveWriter, Version, ArchiveSubFolder));
	}
}

template <typename T>
void AddJsonToArchive(const T& StructReference, FString Filename, FZipArchiveWriter& ArchiveWriter)
{
	FString JsonString;
	FJsonObjectConverter::CustomExportCallback EmbedVerificationReport = FJsonObjectConverter::CustomExportCallback::CreateLambda([](FProperty* Property, const void* Value) -> TSharedPtr<FJsonValue>
	{
		if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			if (ObjectProperty->GetName() == TEXT("VerificationReport"))
			{
				if (const UMetaHumanAssetReport* Report = Cast<UMetaHumanAssetReport>(ObjectProperty->GetObjectPropertyValue(Value)))
				{
					TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
					FJsonObjectConverter::UStructToJsonObject(UMetaHumanAssetReport::StaticClass(), Report, Out);
					return MakeShared<FJsonValueObject>(Out);
				}
			}
		}
		// Returning nullptr will fall-through to default serialisation handling
		return nullptr;
	});
	FJsonObjectConverter::UStructToJsonObjectString(StructReference, JsonString, 0, 0, 0, &EmbedVerificationReport);
	TStringConversion<TStringConvert<TCHAR, char>> Convert = StringCast<ANSICHAR>(*JsonString);
	const TConstArrayView<uint8> JsonView(reinterpret_cast<const uint8*>(Convert.Get()), Convert.Length());
	ArchiveWriter.AddFile(Filename, JsonView, FDateTime::Now());
}

bool UMetaHumanAssetManager::CreateArchive(const TArray<FMetaHumanAssetDescription>& Assets, const FString& ArchivePath)
{
	// TODO - these zips are uncompressed, use Jarl's zlib support.
	IFileHandle* ArchiveFile = FPlatformFileManager::Get().GetPlatformFile().OpenWrite(*ArchivePath);
	if (!ArchiveFile)
	{
		return false;
	}

	FZipArchiveWriter ArchiveWriter(ArchiveFile);

	bool bMultiAssetArchive = Assets.Num() > 1;
	bool bFirstAsset = true;
	FMetaHumanMultiArchiveDescription ArchiveDescription;

	for (const FMetaHumanAssetDescription& Asset : Assets)
	{
		FMetaHumanArchiveContents Contents;
		FString ArchiveSubFolder = bMultiAssetArchive ? Asset.Name.ToString() : TEXT("");
		ArchiveDescription.ContainedArchives.Add(ArchiveSubFolder);
		const FString RootPackage = Asset.AssetData.PackageName.ToString();
		FString RootPackagePath = GetFilename(RootPackage);

		if (Asset.AssetType == EMetaHumanAssetType::CharacterAssembly)
		{
			// If we are a MetaHuman we need to go up another folder to get the proper root path.
			RootPackagePath = FPaths::GetPath(RootPackagePath);
		}

		FString RootPackageFolder = FPaths::GetPath(RootPackagePath);

		for (const FName& Dependency : Asset.DependentPackages)
		{
			WriteUAssetToArchive(Dependency.ToString(), RootPackagePath, ArchiveWriter, Contents, ArchiveSubFolder);
		}

		// Add in optional extra MetaHuman data files
		if (Asset.AssetType == EMetaHumanAssetType::CharacterAssembly)
		{
			const FString VersionFile = RootPackageFolder / Asset.Name.ToString() / TEXT("VersionInfo.txt");
			if (IFileManager::Get().FileExists(*VersionFile))
			{
				Contents.Files.Add(WriteToArchive(VersionFile, RootPackagePath, ArchiveWriter, TEXT("0.0"), ArchiveSubFolder));
			}
			const FString DnaFile = RootPackageFolder / Asset.Name.ToString() / TEXT("SourceAssets") / Asset.Name.ToString() + TEXT(".dna");
			if (IFileManager::Get().FileExists(*DnaFile))
			{
				Contents.Files.Add(WriteToArchive(DnaFile, RootPackagePath, ArchiveWriter, TEXT("0.0"), ArchiveSubFolder));
			}
			const FString CommonVersionFile = RootPackageFolder / TEXT("Common") / TEXT("VersionInfo.txt");
			if (IFileManager::Get().FileExists(*CommonVersionFile))
			{
				Contents.Files.Add(WriteToArchive(CommonVersionFile, RootPackagePath, ArchiveWriter, TEXT("0.0"), ArchiveSubFolder));
			}
		}
		AddJsonToArchive(Asset, ArchiveSubFolder / TEXT("Manifest.json"), ArchiveWriter);
		AddJsonToArchive(Contents, ArchiveSubFolder / TEXT("FileList.json"), ArchiveWriter);
		if (bMultiAssetArchive && bFirstAsset)
		{
			AddJsonToArchive(Asset, TEXT("Manifest.json"), ArchiveWriter);
		}

		if (bFirstAsset)
		{
			AnalyticsEvent(TEXT("ArchiveCreated"), {
								{TEXT("AssetType"), UEnum::GetDisplayValueAsText(Asset.AssetType).ToString()},
								{TEXT("NumAssets"), Assets.Num()}
							});
		}
		bFirstAsset = false;
	}

	if (bMultiAssetArchive)
	{
		AddJsonToArchive(ArchiveDescription, TEXT("ArchiveContents.json"), ArchiveWriter);
	}

	return true;
}

FMetaHumanAssetDescription& UMetaHumanAssetManager::UpdateAssetDependencies(FMetaHumanAssetDescription& Asset)
{
	// Find all dependent packages
	Asset.DependentPackages.Reset();

	Asset.DependentPackages.Add(Asset.AssetData.PackageName);

	// Add in optional WardrobeItem files as dependency roots
	if (Asset.AssetType == EMetaHumanAssetType::OutfitClothing || Asset.AssetType == EMetaHumanAssetType::SkeletalClothing || Asset.AssetType == EMetaHumanAssetType::Groom)
	{
		FName WardrobeItem = GetWardrobeItemPackage(Asset.AssetData.PackageName);
		if (!WardrobeItem.IsNone())
		{
			Asset.DependentPackages.Add(WardrobeItem);
		}
	}

	TSet<FName> Seen(Asset.DependentPackages);
	TQueue<FName> ToProcess;
	for (const FName& Package : Asset.DependentPackages)
	{
		ToProcess.Enqueue(Package);
	}

	FString RootPath = Asset.AssetData.PackagePath.ToString();
	if (Asset.AssetType == EMetaHumanAssetType::CharacterAssembly)
	{
		RootPath = FPaths::GetPath(RootPath);
	}

	FName ThisPackage;
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	while (ToProcess.Dequeue(ThisPackage))
	{
		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(ThisPackage, Dependencies);
		for (const FName& Dependency : Dependencies)
		{
			if (!Seen.Contains(Dependency))
			{
				Seen.Add(Dependency);
				// Note we are excluding all dependencies outside the root folder for the asset. Verification will have
				// to check that any references to packages outside that folder are allowed (i.e. Engine, HairStrands etc.).
				if (FPaths::IsUnderDirectory(Dependency.ToString(), RootPath))
				{
					Asset.DependentPackages.Add(Dependency);
					ToProcess.Enqueue(Dependency);
				}
			}
		}
	}

	return Asset;
}

void AddGroomDetails(FMetaHumanAssetDescription& Asset)
{
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	for (const FName& Package : Asset.DependentPackages)
	{
		TArray<FAssetData> PackagedAssets;
		AssetRegistry.GetAssetsByPackageName(Package, PackagedAssets);
		for (const FAssetData& AssetData : PackagedAssets)
		{
			if (const UClass* AssetClass = AssetData.GetClass())
			{
				if (AssetClass->IsChildOf<UGroomBindingAsset>())
				{
					if (UGroomBindingAsset* GroomBindingAsset = Cast<UGroomBindingAsset>(AssetData.GetSoftObjectPath().TryLoad()))
					{
						if (UGroomAsset* GroomAsset = GroomBindingAsset->GetGroom())
						{
							Asset.Details.NumUniqueGrooms += 1;
							Asset.Details.StrandsCount += GroomAsset->GetHairDescription().GetNumStrands();
							Asset.Details.StrandsPointCount += GroomAsset->GetHairDescription().GetNumVertices();

							Asset.Details.bPhysics = GroomAsset->GetHairGroupsPhysics().Num() != 0;
							Asset.Details.bHasLods = GroomAsset->GetLODCount() != 1;

							for (const FHairGroupsCardsSourceDescription& Cards : GroomAsset->GetHairGroupsCards())
							{
								if (Cards.ImportedMesh)
								{
									Asset.Details.CardMeshCount += 1;
									if (Cards.ImportedMesh->GetNumLODs())
									{
										Asset.Details.CardMeshVertices += Cards.ImportedMesh->GetNumVertices(0);
									}
									for (const TObjectPtr<UTexture2D>& Texture : Cards.Textures.Textures)
									{
										if (Texture)
										{
											Asset.Details.CardMeshTextureResolution.X = TMathUtil<int32>::Max(Asset.Details.CardMeshTextureResolution.X, Texture->GetSizeX());
											Asset.Details.CardMeshTextureResolution.Y = TMathUtil<int32>::Max(Asset.Details.CardMeshTextureResolution.Y, Texture->GetSizeY());
										}
									}
								}
							}
							for (const FHairGroupsMeshesSourceDescription& Meshes : GroomAsset->GetHairGroupsMeshes())
							{
								if (Meshes.ImportedMesh)
								{
									Asset.Details.VolumeMeshCount += 1;
									if (Meshes.ImportedMesh->GetNumLODs())
									{
										Asset.Details.VolumeMeshVertices += Meshes.ImportedMesh->GetNumVertices(0);
									}
									for (const TObjectPtr<UTexture2D>& Texture : Meshes.Textures.Textures)
									{
										if (Texture)
										{
											Asset.Details.VolumeMeshTextureResolution.X = TMathUtil<int32>::Max(Asset.Details.VolumeMeshTextureResolution.X, Texture->GetSizeX());
											Asset.Details.VolumeMeshTextureResolution.Y = TMathUtil<int32>::Max(Asset.Details.VolumeMeshTextureResolution.Y, Texture->GetSizeY());
										}
									}
								}
							}
						}
					}
				}
				else if (AssetClass->IsChildOf<UMaterial>())
				{
					Asset.Details.NumMaterials += 1;
				}
			}
		}
	}
}

void AddClothingDetails(FMetaHumanAssetDescription& Asset)
{
	Asset.Details.NumUniqueClothingItems += 1;

	FClothingAssetDetails ClothingDetails = FMetaHumanCharacterVerification::Get().GetDetailsForClothingAsset(Asset.AssetData.GetAsset());
	Asset.Details.bResizesWithBlendableBodies = ClothingDetails.bResizesWithBlendableBodies;
	Asset.Details.bHasClothingMask = ClothingDetails.bHasClothingMask;

	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	for (const FName& Package : Asset.DependentPackages)
	{
		TArray<FAssetData> PackagedAssets;
		AssetRegistry.GetAssetsByPackageName(Package, PackagedAssets);
		for (const FAssetData& AssetData : PackagedAssets)
		{
			if (const UClass* AssetClass = AssetData.GetClass())
			{
				if (AssetClass->IsChildOf<USkeletalMesh>())
				{
					if (USkeletalMesh* SkelMeshAsset = Cast<USkeletalMesh>(AssetData.GetSoftObjectPath().TryLoad()))
					{
						Asset.Details.IncludedLods = SkelMeshAsset->GetLODNum();
						if (Asset.Details.IncludedLods)
						{
							Asset.Details.Lod0VertCount = SkelMeshAsset->GetMeshDescription(0)->Vertices().Num();
						}
					}
				}
			}
		}
	}
}

void AddCharacterDetails(FMetaHumanAssetDescription& Asset)
{
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	for (const FName& Package : Asset.DependentPackages)
	{
		TArray<FAssetData> PackagedAssets;
		AssetRegistry.GetAssetsByPackageName(Package, PackagedAssets);
		for (const FAssetData& AssetData : PackagedAssets)
		{
			if (const UClass* AssetClass = AssetData.GetClass())
			{
				if (AssetClass->IsChildOf<UGroomBindingAsset>())
				{
					Asset.Details.bContainsGrooms = true;
				}
				else if (AssetClass->IsChildOf<USkeletalMesh>())
				{
					if (!AssetData.GetFullName().EndsWith("_FaceMesh") && !AssetData.GetFullName().EndsWith("_body"))
					{
						Asset.Details.bContainsClothing = true;
					}
				}
				else if (FMetaHumanCharacterVerification::Get().IsOutfitAsset(AssetData.GetAsset()))
				{
					Asset.Details.bContainsClothing = true;
				}
				if (AssetClass->IsChildOf<UBlueprint>())
				{
					static const FName MetaHumanAssetQualityLevelKey = TEXT("MHExportQuality");
					if (const UObject* BluePrint = AssetData.GetSoftObjectPath().TryLoad())
					{
						if (const TMap<FName, FString>* Metadata = FMetaData::GetMapForObject(BluePrint))
						{
							if (const FString* AssetQualityMetaData = Metadata->Find(MetaHumanAssetQualityLevelKey))
							{
								const int64 ParsedValue = StaticEnum<EMetaHumanQualityLevel>()->GetValueByName(FName(*AssetQualityMetaData));
								if (ParsedValue != INDEX_NONE)
								{
									Asset.Details.PlatformsIncluded.Add(static_cast<EMetaHumanQualityLevel>(ParsedValue));
								}
							}
						}
					}
				}
			}
		}
	}

	// Is this an editable character or an assembly?
	Asset.Details.bIsEditableCharacter = FMetaHumanCharacterVerification::Get().IsCharacterAsset(Asset.AssetData.GetAsset());

	if (Asset.Details.PlatformsIncluded.IsEmpty() && !Asset.Details.bIsEditableCharacter)
	{
		// If there is no information in the scene assume there is one cinematic character
		Asset.Details.PlatformsIncluded.Add(EMetaHumanQualityLevel::Cinematic);
	}

	// We only support single characters for now.
	Asset.Details.NumUniqueCharacters = 1;
}

FMetaHumanAssetDescription& UMetaHumanAssetManager::UpdateAssetDetails(FMetaHumanAssetDescription& Asset)
{
	// Gather generic data
	Asset.TotalSize = 0;

	for (const FName& Package : Asset.DependentPackages)
	{
		FString FileName = FPackageName::LongPackageNameToFilename(Package.ToString(), FPackageName::GetAssetPackageExtension());
		Asset.TotalSize += IFileManager::Get().FileSize(*FileName);
	}

	// Gather type-specific details
	Asset.Details = {};
	if (Asset.AssetType == EMetaHumanAssetType::Groom)
	{
		AddGroomDetails(Asset);
	}

	if (Asset.AssetType == EMetaHumanAssetType::SkeletalClothing || Asset.AssetType == EMetaHumanAssetType::OutfitClothing)
	{
		AddClothingDetails(Asset);
	}

	if (Asset.AssetType == EMetaHumanAssetType::CharacterAssembly || Asset.AssetType == EMetaHumanAssetType::Character)
	{
		AddCharacterDetails(Asset);
	}

	Asset.Details.EngineVersion = FEngineVersion::Current().ToString(EVersionComponent::Minor);

	return Asset;
}

FString UMetaHumanAssetManager::GetPackagingFolderForAssetType(const EMetaHumanAssetType AssetType)
{
	const UMetaHumanSDKSettings* ProjectSettings = GetDefault<UMetaHumanSDKSettings>();
	if (AssetType == EMetaHumanAssetType::Groom)
	{
		return ProjectSettings->GroomPackagingPath.Path;
	}
	if (AssetType == EMetaHumanAssetType::SkeletalClothing)
	{
		return ProjectSettings->SkeletalClothingPackagingPath.Path;
	}
	if (AssetType == EMetaHumanAssetType::OutfitClothing)
	{
		return ProjectSettings->OutfitPackagingPath.Path;
	}
	if (AssetType == EMetaHumanAssetType::Character)
	{
		return ProjectSettings->CharacterAssetPackagingPath.Path;
	}
	return ProjectSettings->CharacterAssemblyPackagingPath.Path;
}

FTopLevelAssetPath UMetaHumanAssetManager::GetMainAssetClassPathForAssetType(const EMetaHumanAssetType AssetType)
{
	if (AssetType == EMetaHumanAssetType::Groom)
	{
		return UGroomBindingAsset::StaticClass()->GetClassPathName();
	}
	if (AssetType == EMetaHumanAssetType::SkeletalClothing)
	{
		return USkeletalMesh::StaticClass()->GetClassPathName();
	}
	if (AssetType == EMetaHumanAssetType::OutfitClothing)
	{
		// Use a string to avoid bringing in a plugin dependency just to check a class type
		return FTopLevelAssetPath(FName("/Script/ChaosOutfitAssetEngine"), FName("ChaosOutfitAsset")); // UChaosOutfitAsset::StaticClass()->GetClassPathName();
	}
	if (AssetType == EMetaHumanAssetType::Character)
	{
		// Use a string to avoid bringing in a plugin dependency just to check a class type
		return FTopLevelAssetPath(FName("/Script/MetaHumanCharacter"), FName("MetaHumanCharacter")); // UMetaHumanCharacter::StaticClass()->GetClassPathName();
	}
	return UBlueprint::StaticClass()->GetClassPathName();
}

FName UMetaHumanAssetManager::GetWardrobeItemPackage(FName MainAssetPackage)
{
	FString RootPackageFolder = FPaths::GetPath(GetFilename(MainAssetPackage.ToString()));
	TArray<FString> WardrobeItemFiles;
	IFileManager::Get().FindFiles(WardrobeItemFiles, *RootPackageFolder, TEXT("WI_*.*"));
	if (WardrobeItemFiles.Num())
	{
		FString WardrobeItemPackageName;
		// Take the first "WI_" file if present.
		if (FPackageName::TryConvertFilenameToLongPackageName(RootPackageFolder / WardrobeItemFiles[0], WardrobeItemPackageName))
		{
			return FName(WardrobeItemPackageName);
		}
	}
	return FName();
}

TArray<FMetaHumanAssetDescription> UMetaHumanAssetManager::FindAssetsForPackaging(EMetaHumanAssetType AssetType)
{
	TArray<FMetaHumanAssetDescription> FoundAssets;
	if (AssetType == EMetaHumanAssetType::CharacterAssembly)
	{
		FString CharactersRoot;;
		if (FPackageName::TryConvertLongPackageNameToFilename(GetPackagingFolderForAssetType(EMetaHumanAssetType::CharacterAssembly), CharactersRoot))
		{
			const FString CharactersRootSearchpath = CharactersRoot / TEXT("*");
			TArray<FString> DirectoryList;
			IFileManager::Get().FindFiles(DirectoryList, *CharactersRootSearchpath, false, true);
			for (const FString& Name : DirectoryList)
			{
				TArray<FInstalledMetaHuman> FoundMetaHumans = FInstalledMetaHuman::GetInstalledMetaHumans(CharactersRoot / Name, CharactersRoot / Name / "Common");
				if (!FoundMetaHumans.IsEmpty())
				{
					FAssetData AssetData = IAssetRegistry::GetChecked().GetAssetByObjectPath(FSoftObjectPath::ConstructFromStringPath(FoundMetaHumans[0].GetRootAsset()), true);
					FoundAssets.Emplace(AssetData, EMetaHumanAssetType::CharacterAssembly, FName(FoundMetaHumans[0].GetName()));
				}
			}
		}
	}
	else
	{
		TArray<FAssetData> PotentialAssets;
		IAssetRegistry::GetChecked().GetAssetsByClass(GetMainAssetClassPathForAssetType(AssetType), PotentialAssets);
		for (const FAssetData& AssetData : PotentialAssets)
		{
			if (FPaths::GetPath(FPaths::GetPath(AssetData.PackageName.ToString())) == GetPackagingFolderForAssetType(AssetType))
			{
				FoundAssets.Emplace(AssetData, AssetType);
			}
		}
	}

	return FoundAssets;
}

bool UMetaHumanAssetManager::IsAssetOfType(const FName& RootPackage, EMetaHumanAssetType AssetType)
{
	// Ensure the assets are in the correct folder.
	if (!FPaths::IsUnderDirectory(RootPackage.ToString(), GetPackagingFolderForAssetType(AssetType)))
	{
		return false;
	}

	// Ensure the assets are the correct type.
	TArray<FAssetData> PackageAssets;
	IAssetRegistry::GetChecked().GetAssetsByPackageName(RootPackage, PackageAssets);
	if (PackageAssets.IsEmpty() || PackageAssets[0].AssetClassPath != GetMainAssetClassPathForAssetType(AssetType))
	{
		return false;
	}

	return true;
}

static bool IsSkeletonCompatible(const USkeleton* ToTest, const TMap<FName, FName>& TestHierarchy)
{
	FReferenceSkeleton ReferenceSkeleton = ToTest->GetReferenceSkeleton();
	int FoundMHBones = 0;
	for (int BoneIndex = 1; BoneIndex < ReferenceSkeleton.GetNum(); BoneIndex++)
	{
		FName BoneName = ReferenceSkeleton.GetBoneName(BoneIndex);
		// If the skeleton has a bone from the MH hierarchy, then the nearest ancestor also from the MH hierarchy
		// must be its direct parent in the MH hierarchy. This allows for bones to be added and the tree to be pruned
		// but not for the tree to be re-ordered or for bits of the hierarchy to be removed from the middle.
		if (TestHierarchy.Contains(BoneName))
		{
			FoundMHBones += 1;
			bool FoundParent = false;
			FName ExpectedParent = TestHierarchy[ReferenceSkeleton.GetBoneName(BoneIndex)];
			int ParentIndex = BoneIndex;
			do
			{
				if (ParentIndex == 0)
				{
					// Got all the way to the top looking for the parent.
					return false;
				}
				ParentIndex = ReferenceSkeleton.GetParentIndex(ParentIndex);
				FName ParentBone = ReferenceSkeleton.GetBoneName(ParentIndex);
				if (ExpectedParent == ParentBone)
				{
					FoundParent = true;
				}
				else if (TestHierarchy.Contains(ParentBone))
				{
					// Found another MH bone before we found the parent
					return false;
				}
			}
			while (!FoundParent);
		}
	}

	// This is totally arbitrary - we want it to match more than none, but we don't need it to match the entire skeleton
	// If we require the new skeleton to be a strict super-set of the MH base skeleton then we can set this to be the
	// number of bones in TestHierarchy
	static constexpr int RequiredMHBoneMatches = 4;
	return FoundMHBones > RequiredMHBoneMatches;
}


bool UMetaHumanAssetManager::IsMetaHumanBodyCompatibleSkeleton(const USkeleton* ToTest)
{
	return IsSkeletonCompatible(ToTest, MetaHumanBodyHierarchy);
}

bool UMetaHumanAssetManager::IsMetaHumanFaceCompatibleSkeleton(const USkeleton* ToTest)
{
	return IsSkeletonCompatible(ToTest, MetaHumanFacialHierarchy);
}

TFuture<bool> UMetaHumanAssetManager::ImportArchive(const FString& ArchivePath, const FMetaHumanImportOptions& ImportOptions, UMetaHumanAssetReport* Report)
{
	// Read the manifest from the archive
	IFileHandle* ArchiveFile = FPlatformFileManager::Get().GetPlatformFile().OpenRead(*ArchivePath);
	TSharedPtr<FZipArchiveReader> ZipReader = MakeShared<FZipArchiveReader>(ArchiveFile);
	TArray<uint8> FileContents;
	ZipReader->TryReadFile(TEXT("Manifest.json"), FileContents);
	FString ReadData(FileContents.Num(), reinterpret_cast<ANSICHAR*>(FileContents.GetData()));

	Report->SetVerbose(ImportOptions.bVerbose);

	FMetaHumanAssetDescription SourceDescription;
	FJsonObjectConverter::JsonObjectStringToUStruct(ReadData, &SourceDescription);

	if (SourceDescription.AssetType == EMetaHumanAssetType::CharacterAssembly)
	{
		FString SourcePath = FPaths::GetPath(FPaths::GetPath(SourceDescription.DependentPackages[0].ToString()));
		FMetaHumanImportDescription ImportParams{
			SourceDescription.Name.ToString(),
			TEXT("Common"),
			SourceDescription.Name.ToString(),
			"",
			false,
			SourcePath,
			FMetaHumanImportDescription::DefaultDestinationPath,
			{},
			ImportOptions.bForceUpdate,
			false,
			ZipReader,
			Report
		};
		return FMetaHumanAssetUpdateHandler::Enqueue(ImportParams);
	}

	//TODO: Grooms and clothing

	// Failure to handle package import
	TPromise<bool> ImportResultPromise;
	TFuture<bool> ImportResultFuture = ImportResultPromise.GetFuture();
	Report->AddError({LOCTEXT("UnsupportedImportOperation", "An attempt was made to import an unsupported archive type")});
	ImportResultPromise.SetValue(false);

	return ImportResultFuture;
}

#undef LOCTEXT_NAMESPACE
