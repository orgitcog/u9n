// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compute/PCGComputeCommon.h"
#include "Compute/PCGComputeKernel.h"

#include "Compute/PCGPinPropertiesGPU.h"

#include "PCGCustomHLSLKernel.generated.h"

USTRUCT()
struct FPCGParsedAttributeFunction
{
	GENERATED_BODY()

public:
	FPCGParsedAttributeFunction() = default;
	FPCGParsedAttributeFunction(const FString& InPinLabel, const FString& InFunctionName, int64 InAttributeType, const FString& InAttributeName, int32 InMatchBeginning)
		: PinLabel(InPinLabel)
		, FunctionName(InFunctionName)
		, AttributeType(InAttributeType)
		, AttributeName(InAttributeName)
		, MatchBeginning(InMatchBeginning)
	{}

	UPROPERTY()
	FString PinLabel;

	UPROPERTY()
	FString FunctionName;

	UPROPERTY()
	int64 AttributeType = INDEX_NONE;

	UPROPERTY()
	FString AttributeName;

	UPROPERTY()
	int32 MatchBeginning = INDEX_NONE;
};

USTRUCT()
struct FPCGParsedCopyElementFunction
{
	GENERATED_BODY()

public:
	FPCGParsedCopyElementFunction() = default;
	FPCGParsedCopyElementFunction(const FString& InSourcePin, const FString& InTargetPin)
		: SourcePin(InSourcePin)
		, TargetPin(InTargetPin)
	{}

	UPROPERTY()
	FString SourcePin;

	UPROPERTY()
	FString TargetPin;
};

// @todo_pcg: It would be ideal someday for parsed source to be editor only. Unfortunately not possible right now because we need to validate the parsed attribute functions during execution.
/** Holds the results of parsing for some HLSL source. */
USTRUCT()
struct FPCGCustomHLSLParsedSource
{
	GENERATED_BODY()

	FPCGCustomHLSLParsedSource() = default;

#if WITH_EDITOR
	FPCGCustomHLSLParsedSource(const FString& InSource)
		: Source(InSource)
	{}
#endif

	UPROPERTY()
	TArray<FPCGParsedAttributeFunction> AttributeFunctions;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<FPCGParsedCopyElementFunction> CopyElementFunctions;

	enum class ETokenType : uint8
	{
		Normal,
		Keyword,
		PreProcessorKeyword,
		Operator,
		DoubleQuotedString,
		SingleQuotedString,
		Comment,
		Whitespace
	};

	struct FToken
	{
		ETokenType Type = ETokenType::Normal;
		FTextRange Range;
	};

	FString Source;
	TArray<FToken> Tokens;

	/** Pins identified as being written to. Used to validate that output pins are initialized in some way. */
	TArray<FString> InitializedOutputPins;
#endif
};

UCLASS()
class UPCGCustomHLSLKernel : public UPCGComputeKernel
{
	GENERATED_BODY()

	// ~Begin UPCGComputeKernel interface
public:
	virtual bool IsKernelDataValid(FPCGContext* InContext) const override;
	virtual bool ComputeOutputBindingDataDesc(const UPCGComputeGraph* InGraph, FName InOutputPinLabel, UPCGDataBinding* InBinding, FPCGDataCollectionDesc& OutDataDesc) const override;
#if WITH_EDITOR
	virtual FString GetCookedSource(FPCGGPUCompilationContext& InOutContext) const override;
	virtual FString GetEntryPoint() const override { return EntryPoint; }
#endif
	virtual int ComputeThreadCount(const UPCGDataBinding* Binding) const override;
	virtual void GetDataLabels(FName InPinLabel, TArray<FString>& OutDataLabels) const override;
	virtual void GetKernelAttributeKeys(TArray<FPCGKernelAttributeKey>& OutKeys) const override;
	virtual uint32 GetThreadCountMultiplier() const override;
	virtual uint32 GetElementCountMultiplier(FName InOutputPinLabel) const override;
	virtual void GetInputPins(TArray<FPCGPinProperties>& OutPins) const override;
	virtual void GetOutputPins(TArray<FPCGPinPropertiesGPU>& OutPins) const override;

protected:
#if WITH_EDITOR
	virtual void InitializeInternal() override;
	virtual bool PerformStaticValidation() override;
	// ~End UPCGComputeKernel interface

	void InitEntryPoint();
	void PopulateAttributeKeysFromPinSettings();
	void ParseShaderSource();
	void CreateParsedSources();
	void CollectDataLabels(const FPCGCustomHLSLParsedSource& InParsedSource);
	bool ValidateShaderSource(TArray<FPCGKernelLogEntry>& OutLogEntries) const;
	bool AreAllOutputPinsWritten(TArray<FPCGKernelLogEntry>& OutLogEntries) const;

	/** Apply any code gen/transformations/expansions to the shader source. */
	virtual FString ProcessShaderSource(FPCGGPUCompilationContext& InOutContext, const FPCGCustomHLSLParsedSource& InParsedSource) const;
	virtual FString ProcessAdditionalShaderSources(FPCGGPUCompilationContext& InOutContext) const;
#endif

	/** Will the ThreadCountMultiplier value be applied when calculating the dispatch thread count. */
	bool IsThreadCountMultiplierInUse() const;
	bool AreAttributesValid(FPCGContext* InContext, FText* OutErrorText) const;

	const FPCGPinProperties* GetFirstInputPin() const;
	const FPCGPinPropertiesGPU* GetFirstOutputPin() const;

	int GetElementCountForInputPin(const FPCGPinProperties& InInputPinProps, const UPCGDataBinding* InBinding) const;

public:
#if WITH_EDITORONLY_DATA
	/** The name of the main function in the shader. Generated from the node title. */
	UPROPERTY()
	FString EntryPoint;
#endif

protected:
	UPROPERTY()
	TArray<FPCGCustomHLSLParsedSource> ParsedSources;

	UPROPERTY()
	TArray<FPCGKernelAttributeKey> KernelAttributeKeys;

	UPROPERTY()
	FPCGPinDataLabels PinDataLabels;
};
