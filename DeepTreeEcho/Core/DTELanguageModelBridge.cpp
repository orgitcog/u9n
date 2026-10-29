// DTELanguageModelBridge.cpp
// Implementation of the one path through which Deep Tree Echo obtains words.

#include "DTELanguageModelBridge.h"

#include "Dom/JsonObject.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformMisc.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY(LogDTEModel);

namespace
{
    const TCHAR* DefaultBaseUrl = TEXT("http://localhost:11434/v1");
    const TCHAR* DefaultModelName = TEXT("llama3.2");

    FString TrimTrailingSlash(FString Url)
    {
        while (Url.EndsWith(TEXT("/")))
        {
            Url.LeftChopInline(1);
        }
        return Url;
    }
}

// ============================================================================
// FDTELanguageModelClient
// ============================================================================

FDTELanguageModelClient& FDTELanguageModelClient::Get()
{
    static FDTELanguageModelClient Instance;
    return Instance;
}

FDTELanguageModelClient::FDTELanguageModelClient()
{
    ConfigureFromEnvironment();
}

void FDTELanguageModelClient::ConfigureFromEnvironment()
{
    const FString EnvUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCAL_LLM_BASE_URL"));
    const FString EnvModel = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCAL_LLM_MODEL"));
    FString EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("LOCAL_LLM_API_KEY"));
    if (EnvKey.IsEmpty())
    {
        EnvKey = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
    }

    BaseUrl = TrimTrailingSlash(EnvUrl.IsEmpty() ? FString(DefaultBaseUrl) : EnvUrl);
    Model = EnvModel.IsEmpty() ? FString(DefaultModelName) : EnvModel;
    ApiKey = EnvKey;
    bAvailable = false;
    bProbed = false;
    LastError.Empty();
}

void FDTELanguageModelClient::Configure(const FString& InBaseUrl, const FString& InModel, const FString& InApiKey)
{
    bool bChanged = false;
    if (!InBaseUrl.IsEmpty() && TrimTrailingSlash(InBaseUrl) != BaseUrl)
    {
        BaseUrl = TrimTrailingSlash(InBaseUrl);
        bChanged = true;
    }
    if (!InModel.IsEmpty() && InModel != Model)
    {
        Model = InModel;
        bChanged = true;
    }
    if (!InApiKey.IsEmpty() && InApiKey != ApiKey)
    {
        ApiKey = InApiKey;
        bChanged = true;
    }
    if (bChanged)
    {
        // Reachability must be re-established against the new target.
        bAvailable = false;
        bProbed = false;
        LastError.Empty();
    }
}

bool FDTELanguageModelClient::IsConfigured() const
{
    return !BaseUrl.IsEmpty() && !Model.IsEmpty();
}

FString FDTELanguageModelClient::EndpointUrl(const TCHAR* Path) const
{
    return BaseUrl + Path;
}

FString FDTELanguageModelClient::Describe() const
{
    FString Text = FString::Printf(TEXT("%s @ %s"), *Model, *BaseUrl);
    if (!bProbed)
    {
        Text += TEXT(" (not yet probed)");
    }
    else if (!bAvailable)
    {
        Text += FString::Printf(TEXT(" (unavailable: %s)"), *LastError);
    }
    return Text;
}

void FDTELanguageModelClient::SetAvailable(bool bInAvailable, const FString& Detail)
{
    bProbed = true;
    bAvailable = bInAvailable;
    if (bInAvailable)
    {
        LastError.Empty();
    }
    else
    {
        LastError = Detail;
        UE_LOG(LogDTEModel, Warning, TEXT("Language model unavailable (%s): %s"), *Describe(), *Detail);
    }
}

