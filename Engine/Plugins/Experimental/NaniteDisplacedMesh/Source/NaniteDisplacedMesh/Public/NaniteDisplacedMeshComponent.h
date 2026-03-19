// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/StaticMeshComponent.h"
#include "Rendering/NaniteInterface.h"
#include "NaniteDisplacedMeshComponent.generated.h"

#define UE_API NANITEDISPLACEDMESH_API

class FPrimitiveSceneProxy;
class UStaticMeshComponent;
class UMaterialInterface;
class UTexture;

UCLASS(MinimalAPI, ClassGroup=Rendering, hidecategories=(Object,Activation,Collision,"Components|Activation",Physics), editinlinenew, meta=(BlueprintSpawnableComponent))
class UNaniteDisplacedMeshComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UE_API virtual void PostLoad() override;

	UE_API virtual void BeginDestroy() override;

	UE_API virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	UE_API virtual void OnRegister() override;

	UE_API virtual const Nanite::FResources* GetNaniteResources() const;

	UE_API virtual FPrimitiveSceneProxy* CreateSceneProxy() override;

#if WITH_EDITOR
	UE_API virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	UE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	UE_API virtual void PostEditUndo() override;

	UE_API void OnRebuild();
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Displacement)
	TObjectPtr<class UNaniteDisplacedMesh> DisplacedMesh;

private:
	void UnbindCallback();
	void BindCallback();
};

#undef UE_API
