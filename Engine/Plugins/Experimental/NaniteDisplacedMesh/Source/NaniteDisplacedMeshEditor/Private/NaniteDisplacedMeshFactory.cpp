// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshFactory.h"

#include "Misc/Paths.h"
#include "NaniteDisplacedMesh.h"
#include "NaniteDisplacedMeshEditorModule.h"
#include "NaniteDisplacedMeshLog.h"

#include "AssetToolsModule.h"
#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "DerivedDataBuildVersion.h"
#include "Editor.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Misc/PathViews.h"
#include "UObject/LinkerLoad.h"

#include "Rendering/NaniteResources.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NaniteDisplacedMeshFactory)

#define LOCTEXT_NAMESPACE "NaniteDisplacedMeshEditor"

namespace UE::NaniteDisplacedMesh::Private::Factory
{
	bool bUseNewAggregateID = true;
	FAutoConsoleVariableRef CVarUseNewAggregateID(
		TEXT("NaniteDisplacedMesh.UseNewAggregateID"),
		bUseNewAggregateID,
		TEXT("Change how the nanite displaced mesh linking chose the name of a asset.")
	);

	bool CanLinkAgainstTransientAsset(ELinkDisplacedMeshAssetSetting LinkSetting)
	{
		return LinkSetting == ELinkDisplacedMeshAssetSetting::LinkAgainstTransientAsset
			|| LinkSetting == ELinkDisplacedMeshAssetSetting::CanLinkAgainstPersistentAndTransientAsset;
	}

	bool CanLinkAgainstPersistentAsset(ELinkDisplacedMeshAssetSetting LinkSetting)
	{
		return LinkSetting != ELinkDisplacedMeshAssetSetting::LinkAgainstTransientAsset;
	}

	bool CanLinkAgainstNewAsset(ELinkDisplacedMeshAssetSetting LinkSetting)
	{
		return LinkSetting != ELinkDisplacedMeshAssetSetting::LinkAgainstExistingPersistentAsset;
	}
}


UNaniteDisplacedMeshFactory::UNaniteDisplacedMeshFactory()
{
	bCreateNew = true;
	bEditAfterNew = true;
	SupportedClass = UNaniteDisplacedMesh::StaticClass();
}

UNaniteDisplacedMesh* UNaniteDisplacedMeshFactory::StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return static_cast<UNaniteDisplacedMesh*>(NewObject<UNaniteDisplacedMesh>(InParent, Class, Name, Flags | RF_Transactional | RF_Public | RF_Standalone));
}

UObject* UNaniteDisplacedMeshFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UNaniteDisplacedMesh* NewNaniteDisplacedMesh = StaticFactoryCreateNew(Class, InParent, Name, Flags, Context, Warn);
	NewNaniteDisplacedMesh->bIsEditable = !bCreateReadOnlyAsset;
	NewNaniteDisplacedMesh->MarkPackageDirty();
	return NewNaniteDisplacedMesh;
}

UNaniteDisplacedMesh* LinkDisplacedMeshAsset(UNaniteDisplacedMesh* ExistingDisplacedMesh, const FNaniteDisplacedMeshParams& InParameters, const FString& DisplacedMeshFolder, ELinkDisplacedMeshAssetSetting LinkDisplacedMeshAssetSetting, bool* bOutCreatedNewMesh)
{
	FNaniteDisplacedMeshLinkParameters LinkParameters;
	LinkParameters.DisplacedMeshFolder = MakeStringView(DisplacedMeshFolder);
	LinkParameters.LinkDisplacedMeshAssetSetting = LinkDisplacedMeshAssetSetting;
	LinkParameters.bOutCreatedNewMesh = bOutCreatedNewMesh;
	return LinkDisplacedMeshAsset(ExistingDisplacedMesh, FValidatedNaniteDisplacedMeshParams(InParameters), LinkParameters);
}

