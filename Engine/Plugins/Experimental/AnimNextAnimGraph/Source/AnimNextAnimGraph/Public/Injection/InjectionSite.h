// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/NameTypes.h"

#include "InjectionSite.generated.h"

// Specifies the desired injection site for an injection request.
// Optionally allows for fallback to the Default Injection Site specified by the module
//     if DesiredSiteName is None, or does not exist in the graph
USTRUCT()
struct FAnimNextInjectionSite
{
	GENERATED_BODY()

	FAnimNextInjectionSite()
		: DesiredSiteName(NAME_None)
		, bUseModuleFallback(true)
	{
	}

	FAnimNextInjectionSite(const FName& InSiteName)
		: DesiredSiteName(InSiteName)
		, bUseModuleFallback(DesiredSiteName == NAME_None)
	{
	}

	// The name of the site to inject into
	UPROPERTY()
	FName DesiredSiteName;

	// Flag specifying whether the request can (or should) fallback to the Default Injection Site
	// specified in the module if DesiredSiteName was not found
	UPROPERTY()
	uint8 bUseModuleFallback : 1;
};


namespace UE::AnimNext
{
	using FInjectionSite = FAnimNextInjectionSite;
}