// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MardukRoboticsLab.generated.h"

class UROSBridge;
class UHomeAssistantBridge;
class UOpenCogAgent;

/**
 * Marduk's Robotics Lab - Enhanced Robotics Laboratory
 * Physical-virtual hybrid environment for robotics and IoT experimentation
 * Integrates ROS robots, IoT devices, and cognitive agents in a unified space
 */
UCLASS(Blueprintable, BlueprintType)
class VIRTUALENVIRONMENTSPACE_API AMardukRoboticsLab : public AActor
{
	GENERATED_BODY()

public:
	AMardukRoboticsLab();

	/** Laboratory name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	FString LabName;

	/** Description of the lab's purpose */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	FString LabDescription;

	/** ROS master URI */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	FString ROSMasterURI;

	/** ROS bridge WebSocket port */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	int32 ROSBridgePort;

	/** Home Assistant URL */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	FString HomeAssistantURL;

	/** Home Assistant access token */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Marduk Robotics Lab")
	FString HomeAssistantToken;

	/** Is the lab currently operational */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	bool bIsLabActive;

	/** ROS bridge instance */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	UROSBridge* ROSInterface;

	/** Home Assistant bridge instance */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	UHomeAssistantBridge* IoTInterface;

	/** Connected robots */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	TArray<FString> ConnectedRobots;

	/** Connected IoT devices */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	TArray<FString> ConnectedDevices;

	/** Cognitive agents operating in the lab */
	UPROPERTY(BlueprintReadOnly, Category = "Marduk Robotics Lab")
	TArray<UOpenCogAgent*> LabAgents;

	/** Initialize the robotics laboratory */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	void InitializeLab();

	/** Shutdown the robotics laboratory */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	void ShutdownLab();

	/** Connect to ROS master */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool ConnectToROS();

	/** Connect to Home Assistant */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool ConnectToHomeAssistant();

	/** Register a robot with the lab */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool RegisterRobot(const FString& RobotName);

	/** Register an IoT device with the lab */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool RegisterDevice(const FString& DeviceName);

	/** Add a cognitive agent to the lab */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool AddLabAgent(UOpenCogAgent* Agent);

	/** Subscribe to ROS topic */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool SubscribeToTopic(const FString& TopicName, const FString& MessageType);

	/** Publish to ROS topic */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool PublishToTopic(const FString& TopicName, const FString& Message);

	/** Control IoT device */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	bool ControlDevice(const FString& DeviceName, const FString& Command);

	/** Get lab status report */
	UFUNCTION(BlueprintCallable, Category = "Marduk Robotics Lab")
	FString GetLabStatus() const;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

private:
	/** Initialize ROS connection */
	void InitializeROS();

	/** Initialize Home Assistant connection */
	void InitializeHomeAssistant();
};