UNaniteDisplacedMesh* LinkDisplacedMeshAsset(UNaniteDisplacedMesh* ExistingDisplacedMesh, FValidatedNaniteDisplacedMeshParams&& InParameters, const FNaniteDisplacedMeshLinkParameters& InLinkParameters)
{
	checkf(GEditor, TEXT("There is no need to run that code if we don't have the editor"));

	if (InLinkParameters.bOutCreatedNewMesh)
	{
		*InLinkParameters.bOutCreatedNewMesh = false;
	}

	if (!InParameters.IsDisplacementRequired())
	{
		return nullptr;
	}

	FNaniteDisplacedMeshEditorModule& NaniteDisplacedMeshEditorModule = FNaniteDisplacedMeshEditorModule::GetModule();
	if (NaniteDisplacedMeshEditorModule.OverrideNaniteDisplacedMeshLink.IsBound())
	{
		return NaniteDisplacedMeshEditorModule.OverrideNaniteDisplacedMeshLink.Execute(ExistingDisplacedMesh, MoveTemp(InParameters), InLinkParameters);
	}

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
		if (NaniteDisplacedMeshEditorModule.OnLinkDisplacedMeshOverride.IsBound())
		{
			return NaniteDisplacedMeshEditorModule.OnLinkDisplacedMeshOverride.Execute(InParameters.ConvertToNaniteDisplacedMeshParams(), FString(InLinkParameters.DisplacedMeshFolder), InLinkParameters.LinkDisplacedMeshAssetSetting);
		}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS


		using namespace UE::NaniteDisplacedMesh;
	const bool bCanLinkAgainstPresistentAsset = Private::Factory::CanLinkAgainstPersistentAsset(InLinkParameters.LinkDisplacedMeshAssetSetting);
	const bool bCanLinkAgainstTransientAsset = Private::Factory::CanLinkAgainstTransientAsset(InLinkParameters.LinkDisplacedMeshAssetSetting);
	const bool bCanLinkAgainstNewAsset = Private::Factory::CanLinkAgainstNewAsset(InLinkParameters.LinkDisplacedMeshAssetSetting);

	// Make sure the referenced displaced mesh asset matches the provided combination
	// Note: This is a faster test than generating Ids for LHS and RHS and comparing (this check will occur frequently)
	if (IsValid(ExistingDisplacedMesh))
	{
		if (!ExistingDisplacedMesh->HasAnyFlags(RF_Transient) && ExistingDisplacedMesh->HasAnyFlags(RF_Public))
		{
			// Persistent asset
			if (bCanLinkAgainstPresistentAsset)
			{
				if (ExistingDisplacedMesh->Parameters.IsEquivalent(InParameters))
				{
					return ExistingDisplacedMesh;
				}
			}
		}
		else
		{
			// Transient asset
			if (bCanLinkAgainstTransientAsset)
			{
				if (ExistingDisplacedMesh->Parameters.IsEquivalent(InParameters))
				{
					return ExistingDisplacedMesh;
				}
			}
		}
	}

	// Either the displaced mesh asset is stale (wrong permutation), or it is null.
	// In either case, find or create the correct displaced mesh asset permutation.
	FString DisplacedMeshName = GenerateLinkedDisplacedMeshAssetName(InParameters);

	// Generate unique asset path
	FString DisplacedAssetPath = FPaths::Combine(InLinkParameters.DisplacedMeshFolder, DisplacedMeshName);

	if (bCanLinkAgainstPresistentAsset)
	{
		// The mesh needed might already exist. Using load object because it's faster then using the asset registry which might still be loading
		if (UNaniteDisplacedMesh* LoadedDisplacedMesh = LoadObject<UNaniteDisplacedMesh>(nullptr, *DisplacedAssetPath, nullptr, LOAD_Quiet))
		{
			// Finish loading the object if needed
			LoadedDisplacedMesh->GetPackage()->FullyLoad();

			// The asset path may match, but someone could have (incorrectly) directly modified the parameters
			// on the displaced mesh asset.
			if (LoadedDisplacedMesh->Parameters.IsEquivalent(InParameters))
			{
				return LoadedDisplacedMesh;
			}
			else
			{
				FString LoadedDisplacedMeshId = GetAggregatedIdString(LoadedDisplacedMesh->Parameters);

				UE_LOG(
					LogNaniteDisplacedMesh,
					Error,
					TEXT("The NaniteDisplacementMesh parameters doesn't match the guid from its name (Current parameters: %s). Updating parameters of (%s). Consider saving the displaced mesh again to remove this error."),
					*LoadedDisplacedMeshId,
					*(LoadedDisplacedMesh->GetPathName())
				);

				// If this check assert we will need to update how we generate the id because we have a hash collision.
				ensure(LoadedDisplacedMeshId != GetAggregatedIdString(InParameters));

				if (UPackage* LoadedDisplacedMeshPackage = LoadedDisplacedMesh->GetPackage())
				{
					LoadedDisplacedMeshPackage->SetAssetAccessSpecifier(EAssetAccessSpecifier::Public);
				}

				LoadedDisplacedMesh->PreEditChange(nullptr);
				LoadedDisplacedMesh->Parameters = InParameters.ConvertToNaniteDisplacedMeshParams();
				LoadedDisplacedMesh->bIsEditable = false;
				LoadedDisplacedMesh->PostEditChange();


				return LoadedDisplacedMesh;
			}
		}
	}

	if (bCanLinkAgainstTransientAsset)
	{
		// Use a transient asset

		UPackage* NaniteDisplacedMeshTransientPackage = NaniteDisplacedMeshEditorModule.GetNaniteDisplacementMeshTransientPackage();

		// First check if we already have a valid temp asset
		{
			UObject* PotentialTempAsset = FindObject<UObject>(NaniteDisplacedMeshTransientPackage, *DisplacedMeshName);

			if (IsValid(PotentialTempAsset))
			{
				if (UNaniteDisplacedMesh* TempNaniteDisplacedMesh = Cast<UNaniteDisplacedMesh>(PotentialTempAsset))
				{
					return TempNaniteDisplacedMesh;
				}
			}

			if (!bCanLinkAgainstNewAsset)
			{
				return nullptr;
			}

			// Remove the invalid asset of the way (We don't want to deal with recycled objects)
			if (PotentialTempAsset)
			{
				PotentialTempAsset->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors);
			}
		}

		// Create a transient asset
		UNaniteDisplacedMesh* TempNaniteDisplacedMesh = UNaniteDisplacedMeshFactory::StaticFactoryCreateNew(
			UNaniteDisplacedMesh::StaticClass(),
			NaniteDisplacedMeshTransientPackage,
			*DisplacedMeshName,
			RF_Transactional | RF_Transient,
			nullptr,
			nullptr
		);

		// We want the garbage collector to be able to clean the temp assets when they are no longer referred
		TempNaniteDisplacedMesh->ClearFlags(RF_Standalone);
		TempNaniteDisplacedMesh->bIsEditable = false;
		TempNaniteDisplacedMesh->Parameters = InParameters.ConvertToNaniteDisplacedMeshParams();
		TempNaniteDisplacedMesh->PostEditChange();

		if (InLinkParameters.bOutCreatedNewMesh)
		{
			*InLinkParameters.bOutCreatedNewMesh = true;
		}

		return TempNaniteDisplacedMesh;
	}
	else if (bCanLinkAgainstNewAsset)
	{
		// We need to create a new persistent asset
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

		TStrongObjectPtr<UNaniteDisplacedMeshFactory> DisplacedMeshFactory(NewObject<UNaniteDisplacedMeshFactory>());
		DisplacedMeshFactory->bCreateReadOnlyAsset = true;
		if (UObject* Asset = AssetTools.CreateAsset(DisplacedMeshName, FString(InLinkParameters.DisplacedMeshFolder), UNaniteDisplacedMesh::StaticClass(), DisplacedMeshFactory.Get()))
		{
			UNaniteDisplacedMesh* NewDisplacedMesh = CastChecked<UNaniteDisplacedMesh>(Asset);
			NewDisplacedMesh->Parameters = InParameters.ConvertToNaniteDisplacedMeshParams();
			UPackage* NewDisplacedMeshPackage = NewDisplacedMesh->GetPackage();

			if (InLinkParameters.bForcePackageToBePublic)
			{
				NewDisplacedMeshPackage->SetAssetAccessSpecifier(EAssetAccessSpecifier::Public);
			}

			if (UEditorLoadingAndSavingUtils::SavePackages({ NewDisplacedMeshPackage }, /*bOnlyDirty=*/ false))
			{
				if (InLinkParameters.bOutCreatedNewMesh)
				{
					*InLinkParameters.bOutCreatedNewMesh = true;
				}

				NewDisplacedMesh->PostEditChange();
				return NewDisplacedMesh;
			}
		}
		else
		{
			UE_LOG(
				LogNaniteDisplacedMesh,
				Error,
				TEXT("Failed to create asset for %s in folder %.*s. Consult log for more details"),
				*DisplacedMeshName,
				InLinkParameters.DisplacedMeshFolder.Len(),
				InLinkParameters.DisplacedMeshFolder.GetData()
			);
		}
	}

	return nullptr;
}

