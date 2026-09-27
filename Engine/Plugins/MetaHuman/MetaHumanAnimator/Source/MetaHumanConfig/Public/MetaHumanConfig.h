// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/BulkData.h"

#include "MetaHumanConfig.generated.h"



class METAHUMANCONFIG_API FMetaHumanConfig
{
public:

	/** Gets the config directory and user-friendly display name associated with some capture data */
	static bool GetInfo(class UCaptureData* InCaptureData, const FString& InComponent, FString& OutDisplayName);
	static bool GetInfo(class UCaptureData* InCaptureData, const FString& InComponent, UMetaHumanConfig*& OutConfig);
	static bool GetInfo(class UCaptureData* InCaptureData, const FString& InComponent, FString& OutDisplayName, UMetaHumanConfig*& OutConfig);
};



UENUM()
enum class EMetaHumanConfigType : uint8
{
	Unspecified,
	Solver,
	Fitting,
	PredictiveSolver,
};



/** MetaHuman Config Asset
*
*   Holds configuration info used by other MetaHuman components.
*
*/
UCLASS()
class METAHUMANCONFIG_API UMetaHumanConfig : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Import")
	bool ReadFromDirectory(const FString& InPath);

	FString GetSolverTemplateData() const;
	FString GetSolverConfigData() const;
	FString GetSolverDefinitionsData() const;
	FString GetSolverHierarchicalDefinitionsData() const;
	FString GetSolverPCAFromDNAData() const;

	FString GetFittingTemplateData() const;
	FString GetFittingConfigData() const;
	FString GetFittingConfigTeethData() const;
	FString GetFittingIdentityModelData() const;
	FString GetFittingControlsData() const;

	TArray<uint8> GetPredictiveGlobalTeethTrainingData() const;
	TArray<uint8> GetPredictiveTrainingData() const;

	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Parameters")
	EMetaHumanConfigType Type = EMetaHumanConfigType::Unspecified;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FString Name;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	FString Version;

	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface

private:

	UPROPERTY()
	int32 InternalVersion = 1; // To increase version dont adjust this default, but set InternalVersion in ReadFromDirectory

	FByteBulkData SolverTemplateDataCipherText;
	FByteBulkData SolverConfigDataCipherText;
	FByteBulkData SolverDefinitionsCipherText;
	FByteBulkData SolverHierarchicalDefinitionsCipherText;
	FByteBulkData SolverPCAFromDNACipherText;
	FByteBulkData FittingTemplateDataCipherText;
	FByteBulkData FittingConfigDataCipherText;
	FByteBulkData FittingConfigTeethDataCipherText;
	FByteBulkData FittingIdentityModelDataCipherText;
	FByteBulkData FittingControlsDataCipherText;
	FByteBulkData PredictiveGlobalTeethTrainingData;
	FByteBulkData PredictiveTrainingData;

	bool Encrypt(const FString& InPlainText, FByteBulkData& OutCipherText) const;
	bool Decrypt(const FByteBulkData& InCipherText, FString& OutPlainText) const;

	FString DecryptToString(const FByteBulkData& InCipherText) const;

	UMetaHumanConfig* GetBaseConfig() const;

	bool VerifyFittingConfig(const FString& InFittingTemplateDataJson, const FString& InFittingConfigDataJson, const FString& InFittingConfigTeethDataJson, 
		const FString& InFittingIdentityModelDataJson, const FString& InFittingControlsDataJson, FString& OutErrorString) const;

	bool VerifySolverConfig(const FString& InSolverTemplateDataJson, const FString& InSolverConfigDataJson, const FString& InSolverDefinitionsDataJson,
		const FString& InSolverHierarchicalDefinitionsDataJson, const FString& InSolverPCAFromDNADataJson, FString& OutErrorString) const;

};
