// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPtr.h"

#include "MetaHumanParameterMappingTable.generated.h"

class ITargetPlatform;
class UDataTable;
class UTexture2D;

UENUM(BlueprintType)
enum class EMetaHumanParameterMappingInputSourceType : uint8
{
	Parameter,
	Scalability,
	ConsoleVariable,
	Platform
};

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMappingInput
{
	GENERATED_BODY()

	UPROPERTY()
	EMetaHumanParameterMappingInputSourceType Type = EMetaHumanParameterMappingInputSourceType::Parameter;

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName NameValue;

	UPROPERTY()
	float FloatValue = 0.0f;

	UPROPERTY()
	bool bBoolValue = false;
};

UENUM()
enum class EMetaHumanParameterValueType : uint8
{
	// TODO: Do we want to have an invalid value here? Or just allow a null Texture to be the invalid value?
	Invalid,
	Texture,
	Name,
	Color,
	Float,
	Bool
};

// TODO: Is there a neater way of doing this using existing engine functionality?
USTRUCT(BlueprintType)
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterValue
{
	GENERATED_BODY()

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> TextureValue;

	UPROPERTY()
	FName NameValue;

	UPROPERTY()
	FLinearColor ColorValue = FLinearColor(ForceInit);

	UPROPERTY()
	float FloatValue = 0.0f;

	UPROPERTY()
	bool bBoolValue = false;

	UPROPERTY()
	EMetaHumanParameterValueType Type = EMetaHumanParameterValueType::Invalid;

	bool operator==(const FMetaHumanParameterValue& Other) const;

	bool Matches(const FMetaHumanParameterMappingInput& MappingInput) const;
};

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMappingRow
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FMetaHumanParameterMappingInput> InputParameters;

	UPROPERTY()
	FMetaHumanParameterValue Value;
};

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMapping
{
	GENERATED_BODY()

	UPROPERTY()
	FName ParameterName;

	UPROPERTY()
	TArray<FMetaHumanParameterMappingRow> Rows;
};


USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMappingInputColumnSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	FName TypeColumn;
	
	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	FName NameColumn;

	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	TArray<FName> ValueColumns;
};

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMappingOutputColumnSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	FName NameColumn;

	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	TArray<FName> ValueColumns;
};

/**
 * When a TMap is marked as a UPROPERTY, its value can't be a TArray, but it can be a struct 
 * containing a TArray, so that's the purpose of this struct.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanScalabilityValueSet
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Values;
};

/**
 * An optimized form of the table that is faster to evaluate and doesn't contain rows that would 
 * be unreachable given the target platform and constant parameters.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanCompiledParameterMappingTable
{
	GENERATED_BODY()

	DECLARE_DELEGATE_TwoParams(FOutputParameterDelegate, FName /* Name */, const FMetaHumanParameterValue& /* Value */);

	FMetaHumanCompiledParameterMappingTable(
		TArray<FMetaHumanParameterMapping>&& InMappings,
		TMap<FName, FMetaHumanScalabilityValueSet>&& InReachableScalabilityValues);

	FMetaHumanCompiledParameterMappingTable() = default;

	/**
	 * Evaluates the table using the given parameters as well as the current values of any cvars referenced.
	 * 
	 * OutputParameterDelegate will be called for each parameter set by the table.
	 * 
	 * It's valid to evaluate a default-constructed table. It will simply not call OutputParameterDelegate.
	 */
	void Evaluate(
		const TMap<FName, FMetaHumanParameterValue>& TableInputParameters,
		const TArray<FMetaHumanParameterMappingInput>& ConsoleVariableOverrides,
		const FOutputParameterDelegate& OutputParameterDelegate) const;

private:
	UPROPERTY()
	TArray<FMetaHumanParameterMapping> Mappings;

	/**
	 * The list of values that the mapping table compiler has determined to be reachable on this
	 * target platform for the given scalability console variables.
	 *
	 * The compiler makes assumptions based on this range of values, and so if the current value
	 * for a scalability variable is outside of the expected range, the table evaluation may
	 * produce invalid results.
	 *
	 * Therefore, the range of expected values is saved alongside the compiled table, so that we
	 * can detect when a variable is outside the expected range and fail the evaluation gracefully.
	 */
	UPROPERTY()
	TMap<FName, FMetaHumanScalabilityValueSet> ReachableScalabilityValues;

	/** The name of the target platform that was passed in when this table was compiled, if any */
	UPROPERTY()
	FName TargetPlatformName;
};

/**
 * A table for mapping high level parameters and other data sources, such as scalability variables,
 * to low level parameter values.
 * 
 * For example, a high level "Character Quality" parameter with values such as High, Medium and Low
 * could control several low level parameter values, such as "Texture Size", "Minimum Mesh LOD" and
 * so on.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanParameterMappingTable
{
	GENERATED_BODY()

#if WITH_EDITOR
	/**
	 * Compiles the table into a format that is faster to evaluate.
	 * 
	 * The parameters in ConstantParameters will be locked to constants in the compiled table.
	 * If passed in as parameters at evaluation time, they will be ignored.
	 * 
	 * Any Scalability cvars used as input will be locked to the reachable range for the target
	 * platform based on the scalability and device profile inis.
	 * 
	 * Any unreachable rows, e.g. rows that would only be activated on other platforms, will be 
	 * omitted from the compiled data.
	 * 
	 * TargetPlatform may be null, in which case the compiled data will be usable on any platform.
	 */
	bool TryCompile(
		const TMap<FName, FMetaHumanParameterValue>& ConstantParameters,
		const ITargetPlatform* TargetPlatform,
		FMetaHumanCompiledParameterMappingTable& OutCompiledTable,
		TMap<FName, TArray<FMetaHumanParameterValue>>& OutPossibleParameterValues) const;
#endif // WITH_EDITOR

	/**
	 * Returns true if the Character Pipeline should attempt to compile and use this table.
	 *
	 * Otherwise the Pipeline will assume that the user doesn't want a Parameter Mapping Table
	 * and will build the Character without one.
	 */
	bool IsValid() const;

	/** The data table that will be evaluated */
	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	TObjectPtr<UDataTable> Table;

	/** The table columns to be used for input parameters should be specified here */
	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	TArray<FMetaHumanParameterMappingInputColumnSet> InputColumnSets;

	/** The table columns to be used for output parameters should be specified here */
	UPROPERTY(EditAnywhere, Category = ParameterMapping)
	TArray<FMetaHumanParameterMappingOutputColumnSet> OutputColumnSets;
};
