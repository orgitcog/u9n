// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Misc/EnumRange.h"
#include "Templates/SubclassOf.h"
#include "Textures/SlateIcon.h"
#include "Components/DynamicMeshComponent.h"
#include "MetaHumanPredictiveSolversTask.h"
#include "Serialization/EditorBulkData.h"
#include "CameraCalibration.h"
#include "DepthMapDiagnosticsResult.h"

#include "MetaHumanIdentityParts.generated.h"

enum class EIdentityErrorCode : uint8;

enum class EIdentityPartMeshes : uint8
{
	Invalid,
	Head,
	LeftEye,
	RightEye,
	Teeth
};

UENUM()
enum class ETargetTemplateCompatibility : uint8
{
	Valid = 0,
	InvalidInputMesh,				// Input mesh is null or of wrong type
	MissingImportModel,				// Input mesh does not contain import data
	MissingLOD,
	MissingMeshInfo,
	MismatchNumVertices,
	MismatchStartImportedVertex,
	InvalidArchetype,				// Status for all errors with the archetype, should never occur

	Count UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ETargetTemplateCompatibility, ETargetTemplateCompatibility::Count);

/////////////////////////////////////////////////////
// UMetaHumanIdentityPart

/**
 * The base class for any Part that can be added to a MetaHumanIdentity
 */
UCLASS(Abstract, BlueprintType)
class METAHUMANIDENTITY_API UMetaHumanIdentityPart
	: public UObject
{
	GENERATED_BODY()

public:
	/** Perform any initialization required after the Part is created */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Parts")
	virtual void Initialize() PURE_VIRTUAL(UMetaHumanIdentityPart::Initialize,);

	/** Returns the part name */
	virtual FText GetPartName() const PURE_VIRTUAL(UMetaHumanIdentityPart::GetPartName, return {};);

	/** Returns a short description of the part */
	virtual FText GetPartDescription() const PURE_VIRTUAL(UMetaHumanIdentityPart::GetPartDescription, return {};);

	/** Returns the icon for the part. This can optionally return an icon for the given InPropertyName  */
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const PURE_VIRTUAL(UMetaHumanIdentityPart::GetPartIcon, return {};);

	/** Returns the tooltip for the part. This can optionally return a tooltip for the given InPropertyName  */
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const PURE_VIRTUAL(UMetaHumanIdentityPart::GetPartTooltip, return {};);

	/** Returns true if diagnostics indicates an issue processing this part, and also passes back a warning message if a diagnostic issue has been found */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Diagnostics")
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const PURE_VIRTUAL(UMetaHumanIdentityPart::DiagnosticsIndicatesProcessingIssue, return {};);
};

////////////////////////////////////////////////////
// UMetaHumanIdentityFace

namespace UE::Wrappers
{
	class FMetaHumanConformer;
}

UENUM()
enum class EConformType
{
	/** Use the Face Fitting conformer, i.e. FitIdentity */
	Solve,

	/**
	 * Copy the data from the Neutral Pose face mesh to the Template Mesh.
	 * Assumes the target mesh is already conformed and in the correct topology
	 * expected by the Mesh To MetaHuman service 
	 */
	Copy,
};

UCLASS(HideCategories = ("Preview"))
class METAHUMANIDENTITY_API UMetaHumanIdentityFace
	: public UMetaHumanIdentityPart
{
	GENERATED_BODY()

public:
	UMetaHumanIdentityFace();

	//~UMetaHumanIdentityFace Interface
	virtual void Initialize() override;
	virtual FText GetPartName() const override;
	virtual FText GetPartDescription() const override;
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const override;
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const override;
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const override;

	//~UObject Interface
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent) override;
#endif

	/** Return true if the face has all the required information to run the MetaHuman Identity solve (conforming) */
	bool CanConform() const;

	/** Return true if the face can be submitted to the AutoRigging service, which means is already conformed and the Neutral Pose has a valid Capture Data */
	bool CanSubmitToAutorigging() const;

	/** MetaHuman Identity solve */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Solve")
	EIdentityErrorCode Conform(EConformType InConformType = EConformType::Solve);

	/** Returns true if the conformal rig component is valid and points to a valid skeletal mesh */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Rig")
	bool IsConformalRigValid() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Export")
	void ExportTemplateMesh(const FString& InPath, const FString& InAssetName);