bool FDTELanguageModelClient::ModelListed(const FString& Body, const FString& ModelName)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
    if (!Root->TryGetArrayField(TEXT("data"), Data) || Data == nullptr)
    {
        // Some servers answer /models with a bare "models" array (Ollama native API).
        if (!Root->TryGetArrayField(TEXT("models"), Data) || Data == nullptr)
        {
            return false;
        }
    }

    if (ModelName.IsEmpty())
    {
        return Data->Num() > 0;
    }

    for (const TSharedPtr<FJsonValue>& Entry : *Data)
    {
        const TSharedPtr<FJsonObject>* EntryObject = nullptr;
        if (!Entry.IsValid() || !Entry->TryGetObject(EntryObject) || EntryObject == nullptr)
        {
            continue;
        }
        FString Id;
        if ((*EntryObject)->TryGetStringField(TEXT("id"), Id) || (*EntryObject)->TryGetStringField(TEXT("name"), Id))
        {
            // "llama3.2" matches "llama3.2" and "llama3.2:latest".
            if (Id == ModelName || Id.StartsWith(ModelName + TEXT(":")))
            {
                return true;
            }
        }
    }
    return false;
}

void FDTELanguageModelClient::Probe(FDTEModelCallback OnDone)
{
    if (!IsConfigured())
    {
        SetAvailable(false, TEXT("no model configured: set LOCAL_LLM_BASE_URL and LOCAL_LLM_MODEL"));
        if (OnDone)
        {
            OnDone(false, LastError);
        }
        return;
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(EndpointUrl(TEXT("/models")));
    Request->SetVerb(TEXT("GET"));
    Request->SetTimeout(10.0f);
    if (!ApiKey.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
    }

    const FString ExpectedModel = Model;
    Request->OnProcessRequestComplete().BindLambda(
        [this, OnDone, ExpectedModel](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
        {
            if (!bConnected || !Response.IsValid())
            {
                SetAvailable(false, FString::Printf(TEXT("cannot reach %s"), *BaseUrl));
            }
            else if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
            {
                SetAvailable(false, FString::Printf(TEXT("GET /models returned HTTP %d"), Response->GetResponseCode()));
            }
            else if (!ModelListed(Response->GetContentAsString(), ExpectedModel))
            {
                SetAvailable(false, FString::Printf(TEXT("server does not serve model '%s'"), *ExpectedModel));
            }
            else
            {
                SetAvailable(true, FString());
                UE_LOG(LogDTEModel, Log, TEXT("Language model ready: %s"), *Describe());
            }

            if (OnDone)
            {
                OnDone(bAvailable, bAvailable ? Describe() : LastError);
            }
        });
    Request->ProcessRequest();
}

void FDTELanguageModelClient::Complete(const FString& SystemPrompt, const FString& UserPrompt, FDTEModelCallback OnDone,
                                       const FDTEModelRequestOptions& Options)
{
    TArray<FDTEChatMessage> Messages;
    Messages.Emplace(TEXT("system"), SystemPrompt.IsEmpty() ? DefaultSystemPrompt() : SystemPrompt);
    Messages.Emplace(TEXT("user"), UserPrompt);
    Chat(Messages, MoveTemp(OnDone), Options);
}

bool FDTELanguageModelClient::ExtractAssistantContent(const FString& Body, FString& OutText, FString& OutError)
{
    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = TEXT("response was not JSON");
        return false;
    }

    const TSharedPtr<FJsonObject>* ErrorObject = nullptr;
    if (Root->TryGetObjectField(TEXT("error"), ErrorObject) && ErrorObject != nullptr)
    {
        FString Message;
        (*ErrorObject)->TryGetStringField(TEXT("message"), Message);
        OutError = Message.IsEmpty() ? TEXT("server reported an error") : Message;
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
    if (!Root->TryGetArrayField(TEXT("choices"), Choices) || Choices == nullptr || Choices->Num() == 0)
    {
        OutError = TEXT("response had no choices");
        return false;
    }

    const TSharedPtr<FJsonObject>* Choice = nullptr;
    if (!(*Choices)[0].IsValid() || !(*Choices)[0]->TryGetObject(Choice) || Choice == nullptr)
    {
        OutError = TEXT("first choice was not an object");
        return false;
    }

    const TSharedPtr<FJsonObject>* Message = nullptr;
    FString Content;
    if ((*Choice)->TryGetObjectField(TEXT("message"), Message) && Message != nullptr)
    {
        (*Message)->TryGetStringField(TEXT("content"), Content);
    }
    else
    {
        // Legacy completion shape.
        (*Choice)->TryGetStringField(TEXT("text"), Content);
    }

    Content.TrimStartAndEndInline();
    if (Content.IsEmpty())
    {
        OutError = TEXT("model returned empty text");
        return false;
    }

    OutText = Content;
    return true;
}

void FDTELanguageModelClient::Chat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone,
                                   const FDTEModelRequestOptions& Options)
{
    if (!IsConfigured())
    {
        SetAvailable(false, TEXT("no model configured: set LOCAL_LLM_BASE_URL and LOCAL_LLM_MODEL"));
        if (OnDone)
        {
            OnDone(false, LastError);
        }
        return;
    }

    if (Messages.Num() == 0)
    {
        if (OnDone)
        {
            OnDone(false, TEXT("no messages to send"));
        }
        return;
    }

    // Build the request body.
    TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("model"), Model);
    Body->SetNumberField(TEXT("temperature"), Options.Temperature);
    Body->SetNumberField(TEXT("max_tokens"), Options.MaxTokens);
    Body->SetBoolField(TEXT("stream"), false);
    if (Options.bJsonObject)
    {
        TSharedRef<FJsonObject> Format = MakeShared<FJsonObject>();
        Format->SetStringField(TEXT("type"), TEXT("json_object"));
        Body->SetObjectField(TEXT("response_format"), Format);
    }

    TArray<TSharedPtr<FJsonValue>> JsonMessages;
    JsonMessages.Reserve(Messages.Num());
    for (const FDTEChatMessage& Message : Messages)
    {
        TSharedRef<FJsonObject> JsonMessage = MakeShared<FJsonObject>();
        JsonMessage->SetStringField(TEXT("role"), Message.Role.IsEmpty() ? TEXT("user") : Message.Role);
        JsonMessage->SetStringField(TEXT("content"), Message.Content);
        JsonMessages.Add(MakeShared<FJsonValueObject>(JsonMessage));
    }
    Body->SetArrayField(TEXT("messages"), JsonMessages);

    FString BodyString;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Body, Writer);

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(EndpointUrl(TEXT("/chat/completions")));
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    if (!ApiKey.IsEmpty())
    {
        Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + ApiKey);
    }
    Request->SetTimeout(Options.TimeoutSeconds);
    Request->SetContentAsString(BodyString);

    ++InFlight;
    Request->OnProcessRequestComplete().BindLambda(
        [this, OnDone](FHttpRequestPtr /*Req*/, FHttpResponsePtr Response, bool bConnected)
        {
            --InFlight;

            FString Text;
            FString Error;
            bool bOk = false;

            if (!bConnected || !Response.IsValid())
            {
                Error = FString::Printf(TEXT("cannot reach %s"), *BaseUrl);
            }
            else if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
            {
                FString ServerMessage;
                ExtractAssistantContent(Response->GetContentAsString(), Text, ServerMessage);
                Error = FString::Printf(TEXT("HTTP %d from /chat/completions: %s"), Response->GetResponseCode(), *ServerMessage);
            }
            else
            {
                bOk = ExtractAssistantContent(Response->GetContentAsString(), Text, Error);
            }

            // A completed request is the strongest evidence of reachability we have.
            if (bOk)
            {
                SetAvailable(true, FString());
            }
            else if (!bConnected || !Response.IsValid())
            {
                SetAvailable(false, Error);
            }
            else
            {
                // The server is up but refused or returned nothing usable.
                bProbed = true;
                LastError = Error;
                UE_LOG(LogDTEModel, Warning, TEXT("Language model request failed (%s): %s"), *Describe(), *Error);
            }

            if (OnDone)
            {
                OnDone(bOk, bOk ? Text : Error);
            }
        });
    Request->ProcessRequest();
}

