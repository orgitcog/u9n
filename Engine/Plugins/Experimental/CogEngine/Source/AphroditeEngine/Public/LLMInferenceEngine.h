// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "LLMInferenceEngine.generated.h"

/**
 * LLM Inference Request
 */
USTRUCT(BlueprintType)
struct FLLMInferenceRequest
{
	GENERATED_BODY()

	/** Request ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString RequestID;

	/** Prompt text */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString Prompt;

	/** Maximum tokens to generate */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	int32 MaxTokens;

	/** Temperature for sampling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	float Temperature;

	/** Model name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString ModelName;

	FLLMInferenceRequest()
		: MaxTokens(256)
		, Temperature(0.7f)
		, ModelName(TEXT("default"))
	{
	}
};

/**
 * LLM Inference Response
 */
USTRUCT(BlueprintType)
struct FLLMInferenceResponse
{
	GENERATED_BODY()

	/** Request ID */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString RequestID;

	/** Generated text */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString GeneratedText;

	/** Completion tokens */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	int32 TokensGenerated;

	/** Success status */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	bool bSuccess;

	FLLMInferenceResponse()
		: TokensGenerated(0)
		, bSuccess(false)
	{
	}
};

/**
 * Aphrodite LLM Inference Engine - Distributed network-based LLM inference
 */
UCLASS(Blueprintable, BlueprintType)
class APHRODITEENGINE_API ULLMInferenceEngine : public UObject
{
	GENERATED_BODY()

public:
	ULLMInferenceEngine();

	/** Inference server URL */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString ServerURL;

	/** API key for authentication */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	FString APIKey;

	/** Connection timeout in seconds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Aphrodite")
	float ConnectionTimeout;

	/** Available models */
	UPROPERTY(BlueprintReadOnly, Category = "Aphrodite")
	TArray<FString> AvailableModels;

	/** Initialize inference engine */
	UFUNCTION(BlueprintCallable, Category = "Aphrodite")
	void Initialize(const FString& InServerURL, const FString& InAPIKey);

	/** Submit inference request */
	UFUNCTION(BlueprintCallable, Category = "Aphrodite")
	void SubmitInferenceRequest(const FLLMInferenceRequest& Request);

	/** Get inference response (async) */
	UFUNCTION(BlueprintCallable, Category = "Aphrodite")
	bool GetInferenceResponse(const FString& RequestID, FLLMInferenceResponse& OutResponse);

	/** List available models */
	UFUNCTION(BlueprintCallable, Category = "Aphrodite")
	void RefreshAvailableModels();

	/** Check server health */
	UFUNCTION(BlueprintCallable, Category = "Aphrodite")
	bool CheckServerHealth();

protected:
	/** Send HTTP request to inference server */
	void SendHTTPRequest(const FString& Endpoint, const FString& Payload);

	/** Process HTTP response */
	void ProcessHTTPResponse(const FString& Response);

	TMap<FString, FLLMInferenceResponse> PendingResponses;
	bool bIsInitialized;
};
