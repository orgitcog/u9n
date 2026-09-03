// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "SmartObjectTypes.h"
#include "SmartObjectPersistentCollection.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

namespace EEndPlayReason { enum Type : int; }

class ASmartObjectPersistentCollection;
class UBillboardComponent;
class USmartObjectComponent;
class USmartObjectContainerRenderingComponent;
class USmartObjectDefinition;
class USmartObjectSubsystem;
struct FSmartObjectContainer;
struct FSmartObjectDefinitionReference;

/** Struct representing a unique registered component in the collection actor */
USTRUCT()
struct FSmartObjectCollectionEntry
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FSmartObjectCollectionEntry() = default;
	FSmartObjectCollectionEntry(const FSmartObjectCollectionEntry& Other) = default;
	FSmartObjectCollectionEntry(FSmartObjectCollectionEntry&& Other) = default;
	FSmartObjectCollectionEntry& operator=(const FSmartObjectCollectionEntry& Other) = default;
	FSmartObjectCollectionEntry& operator=(FSmartObjectCollectionEntry&& Other) = default;
	UE_API FSmartObjectCollectionEntry(const FSmartObjectHandle SmartObjectHandle, TNotNull<USmartObjectComponent*> SmartObjectComponent, const uint32 DefinitionIndex);
	UE_DEPRECATED(5.6, "Use the constructor taking a pointer to the component instead.")
	UE_API FSmartObjectCollectionEntry(const FSmartObjectHandle SmartObjectHandle, const USmartObjectComponent& SmartObjectComponent, const uint32 DefinitionIndex);
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	const FSmartObjectHandle& GetHandle() const
	{
		return Handle;
	}

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use GetComponent instead.")
	const FSoftObjectPath& GetPath() const
	{
		return Path_DEPRECATED;
	}
UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif// WITH_EDITORONLY_DATA

	USmartObjectComponent* GetComponent() const;

	FTransform GetTransform() const
	{
		return Transform;
	}

	const FBox& GetBounds() const
	{
		return Bounds;
	}

	FBox GetWorldBounds() const
	{
		return Bounds.MoveTo(Transform.GetLocation());
	}

	uint32 GetDefinitionIndex() const
	{
		return DefinitionIdx;
	}

	const FGameplayTagContainer& GetTags() const
	{
		return Tags;
	}

	friend FString LexToString(const FSmartObjectCollectionEntry& CollectionEntry);

protected:
	// Only the collection can access the path since the way we reference the component
	// might change to better support streaming so keeping this as encapsulated as possible
	friend FSmartObjectContainer;

#if WITH_EDITORONLY_DATA
	void SetDefinitionIndex(const uint32 InDefinitionIndex)
	{
		DefinitionIdx = InDefinitionIndex;
	}

	UE_DEPRECATED(5.6, "Use Component weak pointer instead.")
	UPROPERTY()
	FSoftObjectPath Path_DEPRECATED;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FGameplayTagContainer Tags;

	TWeakObjectPtr<USmartObjectComponent> Component;

	UPROPERTY()
	FTransform Transform;

	UPROPERTY()
	FBox Bounds = FBox(ForceInitToZero);

	UPROPERTY(VisibleAnywhere, Category = SmartObject, meta = (ShowOnlyInnerProperties))
	FSmartObjectHandle Handle;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	uint32 DefinitionIdx = INDEX_NONE;
};


USTRUCT()
struct FSmartObjectContainer
{
	GENERATED_BODY()

	UE_API explicit FSmartObjectContainer(UObject* InOwner = nullptr);
	UE_API ~FSmartObjectContainer();

	UE_API FSmartObjectContainer& operator=(const FSmartObjectContainer& Other);
	UE_API FSmartObjectContainer& operator=(FSmartObjectContainer&& Other);

