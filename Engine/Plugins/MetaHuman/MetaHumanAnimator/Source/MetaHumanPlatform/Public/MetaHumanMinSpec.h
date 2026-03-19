// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



class METAHUMANPLATFORM_API FMetaHumanMinSpec
{
public:

	static bool IsSupported();
	static FText GetMinSpec();

private:

	static bool bIsInitialized;
	static bool bIsSupported;
};
