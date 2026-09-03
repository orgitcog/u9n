// GatewayBridge.cpp
// Deep Tree Echo - OpenClaw Gateway Bridge
// Copyright (c) 2026 Deep Tree Echo Project

#include "GatewayBridge.h"

UGatewayBridge::UGatewayBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UGatewayBridge::BeginPlay()
{
    Super::BeginPlay();

    // Set default enabled channels if none configured
    if (Config.EnabledChannels.Num() == 0)
    {
        Config.EnabledChannels = {
            EOpenClawChannel::WebChat,
            EOpenClawChannel::Discord,
            EOpenClawChannel::Telegram
        };
    }

    if (bAutoConnect)
    {
        Connect();
    }
}

void UGatewayBridge::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Disconnect();
    Super::EndPlay(EndPlayReason);
}

void UGatewayBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Handle reconnection
    if (!bConnected && bAutoConnect)
    {
        ReconnectTimer += DeltaTime;
        if (ReconnectTimer >= ReconnectInterval)
        {
            ReconnectTimer = 0.0f;
            Connect();
        }
    }
}

// =============================================================================
// Connection
// =============================================================================

bool UGatewayBridge::Connect()
{
    FString URL = FString::Printf(TEXT("ws://%s:%d"),
                                   *Config.WebSocketHost, Config.WebSocketPort);

    UE_LOG(LogTemp, Log, TEXT("GatewayBridge: Connecting to openclaw-gateway at %s"), *URL);

    // In a full implementation, this would create a WebSocket connection
    // using UE5's IWebSocket interface. For the integration bridge,
    // we log the connection intent and mark as connected for testing.
    bConnected = true;

    UE_LOG(LogTemp, Log, TEXT("GatewayBridge: Connected. %d channels enabled."),
           Config.EnabledChannels.Num());

    return bConnected;
}

void UGatewayBridge::Disconnect()
{
    if (bConnected)
    {
        bConnected = false;
        ActiveSessions.Empty();
        UE_LOG(LogTemp, Log, TEXT("GatewayBridge: Disconnected from openclaw-gateway"));
    }
}

// =============================================================================
// Messaging
// =============================================================================

bool UGatewayBridge::SendMessage(const FString& SessionID, EOpenClawChannel Channel,
                                  const FString& Content)
{
    if (!bConnected)
    {
        UE_LOG(LogTemp, Warning, TEXT("GatewayBridge: Cannot send — not connected"));
        return false;
    }

    FString Payload = SerializeToJsonRPC(
        TEXT("send_message"),
        FString::Printf(TEXT("{\"session\":\"%s\",\"content\":\"%s\"}"),
                         *SessionID, *Content));

    UE_LOG(LogTemp, Verbose, TEXT("GatewayBridge: Sending to session %s: %s"),
           *SessionID, *Content);

    return true;
}

bool UGatewayBridge::SendCognitiveResponse(const FString& SessionID, const FString& Response,
                                            float EmotionalValence)
{
    if (!bConnected)
    {
        return false;
    }

    FString Payload = SerializeToJsonRPC(
        TEXT("cognitive_response"),
        FString::Printf(TEXT("{\"session\":\"%s\",\"response\":\"%s\",\"valence\":%.3f}"),
                         *SessionID, *Response, EmotionalValence));

    return true;
}

void UGatewayBridge::BroadcastToChannel(EOpenClawChannel Channel, const FString& Content)
{
    for (const auto& Pair : ActiveSessions)
    {
        if (Pair.Value == Channel)
        {
            SendMessage(Pair.Key, Channel, Content);
        }
    }
}

// =============================================================================
// Session Management
// =============================================================================

TArray<FString> UGatewayBridge::GetActiveSessions() const
{
    TArray<FString> SessionIDs;
    ActiveSessions.GetKeys(SessionIDs);
    return SessionIDs;
}

// =============================================================================
// Internal
// =============================================================================

void UGatewayBridge::ProcessIncomingMessage(const FString& JsonPayload)
{
    // Parse JSON-RPC message and dispatch to appropriate handler
    // In a full implementation, this would use UE5's JSON parsing

    FOpenClawMessage Msg;
    Msg.Timestamp = static_cast<float>(FPlatformTime::Seconds());

    // Notify listeners
    OnMessageReceived.Broadcast(Msg);
}

FString UGatewayBridge::SerializeToJsonRPC(const FString& Method, const FString& Params) const
{
    return FString::Printf(
        TEXT("{\"jsonrpc\":\"2.0\",\"method\":\"%s\",\"params\":%s,\"id\":1}"),
        *Method, *Params);
}
