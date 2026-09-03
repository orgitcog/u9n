// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"
#include "MetaHumanCharacterEditorActorInterface.h"

#include "MetaHumanCharacterEditorActor.generated.h"

enum class EMetaHumanCharacterSkinPreviewMaterial : uint8;

/**
 * A simple actor that represents a MetaHuman in the Character Editor Viewport.
 * 
 * This is the default implementation of IMetaHumanCharacterEditorActorInterface, and will be used
 * as a fallback if the implementation specified by the Character Pipeline is invalid.
 * 
 * Since it's used as a fallback for any pipeline, it can't rely on any pipeline-specific data in
 * the passed-in Character Instance.
 */
UCLASS(Transient, NotPlaceable, Blueprintable)
class METAHUMANCHARACTEREDITOR_API AMetaHumanCharacterEditorActor : public AActor, public IMetaHumanCharacterEditorActorInterface
{
	GENERATED_BODY()

public:

	AMetaHumanCharacterEditorActor();

	// Begin IMetaHumanCharacterEditorActorInterface
	virtual void InitializeMetaHumanCharacterEditorActor(
		TNotNull<const UMetaHumanCharacterInstance*> InCharacterInstance,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<USkeletalMesh*> InFaceMesh,
		TNotNull<USkeletalMesh*> InBodyMesh,
		int32 InNumLODs,
		const TArray<int32>& InFaceLODMapping,
		const TArray<int32>& InBodyLODMapping) override;

	virtual void SetForcedLOD(int32 InForcedLOD) override;

	virtual TNotNull<UMetaHumanCharacter*> GetCharacter() const override;
	virtual TNotNull<const USkeletalMeshComponent*> GetFaceComponent() const override;
	virtual TNotNull<const USkeletalMeshComponent*> GetBodyComponent() const override;
	virtual void OnFaceMeshUpdated() override;
	virtual void OnBodyMeshUpdated() override;
	virtual void SetDrivingSkeletalMesh(USkeletalMeshComponent* DrivingSkelMeshComponent) override;
	virtual void UpdateFaceComponentMesh(USkeletalMesh* InFaceMesh) override;
	virtual void UpdateBodyComponentMesh(USkeletalMesh* InBodyMesh) override;
	virtual void ReinitAnimation() override;
	virtual void ResetAnimation() override;
	virtual void SetHairVisibilityState(EMetaHumanHairVisibilityState State) override;
	virtual void SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial = nullptr) override;
	// End IMetaHumanCharacterEditorActorInterface

protected:
	/** Any hair components should be set to the given visibility state */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character", meta = (DisplayName = "SetHairVisibilityState"))
	void Blueprint_SetHairVisibilityState(EMetaHumanHairVisibilityState State);

	/** 
	 * Any clothing components should be set to the given visibility state.
	 * 
	 * If the state is UseOverrideMaterial, the provided material should be applied. 
	 * 
	 * The original materials should be restored on the next time the state is changed.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Character", meta = (DisplayName = "SetClothingVisibilityState"))
	void Blueprint_SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial);

	UPROPERTY(BlueprintReadOnly, Category = "Character")
	TObjectPtr<const class UMetaHumanCharacterInstance> CharacterInstance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class USkeletalMeshComponent> BodyComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class USkeletalMeshComponent> FaceComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	TObjectPtr<class ULODSyncComponent> LODSyncComponent;

	// Retarget component used to read poses from another actor's skeletal mesh to retarget it.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retargeting")
	TObjectPtr<class URetargetComponent> RetargetComponent;

	// IK retargeter used by the retarget component.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retargeting")
	TObjectPtr<class UIKRetargeter> IKRetargeter;

private:
	/**
	 * Pointer back to the Character this actor is based on.
	 * 
	 * This is only here so it can be returned from GetCharacter. The actor shouldn't need to
	 * access the Character apart from that.
	 */
	UPROPERTY()
	TObjectPtr<class UMetaHumanCharacter> Character;
};