#if WITH_EDITOR

	/** Resets face part to its initial state and optionally creates new rig component. */
	void ResetRigComponent(bool bInCreateNewRigComponent = true);

	/** Reset the template mesh */
	void ResetTemplateMesh();

	/** TODO: Come up with good explanation */
	void CopyMeshVerticesFromExistingMesh(class UCaptureData* CaptureData);

	/** Converts ETargetTemplateCompatibility to a string */
	static FString TargetTemplateCompatibilityAsString(ETargetTemplateCompatibility InCompatibility);
	
	/** Helper function to evaluate the compatibility of a given mesh to be used as template */ 
	static ETargetTemplateCompatibility CheckTargetTemplateMesh(const UObject* InAsset);

	/** Returns the DNA Reader for the plugin archetype DNA */
	static TSharedPtr<class IDNAReader> GetPluginArchetypeDNAReader();

	/**
	 * Apply a DNA to the Rig
	 * Depending on the level of detail and usage (e.g.only LOD0 has blend shapes), these options can be turned off to save time/memory
	 */
	EIdentityErrorCode ApplyDNAToRig(TSharedPtr<class IDNAReader> InDNAReader, bool bInUpdateBlendShapes = true, bool bInUpdateSkinWeights = true);

	/**
	 * Populates predictive solver task config.
	 * Returns true if it was populated successfully, false otherwise.
	 */
	bool GetPredictiveSolversTaskConfig(FPredictiveSolversTaskConfig& OutConfig) const;

	/**
	 * Runs predictive solvers training externally (through python script or UE editor).
	 * Returns true if the process was successful, false otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Face")
	bool RunPredictiveSolverTraining();

	/**
	 * Runs predictive solvers training asynchronously.
	 * Returns true if task was successfully scheduled, false otherwise.
	 */
	bool RunAsyncPredictiveSolverTraining(FOnPredictiveSolversProgress InOnProgressCallback, FOnPredictiveSolversCompleted InOnCompletedCallback);
	
	/**
	 * Returns true if predictive solver training is in progress, false otherwise.
	 */
	bool IsAsyncPredictiveSolverTrainingActive() const;

	/**
	 * Returns true if predictive solver training is in cancelling phase, false otherwise.
	 */
	bool IsAsyncPredictiveSolverTrainingCancelling() const;

	/**
	 * Cancels active solver training, if any.
	 */
	void CancelAsyncPredictiveSolverTraining();

	/**
	 * Poll active solver training progress. Output progress range is [0..1].
	 * Returns true if successful (task is active), false otherwise.
	 */
	bool PollAsyncPredictiveSolverTrainingProgress(float& OutProgress);

	/**
	 * Apply a combined DNA. If debugging is turned on, the DNA is saved;
	 */
	EIdentityErrorCode ApplyCombinedDNAToRig(TSharedPtr<class IDNAReader> InDNAReader);

	/** Returns true if provided DNA is compatible with the Face archetype, false otherwise */
	bool CheckDNACompatible(class IDNAReader* InDNAReader) const;

	/** Returns true if provided DNA is compatible with the Face archetype, false otherwise. It also outputs message listing differences between DNAs */
	bool CheckDNACompatible(class IDNAReader* InDNAReader, FString& OutCompatibilityMsg) const;

	/** Returns true if the face rig component is compatible with the Face archetype, false otherwise */
	bool CheckRigCompatible() const;

	/** Returns true if the face rig component is compatible with the Face archetype, false otherwise. It also outputs message listing differences between DNAs */
	bool CheckRigCompatible(FString& OutCompatibilityMsg) const;

