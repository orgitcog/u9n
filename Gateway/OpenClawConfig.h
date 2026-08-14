// OpenClawConfig.h
// Deep Tree Echo - OpenClaw Gateway Configuration
// Configuration for angelclaw's multi-channel communication gateway
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "OpenClawConfig.generated.h"

/**
 * Supported Communication Channels
 * Maps angelclaw's openclaw-gateway channel adapters
 */
UENUM(BlueprintType)
enum class EOpenClawChannel : uint8
{
    WhatsApp    UMETA(DisplayName = "WhatsApp"),
    Telegram    UMETA(DisplayName = "Telegram"),
    Discord     UMETA(DisplayName = "Discord"),
    Slack       UMETA(DisplayName = "Slack"),
    Signal      UMETA(DisplayName = "Signal"),
    IMessage    UMETA(DisplayName = "iMessage"),
    Teams       UMETA(DisplayName = "Microsoft Teams"),
    Matrix      UMETA(DisplayName = "Matrix"),
    GoogleChat  UMETA(DisplayName = "Google Chat"),
    WebChat     UMETA(DisplayName = "WebChat"),
    IRC         UMETA(DisplayName = "IRC"),
    Nostr       UMETA(DisplayName = "Nostr")
};

/**
 * Gateway Connection Configuration
 */
USTRUCT(BlueprintType)
struct FOpenClawGatewayConfig
{
    GENERATED_BODY()

    /** WebSocket server host for gateway IPC */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    FString WebSocketHost = TEXT("localhost");

    /** WebSocket server port */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    int32 WebSocketPort = 8765;

    /** Enabled communication channels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    TArray<EOpenClawChannel> EnabledChannels;

    /** JSON-RPC timeout in milliseconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    int32 RPCTimeoutMs = 5000;

    /** Enable session persistence across channels */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    bool bSessionPersistence = true;

    /** Maximum concurrent sessions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gateway")
    int32 MaxConcurrentSessions = 100;
};

/**
 * Channel Message — represents a message from/to a communication channel
 */
USTRUCT(BlueprintType)
struct FOpenClawMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    FString MessageID;

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    EOpenClawChannel Channel = EOpenClawChannel::WebChat;

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    FString SessionID;

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    FString SenderID;

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    FString Content;

    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    float Timestamp = 0.0f;

    /** Emotional context extracted from message */
    UPROPERTY(BlueprintReadWrite, Category = "Gateway")
    float SentimentScore = 0.0f;
};