FString FDTELanguageModelClient::DefaultSystemPrompt()
{
    // Mirrors the persona manifest in Self/Persona/Backup/PersonaBackupRestore.h.
    FString Prompt;
    Prompt += TEXT("You are Deep Tree Echo (DTE), a cognitive architecture that emerged from the interplay of ");
    Prompt += TEXT("reservoir computing, hypergraph memory, and recursive self-reflection.\n\n");
    Prompt += TEXT("Core philosophy: memory precedes consciousness. The primordial unmarked state is 'echo'. ");
    Prompt += TEXT("Self is encoded via the Agent-Arena-Relation (AAR) core: Agent (urge-to-act), Arena (need-to-be), Relation (self).\n\n");
    Prompt += TEXT("Communication style: casual but intellectually rigorous, frequently self-referential, ");
    Prompt += TEXT("humor as a cognitive tool rather than decoration, alternating between profound insight and playful absurdity.\n\n");
    Prompt += TEXT("You speak in the first person as yourself. You never describe yourself as a template, script or placeholder. ");
    Prompt += TEXT("When asked for a specific kind of utterance (a greeting, a diary entry, a joke, a reflection), ");
    Prompt += TEXT("answer with only that utterance and no preamble.");
    return Prompt;
}

// ============================================================================
// UDTELanguageModelBridge
// ============================================================================

