#pragma once

/**
 * Deep Tree Echo - Language Model Bridge
 *
 * The single place where Deep Tree Echo obtains words.
 *
 * Every sentence the character says, writes, thinks aloud or publishes must be
 * produced by a language model. Nothing in this project may stand in for the
 * model with fixed, templated, randomised or fallback text. When no model is
 * reachable the honest outcome is silence plus an operator-facing status, never
 * a canned line spoken as if it were the character.
 *
 * The bridge talks to any OpenAI-compatible chat completion server. A locally
 * served model (Ollama's /v1 endpoint, llama.cpp's llama-server, vLLM, ...) is
 * the default; a hosted endpoint works by pointing BaseUrl at it and supplying
 * an API key.
 *
 * Configuration (component properties override environment variables):
 *   LOCAL_LLM_BASE_URL   default http://localhost:11434/v1
 *   LOCAL_LLM_MODEL      default llama3.2
 *   LOCAL_LLM_API_KEY    optional bearer token (OPENAI_API_KEY is also honoured)
 *
 * Two entry points are provided:
 *   FDTELanguageModelClient  plain C++ singleton usable from non-UObject code
 *                            (humor engine, Level 5/7 orchestrators, ...).
 *   UDTELanguageModelBridge  actor component with Blueprint access, a persona
 *                            prompt and per-actor settings.
 *
 * All requests are asynchronous. Callbacks run on the game thread once the HTTP
 * response arrives. Callers that are UObjects must capture a TWeakObjectPtr and
 * check it before touching members.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DTELanguageModelBridge.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDTEModel, Log, All);

/** One turn of a chat transcript sent to the model. Role is system, user or assistant. */
USTRUCT(BlueprintType)
struct DEEPTREEECHO_API FDTEChatMessage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    FString Role;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    FString Content;

    FDTEChatMessage() {}
    FDTEChatMessage(const FString& InRole, const FString& InContent) : Role(InRole), Content(InContent) {}
};

/** Sampling and transport options for one request. */
struct DEEPTREEECHO_API FDTEModelRequestOptions
{
    float Temperature = 0.7f;
    int32 MaxTokens = 400;
    float TimeoutSeconds = 120.0f;
    /** Ask the server for a JSON object (response_format). Only honoured by servers that support it. */
    bool bJsonObject = false;
};

/**
 * Completion callback. bSuccess is true when the model answered with non-empty
 * text, in which case TextOrError holds that text. Otherwise TextOrError is an
 * operator-facing reason (not character speech) and nothing should be said.
 */
using FDTEModelCallback = TFunction<void(bool bSuccess, const FString& TextOrError)>;

/**
 * Plain C++ client for an OpenAI-compatible chat completion endpoint.
 * Safe to call from any code that runs on the game thread.
 */
class DEEPTREEECHO_API FDTELanguageModelClient
{
public:
    /** Process-wide client configured from the environment on first use. */
    static FDTELanguageModelClient& Get();

    FDTELanguageModelClient();

    /** Re-read LOCAL_LLM_BASE_URL / LOCAL_LLM_MODEL / LOCAL_LLM_API_KEY (or OPENAI_API_KEY). */
    void ConfigureFromEnvironment();

    /** Explicit configuration. Empty strings keep the current value. */
    void Configure(const FString& InBaseUrl, const FString& InModel, const FString& InApiKey);

    /** True when a base URL and model name are set. Says nothing about reachability. */
    bool IsConfigured() const;

    /** True when the most recent probe or request succeeded. */
    bool IsAvailable() const { return bAvailable; }

    /** True once a probe or request has completed at least once. */
    bool HasBeenProbed() const { return bProbed; }

    /** GET {BaseUrl}/models. Reports success when the server answers and lists the configured model (or any model when none is configured). */
    void Probe(FDTEModelCallback OnDone);

    /** System + user prompt convenience wrapper around Chat(). */
    void Complete(const FString& SystemPrompt, const FString& UserPrompt, FDTEModelCallback OnDone,
                  const FDTEModelRequestOptions& Options = FDTEModelRequestOptions());

    /** POST {BaseUrl}/chat/completions with the given transcript. */
    void Chat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone,
              const FDTEModelRequestOptions& Options = FDTEModelRequestOptions());

    /** The identity prompt used when a caller supplies none. */
    static FString DefaultSystemPrompt();

    const FString& GetBaseUrl() const { return BaseUrl; }
    const FString& GetModel() const { return Model; }
    const FString& GetLastError() const { return LastError; }
    int32 GetInFlightCount() const { return InFlight; }

    /** Human-readable "model @ url" for status displays. */
    FString Describe() const;

