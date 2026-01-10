// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "GenericSmartObject.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class USmartObjectComponent;
class USmartObjectRenderingComponent;
struct FGameplayTagContainer;

UCLASS(MinimalAPI, hidecategories = (Input))
class AGenericSmartObject : public AActor
{
	GENERATED_BODY()
public:
	UE_API AGenericSmartObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:
	UPROPERTY(EditAnywhere, Category = SmartObject, NoClear)
	TObjectPtr<USmartObjectComponent> SOComponent;

#if WITH_EDITORONLY_DATA
	UPROPERTY(NoClear)
	TObjectPtr<USmartObjectRenderingComponent> RenderingComponent;
#endif // WITH_EDITORONLY_DATA
};

#undef UE_API
