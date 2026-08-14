// Copyright Epic Games, Inc. All Rights Reserved.

#include "Verification/VerifyMetaHumanPackageSource.h"

#include "MetaHumanAssetReport.h"
#include "ProjectUtilities/MetaHumanAssetManager.h"

#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Queue.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/RuntimeErrors.h"
#include "UObject/Package.h"
#include "Verification/MetaHumanCharacterVerification.h"

#define LOCTEXT_NAMESPACE "VerifyMetaHumanPackageSource"

enum class EDependencyState: int8
{
	Allowed,
	AllowedDoNotFollow,
	Forbidden
};

static EDependencyState IsDependencyAllowed(FString Dependency, TArray<FString> ContentPaths)
{
	static TArray<FString> AllowedPaths = {
		// Core Engine Assets
		"/Engine",
		"/Script/Engine",
		"/Script/CoreUObject",

		// Commonly used engine types
		"/Script/AnimGraph",
		"/Script/AnimGraphRuntime",
		"/Script/AnimationCore",
		"/Script/AnimationData",
		"/Script/AnimationModifiers",
		"/Script/BlueprintGraph",
		"/Script/Chaos",
		"/Script/ChaosCloth",
		"/Script/ChaosOutfitAssetDataflowNodes",
		"/Script/ChaosClothAssetEngine",
		"/Script/ClothingSystemRuntimeInterface",
		"/Script/ClothingSystemRuntimeNv",
		"/Script/ClothingSystemRuntimeCommon",
		"/Script/DataflowEditor",
		"/Script/DataflowEngine",
		"/Script/IKRig",
		"/Script/IKRigDeveloper",
		"/Script/InterchangeEngine",
		"/Script/LiveLink",
		"/Script/LiveLinkAnimationCore",
		"/Script/LiveLinkGraphNode",
		"/Script/LiveLinkInterface",
		"/Script/MeshDescription",
		"/Script/MetaHumanSDKRuntime",
		"/Script/MovieScene",
		"/Script/MovieSceneTracks",
		"/Script/NavigationSystem",
		"/Script/PBIK",
		"/Script/PhysicsCore",
		"/Script/RigLogicDeveloper",
		"/Script/RigLogicModule",
		"/Script/RigVM",
		"/Script/RigVMDeveloper",
		"/Script/UnrealEd",
		"/Script/USDClasses",

		// Hair-strands plugin
		"/HairStrands",
		"/Script/HairStrands",
		"/Script/HairStrandsCore",

		// Interchange plugin
		"/InterchangeAssets",

		// Niagara plugin
		"/Niagara",
		"/Script/Niagara",
		"/Script/NiagaraCore",
		"/Script/NiagaraEditor",
		"/Script/NiagaraShader",

		// ControlRig plugin
		"/ControlRig",
		"/Script/ControlRig",
		"/Script/ControlRigDeveloper",
		"/Script/ControlRigSpline",

		// MetaHumanCharacter plugin
		"/MetaHumanCharacter",
		"/Script/MetaHumanCharacter",
		"/Script/MetaHumanCharacterPalette",
		"/Script/MetaHumanDefaultPipeline",
		"/Script/MetaHumanDefaultEditorPipeline",
		"/Script/DataHierarchyEditor",

		// ChaosClothAsset plugin
		"/ChaosClothAsset",

		// ChaosOutfitAsset plugin
		"/ChaosOutfitAsset",
		"/Script/ChaosOutfitAssetEngine"
	};

	for (const FString& RootPath : ContentPaths)
	{
		if (FPaths::IsUnderDirectory(Dependency, RootPath))
		{
			return EDependencyState::Allowed;
		}
	}

	for (const FString& RootPath : AllowedPaths)
	{
		if (FPaths::IsUnderDirectory(Dependency, RootPath))
		{
			return EDependencyState::AllowedDoNotFollow;
		}
	}

	return EDependencyState::Forbidden;
}

static UObject* GetMainObjectFromPackageName(const FName& PackageName)
{
	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssetsByPackageName(PackageName, Assets);
	if (Assets.Num())
	{
		return Assets[0].GetAsset();
	}

	return nullptr;
}

static FString StripPrefix(const FString& BaseName)
{
	int32 Index;
	if (BaseName.FindChar('_', Index) && Index < 4)
	{
		return BaseName.RightChop(Index + 1);
	}
	return BaseName;
}

