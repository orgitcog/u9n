// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/AssetUserData.h"

#include "RigMapperDefinition.generated.h"

#define UE_API RIGMAPPER_API


class FJsonObject;
class FJsonValue;
class URigMapperLinkedDefinitions;

UENUM(BlueprintType)
enum class ERigMapperFeatureType : uint8
{
	Input,
	WeightedSum,
	SDK,
	Multiply
};

USTRUCT(BlueprintType)
struct FRigMapperFeature
{
	GENERATED_BODY()

public:
	using FBakedInput = TPair<TSharedPtr<FRigMapperFeature>, TArray<TSharedPtr<FRigMapperFeature>>>;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=0))
	FString Name;

public:
	virtual ~FRigMapperFeature() = default;
	
	FRigMapperFeature() {}

	explicit FRigMapperFeature(const FString& InName) : Name(InName) {}

	virtual bool LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject) { return false; };

	UE_API virtual bool IsValid(const TArray<FString>& InputNames, bool bWarn = false) const;

	virtual ERigMapperFeatureType GetFeatureType() const { return ERigMapperFeatureType::Input; }

	virtual void GetInputs(TArray<FString>& OutInputs) const { }

	virtual bool BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const { return false; }


	bool operator==(const FRigMapperFeature& InOther) const
	{
		return Name == InOther.Name;
	}
	
protected:
	UE_API bool GetJsonArray(TSharedPtr<FJsonObject> JsonObject, TArray<TSharedPtr<FJsonValue>>& OutArray, const FString& Identifier, const FString& OwnerIdentifier="") const;
};


USTRUCT(BlueprintType)
struct FRigMapperMultiplyFeature : public FRigMapperFeature
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=1))
	TArray<FString> Inputs;
	
public:
	FRigMapperMultiplyFeature() {}

	explicit FRigMapperMultiplyFeature(const FString& InName) : FRigMapperFeature(InName) {}

	UE_API virtual bool LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject) override;

	UE_API virtual bool IsValid(const TArray<FString>& InputNames, bool bWarn = false) const override;

	virtual ERigMapperFeatureType GetFeatureType() const override { return ERigMapperFeatureType::Multiply; }

	virtual void GetInputs(TArray<FString>& OutInputs) const override { OutInputs = Inputs; }

	UE_API virtual bool BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const override;

	bool operator==(const FRigMapperMultiplyFeature& InOther) const
	{
		return FRigMapperFeature::operator==(InOther) && Inputs == InOther.Inputs;
	}
};

USTRUCT(BlueprintType)
struct FRigMapperFeatureRange
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	bool bHasLowerBound = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	double LowerBound = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	bool bHasUpperBound = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	double UpperBound = 0;

	bool operator==(const FRigMapperFeatureRange& InOther) const
	{
		return bHasLowerBound == InOther.bHasLowerBound && FMath::IsNearlyEqual(LowerBound, InOther.LowerBound, SMALL_NUMBER)
			&& bHasUpperBound == InOther.bHasUpperBound && FMath::IsNearlyEqual(UpperBound, InOther.UpperBound, SMALL_NUMBER);
	}
};

USTRUCT(BlueprintType)
struct FRigMapperWsFeature : public FRigMapperFeature
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=1))
	TMap<FString, double> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=1))
	FRigMapperFeatureRange Range;
	
public:
	FRigMapperWsFeature() {}

	explicit FRigMapperWsFeature(const FString& InName) : FRigMapperFeature(InName) {}

	UE_API virtual bool LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject) override;

	UE_API virtual bool IsValid(const TArray<FString>& InputNames, bool bWarn = false) const override;

	virtual ERigMapperFeatureType GetFeatureType() const override { return ERigMapperFeatureType::WeightedSum; }

	UE_API virtual void GetInputs(TArray<FString>& OutInputs) const override;

	UE_API virtual bool BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const override;

	bool operator==(const FRigMapperWsFeature& InOther) const
	{
		if (Inputs.Num() != InOther.Inputs.Num())
		{
			return false; 
		}

		for (const TPair<FString, double>& Pair : Inputs)
		{
			const double* OtherValue = InOther.Inputs.Find(Pair.Key);
			if (OtherValue == nullptr || !FMath::IsNearlyEqual(Pair.Value, *OtherValue, SMALL_NUMBER))
			{
				return false; 
			}
		}

		return FRigMapperFeature::operator==(InOther) && Range == InOther.Range;
	}
};

USTRUCT(BlueprintType)
struct FRigMapperSdkKey
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	double In = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	double Out = 0;

	bool operator==(const FRigMapperSdkKey& InOther) const
	{
		return FMath::IsNearlyEqual(In, InOther.In, SMALL_NUMBER) && FMath::IsNearlyEqual(Out, InOther.Out, SMALL_NUMBER);
	}
};

USTRUCT(BlueprintType)
struct FRigMapperSdkFeature : public FRigMapperFeature
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=1))
	FString Input;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper", meta=(DisplayPriority=1))
	TArray<FRigMapperSdkKey> Keys;
	