#endif

	/** Finds a Pose of given type. Returns nullptr if one is not found. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Poses")
	UMetaHumanIdentityPose* FindPoseByType(EIdentityPoseType InPoseType) const;

	/** Adds the given pose to this face. Does nothing if a pose of the same type already exists */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Poses")
	void AddPoseOfType(EIdentityPoseType InPoseType, UMetaHumanIdentityPose* InPose);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Poses")
	bool RemovePose(UMetaHumanIdentityPose* InPose);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Poses")
	const TArray<UMetaHumanIdentityPose*>& GetPoses() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Data")
	bool HasDNABuffer() const;

	UFUNCTION(BlueprintCallable, Category = "MetaHuman|Data")
	bool HasPredictiveSolvers() const;

	/** Set the head mesh for the given pose type to be the active head mesh in the template mesh component */
	void ShowHeadMeshForPose(EIdentityPoseType InPoseType);

	/** Returns the head alignment transform for frontal promoted frame of the Neutral Pose or the MetaHuman Identity transform if there is no frontal frame */
	FTransform GetFrontalViewFrameTransform() const;

	/** Sets the transform for the conformal mesh and notifies subscribers */
	void SetTemplateMeshTransform(const FTransform& InTransform, bool bInUpdateRigTransform = false);

	/** Reset the transform the template mesh to its default value */
	void ResetTemplateMeshTransform();

	/** Return the vertices of the conformed mesh transformed to the space required by the autorigging backend, for face mesh, left eye mesh and right eye mesh 
	if eyes have been fitted. Note that teeth are not available at this stage as we have the neutral pose only and the teeth have not been fitted yet. */
	void GetConformalVerticesForAutoRigging(TArray<FVector>& OutConformedFaceVertices, 
											TArray<FVector>& OutConformedLeftEyeVertices, 
											TArray<FVector>& OutConformedRightEyeVertices) const;

	/** Return world position of conformal face mesh vertices */
	TMap<EIdentityPartMeshes, TArray<FVector>> GetConformalVerticesWorldPos(EIdentityPoseType InPoseType) const;

	/** Return conformal face mesh vertices for a given transform */
	TMap<EIdentityPartMeshes, TArray<FVector>> GetConformalVerticesForTransform(const FTransform& InMeshTransform, EIdentityPoseType InPoseType) const;

	/** Returns whether or not teeth can be conformed */
	bool CanFitTeeth() const;

	bool HasValidPromotedFramesForPose(EIdentityPoseType InPoseType) const;

	/** Fit the teeth to RawCombinedDNABuffer to generate final DNABuffer */
	EIdentityErrorCode FitTeeth();

	/** Finalize changes after DNA has been altered (updates skelmesh and create PCA model using DNABuffer) */
	EIdentityErrorCode Finalize();

	/** Exports DNA Buffer and Brows data to specified files */
	bool ExportDNADataToFiles(const FString& InDnaPathWithName, const FString& InBrowsPathWithName);

	/** Functions to store and retrieve bulk data */

	void SetRawDNABuffer(TConstArrayView<uint8> InRawDNABuffer);
	TArray<uint8> GetRawDNABuffer() const;
	bool HasRawDNABuffer() const;
	void ClearRawDNABuffer();

	void SetRawDeltaDNABuffer(TConstArrayView<uint8> InRawDeltaDNABuffer);
	TArray<uint8> GetRawDeltaDNABuffer() const;
	bool HasRawDeltaDNABuffer() const;
	void ClearRawDeltaDNABuffer();

	void SetCombinedDNABuffer(TConstArrayView<uint8> InRawCombinedDNABuffer);
	TArray<uint8> GetCombinedDNABuffer() const;
	bool HasCombinedDNABuffer() const;
	void ClearCombinedDNABuffer();

	void SetDNABuffer(TConstArrayView<uint8> InDNABuffer);
	TArray<uint8> GetDNABuffer() const;
	void ClearDNABuffer();

	void SetPCARig(TConstArrayView<uint8> InPCARig);
	TArray<uint8> GetPCARig() const;
	bool HasPCARig() const;
	void ClearPCARig();

	void SetBrowsBuffer(TConstArrayView<uint8> InBrowsBuffer);
	TArray<uint8> GetBrowsBuffer() const;
	bool HasBrowsBuffer() const;
	void ClearBrowsBuffer();

	void SetPredictiveSolvers(TConstArrayView<uint8> InPredictiveSolvers);
	TArray<uint8> GetPredictiveSolvers() const;
	void ClearPredictiveSolvers();

	void SetPredictiveWithoutTeethSolver(TConstArrayView<uint8> InPredictiveWithoutTeethSolver);
	TArray<uint8> GetPredictiveWithoutTeethSolver() const;
	bool HasPredictiveWithoutTeethSolver() const;
	void ClearPredictiveWithoutTeethSolver();

	/** Get the camera calibrations associated with the provided pose and frame index, and also return the full camera name for that pose and frame*/
	TArray<FCameraCalibration> GetCalibrationsForPoseAndFrame(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame) const;

	/** Get the full camera name associated with the provided pose and frame index, given a base camera name which may be empty for M2MH, or a RGB or depth camera name for F2MH */
	FString GetFullCameraName(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame, const FString& InBaseCameraName) const;

