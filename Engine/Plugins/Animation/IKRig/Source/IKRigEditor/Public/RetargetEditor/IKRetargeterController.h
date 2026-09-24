// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "RetargetEditor/IKRetargeterPoseGenerator.h"
#include "Retargeter/IKRetargeter.h"

#include "IKRetargeterController.generated.h"

struct FRetargetGlobalSettings;
struct FTargetRootSettings;
struct FTargetChainSettings;
struct FIKRetargetPose;
struct FIKRetargetOpBase;
enum class ERetargetSourceOrTarget : uint8;
enum class ERetargetAutoAlignMethod : uint8;
class FIKRetargetEditorController;
class URetargetChainSettings;
class UIKRigDefinition;
class UIKRetargeter;
class USkeletalMesh;
class UIKRigStructViewer;

// A stateless singleton (1-per-asset) class used to make modifications to a UIKRetargeter asset.
// Use UIKRetargeter.GetController() to get the controller for the asset you want to modify.  
UCLASS(BlueprintType, hidecategories = UObject)
class IKRIGEDITOR_API UIKRetargeterController : public UObject
{
	GENERATED_BODY()

public:

	UIKRetargeterController();

	// UObject
	virtual void PostInitProperties() override;
	
	// Get access to the retargeter asset.
	// Warning: Do not make modifications to the asset directly. Using the controller API guarantees correctness.
	UIKRetargeter* GetAsset() const;
  
private:
	
	// The actual asset that this Controller modifies. This is the only field this class should have.
	TObjectPtr<UIKRetargeter> Asset = nullptr;

public:

	//
	// GENERAL PUBLIC/SCRIPTING API
	//
	