void UVerifyMetaHumanPackageSource::Verify_Implementation(const UObject* ToVerify, UMetaHumanAssetReport* Report, const FMetaHumansVerificationOptions& Options) const
{
	if (!ensureAsRuntimeWarning(ToVerify) || !ensureAsRuntimeWarning(Report))
	{
		return;
	}

	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(ToVerify->GetName()));

	const UPackage* RootPackage = ToVerify->GetPackage();
	FName ThisPackage(RootPackage->GetFName());
	FString RootPath = FPaths::GetPath(RootPackage->GetName()); // The path containing all assets in the asset group
	FString AssetGroupName = FPaths::GetBaseFilename(RootPackage->GetName()); // The name of the asset group given the name of the main asset
	FString ExpectedName = FPaths::GetBaseFilename(RootPath); // The expected name of the main asset given the folder name
	TArray<FString> AllowedDependenciesPaths = {RootPath / AssetGroupName};

	if (UMetaHumanAssetManager::IsAssetOfType(ThisPackage, EMetaHumanAssetType::CharacterAssembly))
	{
		// check we have e.g. Characters/Ada/Ada
		FString NewRootPath = FPaths::GetPath(RootPath);
		if (FPaths::GetBaseFilename(NewRootPath) != FPaths::GetBaseFilename(RootPath))
		{
			Args.Add(TEXT("NewRootPath"), FText::FromString(NewRootPath));
			Args.Add(TEXT("RootPath"), FText::FromString(RootPath));
			Report->AddError({FText::Format(LOCTEXT("CharacterMainAssetPathIncorrect", "The Asset {AssetName} is in the folder \"{NewRootPath}\\{RootPath}\" when it should either be in \"{RootPath}\\{RootPath}\" or \"{NewRootPath}\\{NewRootPath}\""), Args), ToVerify});
		}
		AssetGroupName = StripPrefix(FPaths::GetBaseFilename(RootPackage->GetName()));
		RootPath = NewRootPath; // Go up one in the hierarchy for MetaHumans
		ExpectedName = ExpectedName / "BP_" + ExpectedName;
		AllowedDependenciesPaths = {RootPath / AssetGroupName, {RootPath / "Common"}};
	}
	else
	{
		// Allow for prefix on base asset.
		if (FPaths::GetBaseFilename(RootPath) != AssetGroupName)
		{
			AssetGroupName = StripPrefix(FPaths::GetBaseFilename(RootPackage->GetName()));
			AllowedDependenciesPaths = {RootPath / AssetGroupName};
		}
	}

	// Check Basic Structure
	if (FPaths::GetBaseFilename(RootPath) != AssetGroupName)
	{
		Args.Add(TEXT("ExpectedPath"), FText::FromString(FPaths::GetPath(RootPath) / AssetGroupName));
		Args.Add(TEXT("ExpectedName"), FText::FromString(ExpectedName));
		Args.Add(TEXT("RootPath"), FText::FromString(RootPath));
		Report->AddError({FText::Format(LOCTEXT("AssetPathNotCorrect", "The Asset {AssetName} is at the location {RootPath} when it should either be at {ExpectedPath} or called {ExpectedName}"), Args), ToVerify});
	}

	// Check all dependencies are allowed
	TQueue<FName> ToProcess;
	ToProcess.Enqueue(ThisPackage);
	TSet<FName> Seen = {ThisPackage};
	const IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();

	// Add in optional WardrobeItem files as dependency roots
	if (UMetaHumanAssetManager::IsAssetOfType(ThisPackage, EMetaHumanAssetType::OutfitClothing) || UMetaHumanAssetManager::IsAssetOfType(ThisPackage, EMetaHumanAssetType::SkeletalClothing) || UMetaHumanAssetManager::IsAssetOfType(ThisPackage, EMetaHumanAssetType::Groom))
	{
		FName WardrobeItemPackage = UMetaHumanAssetManager::GetWardrobeItemPackage(ThisPackage);
		if (!WardrobeItemPackage.IsNone())
		{
			UObject* WardrobeItemObject = GetMainObjectFromPackageName(WardrobeItemPackage);
			if (WardrobeItemObject && UE::MetaHuman::FMetaHumanCharacterVerification::Get().IsWardrobeItem(WardrobeItemObject))
			{
				ToProcess.Enqueue(WardrobeItemPackage);
				Seen.Add(WardrobeItemPackage);
			}
			else
			{
				Report->AddError({FText::Format(LOCTEXT("NotAWardrobeItem", "The File {AdditionalFileName} is not a valid wardrobe item."), Args), ToVerify});
			}
		}
	}

	while (ToProcess.Dequeue(ThisPackage))
	{
		TArray<FName> Dependencies;
		AssetRegistry.GetDependencies(ThisPackage, Dependencies);
		for (const FName& Dependency : Dependencies)
		{
			if (!Seen.Contains(Dependency))
			{
				Seen.Add(Dependency);
				// Check that all referenced packages are included in the allowed folders
				Args.Add(TEXT("SourceName"), FText::FromName(ThisPackage));
				Args.Add(TEXT("DependencyName"), FText::FromName(Dependency));
				EDependencyState AllowState = IsDependencyAllowed(Dependency.ToString(), AllowedDependenciesPaths);
				if (AllowState == EDependencyState::Forbidden)
				{
					UObject* SourceObject = GetMainObjectFromPackageName(ThisPackage);
					Report->AddError({FText::Format(LOCTEXT("DependencyOutOfTree", "The Asset {SourceName} is attempting to reference {DependencyName} which is not in the correct folder to be included in the package"), Args), SourceObject});
				}

				if (AllowState == EDependencyState::Allowed)
				{
					if (FPaths::IsUnderDirectory(Dependency.ToString(), RootPath))
					{
						// Check that referenced asset files actually exist
						FString DependencyFilename;
						FPackageName::TryConvertLongPackageNameToFilename(Dependency.ToString(), DependencyFilename, FPackageName::GetAssetPackageExtension());
						if (!IFileManager::Get().FileExists(*DependencyFilename))
						{
							Args.Add(TEXT("DependencyFileName"), FText::FromString(DependencyFilename));
							Report->AddError({FText::Format(LOCTEXT("DependencyOnMissingAsset", "The Asset {SourceName} is attempting to reference {DependencyName} which does not seem to be a file on disk ({DependencyFileName} is missing)."), Args), ToVerify});
						}
					}

					ToProcess.Enqueue(Dependency);
				}
			}
		}
	}

	// Check all files in the folder are included in the package
	FString RootFilepath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(RootPath));
	TArray<FString> PackageFiles;
	IFileManager::Get().FindFilesRecursive(PackageFiles, *RootFilepath, TEXT("*.*"), true, false);
	bool bFirstAdditionalFile = true;
	for (const FString& File : PackageFiles)
	{
		Args.Add(TEXT("AdditionalFileName"), FText::FromString(File));
		// Extra bundled files
		if (FPaths::GetBaseFilename(FPaths::GetPath(File)) == TEXT("SourceAssets") && FPaths::GetExtension(File) == TEXT("dna"))
		{
			// Embedded DNA files
			continue;
		}
		if (FPaths::GetPath(FPaths::GetPath(File)) == RootFilepath && FPaths::GetCleanFilename(File) == TEXT("VersionInfo.txt"))
		{
			// Embedded Version data
			continue;
		}
		FName PackageName(FPackageName::FilenameToLongPackageName(File));
		if (!Seen.Contains(PackageName))
		{
			if (FPaths::GetPath(File) == RootFilepath && FPaths::GetBaseFilename(File).StartsWith(TEXT("WI_")))
			{
				Report->AddError({FText::Format(LOCTEXT("MultipleWardrobeItems", "Found additional Wardrobe Item {AdditionalFileName}. There should only be one Wardrobe Item for the main asset"), Args), ToVerify});
			}
			if (bFirstAdditionalFile)
			{
				Report->AddWarning({FText::Format(LOCTEXT("ExtraFilesInPackageFolder", "The packaging folder contains files not referenced by the main asset {AssetName}. These files will not be included in the package. See the info section for further details."), Args), ToVerify});
				bFirstAdditionalFile = false;
			}
			Report->AddInfo({FText::Format(LOCTEXT("FileNotIncludedInPackage", "The unreferenced file {AdditionalFileName} is in the packaging folder."), Args), ToVerify});
		}
	}
}

#undef LOCTEXT_NAMESPACE
