// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CustomizableSkeletalComponentPrivate.generated.h"

#define UE_API CUSTOMIZABLEOBJECT_API

class UCustomizableSkeletalComponent;
class UCustomizableObjectInstanceUsage;
class UPhysicsAsset;
class USkeletalMesh;


UCLASS(MinimalAPI)
class UCustomizableSkeletalComponentPrivate : public UObject
{
	GENERATED_BODY()

public:
	UE_API UCustomizableSkeletalComponentPrivate();
	
	/** Common end point of all updates. Even those which failed. */
	UE_API void Callbacks() const;
	
	UE_API USkeletalMesh* GetSkeletalMesh() const;

	UE_API USkeletalMesh* GetAttachedSkeletalMesh() const;
	
	UE_API UCustomizableSkeletalComponent* GetPublic();
	
	UE_API const UCustomizableSkeletalComponent* GetPublic() const;

	UPROPERTY(Instanced)
	TObjectPtr<UCustomizableObjectInstanceUsage> InstanceUsage;
};

#undef UE_API
