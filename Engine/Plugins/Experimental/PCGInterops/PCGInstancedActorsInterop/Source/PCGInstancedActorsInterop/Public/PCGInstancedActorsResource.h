// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGManagedResource.h"

#include "InstancedActorsIndex.h"

#include "PCGInstancedActorsResource.generated.h"

UCLASS(BlueprintType)
class PCGINSTANCEDACTORSINTEROP_API UPCGInstancedActorsManagedResource : public UPCGManagedResource
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
	virtual void PostEditImport() override;
	//~End UObject interface

	//~Begin UPCGManagedResource interface
	virtual bool Release(bool bHardRelease, TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool ReleaseIfUnused(TSet<TSoftObjectPtr<AActor>>& OutActorsToDelete) override;
	virtual bool MoveResourceToNewActor(AActor* NewActor) override;
	virtual void MarkAsUsed() override;

#if WITH_EDITOR
	virtual void ChangeTransientState(EPCGEditorDirtyMode NewEditingMode) override;
	virtual void MarkTransientOnLoad() override;
#endif
	//~End UPCGManagedResource interface

public:
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = GeneratedData)
	TArray<FInstancedActorsInstanceHandle> Handles;
};