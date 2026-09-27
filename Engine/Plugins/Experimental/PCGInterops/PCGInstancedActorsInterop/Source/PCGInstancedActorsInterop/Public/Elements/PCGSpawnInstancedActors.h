// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGSettings.h"
#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/Accessors/PCGPropertyAccessor.h"

#include "PCGSpawnInstancedActors.generated.h"

/** Node that allows to spawn instanced actors.
* Important notes:
* - In some cases, the actor class must be properly registered in the project settings prior to spawning. See the Instanced Actor plugin documentation for more details.
* - It is not currently possible to create or remove instanced actors at runtime and will log errors/warnings accordingly.
* - The Instanced Actor plugin does not support the preview/load as preview workflow, and using this node in such a way will log errors/warnings.
*/
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural))
class UPCGSpawnInstancedActorsSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override { return FName(TEXT("SpawnInstancedActors")); }
	virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCGSpawnInstancedActorsElement", "NodeTitle", "Spawn Instanced Actors"); }
	virtual FText GetNodeTooltipText() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return DefaultPointInputPinProperties(); }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Controls whether the actor class to use will be driven by an attribute on the input data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSpawnByAttribute = false;

	/** Attribute specifier for the attribute class to spawn. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "bSpawnByAttribute", EditConditionHides, PCG_Overridable))
	FPCGAttributePropertyInputSelector SpawnAttributeSelector;

	/** Actor class to spawn when not using the 'Spawn by Attribute' mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (EditCondition = "!bSpawnByAttribute", EditConditionHides, OnlyPlaceable, DisallowCreateNew, PCG_Overridable))
	TSubclassOf<AActor> ActorClass;

	/** Mutes warnings on empty class, which can be useful when some points might not have a valid class. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMuteOnEmptyClass = false;
};

class FPCGSpawnInstancedActorsElement : public IPCGElement
{
public:
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* InContext) const override { return true; }
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }
	virtual bool SupportsBasePointDataInputs(FPCGContext* InContext) const override { return true; }

protected:
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};