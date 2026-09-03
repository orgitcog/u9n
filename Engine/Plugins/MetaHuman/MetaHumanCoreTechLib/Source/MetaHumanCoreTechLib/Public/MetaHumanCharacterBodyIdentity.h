// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DNAAsset.h"
#include "MetaHumanBodyType.h"
#include "Memory/SharedBuffer.h"
#include "MetaHumanCharacterBodyIdentity.generated.h"

struct FMetaHumanRigEvaluatedState;

UENUM(BlueprintType)
enum class EBodyBlendOptions : uint8
{
	Skeleton UMETA(Tooltip="Blends only skeletal proportions, enabling proportion changes without altering shaping"),
	Shape UMETA(Tooltip="Blends only shaping, allowing adjustments without affecting skeletal proportions"),
	Both UMETA(Tooltip="Blends both skeletal proportions and shaping simultaneously"),
};


UENUM(BlueprintType)
enum class EMetaHumanCharacterBodyFitOptions : uint8
{
	FitFromMeshOnly				UMETA(Tooltip="Uses mesh only from the DNA file"),
	FitFromMeshAndSkeleton		UMETA(Tooltip="Uses mesh and core (animation) skeleton from the DNA file"),
	FitFromMeshToFixedSkeleton	UMETA(Tooltip="Uses mesh from the DNA file and the core (animation) skeleton from the current MHC state")
};

class METAHUMANCORETECHLIB_API FMetaHumanCharacterBodyIdentity
{
public:
	FMetaHumanCharacterBodyIdentity();
	~FMetaHumanCharacterBodyIdentity();

	bool Init(const FString& InPCAModelPath, const FString& InLegacyBodiesPath);

	class FState;
	TSharedPtr<FState> CreateState() const;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};

struct METAHUMANCORETECHLIB_API FMetaHumanCharacterBodyConstraint
{
	FName Name;
	bool bIsActive = false;	
	float TargetMeasurement = 100.0f;
	float MinMeasurement = 50.0f;
	float MaxMeasurement = 50.0f;
};

struct METAHUMANCORETECHLIB_API PhysicsBodyVolume
{
	FVector Center;
	FVector Extent;
};

//! a Simple struct representing an Eigen::Triplet in UE types which can be mem copied from Eigen::Triplet<float>
struct METAHUMANCORETECHLIB_API FFloatTriplet
{
	int32 Row;
	int32 Col;
	float Value;
};

class METAHUMANCORETECHLIB_API FMetaHumanCharacterBodyIdentity::FState
{
public:
	FState();
	~FState();
	FState(const FState& InOther);

	/** Get the body constraints from the model */
	TArray<FMetaHumanCharacterBodyConstraint> GetBodyConstraints();

	/** Set the body constraints and evaluate the DNA vertices based on the state */
	void EvaluateBodyConstraints(const TArray<FMetaHumanCharacterBodyConstraint>& BodyConstraints);

	/* Get the DNA vertices and vertex normals from the state */
	FMetaHumanRigEvaluatedState GetVerticesAndVertexNormals() const;

	/* Get the number of vertices per LOD */
	TArray<int32> GetNumVerticesPerLOD() const;

	/** Get vertex in UE coordinate system for a specific dna mesh and dna vertex index */
	FVector3f GetVertex(const TArray<FVector3f>& InVertices, int32 InDNAMeshIndex, int32 InDNAVertexIndex) const;

	/** Get gizmo positions used for blending regions */
	TArray<FVector3f> GetRegionGizmos() const;

	/** Blend region based on preset weights */
	void BlendPresets(int32 InGizmoIndex, const TArray<TPair<float, const FState*>>& InStates, EBodyBlendOptions InBodyBlendOptions);

	/** Get the number of constraints from the model */
	int32 GetNumberOfConstraints() const;

	/* Get the actual measurement on the mesh for a particular constraint */
	float GetMeasurement(int32 ConstraintIndex) const;

	/** Obtains measurements map (string to float) for given face and body DNAs */
	void GetMeasurementsForFaceAndBody(TSharedRef<IDNAReader> InFaceDNA, TSharedRef<IDNAReader> InBodyDNA, TMap<FString, float>& OutMeasurements) const;

	/* Get the contour vertex positions on the mesh for a particular constraint */
	TArray<FVector> GetContourVertices(int32 ConstraintIndex) const;

	/* Copy the bind pose transforms */
	TArray<FMatrix44f> CopyBindPose() const;

	int32 GetNumberOfJoints() const;
	void GetNeutralJointTransform(int32 JointIndex, FVector3f& OutJointTranslation, FRotator3f& OutJointRotation) const;

	/* Copy the combined body model skinning weights as an array of triplets which can be used to reconstruct a sparse matrix of skinning weights*/
	void CopyCombinedModelVertexInfluenceWeights(TArray<TPair<int32, TArray<FFloatTriplet>>> & OutCombinedModelVertexInfluenceWeights) const;

	/** Reset the body to the archetype */
	void Reset();

	/** Get MetaHuman body type */
	EMetaHumanBodyType GetMetaHumanBodyType() const;

	/** Set MetaHuman body type */
	void SetMetaHumanBodyType(EMetaHumanBodyType InMetaHumanBodyType, bool bFitFromLegacy = false);

	/* Fit the Character to the supplied DNA */ 
	bool FitToBodyDna(TSharedRef<class IDNAReader> InBodyDna, EMetaHumanCharacterBodyFitOptions InBodyFitOptions);

	/* Fit the Character to the supplied vertices */ 
	bool FitToTarget(const TArray<FVector3f>& InVertices, const TArray<FVector3f>& InComponentJointTranslations, EMetaHumanCharacterBodyFitOptions InBodyFitOptions);

	/* Get and set the body vertex and joint global delta scale */
	float GetGlobalDeltaScale() const;
	void SetGlobalDeltaScale(float InVertexDelta);

	/** Serialize/Deserialize */
	bool Serialize(FSharedBuffer& OutArchive) const;
	bool Deserialize(const FSharedBuffer& InArchive);

	/** Create updated dna from state */
	TSharedRef<IDNAReader> StateToDna(dna::Reader* InDnaReader) const;

	TSharedRef<IDNAReader> StateToDna(UDNAAsset* InBodyDna) const;

	/* Get the list of physics volumes for a joint */
	TArray<PhysicsBodyVolume> GetPhysicsBodyVolumes(const FName& InJointName) const;

	friend class FMetaHumanCharacterBodyIdentity;
	friend class FMetaHumanCharacterIdentity;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};

