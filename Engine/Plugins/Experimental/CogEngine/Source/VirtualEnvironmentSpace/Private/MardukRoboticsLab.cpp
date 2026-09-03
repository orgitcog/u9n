// Copyright Epic Games, Inc. All Rights Reserved.

#include "MardukRoboticsLab.h"
#include "ROSBridge.h"
#include "HomeAssistantBridge.h"
#include "OpenCogAgent.h"

AMardukRoboticsLab::AMardukRoboticsLab()
	: LabName(TEXT("Marduk's Robotics Laboratory"))
	, LabDescription(TEXT("Physical-virtual hybrid robotics and IoT experimentation environment"))
	, ROSMasterURI(TEXT("http://localhost:11311"))
	, ROSBridgePort(9090)
	, HomeAssistantURL(TEXT("http://localhost:8123"))
	, HomeAssistantToken(TEXT(""))
	, bIsLabActive(false)
	, ROSInterface(nullptr)
	, IoTInterface(nullptr)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

void AMardukRoboticsLab::BeginPlay()
{
	Super::BeginPlay();
	InitializeLab();
}

void AMardukRoboticsLab::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	// Periodic lab status monitoring could go here
}

void AMardukRoboticsLab::InitializeLab()
{
	if (bIsLabActive)
	{
		UE_LOG(LogTemp, Warning, TEXT("MardukRoboticsLab: %s already initialized"), *LabName);
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Initializing %s"), *LabName);

	InitializeROS();
	InitializeHomeAssistant();

	ConnectedRobots.Empty();
	ConnectedDevices.Empty();
	LabAgents.Empty();

	bIsLabActive = true;
	UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: %s initialized successfully"), *LabName);
}

void AMardukRoboticsLab::ShutdownLab()
{
	if (!bIsLabActive)
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Shutting down %s"), *LabName);

	if (ROSInterface)
	{
		ROSInterface->Disconnect();
	}

	if (IoTInterface)
	{
		IoTInterface->Disconnect();
	}

	ConnectedRobots.Empty();
	ConnectedDevices.Empty();
	LabAgents.Empty();

	bIsLabActive = false;
	UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: %s shutdown complete"), *LabName);
}

bool AMardukRoboticsLab::ConnectToROS()
{
	if (!ROSInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("MardukRoboticsLab: ROS interface not initialized"));
		return false;
	}

	bool bConnected = ROSInterface->Connect();
	if (bConnected)
	{
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Connected to ROS master at %s"), *ROSMasterURI);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MardukRoboticsLab: Failed to connect to ROS master"));
	}

	return bConnected;
}

bool AMardukRoboticsLab::ConnectToHomeAssistant()
{
	if (!IoTInterface)
	{
		UE_LOG(LogTemp, Error, TEXT("MardukRoboticsLab: Home Assistant interface not initialized"));
		return false;
	}

	bool bConnected = IoTInterface->Connect();
	if (bConnected)
	{
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Connected to Home Assistant at %s"), *HomeAssistantURL);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("MardukRoboticsLab: Failed to connect to Home Assistant"));
	}

	return bConnected;
}

bool AMardukRoboticsLab::RegisterRobot(const FString& RobotName)
{
	if (!bIsLabActive)
	{
		return false;
	}

	if (!ConnectedRobots.Contains(RobotName))
	{
		ConnectedRobots.Add(RobotName);
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Robot %s registered. Total robots: %d"), 
			*RobotName, ConnectedRobots.Num());
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("MardukRoboticsLab: Robot %s already registered"), *RobotName);
	return false;
}

bool AMardukRoboticsLab::RegisterDevice(const FString& DeviceName)
{
	if (!bIsLabActive)
	{
		return false;
	}

	if (!ConnectedDevices.Contains(DeviceName))
	{
		ConnectedDevices.Add(DeviceName);
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Device %s registered. Total devices: %d"), 
			*DeviceName, ConnectedDevices.Num());
		return true;
	}

	UE_LOG(LogTemp, Warning, TEXT("MardukRoboticsLab: Device %s already registered"), *DeviceName);
	return false;
}

bool AMardukRoboticsLab::AddLabAgent(UOpenCogAgent* Agent)
{
	if (!Agent || !bIsLabActive)
	{
		return false;
	}

	if (!LabAgents.Contains(Agent))
	{
		LabAgents.Add(Agent);
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Agent %s added to lab. Total agents: %d"), 
			*Agent->AgentID, LabAgents.Num());
		return true;
	}

	return false;
}

bool AMardukRoboticsLab::SubscribeToTopic(const FString& TopicName, const FString& MessageType)
{
	if (!ROSInterface)
	{
		return false;
	}

	bool bSuccess = ROSInterface->SubscribeTopic(TopicName, MessageType);
	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Subscribed to ROS topic %s"), *TopicName);
	}

	return bSuccess;
}

bool AMardukRoboticsLab::PublishToTopic(const FString& TopicName, const FString& Message)
{
	if (!ROSInterface)
	{
		return false;
	}

	FROSMessage ROSMsg;
	ROSMsg.Topic = TopicName;
	ROSMsg.Data = Message;
	ROSMsg.Timestamp = FPlatformTime::Seconds();

	ROSInterface->PublishMessage(ROSMsg);
	UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Published to ROS topic %s"), *TopicName);

	return true;
}

bool AMardukRoboticsLab::ControlDevice(const FString& DeviceName, const FString& Command)
{
	if (!IoTInterface)
	{
		return false;
	}

	// Parse device name to extract domain and action
	// Format: "domain.entity_id"
	FString Domain, EntityID;
	if (DeviceName.Split(TEXT("."), &Domain, &EntityID))
	{
		bool bSuccess = IoTInterface->CallService(Domain, Command, DeviceName);
		if (bSuccess)
		{
			UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Device %s controlled with command %s"), 
				*DeviceName, *Command);
		}
		return bSuccess;
	}

	return false;
}

FString AMardukRoboticsLab::GetLabStatus() const
{
	FString Status = FString::Printf(
		TEXT("Lab: %s\nActive: %s\nRobots: %d\nDevices: %d\nAgents: %d\nROS: %s\nHA: %s"),
		*LabName,
		bIsLabActive ? TEXT("Yes") : TEXT("No"),
		ConnectedRobots.Num(),
		ConnectedDevices.Num(),
		LabAgents.Num(),
		ROSInterface ? TEXT("Connected") : TEXT("Disconnected"),
		IoTInterface ? TEXT("Connected") : TEXT("Disconnected")
	);

	return Status;
}

void AMardukRoboticsLab::InitializeROS()
{
	ROSInterface = NewObject<UROSBridge>(this);
	if (ROSInterface)
	{
		ROSInterface->Initialize(ROSMasterURI, ROSBridgePort);
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: ROS interface initialized"));
	}
}

void AMardukRoboticsLab::InitializeHomeAssistant()
{
	IoTInterface = NewObject<UHomeAssistantBridge>(this);
	if (IoTInterface)
	{
		IoTInterface->Initialize(HomeAssistantURL, HomeAssistantToken);
		UE_LOG(LogTemp, Log, TEXT("MardukRoboticsLab: Home Assistant interface initialized"));
	}
}
