// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineAssemblySchema.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#endif

const FSoftObjectPath UCineAssemblySchema::DefaultThumbnailPath = FSoftObjectPath(TEXT("/CinematicAssemblyTools/Resources/DefaultSchemaThumbnail.DefaultSchemaThumbnail"));
const FName UCineAssemblySchema::SchemaGuidPropertyName = GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, SchemaGuid);

UCineAssemblySchema::UCineAssemblySchema()
{
	if (!HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		ThumbnailBrush = MakeShared<FSlateBrush>();
		ThumbnailBrush->ImageSize = FVector2D(64.0f, 64.0f);
		UpdateThumbnailBrush();
	}
}

FGuid UCineAssemblySchema::GetSchemaGuid() const
{
	return SchemaGuid;
}

void UCineAssemblySchema::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	for (FAssemblyMetadataDesc& Desc : AssemblyMetadata)
	{
		Ar << Desc.DefaultValue;
	}
}

void UCineAssemblySchema::PostInitProperties()
{
	Super::PostInitProperties();

	if (!HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_NeedLoad | RF_WasLoaded) && !SchemaGuid.IsValid())
	{
		SchemaGuid = FGuid::NewGuid();
	}
}

void UCineAssemblySchema::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	if (!bDuplicateForPIE)
	{
		SchemaGuid = FGuid::NewGuid();
	}
}

void UCineAssemblySchema::PostLoad()
{
	Super::PostLoad();

	if (!SchemaGuid.IsValid())
	{
		SchemaGuid = FGuid::NewGuid();
	}

	UpdateThumbnailBrush();
}

#if WITH_EDITOR
void UCineAssemblySchema::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCineAssemblySchema, ThumbnailImage))
	{
		UpdateThumbnailBrush();
	}
}
#endif

const FSlateBrush* UCineAssemblySchema::GetThumbnailBrush() const
{
	return ThumbnailBrush.Get();
}

bool UCineAssemblySchema::SupportsRename() const
{
	return bSupportsRename;
}

#if WITH_EDITOR
void UCineAssemblySchema::RenameAsset(const FString& InNewName)
{
	// Early-out if the input name already matches the name of this schema
	if (SchemaName == InNewName)
	{
		return;
	}

	// If this schema does not yet have a valid package yet (i.e. it is still being configured), then there is no need to use Asset Tools to rename it
	if (GetPackage() == GetTransientPackage())
	{
		SchemaName = InNewName;
		return;
	}

	// The default behavior for schema assets is to not allow renaming from Content Browser.
	// However, this function relies on renaming being supported, so we temporarily enable to do the programmatic rename.
	bSupportsRename = true;

	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();

	const FString PackagePath = FPackageName::GetLongPackagePath(GetOutermost()->GetName());

	TArray<FAssetRenameData> AssetsAndNames;
	const bool bSoftReferenceOnly = false;
	const bool bAlsoRenameLocalizedVariants = true;
	AssetsAndNames.Emplace(FAssetRenameData(this, PackagePath, InNewName, bSoftReferenceOnly, bAlsoRenameLocalizedVariants));

	EAssetRenameResult Result = AssetTools.RenameAssetsWithDialog(AssetsAndNames);
	if (Result != EAssetRenameResult::Failure)
	{
		SchemaName = InNewName;
	}

	bSupportsRename = false;
}
#endif

void UCineAssemblySchema::UpdateThumbnailBrush()
{
	if (ThumbnailImage)
	{
		ThumbnailBrush->SetResourceObject(ThumbnailImage);
	}
	else
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry");
		const FAssetData ThumbnailAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(DefaultThumbnailPath);

		ThumbnailBrush->SetResourceObject(ThumbnailAssetData.GetAsset());
	}
}
