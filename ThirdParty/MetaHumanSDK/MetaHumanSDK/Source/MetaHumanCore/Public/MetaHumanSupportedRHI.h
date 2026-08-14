// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



class METAHUMANCORE_API FMetaHumanSupportedRHI
{
public:

	static bool IsSupported();
	static FText GetSupportedRHINames();

private:

	static bool bIsInitialized;
	static bool bIsSupported;
};
