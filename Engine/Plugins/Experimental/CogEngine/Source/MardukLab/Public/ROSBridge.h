// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ROSBridge.generated.h"

/**
 * ROS Message
 */
USTRUCT(BlueprintType)
struct FROSMessage
{
	GENERATED_BODY()

	/** Topic name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString Topic;

	/** Message type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString MessageType;

	/** Message data (JSON) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString Data;

	/** Timestamp */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	float Timestamp;

	FROSMessage()
		: Timestamp(0.0f)
	{
	}
};

/**
 * ROS Bridge - Connection to Robot Operating System
 */
UCLASS(Blueprintable, BlueprintType)
class MARDUKLAB_API UROSBridge : public UObject
{
	GENERATED_BODY()

public:
	UROSBridge();

	/** ROS master URI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	FString ROSMasterURI;

	/** WebSocket port */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "MardukLab")
	int32 WebSocketPort;

	/** Subscribed topics */
	UPROPERTY(BlueprintReadOnly, Category = "MardukLab")
	TArray<FString> SubscribedTopics;

	/** Published topics */
	UPROPERTY(BlueprintReadOnly, Category = "MardukLab")
	TArray<FString> PublishedTopics;

	/** Initialize ROS bridge */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void Initialize(const FString& InROSMasterURI, int32 InPort);

	/** Connect to ROS master */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool Connect();

	/** Disconnect from ROS master */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void Disconnect();

	/** Subscribe to ROS topic */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool SubscribeTopic(const FString& Topic, const FString& MessageType);

	/** Unsubscribe from ROS topic */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void UnsubscribeTopic(const FString& Topic);

	/** Publish ROS message */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void PublishMessage(const FROSMessage& Message);

	/** Get latest message from topic */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	bool GetLatestMessage(const FString& Topic, FROSMessage& OutMessage);

	/** Call ROS service */
	UFUNCTION(BlueprintCallable, Category = "MardukLab")
	void CallService(const FString& ServiceName, const FString& Request);

protected:
	/** Process incoming ROS messages */
	void ProcessIncomingMessages();

	TMap<FString, FROSMessage> MessageBuffer;
	bool bIsConnected;
};