FString GetSuggestedDisplacedMeshFolder(const FStringView& InSubPathForDisplacedMesh, const FValidatedNaniteDisplacedMeshParams& InParameters)
{
	if (InParameters.IsDisplacementRequired())
	{
		const FNaniteDisplacedMeshParams& Params = InParameters;
		if (const UPackage* Package = Params.BaseMesh->GetPackage())
		{
			const FString Path = Package->GetName();
			FStringView MountPath = FPathViews::GetMountPointNameFromPath(Path, nullptr, false);
			if (MountPath.Len() < Path.Len())
			{
				MountPath = FStringView(MountPath.GetData(), MountPath.Len() + 1);
			}

			for (const FNaniteDisplacedMeshDisplacementMap& DisplacementMap : Params.DisplacementMaps)
			{
				if (DisplacementMap.Texture)
				{
					if (const UPackage* TexturePackage = DisplacementMap.Texture->GetPackage())
					{
						const FString TexturePackagePath = TexturePackage->GetPathName();
						if (!FStringView(TexturePackagePath).StartsWith(MountPath))
						{
							MountPath.Reset();
							break;
						}
					}
				}
			}

			if (!MountPath.IsEmpty())
			{
				FString Output;
				Output.Reserve(MountPath.Len() + InSubPathForDisplacedMesh.Len());
				Output.Append(MountPath);
				Output.Append(InSubPathForDisplacedMesh);
				return Output;
			}

		}
	}

	return FString();
}

