// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeUsdContext.h"

#include "Objects/USDInfoCache.h"
#include "USDConversionUtils.h"
#include "UsdWrappers/UsdStage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeUsdContext)

#define LOCTEXT_NAMESPACE "InterchangeUSDContext"

UInterchangeUsdContext::UInterchangeUsdContext()
{
}

void UInterchangeUsdContext::BeginDestroy()
{
#if USE_USD_SDK
	if (bShouldCleanUpFromStageCache && StageIdInUsdUtilsStageCache != INDEX_NONE)
	{
		UsdUtils::RemoveStageFromUsdUtilsStageCache(StageIdInUsdUtilsStageCache);
	}

	ReleaseInfoCache();
#endif	  // USE_USD_SDK

	Super::BeginDestroy();
}

int64 UInterchangeUsdContext::GetStageId() const
{
	return StageIdInUsdUtilsStageCache;
}

void UInterchangeUsdContext::SetStageId(int64 InStageId)
{
	StageIdInUsdUtilsStageCache = InStageId;
}

UE::FUsdStage UInterchangeUsdContext::GetUsdStage() const
{
#if USE_USD_SDK
	return UsdUtils::FindUsdUtilsStageCacheStageId(StageIdInUsdUtilsStageCache);
#else
	return {};
#endif	  // USE_USD_SDK
}

bool UInterchangeUsdContext::SetUsdStage(const UE::FUsdStage& InStage)
{
#if USE_USD_SDK
	UE::FUsdStage OurStage = GetUsdStage();
	if (OurStage == InStage)
	{
		return true;
	}

	// This stage is already in the stage cache somehow. Just take its existing id,
	// but let's remember to not remove it from the stage cache whenever we're done, because
	// it wasn't us that put it there in the first place
	int64 ExistingId = UsdUtils::GetUsdUtilsStageCacheStageId(InStage);
	if (ExistingId != INDEX_NONE)
	{
		bShouldCleanUpFromStageCache = false;
		StageIdInUsdUtilsStageCache = ExistingId;
		return true;
	}

	// We're adding this stage to the stage cache, so let's make sure it's cleaned
	// up whenever we're released or else it will remain there forever
	bShouldCleanUpFromStageCache = true;
	StageIdInUsdUtilsStageCache = UsdUtils::InsertStageIntoUsdUtilsStageCache(InStage);
	return StageIdInUsdUtilsStageCache != INDEX_NONE;
#else
	return false;
#endif	  // USE_USD_SDK
}

FUsdInfoCache* UInterchangeUsdContext::GetInfoCache() const
{
	return InfoCache;
}

void UInterchangeUsdContext::SetExternalInfoCache(FUsdInfoCache& InInfoCache)
{
	if (&InInfoCache == GetInfoCache())
	{
		return;
	}

	ReleaseInfoCache();

	InfoCache = &InInfoCache;
}

FUsdInfoCache* UInterchangeUsdContext::CreateOwnedInfoCache()
{
	if (OwnedInfoCache.IsValid())
	{
		return GetInfoCache();
	}

	OwnedInfoCache = MakeUnique<FUsdInfoCache>();
	InfoCache = OwnedInfoCache.Get();
	return GetInfoCache();
}

void UInterchangeUsdContext::ReleaseInfoCache()
{
	InfoCache = nullptr;
	OwnedInfoCache.Reset();
}

#undef LOCTEXT_NAMESPACE