public:

	/** The default solver */
	UPROPERTY(EditAnywhere, Category = "Solver")
	TObjectPtr<class UMetaHumanFaceFittingSolver> DefaultSolver;

	/** The template mesh component for the face. Manages the meshes that represent each pose as well as eyes and teeth */
	UPROPERTY(VisibleAnywhere, Category = "Preview", DisplayName = "Template Mesh")
	TObjectPtr<class UMetaHumanTemplateMeshComponent> TemplateMeshComponent;

	/** The result of the auto-rigging process. This is the conformal mesh with a proper rig able to control the face */
	UPROPERTY(VisibleAnywhere, Category = "Preview", DisplayName = "Skeletal Mesh")
	TObjectPtr<class USkeletalMeshComponent> RigComponent;

	/** True if this face was conformed at least once */
	UPROPERTY(VisibleAnywhere, Category = "Output", AdvancedDisplay)
	uint8 bIsConformed : 1;

	/** True if this face has autorigged DNA applied either through AutoRig Service or if it was imported manually through Import functionality */
	UPROPERTY(VisibleAnywhere, Category = "Output", AdvancedDisplay, NonTransactional)
	uint8 bIsAutoRigged : 1;

	/** True if data-driven eyes was used during mesh conformation */
	UPROPERTY(VisibleAnywhere, Category = "Output", AdvancedDisplay)
	uint8 bHasFittedEyes : 1;

	// The following properties shouldn't be included in the undo history as they are too big for the buffer
	// TODO: Make them into a separate object and use lazy loading to improve the loading times of the Identity

	/** Holds the DNAToScan transform as returned from the autorigging service */
	UPROPERTY(NonTransactional, meta = (DeprecatedProperty, DeprecationMessage = "The new autorigging service doesn't provide DNAToScan transform matrix"))
	FTransform DNAToScanTransform_DEPRECATED;

	UPROPERTY(VisibleAnywhere, Category = "DNA", AdvancedDisplay, NonTransactional);
	bool bShouldUpdateRigComponent = false;

	/** Holds the DNA Pivot as returned from the autorigging service */
	UPROPERTY(NonTransactional, meta = (DeprecatedProperty, DeprecationMessage = "The new autorigging service doesn't provide DNA Pivot"))
	FVector DNAPivot_DEPRECATED;

	/** Holds the DNA Scale as returned from the autorigging service */
	UPROPERTY(NonTransactional, meta = (DeprecatedProperty, DeprecationMessage = "The new autorigging service doesn't provide DNATo Scale"))
	float DNAScale_DEPRECATED = 1.0f;

	/* Flag indicating whether processing diagnostics should be calculated during identity creation */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics")
	bool bSkipDiagnostics = false;

	/* The maximum percentage difference an autorigged face result is allowed to differ from an average MetaHuman. Above this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (UIMin = 0.0, ClampMin = 0.0, ClampMax = 200.0))
	float MaximumScaleDifferenceFromAverage = 25.0f;

	/* The minimum percentage of the face region which should have valid depth-map pixels. Below this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (UIMin = 0.0, UIMax = 100.0, ClampMin = 0.0, ClampMax = 100.0))
	float MinimumDepthMapFaceCoverage = 80.0f;

	/* The minimum required width of the face region on the depth-map in pixels. Below this value a diagnostic warning will be flagged. */
	UPROPERTY(EditAnywhere, Category = "Processing Diagnostics", meta = (ClampMin = 0.0, ClampMax = 10000.0))
	float MinimumDepthMapFaceWidth = 120.0f;

