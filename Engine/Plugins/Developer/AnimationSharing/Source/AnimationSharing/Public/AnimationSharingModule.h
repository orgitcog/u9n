// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Engine/World.h"

#define UE_API ANIMATIONSHARING_API

class UAnimationSharingManager;
class UAnimationSharingSetup;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAnimationSharingManagerCreated, UAnimationSharingManager*, const UWorld*);

class FAnimSharingModule : public FDefaultModuleImpl, public FGCObject
{
public:
	// Begin IModuleInterface overrides
	UE_API virtual void StartupModule() override;
	// End IModuleInterface overrides

	// Begin FGCObject overrides
	UE_API virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FAnimSharingModule");
	}
	// End FGCObject overrides

	FORCEINLINE static UAnimationSharingManager* Get(const UWorld* World)
	{
		return WorldAnimSharingManagers.FindRef(World);
	}

	FORCEINLINE static FOnAnimationSharingManagerCreated& GetOnAnimationSharingManagerCreated()
	{
		return OnAnimationSharingManagerCreated;
	}

	/** Creates an animation sharing manager for the given UWorld (must be a Game World) */
	static UE_API bool CreateAnimationSharingManager(UWorld* InWorld, const UAnimationSharingSetup* Setup);
	
	/** Creates an animation sharing manager for the given UWorld (must be a Game World) - with 
		custom func that is responsible for creation and initialisation */
	using FCreateAnimationSharingManagerFunc = TFunction<UAnimationSharingManager*(UWorld* InWorld)>;
	static UE_API bool CreateAnimationSharingManager(UWorld* InWorld, FCreateAnimationSharingManagerFunc CreateAnimationSharingManagerFunc);

private:	
	static UE_API void OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	static UE_API TMap<const UWorld*, TObjectPtr<UAnimationSharingManager>> WorldAnimSharingManagers;
	static UE_API FOnAnimationSharingManagerCreated OnAnimationSharingManagerCreated;
};

#undef UE_API
