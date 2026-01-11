// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_MAC
#include <libkern/OSByteOrder.h>
#define _byteswap_ulong(x) OSSwapInt32(x)
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogAppleProResMedia, Log, All);
