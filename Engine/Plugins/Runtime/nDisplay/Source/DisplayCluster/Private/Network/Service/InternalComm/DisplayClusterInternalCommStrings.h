// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


/**
 * Internal comm related strings
 */
namespace DisplayClusterInternalCommStrings
{
	constexpr static const TCHAR* ProtocolName = TEXT("InternalComm");
	
	constexpr static const TCHAR* TypeRequest  = TEXT("Request");
	constexpr static const TCHAR* TypeResponse = TEXT("Response");

	constexpr static const TCHAR* ArgumentsDefaultCategory = TEXT("IC");

	namespace GatherServicesHostingInfo
	{
		constexpr static const TCHAR* Name = TEXT("GatherServicesHostingInfo");

		constexpr static const TCHAR* ArgNodeHostingInfo    = TEXT("NodeHostingInfo");
		constexpr static const TCHAR* ArgClusterHostingInfo = TEXT("ClusterHostingInfo");
	}

	namespace PostFailureNegotiate
	{
		constexpr static const TCHAR* Name = TEXT("PostFailureNegotiate");

		constexpr static const TCHAR* ArgSyncStateData = TEXT("SyncState");
		constexpr static const TCHAR* ArgRecoveryData  = TEXT("RecoveryData");
	}

	namespace RequestNodeDrop
	{
		constexpr static const TCHAR* Name = TEXT("RequestNodeDrop");

		constexpr static const TCHAR* ArgNodeId     = TEXT("NodeId");
		constexpr static const TCHAR* ArgDropReason = TEXT("DropReason");
	}
};
