// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "SmartObjectPersistentCollection.h"
#endif
#include "GameFramework/Actor.h"
#include "SmartObjectTypes.h"
#include "SmartObjectCollection.generated.h"

struct FSmartObjectCollectionEntry;
class USmartObjectDefinition;
class USmartObjectComponent;


/** Actor holding smart object persistent data */
UCLASS(MinimalAPI, Deprecated, NotBlueprintable, hidecategories = (Rendering, Replication, Collision, Input, HLOD, Actor, LOD, Cooking, WorldPartition), notplaceable, meta = (DeprecationMessage = "SmartObjectCollection class is deprecated. Please use SmartObjectPersistentCollection instead."))
class ADEPRECATED_SmartObjectCollection : public AActor
{
	GENERATED_BODY()

protected:
	friend class USmartObjectSubsystem;

	SMARTOBJECTSMODULE_API explicit ADEPRECATED_SmartObjectCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	SMARTOBJECTSMODULE_API virtual void PostLoad() override;

	/** Removes all entries from the collection. */
	SMARTOBJECTSMODULE_API void ClearCollection();

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FBox Bounds = FBox(ForceInitToZero);

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectCollectionEntry> CollectionEntries;

	UPROPERTY()
	TMap<FSmartObjectHandle, FSoftObjectPath> RegisteredIdToObjectMap;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<TObjectPtr<const USmartObjectDefinition>> Definitions;

	bool bRegistered = false;

#if WITH_EDITORONLY_DATA
private:
	/** This property used to be exposed to the UI editor. It was replaced with bBuildCollectionAutomatically for greater readability. */
	UPROPERTY()
	bool bBuildOnDemand_DEPRECATED = true;

protected:
	/** if set to true will result in letting the level-less SmartObjects to register. Required for smart object unit testing. */
	bool bIgnoreLevelTesting = false;

	UPROPERTY(VisibleAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bBuildCollectionAutomatically = false;

	bool bBuildingForWorldPartition = false;
#endif // WITH_EDITORONLY_DATA
};