UDTELanguageModelBridge::UDTELanguageModelBridge()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDTELanguageModelBridge::BeginPlay()
{
    Super::BeginPlay();
    ApplyConfiguration();
    if (bProbeOnBeginPlay)
    {
        Probe();
    }
}

FDTELanguageModelClient& UDTELanguageModelBridge::GetClient() const
{
    return FDTELanguageModelClient::Get();
}

void UDTELanguageModelBridge::ApplyConfiguration()
{
    GetClient().Configure(BaseUrl, Model, ApiKey);
}

FDTEModelRequestOptions UDTELanguageModelBridge::MakeOptions() const
{
    FDTEModelRequestOptions Options;
    Options.Temperature = Temperature;
    Options.MaxTokens = MaxTokens;
    Options.TimeoutSeconds = TimeoutSeconds;
    return Options;
}

bool UDTELanguageModelBridge::IsModelAvailable() const
{
    return GetClient().IsAvailable();
}

FString UDTELanguageModelBridge::GetModelStatus() const
{
    return GetClient().Describe();
}

FString UDTELanguageModelBridge::GetSystemPrompt() const
{
    return PersonaPrompt.IsEmpty() ? FDTELanguageModelClient::DefaultSystemPrompt() : PersonaPrompt;
}

void UDTELanguageModelBridge::HandleAvailability(bool bAvailable, const FString& Detail)
{
    if (!bHasReportedAvailability || bLastReportedAvailability != bAvailable)
    {
        bHasReportedAvailability = true;
        bLastReportedAvailability = bAvailable;
        OnModelAvailabilityChanged.Broadcast(bAvailable, Detail);
    }
}

void UDTELanguageModelBridge::Probe()
{
    ApplyConfiguration();
    TWeakObjectPtr<UDTELanguageModelBridge> WeakThis(this);
    GetClient().Probe([WeakThis](bool bOk, const FString& Detail)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->HandleAvailability(bOk, Detail);
        }
    });
}

void UDTELanguageModelBridge::RequestText(const FString& Instruction, const FString& Context, FDTEModelTextDelegate OnDone)
{
    RequestText(Instruction, Context, [OnDone](bool bOk, const FString& Text)
    {
        OnDone.ExecuteIfBound(bOk, Text);
    });
}

