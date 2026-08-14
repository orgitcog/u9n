// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "ChaosClothAsset/SimulationBaseConfigNode.h"
#include "SimulationCollisionConfigNode.generated.h"

/** Physics mesh collision properties configuration node. */
PRAGMA_DISABLE_DEPRECATION_WARNINGS
USTRUCT(Meta = (DataflowCloth))
struct FChaosClothAssetSimulationCollisionConfigNode : public FChaosClothAssetSimulationBaseConfigNode
{
	GENERATED_BODY()
PRAGMA_ENABLE_DEPRECATION_WARNINGS
	DATAFLOW_NODE_DEFINE_INTERNAL(FChaosClothAssetSimulationCollisionConfigNode, "SimulationCollisionConfig", "Cloth", "Cloth Simulation Collision Config")

public:
	/** The added thickness of collision shapes. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", DisplayName = "Collision Thickness", Meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000", InteractorName = "CollisionThickness"))
	FChaosClothAssetImportedFloatValue CollisionThicknessImported = {UE::Chaos::ClothAsset::FDefaultFabric::CollisionThickness};

	/** Friction coefficient for cloth - collider interaction. Currently only Skinned Triangle Meshes use the weighted value. All other collisions only use the Low value. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", DisplayName = "Friction Coefficient", Meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "10", InteractorName = "FrictionCoefficient"))
	FChaosClothAssetWeightedValue FrictionCoefficientWeighted = 
	{ true, UE::Chaos::ClothAsset::FDefaultFabric::Friction, UE::Chaos::ClothAsset::FDefaultFabric::Friction, TEXT("FrictionCoefficient"), true };

	/** Enable colliding against any simple (e.g., capsules, convexes, spheres, boxes) colliders..*/
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (InteractorName = "EnableSimpleColliders"))
	bool bEnableSimpleColliders = true;

	/** Use Planar Constraints for simple (e.g., capsules, convexes, spheres, boxes) colliders when doing multiple iterations. Planar constraints are cheaper than full collision detection, but less accurate. */
	UPROPERTY(EditAnywhere, Category="Collision Properties", Meta = (EditCondition = "bEnableSimpleColliders", InteractorName = "UsePlanarConstraintForSimpleColliders"))
	bool bUsePlanarConstraintForSimpleColliders = false;

	/** Enable colliding against any complex (e.g., SkinnedLevelSet, MLLevelSet) colliders.*/
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (InteractorName = "EnableComplexColliders"))
	bool bEnableComplexColliders = true;

	/** Use Planar Constraints for complex (e.g., SkinnedLevelSet, MLLevelSet) colliders when doing multiple iterations. Planar constraints are cheaper than full collision detection, but less accurate. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (EditCondition = "bEnableComplexColliders", InteractorName = "UsePlanarConstraintForComplexColliders"))
	bool bUsePlanarConstraintForComplexColliders = true;

	/** Enable colliding against any Skinned Triangle Mesh colliders.*/
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (InteractorName = "EnableSkinnedTriangleMeshCollisions"))
	bool bEnableSkinnedTriangleMeshCollisions = true;

	/** Use 'NumSelfCollisionSubsteps' (Located on SimulationSolverConfig) to also control Skinned Triangle Mesh collision updates */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (EditCondition = "bEnableSkinnedTriangleMeshCollisions", InteractorName = "UseSelfCollisionSubstepsForSkinnedTriangleMeshes"))
	bool bUseSelfCollisionSubstepsForSkinnedTriangleMeshes = true;

	/** Thickness added to the cloth when colliding against collision shapes. Currently only Skinned Triangle Meshes use the weighted value. All other collisions only use the Low value. */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (InteractorName = "SoftBodyCollisionThickness"))
	FChaosClothAssetWeightedValue ClothCollisionThickness = { true, UE::Chaos::ClothAsset::FDefaultFabric::ClothCollisionThickness,
		UE::Chaos::ClothAsset::FDefaultFabric::ClothCollisionThickness, TEXT("ClothCollisionThickness"), true };

	/** Stiffness for proximity repulsion forces (Force-based solver only). Units = kg cm/ s^2 (same as XPBD springs)*/
	UPROPERTY(EditAnywhere, Category = "Proximity Force Properties", Meta = (UIMin = "0", UIMax = "10000", ClampMin = "0", ClampMax = "10000000", InteractorName = "ProximityStiffness"))
	float ProximityStiffness = 100.f;

	/**
	 * Use continuous collision detection (CCD) to prevent any missed collisions between fast moving particles and colliders.
	 * This has a negative effect on performance compared to when resolving collision without using CCD.
	 */
	UPROPERTY(EditAnywhere, Category = "Collision Properties", Meta = (InteractorName = "UseCCD"))
	bool bUseCCD = false;


	UE_DEPRECATED(5.6, "Use FrictionCoefficientWeighted instead.")
	UPROPERTY()
	FChaosClothAssetImportedFloatValue FrictionCoefficientImported = { UE::Chaos::ClothAsset::FDefaultFabric::Friction };

	FChaosClothAssetSimulationCollisionConfigNode(const UE::Dataflow::FNodeParameters& InParam, FGuid InGuid = FGuid::NewGuid());

	virtual void Serialize(FArchive& Ar) override;

private:
	virtual void AddProperties(FPropertyHelper& PropertyHelper) const override;

	// Deprecated properties
#if WITH_EDITORONLY_DATA
	static constexpr float CollisionThicknessDeprecatedDefault = 1.0f;
	UPROPERTY()
	float CollisionThickness_DEPRECATED = CollisionThicknessDeprecatedDefault;

	static constexpr float FrictionCoefficientDeprecatedDefault = 0.8f;
	UPROPERTY()
	float FrictionCoefficient_DEPRECATED  = FrictionCoefficientDeprecatedDefault;
#endif
};
