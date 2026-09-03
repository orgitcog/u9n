// Copyright Epic Games, Inc. All Rights Reserved.

#include "ROSBridge.h"

UROSBridge::UROSBridge()
	: ROSMasterURI(TEXT("http://localhost:11311"))
	, WebSocketPort(9090)
	, bIsConnected(false)
{
}

void UROSBridge::Initialize(const FString& InROSMasterURI, int32 InPort)
{
	ROSMasterURI = InROSMasterURI;
	WebSocketPort = InPort;
	UE_LOG(LogTemp, Log, TEXT("ROSBridge: Initialized with ROS master at %s:%d"), *ROSMasterURI, WebSocketPort);
}

bool UROSBridge::Connect()
{
	bIsConnected = true;
	UE_LOG(LogTemp, Log, TEXT("ROSBridge: Connected to ROS master at %s"), *ROSMasterURI);
	return true;
}

void UROSBridge::Disconnect()
{
	bIsConnected = false;
	SubscribedTopics.Empty();
	PublishedTopics.Empty();
	MessageBuffer.Empty();
	UE_LOG(LogTemp, Log, TEXT("ROSBridge: Disconnected from ROS master"));
}

bool UROSBridge::SubscribeTopic(const FString& Topic, const FString& MessageType)
{
	if (!bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROSBridge: Not connected to ROS master"));
		return false;
	}

	if (!SubscribedTopics.Contains(Topic))
	{
		SubscribedTopics.Add(Topic);
		UE_LOG(LogTemp, Log, TEXT("ROSBridge: Subscribed to topic %s (type: %s)"), *Topic, *MessageType);
		return true;
	}
	return false;
}

void UROSBridge::UnsubscribeTopic(const FString& Topic)
{
	SubscribedTopics.Remove(Topic);
	MessageBuffer.Remove(Topic);
	UE_LOG(LogTemp, Log, TEXT("ROSBridge: Unsubscribed from topic %s"), *Topic);
}

void UROSBridge::PublishMessage(const FROSMessage& Message)
{
	if (!bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROSBridge: Not connected, cannot publish"));
		return;
	}

	if (!PublishedTopics.Contains(Message.Topic))
	{
		PublishedTopics.Add(Message.Topic);
	}

	UE_LOG(LogTemp, Verbose, TEXT("ROSBridge: Publishing to topic %s: %s"), *Message.Topic, *Message.Data);
}

bool UROSBridge::GetLatestMessage(const FString& Topic, FROSMessage& OutMessage)
{
	FROSMessage* Message = MessageBuffer.Find(Topic);
	if (Message)
	{
		OutMessage = *Message;
		return true;
	}
	return false;
}

void UROSBridge::CallService(const FString& ServiceName, const FString& Request)
{
	if (!bIsConnected)
	{
		UE_LOG(LogTemp, Warning, TEXT("ROSBridge: Not connected, cannot call service"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ROSBridge: Calling service %s with request: %s"), *ServiceName, *Request);
}

void UROSBridge::ProcessIncomingMessages()
{
	// Process messages from ROS topics
}