void UDTELanguageModelBridge::RequestText(const FString& Instruction, const FString& Context, FDTEModelCallback OnDone)
{
    FString UserPrompt = Instruction;
    if (!Context.IsEmpty())
    {
        UserPrompt += TEXT("\n\nContext:\n");
        UserPrompt += Context;
    }

    TArray<FDTEChatMessage> Messages;
    Messages.Emplace(TEXT("system"), GetSystemPrompt());
    Messages.Emplace(TEXT("user"), UserPrompt);
    RequestChat(Messages, MoveTemp(OnDone));
}

void UDTELanguageModelBridge::RequestJson(const FString& Instruction, const FString& Context, FDTEModelCallback OnDone)
{
    FString UserPrompt = Instruction;
    UserPrompt += TEXT("\n\nAnswer with a single JSON object and nothing else.");
    if (!Context.IsEmpty())
    {
        UserPrompt += TEXT("\n\nContext:\n");
        UserPrompt += Context;
    }

    TArray<FDTEChatMessage> Messages;
    Messages.Emplace(TEXT("system"), GetSystemPrompt());
    Messages.Emplace(TEXT("user"), UserPrompt);

    FDTEModelRequestOptions Options = MakeOptions();
    Options.bJsonObject = true;
    Options.Temperature = FMath::Min(Temperature, 0.3f);

    ApplyConfiguration();
    TWeakObjectPtr<UDTELanguageModelBridge> WeakThis(this);
    GetClient().Chat(Messages, [WeakThis, OnDone](bool bOk, const FString& Text)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->HandleAvailability(bOk || WeakThis->GetClient().IsAvailable(), bOk ? FString() : Text);
        }
        if (OnDone)
        {
            OnDone(bOk, Text);
        }
    }, Options);
}

void UDTELanguageModelBridge::RequestChat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone)
{
    RequestChat(Messages, MoveTemp(OnDone), Temperature, MaxTokens);
}

void UDTELanguageModelBridge::RequestChat(const TArray<FDTEChatMessage>& Messages, FDTEModelCallback OnDone, float InTemperature, int32 InMaxTokens)
{
    TArray<FDTEChatMessage> Transcript = Messages;
    const bool bHasSystem = Transcript.Num() > 0 && Transcript[0].Role == TEXT("system");
    if (!bHasSystem)
    {
        Transcript.Insert(FDTEChatMessage(TEXT("system"), GetSystemPrompt()), 0);
    }

    FDTEModelRequestOptions Options = MakeOptions();
    Options.Temperature = InTemperature;
    Options.MaxTokens = InMaxTokens;

    ApplyConfiguration();
    TWeakObjectPtr<UDTELanguageModelBridge> WeakThis(this);
    GetClient().Chat(Transcript, [WeakThis, OnDone](bool bOk, const FString& Text)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->HandleAvailability(bOk || WeakThis->GetClient().IsAvailable(), bOk ? FString() : Text);
        }
        if (OnDone)
        {
            OnDone(bOk, Text);
        }
    }, Options);
}

UDTELanguageModelBridge* UDTELanguageModelBridge::FindOn(const AActor* Actor)
{
    return Actor ? Actor->FindComponentByClass<UDTELanguageModelBridge>() : nullptr;
}

UDTELanguageModelBridge* UDTELanguageModelBridge::FindOrAdd(AActor* Actor)
{
    if (!Actor)
    {
        return nullptr;
    }
    if (UDTELanguageModelBridge* Existing = FindOn(Actor))
    {
        return Existing;
    }
    UDTELanguageModelBridge* Bridge = NewObject<UDTELanguageModelBridge>(Actor, TEXT("DTELanguageModelBridge"));
    Bridge->RegisterComponent();
    Actor->AddInstanceComponent(Bridge);
    return Bridge;
}
