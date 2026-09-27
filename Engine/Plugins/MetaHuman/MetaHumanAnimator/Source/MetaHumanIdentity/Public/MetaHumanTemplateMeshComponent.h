// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"

#include "MetaHumanTemplateMeshComponent.generated.h"

enum class EIdentityPoseType : uint8;

enum class ETemplateVertexConversion : uint8
{
	ConformerToUE,
	UEToConformer,
	None
};

/**
 * Component to manages different aspects of the MetaHuman Identity Template Mesh.
 * This component makes use of Dynamic Meshes to store and display data.
 * It can be used to store one mesh for each supported pose type, currently these are Neutral and Teeth.
 * ShowHeadMeshForPose can be used to change which is currently active Head Mesh being displayed
 */
UCLASS()
class METAHUMANIDENTITY_API UMetaHumanTemplateMeshComponent
	: public UPrimitiveComponent
{
	GENERATED_BODY()

public:

	UMetaHumanTemplateMeshComponent();

	//~Begin UActorComponent Interface
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnVisibilityChanged() override;
	//~End UActorComponent Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif
	//~End UActorComponent Interface

	//~Begin USceneComponent interface
	virtual FBoxSphereBounds CalcBounds(const FTransform& InLocalToWorld) const override;
	//~End USceneComponent interface

public:
	/** Load the mesh assets used by the template */
	void LoadMeshAssets();

	/** Set the override materials for template component meshes. Only needs to happen once */
	void LoadMaterialsForMeshes();

	/** Returns the dynamic mesh object for the given pose type */
	class UDynamicMesh* GetPoseHeadMesh(EIdentityPoseType InPoseType) const;

	/** Set the currently active head pose mesh */
	void ShowHeadMeshForPose(EIdentityPoseType InPoseType) const;

	/** Set the pose head mesh from the given array of vertices. The conversion type is used to perform conversions between the different spaces MetaHuman Identity operates in */
	void SetPoseHeadMeshVertices(EIdentityPoseType InPoseType, TConstArrayView<FVector3f> InNewVertices, ETemplateVertexConversion InConversionType) const;

	/** Returns the vertices for the head mesh of the given pose type. The conversion type can be used to convert from UE space to rig space, which is the space expected by the autorigging service for example */
	void GetPoseHeadMeshVertices(EIdentityPoseType InPoseType, const FTransform& InTransform, ETemplateVertexConversion InConversionType, TArray<FVector>& OutPoseHeadVertices) const;

	/** Returns the vertices for both left and right eyes. A transform can be passed in to be baked in the vertices before doing the conversion to the requested space */
	void GetEyeMeshesVertices(const FTransform& InTransform, ETemplateVertexConversion InConversionType, TArray<FVector>& OutLeftEyeVertices, TArray<FVector>& OutRightEyeVertices) const;

	/** Returns the vertices for teeth mesh. A transform can be passed in to be baked in the vertices before doing the conversion to the requested space */
	void GetTeethMeshVertices(const FTransform& InTransform, ETemplateVertexConversion InConversionType, TArray<FVector>& OutTeethVertices) const;

	/** Set the vertices for both left and right eyes. The conversion type can be used to convert from rig to UE space or vice versa */
	void SetEyeMeshesVertices(TConstArrayView<FVector3f> InLeftEyeVertices, TConstArrayView<FVector3f> InRightEyeVertices, ETemplateVertexConversion InConversionType);

	/** Updates the eye meshes visibility */
	void SetEyeMeshesVisibility(bool bInVisible);

	/** Updates the teeth mesh visibility */
	void SetTeethMeshVisibility(bool bInVisible);

	/** Bake a transform into the vertices of the eye meshes */
	void BakeEyeMeshesTransform(const FTransform& InTransform);

	/** Set the vertices of the teeth mesh */
	void SetTeethMeshVertices(TConstArrayView<FVector3f> InNewVertices, ETemplateVertexConversion InConversionType) const;

	/** Bake a transform into the vertices of the teeth meshes */
	void BakeTeethMeshTransform(const FTransform& InTransform);

	/** Resets the template mesh */
	void ResetMeshes();

	/** Convert a vector from one coordinate system to another */
	static FVector ConvertVertex(const FVector& InVertex, ETemplateVertexConversion InConversionType);

public:

	/** Transform that can be used with the Bake functions to align the meshes with conformed data */
	static FTransform UEToRigSpaceTransform;

public:

	/** Whether or not the fitted teeth mesh is active */
	UPROPERTY(EditAnywhere, Category = "Preview")
	uint8 bShowFittedTeeth : 1;

	/** Whether or not to show the eye meshes */
	UPROPERTY(EditAnywhere, Category = "Preview")
	uint8 bShowEyes : 1;

	/** Whether or not to show the teeth mesh */
	UPROPERTY(EditAnywhere, Category = "Preview")
	uint8 bShowTeethMesh : 1;

	/** The component that displays the currently active head mesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMeshComponent> HeadMeshComponent;

	/** Stores the dynamic meshes for each support pose. ShowHeadMeshForPose will select a mesh from this map */
	UPROPERTY()
	TMap<EIdentityPoseType, TObjectPtr<class UDynamicMesh>> PoseHeadMeshes;

	/** The component that displays the currently active teeth mesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMeshComponent> TeethMeshComponent;

	/** The component that displays the left eye mesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMeshComponent> LeftEyeComponent;

	/** The component that displays the right eye mesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMeshComponent> RightEyeComponent;

	/** Keeps the teeth mesh before fitting. Changing bShowFittedTeeth will toggle between this and FittedTeethMesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMesh> OriginalTeethMesh;

	/** Stores the fitted teeth mesh. Changing bShowFittedTeeth will toggle between this and OriginalTeethMesh */
	UPROPERTY()
	TObjectPtr<class UDynamicMesh> FittedTeethMesh;

	/** Delegate called any time one of the mesh components is modified. This is mainly used to update component instances */
	FSimpleMulticastDelegate OnTemplateMeshChanged;
};