private:

	/** Holds the raw dna file as returned from the autorigging service */
	UE::Serialization::FEditorBulkData RawDNABufferBulkData;

	/** Holds the raw delta dna file as returned from the autorigging service */
	UE::Serialization::FEditorBulkData RawDeltaDNABufferBulkData;

	/** Holds the combined raw dna and delta dna file as returned from the autorigging service */
	UE::Serialization::FEditorBulkData RawCombinedDNABufferBulkData;

	/** Holds the final dna (RawCombinedDNABuffer with teeth fitting modifications). This is also stored as a DNAAsset in the SkelMesh. */
	UE::Serialization::FEditorBulkData DNABufferBulkData;

	/** Holds the PCA model of DNABuffer */
	UE::Serialization::FEditorBulkData PCARigBulkData;

	/** Holds the brows.json data produced by conforming and needed in animation generation */
	UE::Serialization::FEditorBulkData BrowsBufferBulkData;

	/** Holds the trained predictive solvers, which are used for the preview solve */
	UE::Serialization::FEditorBulkData PredictiveSolversBulkData;

	/** Holds the trained predictive solver without teeth,which is used for the global teeth solve */
	UE::Serialization::FEditorBulkData PredictiveWithoutTeethSolverBulkData;

private:

	UPROPERTY(NonTransactional)
	TArray<uint8> RawDNABuffer_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> RawDeltaDNABuffer_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> RawCombinedDNABuffer_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> DNABuffer_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> PCARig_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> BrowsBuffer_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> PredictiveSolvers_DEPRECATED;

	UPROPERTY(NonTransactional)
	TArray<uint8> PredictiveWithoutTeethSolver_DEPRECATED;

private:
	/**
	* A private function uses to apply the supplied DNA to the rig component.
	* Depending on the level of detail and usage (e.g.only LOD0 has blend shapes), these options can be turned off to save time/memory
	*/
	void ApplyDNAToRigComponent(TSharedPtr<class IDNAReader>  InDNAReader, bool bInUpdateBlendShapes = false, bool bInUpdateSkinWeights = true);

	/** Loads default solvers for face fitting */
	void LoadDefaultFaceFittingSolvers();

	// Calls the interchange system to create skelmesh/skeleton assets from the archetype dna file in the plugin content
	class USkeletalMesh* CreateFaceArchetypeSkelmesh(const FString& InNewRigAssetName, const FString& InNewRigPath);

	/** An array of poses that will be used to fit the conformal mesh to the input data. See UMetaHumanIdentityPose */
	UPROPERTY(VisibleAnywhere, Category = "Poses")
	TArray<TObjectPtr<UMetaHumanIdentityPose>> Poses;

	UPROPERTY()
	TObjectPtr<class UMetaHumanTemplateMesh> ConformalMeshComponent_DEPRECATED;

	/** Stored conformed left eye mesh vertices in rig coordinate space */
	UPROPERTY()
	TArray<FVector> ConformalVertsLeftEyeRigSpace_DEPRECATED;

	/** Stored conformed right eye mesh vertices in rig coordinate space */
	UPROPERTY()
	TArray<FVector> ConformalVertsRightEyeRigSpace_DEPRECATED;


private:

#if WITH_EDITOR

	/** Initializes the Rig by copying the Face Archetype provided by the plugin */
	EIdentityErrorCode InitializeRig();

	/** Currently active predictive solver task */
	class FPredictiveSolversTask* CurrentPredictiveSolversTask = nullptr;

#endif

	/** Returns initial template mesh transform */
	FTransform GetTemplateMeshInitialTransform() const;

	/** Get the filename for the device specific DNA to PCA config*/
	FString GetDeviceDNAToPCAConfig(class UCaptureData* InCaptureData) const;

	/** Moves rig component to the template mesh position. */
	void UpdateRigTransform();

	bool SetConformerCameraParameters(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer) const;
	bool SetConformerScanInputData(const UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer, bool &bOutInvalidMeshTopology) const;
	bool SetConformerDepthInputData(const UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer) const;
	EIdentityErrorCode RunMeshConformer(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer);

	void WriteConformalVerticesToFile(const FString& InNameSuffix = TEXT("")) const;
	void WriteTargetMeshToFile(class UStaticMesh* InTargetMesh, const FString& InNameSuffix = TEXT("")) const;

	FString GetPluginContentDir() const;

	/** Returns a name for a promoted frame suitable to be used with the conforming API */
	FString GetFrameNameForConforming(class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex) const;

	/** combine frame name and view name to give a unique identifier */
	static FString CombineFrameNameAndCameraViewName(const FString& InFrameName, const FString& InCameraViewName)
	{
		return FString::Format(TEXT("{0}_{1}"), { InFrameName, InCameraViewName });
	}

	bool SaveDebuggingData(UMetaHumanIdentityPose* InPose, UE::Wrappers::FMetaHumanConformer& OutConformer, const FString& InAssetSavedFolder) const;

	/** The list of curves required to be active when eye fitting is enabled */
	static const TArray<FString> CurveNamesForEyeFitting;

	/** Updates ImportData structs for SkeletalMesh to preserve the mesh */
	void UpdateSourceData(class USkeletalMesh* SkelMesh, class IDNAReader* DNAReader, class FDNAToSkelMeshMap* DNAToSkelMeshMap);

	/** Sets the head alignments for the given pose */
	void SetHeadAlignmentForPose(class UMetaHumanIdentityPose* InPose, TConstArrayView<FMatrix44f> InStackedTransforms, TConstArrayView<float> InStackedScales);

	/** Updates the capture data config name in each pose */
	void UpdateCaptureDataConfigName();

	/** Get the camera calibrations associated with the provided pose */
	TArray<FCameraCalibration> GetCalibrations(UMetaHumanIdentityPose* InPose) const;

	/** Get the calibration(s) for the supplied promoted frame and frame index */
	TArray<FCameraCalibration> GetCalibrations(UMetaHumanIdentityPose* InPose, class UMetaHumanIdentityPromotedFrame* InPromotedFrame, int32 InFrameIndex) const;

};