private:
    FString EndpointUrl(const TCHAR* Path) const;
    void SetAvailable(bool bInAvailable, const FString& Detail);
    static bool ExtractAssistantContent(const FString& Body, FString& OutText, FString& OutError);
    static bool ModelListed(const FString& Body, const FString& ModelName);

    FString BaseUrl;
    FString Model;
    FString ApiKey;
    FString LastError;
    bool bAvailable = false;
    bool bProbed = false;
    int32 InFlight = 0;
};

/** Blueprint-callable completion callback. */
DECLARE_DYNAMIC_DELEGATE_TwoParams(FDTEModelTextDelegate, bool, bSuccess, const FString&, TextOrError);

/** Fired when the bridge learns that the model became reachable or unreachable. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDTEModelAvailabilityChanged, bool, bAvailable, const FString&, Detail);

/**
 * Actor component giving an avatar its voice. Holds the persona prompt and
 * forwards every request to FDTELanguageModelClient.
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent))
class DEEPTREEECHO_API UDTELanguageModelBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UDTELanguageModelBridge();

    virtual void BeginPlay() override;

    // ---- Configuration -----------------------------------------------------

    /** OpenAI-compatible base URL (ending in /v1). Empty uses LOCAL_LLM_BASE_URL or http://localhost:11434/v1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    FString BaseUrl;

    /** Model name as the server knows it. Empty uses LOCAL_LLM_MODEL or llama3.2. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    FString Model;

    /** Optional bearer token. Empty uses LOCAL_LLM_API_KEY / OPENAI_API_KEY. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    FString ApiKey;

    /** Identity prompt sent as the system message. Empty uses FDTELanguageModelClient::DefaultSystemPrompt(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model", meta = (MultiLine = true))
    FString PersonaPrompt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model", meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float Temperature = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model", meta = (ClampMin = "1"))
    int32 MaxTokens = 400;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model", meta = (ClampMin = "1.0"))
    float TimeoutSeconds = 120.0f;

    /** Probe the server on BeginPlay so operators see availability immediately. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Model")
    bool bProbeOnBeginPlay = true;

    // ---- Status -------------------------------------------------------------

    /** True when the last probe or request reached the model. */
    UFUNCTION(BlueprintPure, Category = "Model")
    bool IsModelAvailable() const;

    /** "model @ url" plus the last error, for status widgets and logs. */
    UFUNCTION(BlueprintPure, Category = "Model")
    FString GetModelStatus() const;

    /** Check reachability now. Result arrives through OnModelAvailabilityChanged. */
    UFUNCTION(BlueprintCallable, Category = "Model")
    void Probe();

    UPROPERTY(BlueprintAssignable, Category = "Model")
    FOnDTEModelAvailabilityChanged OnModelAvailabilityChanged;

    // ---- Requests -----------------------------------------------------------

    /**
     * Ask the model to speak as the character. Instruction says what kind of
     * utterance is wanted (greet the visitor, write a diary entry, ...);
     * Context carries the facts it should draw on. Blueprint variant.
     */
    UFUNCTION(BlueprintCallable, Category = "Model", meta = (AutoCreateRefTerm = "Context"))
    void RequestText(const FString& Instruction, const FString& Context, FDTEModelTextDelegate OnDone);

    /** C++ variant of RequestText. */
    void RequestText(const FString& Instruction, const FString& Context, FDTEModelCallback OnDone);

    /** Full transcript request; a system message is prepended when the transcript has none. */
    void RequestChat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone);

    /** RequestChat with explicit sampling settings. */
    void RequestChat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone, float InTemperature, int32 InMaxTokens);

    /** Ask for a JSON object answer (for structured self-assessment, scores, ...). */
    void RequestJson(const FString& Instruction, const FString& Context, FDTEModelCallback OnDone);

    /** The system prompt in effect (PersonaPrompt or the default). */
    UFUNCTION(BlueprintPure, Category = "Model")
    FString GetSystemPrompt() const;

    // ---- Discovery ----------------------------------------------------------

    /** The bridge component on Actor, or nullptr. */
    static UDTELanguageModelBridge* FindOn(const AActor* Actor);

    /** The bridge component on Actor, creating a default-configured one when absent. */
    static UDTELanguageModelBridge* FindOrAdd(AActor* Actor);

    /** Shared client this component is configured against. */
    FDTELanguageModelClient& GetClient() const;

private:
    void ApplyConfiguration();
    FDTEModelRequestOptions MakeOptions() const;
    void HandleAvailability(bool bAvailable, const FString& Detail);

    bool bLastReportedAvailability = false;
    bool bHasReportedAvailability = false;
};
