// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Experimental/UnifiedError/UnifiedError.h"


class FIoStatus;

DECLARE_ERROR_MODULE(IoStore, 0x15);


namespace UE::UnifiedError::IoStore
{
	CORE_API UE::UnifiedError::FError ConvertError(const FIoStatus& Status);
}