/////////////////////////////////////////////////////
// UMetaHumanIdentityBody

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityBody
	: public UMetaHumanIdentityPart
{
	GENERATED_BODY()

public:
	UMetaHumanIdentityBody();

	//~UMetaHumanIdentityBody Interface
	virtual void Initialize() override {}
	virtual FText GetPartName() const override;
	virtual FText GetPartDescription() const override;
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const override;
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const override;
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const override;

#if WITH_EDITOR
	virtual void PostTransacted(const FTransactionObjectEvent& InTransactionEvent) override;
#endif

public:

	UPROPERTY(BlueprintReadWrite, Category = "Body")
	int32 Height;

	UPROPERTY(BlueprintReadWrite, Category = "Body")
	int32 BodyTypeIndex;

	FSimpleMulticastDelegate OnMetaHumanIdentityBodyChangedDelegate;
};

/////////////////////////////////////////////////////
// UMetaHumanIdentityHands

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityHands
	: public UMetaHumanIdentityPart
{
	GENERATED_BODY()

public:
	UMetaHumanIdentityHands();

	//~UMetaHumanIdentityHands Interface
	virtual void Initialize() override {}
	virtual FText GetPartName() const override;
	virtual FText GetPartDescription() const override;
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const override;
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const override;
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const override;

};

/////////////////////////////////////////////////////
// UMetaHumanIdentityOutfit

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityOutfit
	: public UMetaHumanIdentityPart
{
	GENERATED_BODY()

public:
	UMetaHumanIdentityOutfit();

	//~UMetaHumanIdentityOutfit Interface
	virtual void Initialize() override {}
	virtual FText GetPartName() const override;
	virtual FText GetPartDescription() const override;
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const override;
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const override;
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const override;
};

/////////////////////////////////////////////////////
// UMetaHumanIdentityProp

UCLASS()
class METAHUMANIDENTITY_API UMetaHumanIdentityProp
	: public UMetaHumanIdentityPart
{
	GENERATED_BODY()

public:
	UMetaHumanIdentityProp();

	//~UMetaHumanIdentityProp Interface
	virtual void Initialize() override {}
	virtual FText GetPartName() const override;
	virtual FText GetPartDescription() const override;
	virtual FSlateIcon GetPartIcon(const FName& InPropertyName = NAME_None) const override;
	virtual FText GetPartTooltip(const FName& InPropertyName = NAME_None) const override;
	virtual bool DiagnosticsIndicatesProcessingIssue(FText& OutDiagnosticsWarningMessage) const override;
};

/////////////////////////////////////////////////////
// UMetaHumanTemplateMesh

UCLASS(HideCategories = (DynamicMeshComponent, Physics, Collision, HLOD, Navigation, VirtualTexture, Tags, ComponentReplication, Activation, Variable, Cooking, MaterialParameters, TextureStreaming, Mobile, AssetUserData))
class METAHUMANIDENTITY_API UMetaHumanTemplateMesh
	: public UDynamicMeshComponent
{
	GENERATED_BODY()

public:

	UPROPERTY()
	int32 MaskPreset = 0;
};
