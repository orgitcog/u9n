// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CaptureManagerUnrealEndpoint.h"

#include "Templates/PimplPtr.h"

#include "CoreMinimal.h"

namespace UE::CaptureManager
{

/**
* @brief Detects and manages ingest endpoints (UE/UEFN instances) for the Capture Manager.
*/
UE_INTERNAL class CAPTUREMANAGERUNREALENDPOINT_API FUnrealEndpointManager
{
public:
	DECLARE_TS_MULTICAST_DELEGATE(FEndpointsChanged);

	FUnrealEndpointManager();
	~FUnrealEndpointManager();

	/**
	* @brief Start discovering endpoints.
	*/
	void Start();

	/**
	* @brief Stop discovering endpoints.
	*/
	void Stop();

	/**
	* @brief Blocks until an endpoint matching the given predicate is discovered or the timeout (in milliseconds) is reached.
	*
	* @returns The endpoint (if discovered) else nullptr.
	*/
	TSharedPtr<FUnrealEndpoint> WaitForEndpoint(TFunction<bool(const FUnrealEndpoint&)> InPredicate, int32 InTimeoutMS);

	/**
	* @brief Finds a discovered endpoint matching the given predicate.
	*
	* @returns The endpoint (if found) else nullptr.
	*/
	TSharedPtr<FUnrealEndpoint> FindEndpointByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate);

	/**
	* @returns A list of all discovered endpoints matching the given predicate.
	*/
	TArray<TSharedRef<FUnrealEndpoint>> FindEndpointsByPredicate(TFunction<bool(const FUnrealEndpoint&)> InPredicate);

	/**
	* @returns A list of all discovered endpoints.
	*/
	TArray<TSharedRef<FUnrealEndpoint>> GetEndpoints();

	/**
	* @returns The number of discovered endpoints
	*/
	int32 GetNumEndpoints() const;

	/**
	* @returns A delegate which fires whenever the discovered endpoints have changed.
	*/
	FEndpointsChanged& EndpointsChanged();

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};

}