public:
	FRigMapperSdkFeature() {}

	explicit FRigMapperSdkFeature(const FString& InName) : FRigMapperFeature(InName) {};

	UE_API virtual bool LoadFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject) override;
	
	UE_API virtual bool IsValid(const TArray<FString>& InputNames, bool bWarn = false) const override;

	virtual ERigMapperFeatureType GetFeatureType() const override { return ERigMapperFeatureType::SDK; }
	
	virtual void GetInputs(TArray<FString>& OutInputs) const override { OutInputs = { Input }; };

	UE_API virtual bool BakeInput(URigMapperLinkedDefinitions* LinkedDefinitions, int32 DefinitionIndex, FBakedInput& OutBakedInput) const override;

	static UE_API bool BakeKeys(const FRigMapperSdkFeature& InSdk, const FRigMapperSdkFeature& OutSdk, TArray<FRigMapperSdkKey>& BakedKeys);

	bool operator==(const FRigMapperSdkFeature& InOther) const
	{
		return FRigMapperFeature::operator==(InOther) && Input == InOther.Input && Keys == InOther.Keys;
	}
};

USTRUCT(BlueprintType)
struct FRigMapperFeatureDefinitions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<FRigMapperMultiplyFeature> Multiply;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<FRigMapperWsFeature> WeightedSums;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<FRigMapperSdkFeature> SDKs;

public:
	UE_API bool AddFromJsonObject(const FString& FeatureName, const TSharedPtr<FJsonObject>& JsonObject);

	UE_API bool GetFeatureNames(TArray<FString>& OutFeatureNames) const;
	
	UE_API bool IsValid(const TArray<FString>& InputNames, bool bWarn = false) const;

	UE_API void Empty();
	
	UE_API FRigMapperFeature* Find(const FString& FeatureName, ERigMapperFeatureType& OutFeatureType);

	bool operator==(const FRigMapperFeatureDefinitions& InOther) const
	{
		return Multiply == InOther.Multiply && WeightedSums == InOther.WeightedSums && SDKs == InOther.SDKs;
	}
};


DECLARE_MULTICAST_DELEGATE(FOnRigMapperDefinitionUpdated);

UCLASS(MinimalAPI, BlueprintType)
class URigMapperDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	// Delegate to notify listeners that we have loaded definitions
	FOnRigMapperDefinitionUpdated OnRigMapperDefinitionUpdated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<FString> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	FRigMapperFeatureDefinitions Features;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TMap<FString, FString> Outputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<FString> NullOutputs;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Animation|Rig Mapper")
	bool bValidated = false;
	
private:
	UPROPERTY()
	FString JsonSource;

public:
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool LoadFromJsonFile(const FFilePath& JsonFilePath);
	
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool LoadFromJsonString(const FString& JsonString);
	
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool ExportAsJsonString(FString& OutJsonString) const;

	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool ExportAsJsonFile(const FFilePath& JsonFilePath) const;
	
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API void Empty();

	UFUNCTION(BlueprintCallable, Category = "Animation|Rig Mapper")
	UE_API bool IsDefinitionValid(bool bWarn = false, bool bForce = false) const;

	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool Validate();

	UFUNCTION(BlueprintCallable, Category = "Animation|Rig Mapper")
	UE_API bool WasDefinitionValidated() const;

	
private:
	bool LoadInputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	bool LoadFeaturesFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, TArray<FString>& FeatureNames);
	bool LoadOutputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject, const TArray<FString>& FeatureNames);
	bool LoadNullOutputsFromJsonObject(const TSharedPtr<FJsonObject>& JsonObject);
	void SetDefinitionValid(bool bValid);

#if WITH_EDITOR
public:
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

private:	
	UE_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
	// todo: helper function to generate new asset from json file
	// todo: invalidate on property changed
};

UCLASS(MinimalAPI, BlueprintType)
class URigMapperLinkedDefinitions : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<TObjectPtr<URigMapperDefinition>> SourceDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TObjectPtr<URigMapperDefinition> BakedDefinition;
	
public:
	// todo: bake to new
	// todo: invalidate on property or source definition changed 
	
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool BakeDefinitions();
	
	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool Validate();

	UFUNCTION(BlueprintCallable, Category="Animation|Rig Mapper")
	UE_API bool AreLinkedDefinitionsValid() const;

	UE_API TArray<FRigMapperFeature::FBakedInput> GetBakedInputs(const TArray<TPair<FString, FString>>& PairedOutputs);
	
	UE_API bool GetBakedInputRec(const FString& InputName, const int32 DefinitionIndex, FRigMapperFeature::FBakedInput& OutBakedInput, bool& bOutMissingIsNotNullOutput);
	
private:
	bool AddBakedInputFeature(const TSharedPtr<FRigMapperFeature>& Feature) const;
	
	void AddBakedInputs(const TArray<FRigMapperFeature::FBakedInput>& BakedInputs, const TArray<TPair<FString, FString>>& PairedOutputs) const;
	// todo: helper functions to bake to new / existing asset
	// todo: helper function to import/export from/to json files

#if WITH_EDITOR
	UE_API virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};

UCLASS(MinimalAPI, NotBlueprintable, HideCategories = (Object))
class URigMapperDefinitionUserData : public UAssetUserData
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Animation|Rig Mapper")
	TArray<TObjectPtr<URigMapperDefinition>> Definitions;
};

#undef UE_API
