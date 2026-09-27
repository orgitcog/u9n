// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Memory/SharedBuffer.h"
#include "Templates/PimplPtr.h"
#include "Templates/SharedPointerFwd.h"
#include "Templates/Tuple.h"
#include "Containers/Array.h"
#include "MetaHumanCharacterIdentity.generated.h"

struct FMetaHumanRigEvaluatedState;

namespace dna {
	class Reader;
}

enum class EMetaHumanCharacterOrientation : uint8
{
	Y_UP = 0,
	Z_UP = 1
};

struct FFloatTriplet;

//! the alignment options used when performing FitToTarget
UENUM(BlueprintType)
enum class EAlignmentOptions : uint8
{
	None,
	Translation,
	RotationTranslation,
	ScalingTranslation,
	ScalingRotationTranslation
};

//! the options for performing fit to target: how alignment of the head is performed, and whether or not the neck is adapted to fit to the body
struct METAHUMANCORETECHLIB_API FFitToTargetOptions
{
	EAlignmentOptions AlignmentOptions{ EAlignmentOptions::ScalingRotationTranslation };
	bool bAdaptNeck = true;
	bool bDisableHighFrequencyDelta = true;
};

//! the options used when performing Blend
UENUM(BlueprintType)
enum class EBlendOptions : uint8
{
	Proportions,
	Features,
	Both
};

class METAHUMANCORETECHLIB_API FMetaHumanCharacterIdentity
{
public:
	FMetaHumanCharacterIdentity();
	~FMetaHumanCharacterIdentity();

	bool Init(const FString& InMHCDataPath, const FString& InBodyMHCDataPath, class UDNAAsset* InDNAAsset, EMetaHumanCharacterOrientation InDNAAssetOrient);

	class FState;

	TSharedPtr<FState> CreateState() const;

	class FSettings;


	/** Retrieve all available presets */
	TArray<FString> GetPresetNames() const;

	/** Copy joint bind poses from body to the face dna */
	TSharedPtr<class IDNAReader> CopyBodyJointsToFace(dna::Reader* InBodyDnaReader, dna::Reader* InFaceDnaReader) const;

	/** Update skin weights for the overlapping joints in the face from the body and vertex normals */
	TSharedPtr<class IDNAReader> UpdateFaceSkinWeightsFromBodyAndVertexNormals(const TArray<TPair<int32, TArray<FFloatTriplet>>>& InCombinedBodySkinWeights, dna::Reader* InFaceDnaReader, const FMetaHumanCharacterIdentity::FState& InState) const;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};

class METAHUMANCORETECHLIB_API FMetaHumanCharacterIdentity::FSettings
{
public:
	FSettings();
	~FSettings();
	FSettings(const FSettings& InOther);

	/** Return the global per vertex delta used when evaluating */
	float GlobalVertexDeltaScale() const;

	/** Set the global per vertex delta used when evaluating */
	void SetGlobalVertexDeltaScale(float InGlobalVertexDeltaScale);

	/** Return true if apply body delta when evaluating */
	bool UseBodyDeltaInEvaluation() const;

	/** Set whether or not we are applying body delta when evaluating */
	void SetBodyDeltaInEvaluation(bool bInIsBodyDeltaInEvaluation);

	/** Return the global scale used for applying high frequency variant */
	float GlobalHighFrequencyScale() const;

	/** Set the global scale used for applying high frequency variant */
	void SetGlobalHighFrequencyScale(float InGlobalHighFrequencyScale);

	/** Set the iterations used when applying high frequency variant */
	void SetHighFrequencyIteration(int32 InHighFrequencyScale);

	friend class FMetaHumanCharacterIdentity::FState;


private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};


class METAHUMANCORETECHLIB_API FMetaHumanCharacterIdentity::FState
{
public:
	FState();
	~FState();
	FState(const FState& InOther);

	/** Evaluate the DNA vertices and vertex normals based on the state */
	FMetaHumanRigEvaluatedState Evaluate() const;

	/** Get vertex in UE coordinate system for a specific dna mesh and dna vertex index */
	FVector3f GetVertex(const TArray<FVector3f>& InVertices, int32 InDNAMeshIndex, int32 InDNAVertexIndex) const;

	/** Get vertex in unconverted for a specific dna mesh and dna vertex index */
	FVector3f GetRawVertex(const TArray<FVector3f>& InVertices, int32 InDNAMeshIndex, int32 InDNAVertexIndex) const;

	/** Get the raw bind pose (in DNA coord system) */
	void GetRawBindPose(const TArray<FVector3f>& InVertices, TArray<float>& OutBindPose) const;

	/** Get the coefficients of the underlying model */
	void GetCoefficients(TArray<float>& OutCoefficients) const;

	/** Get the identifier of the underlying model */
	void GetModelIdentifier(FString& OutModelIdentifier) const;

	/** Evaluate the Gizmos */
	TArray<FVector3f> EvaluateGizmos(const TArray<FVector3f>& InVertices) const;

	/** Get the number of gizmos */
	int32 NumGizmos() const;
	
	/** Evaluate the Landmarks */
	TArray<FVector3f> EvaluateLandmarks(const TArray<FVector3f>& InVertices) const;

	/** get the number of landmarks */
	int32 NumLandmarks() const;

	/** Is there a landmark present for the supplied vertex index */
	bool HasLandmark(int32 InVertexIndex) const ;

	/** Adds a single landmark.  */
	void AddLandmark(int32 InVertexIndex);

	/** Removes a single landmark for a given landmark index. The landmark index must be in the range 0-NumLandmarks() - 1 */
	void RemoveLandmark(int32 InLandmarkIndex);