const TCHAR* LinkedDisplacedMeshAssetNamePrefix = TEXT("NaniteDisplacedMesh_");

FString GenerateLinkedDisplacedMeshAssetName(const FNaniteDisplacedMeshParams& InParameters)
{
	TStringBuilder<512> StringBuilder;

	StringBuilder.Append(LinkedDisplacedMeshAssetNamePrefix);
	StringBuilder.Append(GetAggregatedIdString(InParameters));
	return StringBuilder.ToString();
}

FGuid GetAggregatedId(const FNaniteDisplacedMeshParams& DisplacedMeshParams)
{
	UE::DerivedData::FBuildVersionBuilder IdBuilder;

	if (UE::NaniteDisplacedMesh::Private::Factory::CVarUseNewAggregateID->GetBool())
	{
		IdBuilder << NANITE_DISPLACED_MESH_ID_VERSION;

		IdBuilder << DisplacedMeshParams.RelativeError;

		if (IsValid(DisplacedMeshParams.BaseMesh))
		{
			IdBuilder << DisplacedMeshParams.BaseMesh->GetPathName();
		}

		for (int32 Index = 0; Index < DisplacedMeshParams.DisplacementMaps.Num(); ++Index)
		{
			const FNaniteDisplacedMeshDisplacementMap& DisplacementMap = DisplacedMeshParams.DisplacementMaps[Index];
			if (IsValid(DisplacementMap.Texture) && DisplacementMap.Magnitude > 0.f)
			{
				IdBuilder << Index;
				IdBuilder << DisplacementMap.Texture->GetPathName();
				IdBuilder << DisplacementMap.Magnitude;
				IdBuilder << DisplacementMap.Center;
			}
		}
	}
	else
	{
		constexpr int32 OldNaniteDisplacedMeshIdVersion = 1;
		IdBuilder << OldNaniteDisplacedMeshIdVersion;
		IdBuilder << DisplacedMeshParams.RelativeError;

		if (IsValid(DisplacedMeshParams.BaseMesh))
		{
			IdBuilder << DisplacedMeshParams.BaseMesh->GetPackage()->GetPersistentGuid();
		}

		for (auto& DisplacementMap : DisplacedMeshParams.DisplacementMaps)
		{
			if (IsValid(DisplacementMap.Texture))
			{
				IdBuilder << DisplacementMap.Texture->GetPackage()->GetPersistentGuid();
				IdBuilder << DisplacementMap.Texture->AddressX;
				IdBuilder << DisplacementMap.Texture->AddressY;
			}

			IdBuilder << DisplacementMap.Magnitude;
			IdBuilder << DisplacementMap.Center;
		}
	}

	return IdBuilder.Build();
}

FGuid GetAggregatedId(const UNaniteDisplacedMesh& DisplacedMesh)
{
	return GetAggregatedId(DisplacedMesh.Parameters);
}

FString GetAggregatedIdString(const FNaniteDisplacedMeshParams& DisplacedMeshParams)
{
	return GetAggregatedId(DisplacedMeshParams).ToString();
}

FString GetAggregatedIdString(const UNaniteDisplacedMesh& DisplacedMesh)
{
	return GetAggregatedId(DisplacedMesh).ToString();
}

#undef LOCTEXT_NAMESPACE
