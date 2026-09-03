// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LevelSequence.h"

#include "CineAssemblySchema.h"
#include "Dom/JsonObject.h"
#include "UObject/TemplateString.h"

#include "CineAssembly.generated.h"

class UMovieSceneSubSection;

/** 
 * A cinematic building block that associates a level sequence with a level
 */
UCLASS(MinimalAPI, BlueprintType, HideCategories=Animation)
class UCineAssembly : public ULevelSequence
{
	GENERATED_BODY()

public:
	UCineAssembly();

	friend class SCineAssemblyConfigWindow;

	//~ Begin ULevelSquence Interface
	CINEASSEMBLYTOOLS_API virtual void Initialize() override;
	//~ End ULevelSquence Interface

	//~ Begin UObject Interface
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	virtual void PostInitProperties() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void GetAssetRegistryTagMetadata(TMap<FName, FAssetRegistryTagMetadata>& OutMetadata) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

	/** Get the unique ID of this assembly */
	CINEASSEMBLYTOOLS_API FGuid GetAssemblyGuid() const;

	/** Get the base schema for this assembly */
	CINEASSEMBLYTOOLS_API const UCineAssemblySchema* GetSchema() const;

	/** Set the base schema for this assembly, only if one is not already set */
	CINEASSEMBLYTOOLS_API void SetSchema(UCineAssemblySchema* InSchema);

#if WITH_EDITOR
	/** Creates one or more subsequence assets, parented to this assembly, based on the schema */
	CINEASSEMBLYTOOLS_API void CreateSubAssemblies();
#endif

	/** Get the target level associated with this assembly */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API TSoftObjectPtr<UWorld> GetLevel();

	/** Set the target level associated with this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetLevel(TSoftObjectPtr<UWorld> InLevel);

	/** Get the note text associated with this assembly */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API FString GetNoteText();

	/** Set the note text associated with this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetNoteText(FString InNote);

	/** Append to the note text associated with this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void AppendToNoteText(FString InNote);

	/** Get the production ID associated with this assembly */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API FGuid GetProductionID();

	/** Get the production name associated with this assembly */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API FString GetProductionName();

	/** Get the parent assembly of this assembly */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API TSoftObjectPtr<UCineAssembly> GetParentAssembly();

	/** Set the parent assembly of this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetParentAssembly(TSoftObjectPtr<UCineAssembly> InParent);

	/** Get all of the metadata for this assembly as a formatted JSON string */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API FString GetFullMetadataString() const;

	/** Add a string as metadata to this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetMetadataAsString(FString InKey, FString InValue);

	/** Add a boolean as metadata to this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetMetadataAsBool(FString InKey, bool InValue);

	/** Add an integer as metadata to this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetMetadataAsInteger(FString InKey, int32 InValue);

	/** Add a floating point number as metadata to this assembly */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	CINEASSEMBLYTOOLS_API void SetMetadataAsFloat(FString InKey, float InValue);

	/** Get the metadata value for the input key as a string (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta = (ReturnDisplayName = "FoundKey"))
	CINEASSEMBLYTOOLS_API bool GetMetadataAsString(FString InKey, FString& OutValue) const;

	/** Get the metadata value for the input key as a boolean (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta = (ReturnDisplayName = "FoundKey"))
	CINEASSEMBLYTOOLS_API bool GetMetadataAsBool(FString InKey, bool& OutValue) const;

	/** Get the metadata value for the input key as an integer (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta = (ReturnDisplayName = "FoundKey"))
	CINEASSEMBLYTOOLS_API bool GetMetadataAsInteger(FString InKey, int32& OutValue) const;

	/** Get the metadata value for the input key as a floating-point number (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta = (ReturnDisplayName = "FoundKey"))
	CINEASSEMBLYTOOLS_API bool GetMetadataAsFloat(FString InKey, float& OutValue) const;

	/** Adds a new metadata key to the list of supported naming tokens for assemblies */
	CINEASSEMBLYTOOLS_API void AddMetadataNamingToken(const FString& InKey);

private:
	/** Sets the base schema for this assembly and re-initializes the metadata inherited from the schema */
	CINEASSEMBLYTOOLS_API void ChangeSchema(UCineAssemblySchema* InSchema);

	/** Update the underlying json object whenever keys/values in the instance metadata map are added/removed/modified */
	void UpdateInstanceMetadata();

public:
	/** The assembly name, which supports tokens */
	UPROPERTY()
	FTemplateString AssemblyName;

	/** The level to open before opening this asset in Sequencer */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath Level;

	/** User added metadata key/value pairs, which will be added as additional asset registry tags */
	UPROPERTY(EditAnywhere, Category = "Instance Metadata")
	TMap<FName, FString> InstanceMetadata;

	/** User-facing notes about this assembly asset */
	UPROPERTY()
	FString AssemblyNote;

	/** Reference to another assembly asset that is the parent of this assembly */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (AllowedClasses = "/Script/CineAssemblyTools.CineAssembly"))
	FSoftObjectPath ParentAssembly;

	/** The ID of the Cinematic Production that this assembly is associated with */
	UPROPERTY(EditAnywhere, Category = "Default")
	FGuid Production;

	/** The name of the Cinematic Production that this assembly is associated with */
	UPROPERTY()
	FString ProductionName;

	/** Array of template names (possibly containing tokens), based on the schema, used to create the SubAssemblies */
	UPROPERTY()
	TArray<FTemplateString> SubAssemblyNames;

	/** Array of template names (possibly containing tokens), based on the schema, used to create folders for this assembly */
	UPROPERTY()
	TArray<FTemplateString> DefaultFolderNames;

	/** Array of Subsequence Sections created based on the schema */
	UPROPERTY()
	TArray<TObjectPtr<UMovieSceneSubSection>> SubAssemblies;

	/** The asset registry tag that contains the assembly type information */
	static CINEASSEMBLYTOOLS_API const FName AssetRegistryTag_AssemblyType;
	static CINEASSEMBLYTOOLS_API const FName AssemblyGuidPropertyName;

private:
	/** Unique ID for this assembly, assigned at object creation */
	UPROPERTY(AssetRegistrySearchable, meta = (IgnoreForMemberInitializationTest))
	FGuid AssemblyGuid;

	/** The schema that was used as a base when creating this assembly (can be null if no schema was used) */
	UPROPERTY()
	TObjectPtr<UCineAssemblySchema> BaseSchema;

	/** Copy of the keys present in the InstanceMetadata map, used to keep the json representation consistent with the map contents */
	TArray<FName> InstanceMetadataKeys;

	/** Json object responsible for storing the schema and instance metadata for this assembly */
	TSharedPtr<FJsonObject> MetadataJsonObject;
};
