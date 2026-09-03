// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/SavePackage.h"

#if UE_WITH_SAVEPACKAGE
#include "Misc/Optional.h"
#include "UObject/ArchiveCookContext.h"

#if WITH_EDITOR

COREUOBJECT_API extern bool GOutputCookingWarnings;

#endif

FSavePackageResultStruct UPackage::Save(UPackage* InOuter, UObject* InAsset, const TCHAR* Filename,
	const FSavePackageArgs& SaveArgs)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPackage::Save);

	return UPackage::Save2(InOuter, InAsset, Filename, SaveArgs);
}

bool UPackage::SavePackage(UPackage* InOuter, UObject* InAsset, const TCHAR* Filename, const FSavePackageArgs& SaveArgs)
{
	const FSavePackageResultStruct Result = Save(InOuter, InAsset, Filename, SaveArgs);
	return Result == ESavePackageResult::Success;
}

FSavePackageContext::~FSavePackageContext()
{
	delete PackageWriter;
}

#endif	// UE_WITH_SAVEPACKAGE
