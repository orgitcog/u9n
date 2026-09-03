// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/RecursiveMutex.h"
#include "AutoRTFM.h"

#if UE_AUTORTFM
#include "Misc/TransactionallySafeCriticalSection.h"
#endif

namespace UE
{

#if UE_AUTORTFM
using FTransactionallySafeRecursiveMutex = ::FTransactionallySafeCriticalSection;
#else
using FTransactionallySafeRecursiveMutex = ::UE::FRecursiveMutex;
#endif // UE_AUTORTFM

} // UE
