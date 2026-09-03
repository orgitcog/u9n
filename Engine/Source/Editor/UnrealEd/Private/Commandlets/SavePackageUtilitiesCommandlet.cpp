// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/SavePackageUtilitiesCommandlet.h"

#include "CoreGlobals.h"

USavePackageUtilitiesCommandlet::USavePackageUtilitiesCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 USavePackageUtilitiesCommandlet::Main(const FString& Params)
{
	UE_LOG(LogCore, Error, TEXT("SavePackageUtilitiesCommandlet is no longer needed. Contact Epic if you need this functionality."));
	return 0;
}