	/** Get the controller for the given retargeter asset
	 * @param InRetargeterAsset an IK Retarget asset
	 * @return the controller with an API for modifying the given retarget asset */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe, DisplayName = "Get IK Retarget Controller"))
	static UIKRetargeterController* GetController(const UIKRetargeter* InRetargeterAsset);
	
	/** Set the IK Rig to use as the source or target (to copy animation FROM/TO)
	 * @param SourceOrTarget an enum specifying either "Source" or "Target"
	 * @param the IK Rig asset to apply as either the "Source" (to copy FROM) or "Target" (to copy TO)*/
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe, DisplayName = "Set IK Rig"))
	void SetIKRig(const ERetargetSourceOrTarget SourceOrTarget, UIKRigDefinition* IKRig) const;
	
	/** Get either source or target IK Rig
	 * @param SourceOrTarget an enum specifying either "Source" or "Target"
	 * @return the IK Rig asset associated with either the Source or Target of the IK Retargeter */ 
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe, DisplayName = "Get IK Rig"))
	const UIKRigDefinition* GetIKRig(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	/** Get all target IK Rigs referenced by all ops
	 * @return an array of all target IK Rig assets associated with all the ops in the stack */ 
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	TArray<UIKRigDefinition*> GetAllTargetIKRigs() const;

	/** Set the preview skeletal mesh for either source or target
	* @param SourceOrTarget an enum specifying either "Source" or "Target"
	* @param InPreviewMesh a skeletal mesh asset to use as the preview mesh in the retarget editor (may be used on other meshes at runtime) */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void SetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget, USkeletalMesh* InPreviewMesh) const;
	
	/** Get the preview skeletal mesh
	 * @param SourceOrTarget an enum specifying either "Source" or "Target"
	 * @return the skeletal mesh asset currently being used as a preview mesh */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	USkeletalMesh* GetPreviewMesh(const ERetargetSourceOrTarget SourceOrTarget) const;

	//
	// RETARGET OPS PUBLIC/SCRIPTING API
	//

	/** Add a new retarget op of the given type to the bottom of the stack. Returns the stack index.
	 * @param InIKRetargetOpType: the full package path of the UStruct type, ie /Script/IKRig.IKRetargetPinBoneOp */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget")
	int32 AddRetargetOp(const FString InIKRetargetOpType) const;

	/** Remove the retarget op at the given stack index
	 * NOTE: if this op is a parent, all it's children will be removed as well
	 * @param InOpIndex: the index of the op to remove
	 * @return true if op was found and removed, false otherwise */ 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget")
	bool RemoveRetargetOp(const int32 InOpIndex) const;

	/** Remove all ops in the stack.
	 * @return true if any ops were removed, false otherwise */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	bool RemoveAllOps() const;

	/** Set the name of the op at the given index in the stack.
	 * @param InName the new name to use
	 * @param InOpIndex the index of the op to be renamed
	 * @return the new name of the op (after it's made unique) */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	FName SetOpName(const FName InName, const int32 InOpIndex) const;
	
	/** Get the name of the op at the given index in the stack.
	 * @param InOpIndex the index of the op to get the name of
	 * @return the name of the op or None if the index was invalid */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	FName GetOpName(const int32 InOpIndex) const ;

	/** Set the name of the op to parent this op to
	 * Children ops are forced to execute before their parent
	 * @param InChildOpName the op to be parented
	 * @param InParentOpName the name of the op to parent to
	 * @return true if both parent and child were found, and the parent relationship was allowed */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	bool SetParentOpByName(const FName InChildOpName, const FName InParentOpName) const;
	
	/** Get the name of the parent op for the given op.
	 * @param InOpName the name of the op to get the parent of
	 * @return the name of the parent op or None if the op was not found or did not have a parent */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	FName GetParentOpByName(const FName InOpName) const ;

	/** Get the index of an op.
	 * @param InOpName the name of the op to get the index of
	 * @return the integer index of the op in the stack or -1 if op not found. */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	int32 GetIndexOfOpByName(const FName InOpName) const;

	/** Automatically add basic retargeting operations
	 * Adds these ops in the following order: Pelvis Motion, FK Chains, IK Chains, IK Solve and Root Motion
	 * If any of these ops are already present, they will not be re-added.
	 * @param bRunInitialSetup: if true, will run the initial setup routine on each op after they are all added to the stack. */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void AddDefaultOps() const;

	/** Force the op to run the initial setup. This is normally run when an Op is added through the editor UI.
	 * NOTE: ops may not have custom initial setup routines; the exact behavior is op-dependent
	 * @param InOpIndex the index of the op to run setup on */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void RunOpInitialSetup(const int32 InOpIndex) const;

	// Tell each op to reset any settings associated with this chain
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	void ResetChainSettingsInAllOps(const FName InTargetChainName) const;

	/** Get the number of Ops in the stack.
	 * @return int, the number of ops */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	int32 GetNumRetargetOps() const;

	/** Move the retarget op at the given index to the target index.
	 * NOTE: due to constraints on execution order, the actual index may differ from what is requested
	 * @InOpToMoveIndex the index of the op to be moved
	 * @InTargetIndex the index where the op should be moved to */ 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget")
	bool MoveRetargetOpInStack(int32 InOpToMoveIndex, int32 InTargetIndex) const;

	/** Toggle an op on/off.
	 * @param InRetargetOpIndex the index of the op to modify
	 * @param bIsEnabled if true, turns the op On, else Off
	 * @return true if op was found at index */ 
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget")
	bool SetRetargetOpEnabled(int32 InRetargetOpIndex, bool bIsEnabled) const;

	/** Get enabled status of the given Op.
	 * @param InRetargetOpIndex the index of the op to get the enabled state for
	 * @return true if op is enabled, false if op is disabled or not found */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	bool GetRetargetOpEnabled(int32 InRetargetOpIndex) const;
	
	/** Get a controller to get/set the settings for a given op in the stack
	 * NOTE: the returned UIKRetargetOpControllerBase* should be cast to the type specific to your op to get full functionality for that op.
	 * @param InOpIndex: the index of the op in the stack to return a controller for
	 * @return a pointer to the base op controller UClass or null if the op was not found */
	UFUNCTION(BlueprintCallable, Category="IK Rig")
	UIKRetargetOpControllerBase* GetOpController(int32 InOpIndex);

	//
	// RETARGET OPS C++ ONLY API
	//
	
	// add a new retarget op to the stack
	int32 AddRetargetOp(const UScriptStruct* InRetargetOpType, const FName InParentOpName = NAME_None) const;

	// Gather up all the ops that are children of this op
	TArray<int32> GetChildOpIndices(const int32 InOpIndex) const;

	// Returns true if this op type can contain child ops (ops forced to be ordered before the parent)
	bool GetCanOpHaveChildren(const int32 InOpIndex) const;

	// Get the index of the parent op or INDEX_NONE if it's root level
	int32 GetParentOpIndex(const int32 InOpIndex) const;

	// Return NameToMakeUnique with a numbered suffix that makes it unique in the stack
	FName GetUniqueOpName(const FName NameToMakeUnique, int32 OpIndexToIgnore) const;

	// Get access to the first retarget operation of the given type
	template <typename T>
	T* GetFirstRetargetOpOfType() const
	{
		return Asset->GetFirstRetargetOpOfType<T>();
	}

	// Get access to the given retarget operation by index.
	FIKRetargetOpBase* GetRetargetOpByIndex(const int32 InOpIndex) const;

	// Get access to the given retarget operation by name.
	FIKRetargetOpBase* GetRetargetOpByName(const FName InOpName) const;

	// Get access to the given retarget operation.
	FInstancedStruct* GetRetargetOpStructAtIndex(int32 Index) const;

	// Get the index of a given retarget operation. 
	int32 GetIndexOfRetargetOp(FIKRetargetOpBase* RetargetOp) const;

	// A callback whenever the property of an op is modified
	void OnOpPropertyChanged(const FName& InOpName, const FPropertyChangedEvent& InPropertyChangedEvent) const;

	//
	// GENERAL C++ ONLY API
	//
	
	// Ensures all internal data is compatible with assigned meshes and ready to edit.
	// - Removes bones from retarget poses that are no longer in skeleton
	// - Removes chain settings for chains that are no longer in target IK Rig
	void CleanAsset() const;
	
	// Get either source or target IK Rig 
	UIKRigDefinition* GetIKRigWriteable(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	// Get if we've already asked to fix the root height for the given skeletal mesh 
	bool GetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh) const;
	
	// Set if we've asked to fix the root height for the given skeletal mesh 
	void SetAskedToFixRootHeightForMesh(USkeletalMesh* Mesh, bool InAsked) const;
	
	// Get name of the Root bone used for retargeting. 
	FName GetPelvisBone(const ERetargetSourceOrTarget SourceOrTarget) const;

	//
	// RETARGET CHAIN MAPPING PUBLIC/SCRIPTING API
	//
	
	/** Use string comparision to find "best" Source chain to map to each Target chain or clear the mappings
	 * @param AutoMapType an enum specifying the type of mapping to perform (ie, Exact, Fuzzy or Clear)
	 * @param bForceRemap if false, will only remap those mappings that unset/None
	 * @param InOpName if specified, will auto-map only the chain mapping in the specified op, otherwise applies to all ops*/
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void AutoMapChains(const EAutoMapChainType AutoMapType, const bool bForceRemap, const FName InOpName=NAME_None) const;

	/** Assign a source chain to the given target chain. Animation will be copied from the source to the target.
	 * @param InSourceChainName the name of the source retarget chain to assign
	 * @param InTargetChainName the name of the target chain to map
	 * @param InOpName if specified, will set the source chain only for that op (otherwise applies to all ops with a chain mapping)*/
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	bool SetSourceChain(FName InSourceChainName, FName InTargetChainName, const FName InOpName=NAME_None) const;

	/** Get the name of the source chain mapped to a given target chain (the chain animation is copied FROM).
	 * @param InTargetChainName the name of the target retarget chain to get a mapping for
	 * @param InOpName if specified, will get the source chain for that op (otherwise gets the first chain mapping in the op stack)
	 * @return the name of the source chain mapped to the given target chain (or None if not mapped) */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	FName GetSourceChain(const FName& InTargetChainName, const FName InOpName=NAME_None) const;

	/** Reset the settings for the given chain in the given op back to the defaults.
	 * @param InTargetChainName the name of the chain
	 * @param InOpName the name of the op containing the chain settings */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void ResetChainSettingsToDefault(const FName InTargetChainName, const FName InOpName) const;

	/** Get the target IK Rig associated with the given Op. May be different than the default IK Rig.
	 * NOTE: all ops use the global SOURCE IK rig, but each op may use its own custom TARGET IK Rig.
	 * NOTE: not all ops maintain their own target IK Rig in which case this function returns nullptr
	 * NOTE: some ops may refer to the target IK Rig used by their parent op. In that case, this function returns the parent Op's IK Rig.
	 * @param InOpName the name of the retarget op
	 * @return the IK Rig assset associated with this op */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	const UIKRigDefinition* GetTargetIKRigForOp(const FName InOpName) const;

	//
	// RETARGET CHAIN MAPPING C++ ONLY API
	//

	/** Get access to the mapping between source/target chains for the particular op (may be null)
	 * If no op name is specified, it returns the first chain mapping it finds*/
	const FRetargetChainMapping* GetChainMapping(const FName InOpName = NAME_None) const;

	// Clean all chain mappings in all ops, or if InOpName is specified, just that op
	void CleanChainMaps(const FName InOpName=NAME_None) const;
	
	// Ask if the given op has settings for the given chain that are not at the default
	bool AreChainSettingsAtDefault(const FName InTargetChainName, const FName InOpName) const;
	
	// Get whether the given chain's IK goal is connected to a solver 
	bool IsChainGoalConnectedToASolver(const FName& GoalName) const;
	
	// Call this when IK Rig chain is added or removed. 
	void HandleRetargetChainAdded(UIKRigDefinition* IKRig) const;
	
	// Call this when IK Rig chain is renamed. Retains existing mappings using the new name 
	void HandleRetargetChainRenamed(UIKRigDefinition* InIKRig, FName OldChainName, FName NewChainName) const;
	
	// Call this when IK Rig chain is removed. 
	void HandleRetargetChainRemoved(UIKRigDefinition* IKRig, const FName& InChainRemoved) const;

	//
	// RETARGET POSE PUBLIC/SCRIPTING API
	//
	
	/** Add new retarget pose.
	 * @param NewPoseName The name to assign to the new retarget pose
	 * @param SourceOrTarget Specifies whether the pose applies to the source or target skeleton
	 * @return The name of the newly created retarget pose */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	FName CreateRetargetPose(const FName& NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Remove a retarget pose.
	 * @param PoseToRemove The name of the retarget pose to remove
	 * @param SourceOrTarget Specifies whether the pose belongs to the source or target skeleton
	 * @return True if the pose was found and successfully removed, false otherwise */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	bool RemoveRetargetPose(const FName& PoseToRemove, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Duplicate a retarget pose.
	 * @param PoseToDuplicate The name of the retarget pose to duplicate
	 * @param NewName The name to assign to the duplicated pose
	 * @param SourceOrTarget Specifies whether the pose belongs to the source or target skeleton
	 * @return The name of the new duplicated pose, or NAME_None if PoseToDuplicate is not found */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	FName DuplicateRetargetPose(const FName PoseToDuplicate, const FName NewName, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Rename current retarget pose.
	 * @param OldPoseName The current name of the retarget pose to rename
	 * @param NewPoseName The new name to assign to the retarget pose
	 * @param SourceOrTarget Specifies whether the pose belongs to the source or target skeleton
	 * @return True if the pose was found and successfully renamed, false otherwise */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	bool RenameRetargetPose(const FName OldPoseName, const FName NewPoseName, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Change which retarget pose is used by the retargeter at runtime.
	 * @param CurrentPose The name of the retarget pose to set as current
	 * @param SourceOrTarget Specifies whether the pose applies to the source or target skeleton
	 * @return True if the current pose was successfully set, false otherwise */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	bool SetCurrentRetargetPose(FName CurrentPose, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Get the name of the current retarget pose.
	 * @param SourceOrTarget Specifies whether to retrieve the pose from the source or target skeleton
	 * @return The name of the current retarget pose */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta =(BlueprintThreadSafe))
	FName GetCurrentRetargetPoseName(const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Get access to array of retarget poses.
	 * @param SourceOrTarget Specifies whether to retrieve poses from the source or target skeleton
	 * @return A reference to the map of retarget pose names to their pose data */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta =(BlueprintThreadSafe))
	TMap<FName, FIKRetargetPose>& GetRetargetPoses(const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Get the current retarget pose.
	 * @param SourceOrTarget Specifies whether to retrieve the pose from the source or target skeleton
	 * @return A reference to the current retarget pose data */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta =(BlueprintThreadSafe))
	FIKRetargetPose& GetCurrentRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Reset a retarget pose for the specified bones.
	 * @param PoseToReset The name of the retarget pose to reset
	 * @param BonesToReset The array of bone names to reset; if empty, resets all bones to the reference pose
	 * @param SourceOrTarget Specifies whether the pose belongs to the source or target skeleton */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	void ResetRetargetPose(const FName& PoseToReset, const TArray<FName>& BonesToReset, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Set a delta rotation for a given bone in the current retarget pose.
	 * @param BoneName The name of the bone to apply the rotation offset to
	 * @param RotationOffset The quaternion representing the rotation offset to apply
	 * @param SkeletonMode Specifies whether the bone belongs to the source or target skeleton */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	void SetRotationOffsetForRetargetPoseBone(const FName& BoneName, const FQuat& RotationOffset, const ERetargetSourceOrTarget SkeletonMode) const;

	/** Get a delta rotation for a given bone in the current retarget pose.
	 * @param BoneName The name of the bone to retrieve the rotation offset for
	 * @param SourceOrTarget Specifies whether the bone belongs to the source or target skeleton
	 * @return The quaternion representing the rotation offset for the specified bone */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	FQuat GetRotationOffsetForRetargetPoseBone(const FName& BoneName, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Set the translation offset on the retarget pelvis bone for the current retarget pose.
	 * @param TranslationOffset The vector representing the translation offset to apply to the pelvis bone
	 * @param SourceOrTarget Specifies whether the pelvis bone belongs to the source or target skeleton */
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta =(BlueprintThreadSafe))
	void SetRootOffsetInRetargetPose(const FVector& TranslationOffset, const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Get the translation offset on the retarget pelvis bone for the current retarget pose.
	 * @param SourceOrTarget Specifies whether the pelvis bone belongs to the source or target skeleton
	 * @return The vector representing the translation offset of the pelvis bone */
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	FVector GetRootOffsetInRetargetPose(const ERetargetSourceOrTarget SourceOrTarget) const;

	/** Automatically align all bones in mapped chains and store in the current retarget pose.
	 * @param SourceOrTarget Specifies whether to align bones in the source or target skeleton
	 * @param Method The method to use for automatic alignment (defaults to ChainToChain) */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	void AutoAlignAllBones(ERetargetSourceOrTarget SourceOrTarget, const ERetargetAutoAlignMethod Method = ERetargetAutoAlignMethod::ChainToChain) const;

	/** Automatically align an array of bones and store in the current retarget pose.
	 * @param BonesToAlign The array of bone names to align; bones not in mapped chains are ignored
	 * @param Method The method to use for automatic alignment
	 * @param SourceOrTarget Specifies whether to align bones in the source or target skeleton */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	void AutoAlignBones(const TArray<FName>& BonesToAlign, const ERetargetAutoAlignMethod Method, ERetargetSourceOrTarget SourceOrTarget) const;

	/** Moves the entire skeleton vertically until the specified bone is the same height off the ground as in the reference pose.
	 * @param ReferenceBone The name of the bone to use as a reference for ground snapping
	 * @param SourceOrTarget Specifies whether the skeleton belongs to the source or target */
	UFUNCTION(BlueprintCallable, Category="IK Retarget")
	void SnapBoneToGround(FName ReferenceBone, ERetargetSourceOrTarget SourceOrTarget);

	//
	// RETARGET POSE C++ ONLY API
	//
	
	// Add a numbered suffix to the given pose name to make it unique. 
	FName MakePoseNameUnique(
		const FString& PoseName,
		const ERetargetSourceOrTarget SourceOrTarget) const;
	
private:
	
	// Remove bones from retarget poses that are no longer in skeleton 
	void CleanPoseList(const ERetargetSourceOrTarget SourceOrTarget) const;
	
	//
	// BEGIN DEPRECATED PUBLIC API
	//
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
public:

	// get access to the mapping between source/target chains
	UE_DEPRECATED(5.6, "Chain mappings are stored on individual ops. Use the version that takes an op name. ")
	const FRetargetChainMapping& GetChainMapping() const;
	
	// Get a copy of the retarget root settings for this asset.
	UE_DEPRECATED(5.6, "Root settings are now accessed through the Pelvis Motion Op controller.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	FTargetRootSettings GetRootSettings() const;
	
	// Set the retarget root settings for this asset.
	UE_DEPRECATED(5.6, "Root settings are now accessed through the Pelvis Motion Op controller.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void SetRootSettings(const FTargetRootSettings& RootSettings) const;

	// Get a copy of the global settings for this asset.
	UE_DEPRECATED(5.6, "Global settings are now accessed through Op controllers that perform the same duties.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	FRetargetGlobalSettings GetGlobalSettings() const;

	// Get a copy of the global settings for this asset.
	UE_DEPRECATED(5.6, "Global settings are now accessed through Op controllers that perform the same duties.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	void SetGlobalSettings(const FRetargetGlobalSettings& GlobalSettings) const;

	// Get a copy of the settings for the target chain by name.
	UE_DEPRECATED(5.6, "Access to chain settings must go through an Op controller now.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	FTargetChainSettings GetRetargetChainSettings(const FName& TargetChainName) const;

	// Set the settings for the target chain by name. Returns true if the chain settings were applied, false otherwise.
	UE_DEPRECATED(5.6, "Access to chain settings must go through an Op controller now.")
	UFUNCTION(BlueprintCallable, BlueprintPure=false, Category="IK Retarget", meta = (BlueprintThreadSafe))
	bool SetRetargetChainSettings(const FName& TargetChainName, const FTargetChainSettings& Settings) const;

	// Get read-only access to the list of settings for each target chain
	UE_DEPRECATED(5.6, "Access to chain settings must go through an Op controller now.")
	UFUNCTION(BlueprintCallable, Category="IK Retarget", meta = (BlueprintThreadSafe))
	const TArray<URetargetChainSettings*>& GetAllChainSettings() const;

	// convenience to get chain settings UObject by name
	UE_DEPRECATED(5.6, "Access to chain settings must go through an Op controller now.")
	URetargetChainSettings* GetChainSettings(const FName& TargetChainName) const;

	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	//
	// END DEPRECATED PUBLIC API
	//
	
	//
	// DELEGATE CALLBACKS FOR OUTSIDE SYSTEMS
	//
	
private:
	
	DECLARE_MULTICAST_DELEGATE(FOnRetargeterNeedsInitialized);
	FOnRetargeterNeedsInitialized RetargeterNeedsInitialized;

	DECLARE_MULTICAST_DELEGATE(FOnOpStackModified);
	FOnOpStackModified OpStackModified;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnIKRigReplaced, ERetargetSourceOrTarget);
	FOnIKRigReplaced IKRigReplaced;
	
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPreviewMeshReplaced, ERetargetSourceOrTarget);
	FOnPreviewMeshReplaced PreviewMeshReplaced;

	// auto pose generator
	TUniquePtr<FRetargetAutoPoseGenerator> AutoPoseGenerator;

	// only allow modifications to data model from one thread at a time
	mutable FCriticalSection ControllerLock;

	// prevent reinitializing from inner operations
	mutable int32 ReinitializeScopeCounter = 0;
	
	// a dummy UObject to wrap UStructs for the details panel
	UPROPERTY(transient)
	TObjectPtr<UIKRigStructViewer> StructViewer = nullptr;
	
public:

	// Attach a delegate to be notified whenever either the source or target Preview Mesh asset's are swapped out.
	FOnPreviewMeshReplaced& OnPreviewMeshReplaced(){ return PreviewMeshReplaced; };
	
	// Attach a delegate to be notified whenever either the source or target IK Rig asset's are swapped out.
	FOnIKRigReplaced& OnIKRigReplaced(){ return IKRigReplaced; };
	
	// Attach a delegate to be notified whenever the retargeter is modified in such a way that would require re-initialization of the processor.
	FOnRetargeterNeedsInitialized& OnRetargeterNeedsInitialized(){ return RetargeterNeedsInitialized; };

	// Attach a delegate to be notified whenever the op stack is modified.
	FOnOpStackModified& OnOpStackModified(){ return OpStackModified; };

	// get the singleton struct viewer for editing structs belonging to the asset this controller controls
	UIKRigStructViewer* GetStructViewer();
	
	friend class UIKRetargeter;
	friend struct FScopedReinitializeIKRetargeter;
};

/** Retargeter can reinit just the processor or other UI as well depending on the data model modification type */
enum class ERetargetRefreshMode : uint8
{
	ProcessorOnly,
	ProcessorAndOpStack,
	ProcessorAndFullUI
};

struct FScopedReinitializeIKRetargeter
{
	FScopedReinitializeIKRetargeter(const UIKRetargeterController *InController, ERetargetRefreshMode InRefreshMode = ERetargetRefreshMode::ProcessorOnly)
	{
		InController->ReinitializeScopeCounter++;
		Controller = InController;
		RefreshMode = InRefreshMode;
	}
	~FScopedReinitializeIKRetargeter()
	{
		if (--Controller->ReinitializeScopeCounter == 0)
		{
			Controller->RetargeterNeedsInitialized.Broadcast();

			if (RefreshMode == ERetargetRefreshMode::ProcessorAndOpStack)
			{
				Controller->OpStackModified.Broadcast();
			}
		}
	};

	const UIKRetargeterController* Controller;
	ERetargetRefreshMode RefreshMode = ERetargetRefreshMode::ProcessorOnly;
};

