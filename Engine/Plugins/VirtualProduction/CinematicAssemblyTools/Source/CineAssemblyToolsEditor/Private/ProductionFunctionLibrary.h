// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProductionSettings.h"

#include "ProductionFunctionLibrary.generated.h"

class UCineAssembly;
class UCineAssemblySchema;

/**
 * Library of Blueprint/Python accessible functions to interface with the Cinematic Production Settings
 */
UCLASS()
class UProductionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Returns an array of all available Cinematic Productions */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	static const TArray<FCinematicProduction> GetAllProductions();

	/** Get the Cinematic Production matching the input ProductionID (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta=(ReturnDisplayName="IsValid"))
	static bool GetProduction(FGuid ProductionID, FCinematicProduction& Production);

	/** Get the active Cinematic Production (if it exists) */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools", meta=(ReturnDisplayName = "IsValid"))
	static bool GetActiveProduction(FCinematicProduction& Production);

	/**
	 * Sets the input Production as the current Active Production 
	 * If no input is provided, the Active Production will be set to None
	 */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void SetActiveProduction(FCinematicProduction Production = FCinematicProduction());

	/** Sets the Production matching the input ProductionID as the current Active Production */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void SetActiveProductionByID(FGuid ProductionID);

	/** Sets the active Cinematic Production to None */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void ClearActiveProduction();

	/** Returns true if input ProductionID matches the ID of the current Active Production */
	UFUNCTION(BlueprintPure, Category = "Cine Assembly Tools")
	static bool IsActiveProduction(FGuid ProductionID);

	/** Add the input Cinematic Production to the Production Settings' list of productions */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void AddProduction(FCinematicProduction Production);

	/** Removes the Cinematic Production matching the input ProductionID from the Prodctuion Settings' list of productions */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void DeleteProduction(FGuid ProductionID);

	/** Renames the Cinematic Production matching the input ProductionID */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static void RenameProduction(FGuid ProductionID, FString NewName);

	/** 
	 * Create a new CineAssembly asset using the input Schema, Level, and Metadata.
	 * If bUseDefaultNameFromSchema is true, the default assembly name from the specified schema will be used as the new asset name.
	 * It is important that any metadata required for resolving asset naming tokens is provided to this function so that the Assembly and SubAssemblies are all named correctly. 
	 */
	UFUNCTION(BlueprintCallable, Category = "Cine Assembly Tools")
	static UCineAssembly* CreateAssembly(UCineAssemblySchema* Schema, TSoftObjectPtr<UWorld> Level, TSoftObjectPtr<UCineAssembly> ParentAssembly, TMap<FString, FString> Metadata, const FString& Path, const FString& Name, bool bUseDefaultNameFromSchema = true);

private:
	/** Adds the input map of metadata to the input assembly */
	static void AddMetadataToAssembly(UCineAssembly* Assembly, TMap<FString, FString> Metadata);
};
