// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CookPackageSplitter.h"
#include "Engine/World.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "WorldPartition/Cook/WorldPartitionCookPackageContext.h"

class FWorldPartitionCookPackageSplitter : public FGCObject, public ICookPackageSplitter
{
public:
	//~ Begin of ICookPackageSplitter
	static bool ShouldSplit(UObject* SplitData);
	static FString GetSplitterDebugName() { return TEXT("FWorldPartitionCookPackageSplitter"); }

	FWorldPartitionCookPackageSplitter();
	virtual ~FWorldPartitionCookPackageSplitter();

	virtual void Teardown(ETeardown Status) override;
	virtual bool UseInternalReferenceToAvoidGarbageCollect() override
	{
		return true;
	}
	virtual EGeneratedRequiresGenerator DoesGeneratedRequireGenerator() override
	{
		return EGeneratedRequiresGenerator::Populate;
	}
	virtual bool RequiresGeneratorPackageDestructBeforeResplit() override
	{
		return true;
	}

	virtual TArray<ICookPackageSplitter::FGeneratedPackage> GetGenerateList(const UPackage* OwnerPackage, const UObject* OwnerObject) override;
	virtual bool PopulateGeneratedPackage(FPopulateContext& PopulateContext) override;
	virtual bool PopulateGeneratorPackage(FPopulateContext& PopulateContext) override;
	virtual void OnOwnerReloaded(UPackage* OwnerPackage, UObject* OwnerObject) override;
	//~ End of ICookPackageSplitter

private:
	void OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources);

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

	void BuildPackagesToGenerateList(TArray<ICookPackageSplitter::FGeneratedPackage>& PackagesToGenerate) const;
	
	TObjectPtr<UWorld> ReferencedWorld = nullptr;

	FWorldPartitionCookPackageContext CookContext;

	bool bForceInitializedWorld = false;
	bool bInitializedPhysicsSceneForSave = false;
};

#endif