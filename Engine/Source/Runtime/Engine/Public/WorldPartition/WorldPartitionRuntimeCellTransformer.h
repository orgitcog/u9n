// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WorldPartitionRuntimeCellTransformer.generated.h"

UCLASS(MinimalAPI, Abstract)
class UWorldPartitionRuntimeCellTransformer : public UObject
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
public:
	virtual void PreTransform(ULevel* InLevel) {}
	virtual void Transform(ULevel* InLevel) {}
	virtual void PostTransform(ULevel* InLevel) {}
#endif

	bool IsEnabled() const { return bEnabled; }
	
protected:
	// Tag used to force exclude actors from any cell transformation
	ENGINE_API static const FName NAME_CellTransformerIgnoreActor;

private:
	UPROPERTY(EditAnywhere, Category = Transformer)
	bool bEnabled = true;
};