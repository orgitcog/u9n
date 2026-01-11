// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "Components/LODSyncComponent.h"

#include "MetaHumanCharacterEditorActorInterface.generated.h"

class UMaterialInterface;
class UMetaHumanCharacter;
class UMetaHumanCharacterInstance;
class USkeletalMesh;
class USkeletalMeshComponent;

UENUM(BlueprintType)
enum class EMetaHumanHairVisibilityState : uint8
{
	Shown,
	Hidden
};

UENUM(BlueprintType)
enum class EMetaHumanClothingVisibilityState : uint8
{
	Shown,
	UseOverrideMaterial,
	Hidden
};

/**
 * An actor implementing this interface can be used as a preview actor in the MetaHuman Character editor.
 * 
 * The MetaHuman Character Pipeline determines the type of preview actor to spawn.
 */
UINTERFACE(BlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class METAHUMANCHARACTERPALETTEEDITOR_API UMetaHumanCharacterEditorActorInterface : public UInterface
{
	GENERATED_BODY()
};

class METAHUMANCHARACTERPALETTEEDITOR_API IMetaHumanCharacterEditorActorInterface : public IInterface
{
	GENERATED_BODY()

public:

	/**
	 * Called by the Character editor to initialize the preview actor.
	 * 
	 * Will only be called once on each instance, after object construction but before actor spawn,
	 * and hence before the Blueprint construction script runs, if this is a Blueprint actor.
	 * 
	 * All other functions will be called after actor spawn, except where noted.
	 * 
	 * InCharacterInstance is a valid instance using the same Pipeline that spawned this actor.
	 * 
	 * InCharacter is the Character being edited. This should be returned from GetCharacter.
	 * 
	 * InFaceMesh and InBodyMesh should be assigned to SkeletalMeshComponents.
	 * 
	 * InNumLODs is the number of LODs the actor should have.
	 * 	This can be assigned to the LODSyncComponent's NumLODs property if using a LODSyncComponent.
	 * 	The InFaceLODMapping and InBodyLODMapping arrays will have this number of elements.
	 * 
	 * InFaceLODMapping is a mapping from the actor LOD to the face mesh LOD.
	 * 	For example, if InNumLODs is 4 and InFaceLODMapping is { 0, 0, 1, 1 }, then if SetForcedLOD
	 * 	is called with 3 as the argument, this selects actor LOD 3, which corresponds to
	 * 	InFaceLODMapping[3], which is 1, so the face mesh component should be set to LOD 1.
	 * 	
	 * 	If this actor uses the LODSyncComponent, it can make a CustomLODMapping entry for the Face
	 * 	component and assign InFaceLODMapping to it. See AMetaHumanCharacterEditorActor for reference.
	 */
	virtual void InitializeMetaHumanCharacterEditorActor(
		TNotNull<const UMetaHumanCharacterInstance*> InCharacterInstance,
		TNotNull<UMetaHumanCharacter*> InCharacter,
		TNotNull<USkeletalMesh*> InFaceMesh,
		TNotNull<USkeletalMesh*> InBodyMesh,
		int32 InNumLODs,
		const TArray<int32>& InFaceLODMapping,
		const TArray<int32>& InBodyLODMapping) = 0;

	/**
	 * Forces the given actor LOD to be displayed.
	 * 
	 * The face and body LODs to be used are determined by the mapping arrays passed into 
	 * InitializeMetaHumanCharacterEditorActor.
	 * 
	 * If InForcedLOD is INDEX_NONE, the actor LOD is not forced and should be selected naturally,
	 * e.g. based on screen size.
	 */
	virtual void SetForcedLOD(int32 InForcedLOD) = 0;

	/** 
	 * Returns the Character passed into InitializeMetaHumanCharacterEditorActor.
	 * 
	 * May be called at any time after InitializeMetaHumanCharacterEditorActor.
	 */
	virtual TNotNull<UMetaHumanCharacter*> GetCharacter() const = 0;

	/** Returns the component that the Face mesh is assigned to */
	virtual TNotNull<const USkeletalMeshComponent*> GetFaceComponent() const = 0;

	/** Returns the component that the Body mesh is assigned to */
	virtual TNotNull<const USkeletalMeshComponent*> GetBodyComponent() const = 0;

	/** Follow and retarget from the pose of the driving skeletal mesh. */
	virtual void SetDrivingSkeletalMesh(USkeletalMeshComponent* DrivingSkelMeshComponent) = 0;

	/** 
	 * Will be called when the Face mesh is updated.
	 * 
	 * Implementers will need to call MarkRenderStateDirty and UpdateBounds on the component to 
	 * ensure any material or geometry changes are correctly applied.
	 */
	virtual void OnFaceMeshUpdated() = 0;

	/**
	 * Enable animation.
	 * This could be assigning an anim graph or a sequence onto the body and/or face.
	 */
	virtual void ReinitAnimation() = 0;

	/**
	 * Disable animation.
	 * Disconnects anim instances and reset the skeletal meshes to their reference pose.
	 **/
	virtual void ResetAnimation() = 0;

	/**
	 * Will be called when the Body mesh is updated.
	 * 
	 * Implementers will need to call MarkRenderStateDirty and UpdateBounds on the component to 
	 * ensure any material or geometry changes are correctly applied.
	 */
	virtual void OnBodyMeshUpdated() = 0;

	/** Updates Face Mesh with the new object when it is created in the character **/
	virtual void UpdateFaceComponentMesh(USkeletalMesh* InFaceMesh) = 0;

	/** Updates Body Mesh with the new object when it is created in the character **/
	virtual void UpdateBodyComponentMesh(USkeletalMesh* InBodyMesh) = 0;

	/** Any hair components on the actor should be set to the given visibility state */
	virtual void SetHairVisibilityState(EMetaHumanHairVisibilityState State) = 0;

	/** 
	 * Any clothing components should be set to the given visibility state.
	 * 
	 * If the state is UseOverrideMaterial, the provided material should be applied. 
	 * 
	 * The original materials should be restored on the next time the state is changed.
	 */
	virtual void SetClothingVisibilityState(EMetaHumanClothingVisibilityState State, UMaterialInterface* OverrideMaterial = nullptr) = 0;
};
