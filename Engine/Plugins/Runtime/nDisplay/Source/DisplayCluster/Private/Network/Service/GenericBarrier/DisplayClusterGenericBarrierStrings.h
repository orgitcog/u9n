// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"


/**
 * Generic barriers protocol strings
 */
namespace DisplayClusterGenericBarrierStrings
{
	constexpr static const TCHAR* ProtocolName = TEXT("GenericBarrier");

	constexpr static const TCHAR* TypeRequest  = TEXT("Request");
	constexpr static const TCHAR* TypeResponse = TEXT("Response");

	constexpr static const TCHAR* ArgumentsDefaultCategory = TEXT("GB");

	// Shared arguments
	constexpr static const TCHAR* ArgBarrierId = TEXT("BarrierId");
	constexpr static const TCHAR* ArgResult    = TEXT("CtrlResult");


	namespace CreateBarrier
	{
		constexpr static const TCHAR* Name = TEXT("CreateBarrier");

		constexpr static const TCHAR* ArgCallers = TEXT("SyncCallers");
		constexpr static const TCHAR* ArgTimeout = TEXT("Timeout");
	}

	namespace WaitUntilBarrierIsCreated
	{
		constexpr static const TCHAR* Name = TEXT("WaitUntilBarrierIsCreated");
	}

	namespace IsBarrierAvailable
	{
		constexpr static const TCHAR* Name = TEXT("IsBarrierAvailable");
	}

	namespace ReleaseBarrier
	{
		constexpr static const TCHAR* Name = TEXT("ReleaseBarrier");
	}

	namespace SyncOnBarrier
	{
		constexpr static const TCHAR* Name = TEXT("Sync");

		constexpr static const TCHAR* ArgCallerId = TEXT("CallerId");
	}

	namespace SyncOnBarrierWithData
	{
		constexpr static const TCHAR* Name = TEXT("SyncWithData");

		constexpr static const TCHAR* ArgCallerId     = TEXT("CallerId");
		constexpr static const TCHAR* ArgRequestData  = TEXT("ReqData");
		constexpr static const TCHAR* ArgResponseData = TEXT("RespData");
	}
};
