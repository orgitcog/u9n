// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

#include "CineAssemblySchema.generated.h"

/** The types of assembly metadata supported by Cine Assembly Schemas */
UENUM()
enum class ECineAssemblyMetadataType : uint8
{
	String = 0,
	Bool,
	Integer,
	Float,
	AssetPath,
	CineAssembly
};

/** Structure defining a single metadata field that can be associated with an assembly built from this schema, including its type, key, and default value */
USTRUCT()
struct FAssemblyMetadataDesc
{
	GENERATED_BODY()

	/** Metadata type */
	UPROPERTY(EditAnywhere, Category = "Metadata")
	ECineAssemblyMetadataType Type = ECineAssemblyMetadataType::String;

	/** The key associated with this field */
	UPROPERTY(EditAnywhere, Category = "Metadata")
	FString Key;

	/** For AssetPath types, the class to restrict the value of this metadata field to */
	UPROPERTY(EditAnywhere, Category = "Metadata", meta = (AllowAbstract = false, HideViewOptions, EditCondition = "Type == ECineAssemblyMetadataType::AssetPath", EditConditionHides))
	FSoftClassPath AssetClass;

	/** For CineAssembly types, the schema type to restrict the value of this metadata field to */
	UPROPERTY(EditAnywhere, Category = "Metadata", meta = (AllowedClasses="/Script/CineAssemblyTools.CineAssemblySchema", EditCondition = "Type == ECineAssemblyMetadataType::CineAssembly", EditConditionHides))
	FSoftObjectPath SchemaType;

	/** The default value for this metadata field */
	TVariant<FString, bool, int32, float> DefaultValue;
};

/**
 * A template object for building different Cine Assembly types
 */
UCLASS(MinimalAPI)
class UCineAssemblySchema : public UObject
{
	GENERATED_BODY()

public:
	UCineAssemblySchema();

	//~ Begin UObject overrides
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject overrides

	/** Get the unique ID of this schema */
	CINEASSEMBLYTOOLS_API FGuid GetSchemaGuid() const;

	/** Returns the thumbnail brush used by this schema */
	CINEASSEMBLYTOOLS_API const FSlateBrush* GetThumbnailBrush() const;

	/** Whether or not this schema can be renamed */
	CINEASSEMBLYTOOLS_API bool SupportsRename() const;

#if WITH_EDITOR
	/** Renames the underlying schema asset */
	CINEASSEMBLYTOOLS_API void RenameAsset(const FString& InNewName);
#endif

private:
	/** Updates the thumbnail brush resource */
	void UpdateThumbnailBrush();

public:
	/** The schema name, which will be used by assemblies made from this schema as their "assembly type" */
	UPROPERTY(EditAnywhere, Category = "Default")
	FString SchemaName;

	/** A user-facing text description of this schema */
	UPROPERTY(EditAnywhere, Category = "Default")
	FString Description;

	/** The default name to be use when creating assemblies from this schema */
	UPROPERTY(EditAnywhere, Category = "Default")
	FString DefaultAssemblyName;

	/** 
	 * The default path to use when creating assemblies from this schema
	 * When an assembly asset is created, this path will be appended to path where the asset would normally have been created.
	 */
	UPROPERTY()
	FString DefaultAssemblyPath;

	/** Restricts assemblies made from this schema to using this Schema when picking a Parent Assembly */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (AllowedClasses = "/Script/CineAssemblyTools.CineAssemblySchema"))
	FSoftObjectPath ParentSchema;

	/** The thumbnail image to use for this schema and assemblies built from this schema */
	UPROPERTY(EditAnywhere, Category = "Default")
	TObjectPtr<UTexture2D> ThumbnailImage;

	/** List of metadata fields that should be automatically added to assemblies made from this schema */
	UPROPERTY(EditAnywhere, Category = "Metadata")
	TArray<FAssemblyMetadataDesc> AssemblyMetadata;

	/** Paths of subsequence assets that should be created for assemblies that use this schema, relative to the path of the top-level assembly */
	UPROPERTY()
	TArray<FString> SubsequencesToCreate;

	/** Paths of folders that should be created for assemblies that use this schema, relative to the path of the top-level assembly */
	UPROPERTY()
	TArray<FString> FoldersToCreate;

	static CINEASSEMBLYTOOLS_API const FName SchemaGuidPropertyName;

private:
	/** Unique ID for this schema, assigned at object creation */
	UPROPERTY(AssetRegistrySearchable, meta = (IgnoreForMemberInitializationTest))
	FGuid SchemaGuid;

	/** Slate brush used to actually render the thumbnail image */
	TSharedPtr<FSlateBrush> ThumbnailBrush;

	/** Whether the thumbnail brush needs to be updated */
	bool bThumbnailPendingReset = false;

	/** Whether the schema asset supports renaming */
	bool bSupportsRename = false;

	/** Asset path of the default thumbnail to use for schema assets */
	static const FSoftObjectPath DefaultThumbnailPath;
};
