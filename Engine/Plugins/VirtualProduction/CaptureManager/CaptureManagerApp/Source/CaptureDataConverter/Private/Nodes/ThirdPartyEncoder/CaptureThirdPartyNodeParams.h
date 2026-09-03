// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"

namespace UE::CaptureManager
{

static const FString VideoCommandArgumentTemplate = TEXT("-noautorotate -i {input} -an {params} -q:v 1 -qmax 1 -qmin 1 -start_number 0 {output}");
static const FString AudioCommandArgumentTemplate = TEXT("-i {input} -vn {output}");

}

struct FCaptureThirdPartyNodeParams
{
	FString Encoder;
	FString CommandArguments;
};