	/**
	 * Creates a new entry for a given component.
	 * @param SOComponent SmartObject Component for which a new entry must be created
	 * @param bOutAlreadyInCollection Output parameter to indicate if an existing entry was returned instead of a newly created one.
	 * @return Pointer to the created or existing entry. An unset value indicates a registration error.
	 */
	UE_API FSmartObjectCollectionEntry* AddSmartObject(TNotNull<USmartObjectComponent*> SOComponent, bool& bOutAlreadyInCollection);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	FSmartObjectCollectionEntry* AddSmartObject(USmartObjectComponent& SOComponent, bool& bOutAlreadyInCollection)
	{
		return AddSmartObject(&SOComponent, bOutAlreadyInCollection);
	}

	UE_API bool RemoveSmartObject(TNotNull<USmartObjectComponent*> SOComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool RemoveSmartObject(USmartObjectComponent& SOComponent)
	{
		return RemoveSmartObject(&SOComponent);
	}
	
#if WITH_EDITORONLY_DATA
	/** 
	 * If SOComponent is already contained by this FSmartObjectContainer instance then data relating to it will get updated 
	 * @return whether this container instance contains SOComponent
	 */
	UE_API bool UpdateSmartObject(TNotNull<const USmartObjectComponent*> SOComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	bool UpdateSmartObject(const USmartObjectComponent& SOComponent)
	{
		return UpdateSmartObject(&SOComponent);
	}

#endif // WITH_EDITORONLY_DATA

	UE_API USmartObjectComponent* GetSmartObjectComponent(const FSmartObjectHandle SmartObjectHandle) const;

	UE_DEPRECATED(5.6, "Use the overload taking a UWorld as parameter.")
	const USmartObjectDefinition* GetDefinitionForEntry(const FSmartObjectCollectionEntry& Entry) const
	{
		return nullptr;
	}

	UE_API const USmartObjectDefinition* GetDefinitionForEntry(const FSmartObjectCollectionEntry& Entry, TNotNull<UWorld*> World) const;

	TConstArrayView<FSmartObjectCollectionEntry> GetEntries() const
	{
		return CollectionEntries;
	}

	void SetBounds(const FBox& InBounds)
	{
		Bounds = InBounds;
	}

	const FBox& GetBounds() const
	{
		return Bounds;
	}

	bool IsEmpty() const
	{
		return CollectionEntries.Num() == 0;
	}

	UE_API void Append(const FSmartObjectContainer& Other);
	UE_API int32 Remove(const FSmartObjectContainer& Other);

	UE_API void ValidateDefinitions();

	/** Note that this implementation is only expected to be used in the editor - it's pretty slow */
	friend SMARTOBJECTSMODULE_API uint32 GetTypeHash(const FSmartObjectContainer& Instance);

protected:
	friend USmartObjectSubsystem;
	friend ASmartObjectPersistentCollection;

	// assumes SOComponent to not be part of the collection yet
	UE_API FSmartObjectCollectionEntry* AddSmartObjectInternal(const FSmartObjectHandle Handle, TNotNull<USmartObjectComponent*> SOComponent);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead")
	FSmartObjectCollectionEntry* AddSmartObjectInternal(const FSmartObjectHandle Handle, const USmartObjectDefinition&, const USmartObjectComponent& SOComponent)
	{
		return AddSmartObjectInternal(Handle, const_cast<USmartObjectComponent*>(&SOComponent));
	}

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FBox Bounds = FBox(ForceInitToZero);

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectCollectionEntry> CollectionEntries;

	UE_DEPRECATED(5.6, "Use HandleToComponentMappings instead.")
	UPROPERTY()
	TMap<FSmartObjectHandle, FSoftObjectPath> RegisteredIdToObjectMap_DEPRECATED;

	UPROPERTY()
	TMap<FSmartObjectHandle, TObjectPtr<USmartObjectComponent>> HandleToComponentMappings;

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	TArray<FSmartObjectDefinitionReference> DefinitionReferences;

	// used for reporting and debugging
	UPROPERTY()
	TObjectPtr<const UObject> Owner;

#if WITH_EDITORONLY_DATA
	FString GetFullName() const
	{
		return Owner ? Owner->GetFullName() : TEXT("None");
	}

	UE_API void ConvertDeprecatedDefinitionsToReferences();
	UE_API void ConvertDeprecatedEntries();

	UE_DEPRECATED(5.6, "Use DefinitionReferences instead.")
	UPROPERTY()
	TArray<TObjectPtr<const USmartObjectDefinition>> Definitions_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};


/** Actor holding smart object persistent data */
UCLASS(MinimalAPI, NotBlueprintable, hidecategories = (Rendering, Replication, Collision, Input, HLOD, Actor, LOD, Cooking, WorldPartition))
class ASmartObjectPersistentCollection : public AActor
{
	GENERATED_BODY()

public:
	const TArray<FSmartObjectCollectionEntry>& GetEntries() const
	{
		return SmartObjectContainer.CollectionEntries;
	}

	void SetBounds(const FBox& InBounds)
	{
		SmartObjectContainer.Bounds = InBounds;
	}

	const FBox& GetBounds() const
	{
		return SmartObjectContainer.Bounds;
	}

	const FSmartObjectContainer& GetSmartObjectContainer() const
	{
		return SmartObjectContainer;
	};

	FSmartObjectContainer& GetMutableSmartObjectContainer()
	{
		return SmartObjectContainer;
	};

	bool IsEmpty() const
	{
		return SmartObjectContainer.IsEmpty();
	}

#if WITH_EDITORONLY_DATA
	UE_API void ResetCollection(const int32 ExpectedNumElements = 0);
	bool ShouldDebugDraw() const
	{
		return bEnableDebugDrawing;
	}
#endif // WITH_EDITORONLY_DATA

protected:
	friend class USmartObjectSubsystem;

	UE_API explicit ASmartObjectPersistentCollection(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UE_API virtual void PostLoad() override;
	UE_API virtual void PostActorCreated() override;
	UE_API virtual void Destroyed() override;
	UE_API virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	UE_API virtual void PreRegisterAllComponents() override;
	UE_API virtual void PostUnregisterAllComponents() override;

#if WITH_EDITOR
	UE_API virtual void PostEditUndo() override;
	
	/** Removes all entries from the collection. */
	UFUNCTION(CallInEditor, Category = SmartObject)
	UE_API void ClearCollection();

	/** Rebuild entries in the collection using all the SmartObjectComponents currently loaded in the level. */
	UFUNCTION(CallInEditor, Category = SmartObject)
	UE_API void RebuildCollection();

	/** Adds contents of InComponents to the stored SmartObjectContainer. Note that function does not clear out 
	 * the existing contents of SmartObjectContainer. Call ClearCollection or RebuildCollection if that is required. */
	UE_API void AppendToCollection(const TConstArrayView<USmartObjectComponent*> InComponents);

	UE_API void OnSmartObjectComponentChanged(TNotNull<const USmartObjectComponent*> Instance);

	UE_DEPRECATED(5.6, "Use the overload taking a pointer to the component instead.")
	void OnSmartObjectComponentChanged(const USmartObjectComponent& Instance)
	{
		OnSmartObjectComponentChanged(&Instance);
	}
#endif // WITH_EDITOR

	UE_API virtual bool RegisterWithSubsystem(const FString& Context);
	UE_API virtual bool UnregisterWithSubsystem(const FString& Context);

	UE_API void OnRegistered();
	bool IsRegistered() const
	{
		return bRegistered;
	}

	UE_API void OnUnregistered();

	UPROPERTY(VisibleAnywhere, Category = SmartObject)
	FSmartObjectContainer SmartObjectContainer;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	TObjectPtr<UBillboardComponent> SpriteComponent;

	UPROPERTY(transient)
	TObjectPtr<USmartObjectContainerRenderingComponent> RenderingComponent;

private:
	FDelegateHandle OnSmartObjectChangedDelegateHandle;

protected:
	UPROPERTY(EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bUpdateCollectionOnSmartObjectsChange = true;

	UPROPERTY(EditAnywhere, Category = SmartObject, AdvancedDisplay)
	bool bEnableDebugDrawing = true;
#endif // WITH_EDITORONLY_DATA

protected:
	bool bRegistered = false;
};

#undef UE_API