	/** Selects a face vertex given the input ray */
	int32 SelectFaceVertex(FVector3f InOrigin, FVector3f InDirection, FVector3f& OutVertex, FVector3f& OutNormal);

	/** Reset the face to the archetype */
	void Reset();

	/** Reset the neck region to the body state */
	void ResetNeckRegion();

	/** Randomize the face */
	void Randomize(float InMagnitude);

	/** Create a state based on preset */
	void GetPreset(const FString& PresetName, int32 InPresetType, int32 InPresetRegion);

	/** Blend region based on preset weights */
	void BlendPresets(int32 InGizmoIndex, const TArray<TPair<float, const FState*>>& InStates, EBlendOptions InBlendOptions, bool bInBlendSymmetrically);

	/** Set the gizmo position */
	void SetGizmoPosition(int32 InGizmoIndex, const FVector3f& InPosition, bool bInSymmetric, bool bInEnforceBounds);

	/** Get the gizmo position */
	void GetGizmoPosition(int32 InGizmoIndex, FVector3f& OutPosition) const;

	/** Get the gizmo position bounds */
	void GetGizmoPositionBounds(int32 InGizmoIndex, FVector3f& OutMinPosition, FVector3f& OutMaxPosition, float InBBoxReduction, bool bInExpandToCurrent) const;

	/** Set the gizmo rotation */
	void SetGizmoRotation(int32 InGizmoIndex, const FVector3f& InRotation, bool bInSymmetric, bool bInEnforceBounds);

	/** Get the gizmo rotation */
	void GetGizmoRotation(int32 InGizmoIndex, FVector3f& OutRotation) const;

	/** Get the gizmo rotation bounds */
	void GetGizmoRotationBounds(int32 InGizmoIndex, FVector3f& OutMinRotation, FVector3f& OutMaxRotation, bool bInExpandToCurrent) const;

	/** Scale the gizmo */
	void SetGizmoScale(int32 InGizmoIndex, float InScale, bool bInSymmetric, bool bInExpandToCurrent);

	/** Get the gizmo scale */
	void GetGizmoScale(int32 InGizmoIndex, float& OutScale) const;

	/** Get the gizmo scale bounds */
	void GetGizmoScaleBounds(int32 InGizmoIndex, float& OutMinScale, float& OutMaxScale, bool bInExpandToCurrent) const;

	/** Translate the landmarks */
	void TranslateLandmark(int32 InLandmarkIndex, const FVector3f& InDelta, bool bInSymmetric);

	/** Set the face scale relative to the body. */
	void SetFaceScale(float InFaceScale);

	/** Returns the face scale relative to the body. */
	float GetFaceScale() const;

	/* Update the face state from body (bind pose, vertices) */
	void SetBodyJointsAndBodyFaceVertices(const TArray<FMatrix44f>& InBodyJoints, const TArray<FVector3f>& InVertices);

	/* Set the body vertex normals, and an array giving the number of vertices for each lod */
	// TODO perhaps combined this with the method above
	void SetBodyVertexNormals(const TArray<FVector3f>& InVertexNormals, const TArray<int32>& InNumVerticesPerLod);

	/** Reset the neck exclusion mask */
	void ResetNeckExclusionMask();

	/** Returns the number of variants for variant of name InVariantName (can be "eyelashes" or "teeth") */
	int32 GetVariantsCount(const FString& InVariantName) const;

	/** Sets variant of name InVariantName  to State (can be "eyelashes" or "teeth")  **/
	void SetVariant(const FString& InVariantName, TConstArrayView<float> InVariantWeights);

	/** Set the expression activations for the face state to those defined in the InExpressionActivations map*/	
	void SetExpressionActivations(TMap<FString, float>& InExpressionActivations);

	/** Returns the maximum number of High Frequency variants supported by the state */
	int32 GetNumHighFrequencyVariants() const;

	/** Set the high frequency variant to be used by this state. Set <0 for no variant. */
	void SetHighFrequenctVariant(int32 InHighFrequencyVariant);

	/** Returns the high frequency variant used by this state. */
	int32 GetHighFrequenctVariant() const;


	/** 
	 * Fit the Character Identity to the map of supplied part vertices (which must contain the Head, but also optionally can contain Eyes and Teeth), using the supplied options.
	 * Note that this leaves the Identity in a state where it needs autorigging. Returns true if successful, false if not
	 */
	bool FitToTarget(const TMap<int32, TArray<FVector3f>>& InPartsVertices, const FFitToTargetOptions& InFitToTargetOptions);

	/** 
	 * Fit the Character Identity to the supplied DNA, using the supplied options.
	 * Note that this leaves the Identity in a state where it needs autorigging. Returns true if successful, false otherwise (for example if the DNA selected is not appropriate)
	 */
	bool FitToFaceDna(TSharedRef<class IDNAReader> InFaceDna, const FFitToTargetOptions& InFitToTargetOptions); 

	/** get settings */
	FMetaHumanCharacterIdentity::FSettings GetSettings() const;

	/** set settings */
	void SetSettings(const FMetaHumanCharacterIdentity::FSettings& InSettings);

	/** get global scale */
	bool GetGlobalScale(float& scale) const;

	void WriteDebugAutoriggingData(const FString& DirectoryPath) const;

	void Serialize(FSharedBuffer& OutArchive) const;
	bool Deserialize(const FSharedBuffer& InArchive);

	TSharedRef<class IDNAReader> StateToDna(dna::Reader* InDnaReader) const;
	TSharedRef<class IDNAReader> StateToDna(class UDNAAsset* InFaceDNA) const;

	friend class FMetaHumanCharacterIdentity;

private:
	struct FImpl;
	TPimplPtr<FImpl> Impl;
};

