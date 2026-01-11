// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tickable.h"

#include "CustomizableObjectInstanceUsagePrivate.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UCustomizableObjectInstance;
class UCustomizableObjectInstanceUsage;
class UCustomizableSkeletalComponent;
class UPhysicsAsset;
class USkeletalMesh;
class USkeletalMeshComponent;

UCLASS(MinimalAPI)
class UCustomizableObjectInstanceUsagePrivate : public UObject
{
	GENERATED_BODY()

public:
	// Own interface
	
	/** Common end point of all updates. Even those which failed. */
	UE_API void Callbacks();

#if WITH_EDITOR
	// Used to generate instances outside the CustomizableObject editor and PIE
	UE_API void UpdateDistFromComponentToLevelEditorCamera(const FVector& CameraPosition);
#endif

	UE_API USkeletalMesh* GetSkeletalMesh() const;

	UE_API USkeletalMesh* GetAttachedSkeletalMesh() const;
	
	UE_API void UpdateDistFromComponentToPlayer(const AActor* const Pawn, bool bForceEvenIfNotBegunPlay = false);
	
	/** Return a valid pointer if this Usage is being used inside a UCustomizableSkeletalComponent. Nullptr if it is being used in Standalone mode. */
	UE_API UCustomizableSkeletalComponent* GetCustomizableSkeletalComponent() const;
	
	UE_API UCustomizableObjectInstanceUsage* GetPublic();

	UE_API const UCustomizableObjectInstanceUsage* GetPublic() const;

	// Returns true if the NetMode of the associated UCustomizableSkeletalComponent (or the associated SkeletalMeshComponent if the former does not exist) is equal to InNetMode
	UE_API bool IsNetMode(ENetMode InNetMode) const;

	/** Used to replace the Skeletal Mesh of the parent component by the Reference Skeletal Mesh or the generated Skeletal Mesh. */ 
	bool bPendingSetSkeletalMesh = true;
};

#undef UE_API
