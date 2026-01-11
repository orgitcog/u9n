// Copyright Epic Games, Inc. All Rights Reserved.

#include "LLMInferenceEngine.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

ULLMInferenceEngine::ULLMInferenceEngine()
	: ServerURL(TEXT("http://localhost:2242"))
	, ConnectionTimeout(30.0f)
	, bIsInitialized(false)
{
}

void ULLMInferenceEngine::Initialize(const FString& InServerURL, const FString& InAPIKey)
{
	ServerURL = InServerURL;
	APIKey = InAPIKey;
	bIsInitialized = true;
	
	UE_LOG(LogTemp, Log, TEXT("LLMInferenceEngine: Initialized with server %s"), *ServerURL);
	
	RefreshAvailableModels();
}

void ULLMInferenceEngine::SubmitInferenceRequest(const FLLMInferenceRequest& Request)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("LLMInferenceEngine: Engine not initialized"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("LLMInferenceEngine: Submitting request %s with prompt: %s"), 
		*Request.RequestID, *Request.Prompt.Left(50));

	// Create pending response
	FLLMInferenceResponse PendingResponse;
	PendingResponse.RequestID = Request.RequestID;
	PendingResponse.bSuccess = false;
	PendingResponses.Add(Request.RequestID, PendingResponse);

	// Simulate network request
	FString Payload = FString::Printf(TEXT("{\"prompt\":\"%s\",\"max_tokens\":%d,\"temperature\":%f,\"model\":\"%s\"}"),
		*Request.Prompt, Request.MaxTokens, Request.Temperature, *Request.ModelName);
	
	SendHTTPRequest(TEXT("/v1/completions"), Payload);
}

bool ULLMInferenceEngine::GetInferenceResponse(const FString& RequestID, FLLMInferenceResponse& OutResponse)
{
	FLLMInferenceResponse* Response = PendingResponses.Find(RequestID);
	if (Response && Response->bSuccess)
	{
		OutResponse = *Response;
		PendingResponses.Remove(RequestID);
		return true;
	}
	return false;
}

void ULLMInferenceEngine::RefreshAvailableModels()
{
	AvailableModels.Empty();
	AvailableModels.Add(TEXT("llama-3-70b"));
	AvailableModels.Add(TEXT("mistral-7b"));
	AvailableModels.Add(TEXT("mixtral-8x7b"));
	AvailableModels.Add(TEXT("codellama-34b"));
	
	UE_LOG(LogTemp, Log, TEXT("LLMInferenceEngine: Refreshed %d available models"), AvailableModels.Num());
}

bool ULLMInferenceEngine::CheckServerHealth()
{
	if (!bIsInitialized)
	{
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("LLMInferenceEngine: Checking server health at %s"), *ServerURL);
	return true; // Simulated health check
}

void ULLMInferenceEngine::SendHTTPRequest(const FString& Endpoint, const FString& Payload)
{
	UE_LOG(LogTemp, Verbose, TEXT("LLMInferenceEngine: Sending HTTP request to %s%s"), *ServerURL, *Endpoint);
	
	// Simulate async HTTP request
	// In production, this would use FHttpModule
	
	// Simulate response after delay
	ProcessHTTPResponse(TEXT("{\"choices\":[{\"text\":\"Generated response from LLM\"}]}"));
}

void ULLMInferenceEngine::ProcessHTTPResponse(const FString& Response)
{
	UE_LOG(LogTemp, Verbose, TEXT("LLMInferenceEngine: Processing HTTP response"));
	
	// Parse and process response
	// In production, parse JSON and update PendingResponses
}
