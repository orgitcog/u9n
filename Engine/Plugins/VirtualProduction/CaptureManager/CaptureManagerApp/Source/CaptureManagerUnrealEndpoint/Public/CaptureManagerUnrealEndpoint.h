// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerTakeMetadata.h"

#include "IMessageContext.h"
#include "CoreMinimal.h"

namespace UE::CaptureManager
{

/**
* @brief An immutable container for upload task information, also provides a place to tie into update delegates.
*/
UE_INTERNAL class CAPTUREMANAGERUNREALENDPOINT_API FTakeUploadTask
{
public:
	DECLARE_DELEGATE_OneParam(FUploadProgressed, double);
	DECLARE_DELEGATE_TwoParams(FUploadComplete, const FString& InMessage, int32 InStatusCode);

	FTakeUploadTask(
		FGuid InTaskID,
		FGuid InCaptureSourceID,
		FString InCaptureSourceName,
		FString InDataDirectory,
		FTakeMetadata InMetadata
	);

	const FGuid& GetTaskID() const;
	const FGuid& GetCaptureSourceID() const;
	const FString& GetCaptureSourceName() const;
	const FString& GetDataDirectory() const;
	const FTakeMetadata& GetTakeMetadata() const;

	FUploadProgressed& Progressed();
	FUploadComplete& Complete();

private:
	const FGuid TaskID;
	const FGuid CaptureSourceID;
	const FString CaptureSourceName;
	const FString DataDirectory;
	const FTakeMetadata TakeMetadata;

	FUploadProgressed ProgressedDelegate;
	FUploadComplete CompleteDelegate;
};

UE_INTERNAL struct FUnrealEndpointInfo
{
	FGuid EndpointID;
	FMessageAddress MessageAddress;
	FString IPAddress;
	FString HostName;
	int32 ImportServicePort = 0;
};

/**
* @brief Converts an endpoint info object into a string, useful for logging
*/
UE_INTERNAL CAPTUREMANAGERUNREALENDPOINT_API FString UnrealEndpointInfoToString(const FUnrealEndpointInfo& InEndpointInfo);

/**
* @brief An ingest endpoint (UE/UEFN instance) for the Capture Manager.
*/
UE_INTERNAL class CAPTUREMANAGERUNREALENDPOINT_API FUnrealEndpoint
{
public:
	enum class EConnectionState
	{
		Disconnected,
		Connected
	};

	FUnrealEndpoint(FUnrealEndpointInfo InEndpointInfo);
	~FUnrealEndpoint();

	/**
	* @brief Starts the connection to the endpoint (if not already started).
	*
	* This function returns before the connection is made, use WaitForConnectionState to block until the connection is established.
	*/
	void StartConnection();

	/**
	* @brief Stops the connection to the endpoint (if not already stopped).
	*
	* This function returns before the connection is stopped, use WaitForConnectionState to block until the connection is terminated.
	*/
	void StopConnection();

	/**
	* @brief Blocks until either the connection is made or the timeout is exceeded.
	*
	* @returns true if the connection is ready, false if the timeout was exceeded.
	*/
	bool WaitForConnectionState(EConnectionState InConnectionState, int32 InTimeoutMs) const;

	/**
	* @brief Adds a take upload task to the queue for this endpoint.
	*
	* @returns true if successful, false if errors occurred.
	*/
	bool AddTakeUploadTask(TUniquePtr<FTakeUploadTask> InTakeUploadTask, FGuid& OutTaskID);

	/**
	* @brief Cancels the upload task with the supplied task ID.
	*/
	void CancelTakeUploadTask(FGuid InTakeUploadTask);

	/**
	* @returns Information about the unreal endpoint.
	*/
	const FUnrealEndpointInfo& GetInfo() const;

private:
	struct FImpl;
	TSharedPtr<FImpl> Impl;
};

}