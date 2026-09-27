// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UsdWrappers/UsdStage.h"

#include "CoreMinimal.h"

#include "InterchangeUsdContext.generated.h"

class FUsdInfoCache;

UCLASS(BlueprintType)
class INTERCHANGEOPENUSDIMPORT_API UInterchangeUsdContext : public UObject
{
	GENERATED_BODY()

public:
	UInterchangeUsdContext();
	virtual void BeginDestroy() override;

	// Returns the ID the provided stage has within the USDUtils' singleton StageCache.
	UFUNCTION(BlueprintCallable, Category = "Interchange | USD")
	int64 GetStageId() const;

	// Sets the ID of a particular stage from the UsdUtils' singleton StageCache.
	// If this corresponds to a valid USD Stage, that stage will be used for the Interchange import.
	UFUNCTION(BlueprintCallable, Category = "Interchange | USD")
	void SetStageId(int64 InStageId);

	// Convenience functions to get/set the stage directly, although it will internally just
	// get/set the stage into the UsdUtils' singleton StageCache and track it's Id instead.
	UE::FUsdStage GetUsdStage() const;
	bool SetUsdStage(const UE::FUsdStage& InStage);

	// Returns the current info cache, whether it is one we fully own, or just an external reference we're tracking
	FUsdInfoCache* GetInfoCache() const;

	// Receive a reference to an info cache that is external to this object and set it as the current info cache.
	// Note that this will discard our owned info cache if we had one before.
	void SetExternalInfoCache(FUsdInfoCache& InInfoCache);

	// Create an info cache that is fully owned by this UInterchangeUsdContext object, and set it as the current info cache.
	FUsdInfoCache* CreateOwnedInfoCache();

	// Properly releases the reference to the current stage cache, whether owned or not (deleting it if owned)
	void ReleaseInfoCache();

private:
	// We never store the stage itself, but only it's Id within the UsdUtils' singleton StageCache.
	// The intent here is to allow Python stages to be passed in and manipulated via Python in case of Python imports or pipelines.
	int64 StageIdInUsdUtilsStageCache = INDEX_NONE;
	bool bShouldCleanUpFromStageCache = false;

	FUsdInfoCache* InfoCache = nullptr;
	TUniquePtr<FUsdInfoCache> OwnedInfoCache;
};
