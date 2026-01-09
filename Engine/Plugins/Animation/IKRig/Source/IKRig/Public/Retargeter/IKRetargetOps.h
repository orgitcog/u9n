// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IKRetargetOps.generated.h"

#define UE_API IKRIG_API

class UIKRigDefinition;
struct FIKRetargetProcessor;
struct FRetargetSkeleton;
struct FTargetSkeleton;
struct FIKRigLogger;
class UIKRetargeter;
struct FInstancedStruct;
class USkeleton;
struct FPoseContext;
struct FRetargetChainMapping;

#if WITH_EDITOR
class FPrimitiveDrawInterface;
class FIKRetargetEditorController;
struct FIKRetargetDebugDrawState;
#endif

/** This is the base class for defining editable settings for your custom retargeting operation.
 * All user-configurable properties for your "op" should be stored in a subclass of this.
 * These settings will automatically be:
 * 1. Displayed in the details panel when the op is selected
 * 2. Saved/loaded with the op in the retarget asset
 * 3. Applied to the op at runtime as part of a profile
 * 
 * NOTE: the derived type must be returned by the op's GetSettingsType() and GetSettings()
 * 
 * NOTE: UProperties that require reinitialization when modified must be marked meta=(ReinitializeOnEdit)
 * When modified, in the editor, these properties will trigger a reinitialization at which point
 * the runtime Op will get the latest values automatically.
 */
USTRUCT(BlueprintType)
struct FIKRetargetOpSettingsBase
{
	GENERATED_BODY()
	
	virtual ~FIKRetargetOpSettingsBase() {};

	/** (required) override to specify how settings should be applied in a way that will not require reinitialization (ie runtime compatible)*/
	virtual void CopySettingsAtRuntime(const FIKRetargetOpSettingsBase* InSettingsToCopyFrom) { checkNoEntry(); };
	
	/** (optional, but recommended) provide a custom controller type (deriving from UIKRetargetOpControllerBase) as an API for editing the op
	 * NOTE: this type will automatically be lazy instantiated when/if the user calls GetController() on the op. */
	UE_API virtual const UClass* GetControllerType() const;
	
	/** provide a scripting object to edit your custom settings type in the editor via BP/Python
	* NOTE: this returns an instance of whatever type is returned by GetControllerType(). This can be custom per op. */
	UE_API UIKRetargetOpControllerBase* GetController(UObject* Outer);

private:

	// helper function for settings structs to instantiate their own controller
	UE_API UIKRetargetOpControllerBase* CreateControllerIfNeeded(UObject* Outer);
	
	// the controller used to edit this op by script/blueprint (lazy instantiated when needed)
	TStrongObjectPtr<UIKRetargetOpControllerBase> Controller = nullptr;

#if WITH_EDITORONLY_DATA
public:
	/** the op these settings belong to */
	UPROPERTY(transient)
	FName OwningOpName;
	/** the instance of this op currently running in the editor viewport */
	mutable FIKRetargetOpSettingsBase* EditorInstance;
	/** a reference to the skeletons for bone selector widgets (can return these from GetSkeleton) */
	mutable const USkeleton* SourceSkeletonAsset;
	mutable const USkeleton* TargetSkeletonAsset;
	/** allow settings to provide a skeleton for any given FBoneReference widget */
	virtual USkeleton* GetSkeleton(const FName InPropertyName) { return nullptr; };
#endif
};

/**
 * This is the base class for defining operations that live in the retargeter "op" stack.
 * These operations are executed in order by calling the virtual Run() function on each one in order.
 * The Run() function takes an input pose on the source skeletal mesh and affects the output pose on the target mesh.
 * NOTE: any user defined settings associated with a retarget op must be aggregated into a custom UStruct derived from FIKRetargetOpSettingsBase
 * This ensures that the settings are user editable in the details panel with full undo/redo support and serialization.
*/ 
USTRUCT(BlueprintType)
struct FIKRetargetOpBase
{
	GENERATED_BODY()
	
public:

	virtual ~FIKRetargetOpBase() {};

	/** (optional) override to cache internal data when initializing the processor
	 * NOTE: you must set bIsInitialized to true to inform the retargeter that this op is ok to execute. */
	virtual bool Initialize(
		const FIKRetargetProcessor& InProcessor,
		const FRetargetSkeleton& InSourceSkeleton,
		const FTargetSkeleton& InTargetSkeleton,
		const FIKRetargetOpBase* InParentOp,
		FIKRigLogger& Log) { bIsInitialized = true; return true; };

	/** (optional) override to evaluate this operation and modify the output pose */
	virtual void Run(
		FIKRetargetProcessor& InProcessor,
		const double InDeltaTime,
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose){};
	
	/** (optional) a second pass of initialization that ops can use after Op::Initialize() is called on all ops. */
	virtual void PostInitialize(
		const FIKRetargetProcessor& InProcessor,
		const FRetargetSkeleton& InSourceSkeleton,
		const FTargetSkeleton& InTargetSkeleton,
		FIKRigLogger& InLog) {};
	
	/** (optional) override to automate initial setup after being added to the stack */
	virtual void OnAddedToStack(const UIKRetargeter* InRetargetAsset, const FIKRetargetOpBase* InParentOp) {};
	
	/** get if this op is enabled */
	bool IsEnabled() const { return bIsEnabled; };

	/** turn this operation on/off (will be skipped during execution if disabled) */
	void SetEnabled(const bool bEnabled){ bIsEnabled = bEnabled; };

	/** return true if this op is initialized and ready to run */
	bool IsInitialized() const { return bIsInitialized; };

	/** (required) override and return a pointer to the settings struct used by this operation */ 
	virtual FIKRetargetOpSettingsBase* GetSettings() { checkNoEntry(); return nullptr; };
	
	/** (optional) override to react when settings are applied at runtime
	 * NOTE:
	 * This is called while the Op is running AFTER Initialize() so it up to the Op author
	 * to copy only those settings which are safe to be updated while the op is running.
	 * The default implementation uses CopySettingsAtRuntime() which must be implemented on the settings type.
	 * @param InSettings - pointer to the settings to copy, can be safely cast to the type returned by GetSettingsType() */ 
	UE_API virtual void SetSettings(const FIKRetargetOpSettingsBase* InSettings);
	
	/** (required) override and return the type used to house the settings for this operation */
	virtual const UScriptStruct* GetSettingsType() const { checkNoEntry(); return nullptr; };

	/** (required) override and return the type of this op (the derived subclass) */
	virtual const UScriptStruct* GetType() const { checkNoEntry(); return FIKRetargetOpBase::StaticStruct(); };

	/** (optional) override to reset any internal state when animation playback is reset or stopped (ie, springs / dampers etc) */
	virtual void OnPlaybackReset() {};

	/** (optional) override to get any data from the source or target skeletal mesh component
	 * NOTE: this is called during AnimGraph::PreUpdate() which runs on the main thread, use caution. */
	virtual void AnimGraphPreUpdateMainThread(USkeletalMeshComponent& SourceMeshComponent, USkeletalMeshComponent& TargetMeshComponent) {};

	/** (optional) override to get any data from the anim graph as it's evaluating.
	 * NOTE: this is called during AnimGraph::Evaluate_AnyThread() BEFORE the retargeting Ops are executed. */
	virtual void AnimGraphEvaluateAnyThread(FPoseContext& Output) {};

	/** (optional) override and add any bones that your op modifies to the output TSet of bone indices
	 * Add indices of any bone that this op modifies. Any bone not registered here will be FK parented by other operations*/
	virtual void CollectRetargetedBones(TSet<int32>& OutRetargetedBones) const {};

	/** (optional) ops can optionally behave as a 'parent' where child ops must be executed first. */
	virtual bool CanHaveChildOps() const { return false; };

	/** (optional) ops can optionally behave as a 'child' where they can be parented to ops of the type returned by this function. */
	virtual const UScriptStruct* GetParentOpType() const { return nullptr; };

	/** (optional) return true to disallow multiple copies of this op in the stack */
	virtual bool IsSingleton() const { return false; };

	/** (optional) override and supply the target IK Rig this op references
	 * NOTE: The retarget processor will resolve the bone chains in the rig so that they can be queried by the op. */
	virtual const UIKRigDefinition* GetCustomTargetIKRig() const { return nullptr; };
	
	/** (optional) ops can optionally store their own chain mapping, this allows outside systems to query/edit it. */
	virtual FRetargetChainMapping* GetChainMapping() { return nullptr; };

	/** (optional) implement this if the op stores chain settings by name to allow the settings to be maintained after a chain is renamed
	 * NOTE: this is only called when the target IK Rig this op references has a chain that is renamed.
	 * NOTE: ops do not have to update chain mappings in this function, that is managed for them if GetChainMapping() is implemented */
	virtual void OnTargetChainRenamed(const FName InOldChainName, const FName InNewChainName) {};

	/** (optional) override to allow ops to react when a property marked "ReinitializeOnEdit" is modified
	 * NOTE: this is called before Initialize() is called again to give op a chance to auto-configure itself based on its new state
	 * NOTE: this is called in several places where InPropertyChangedEvent may be null */
	virtual void OnReinitPropertyEdited(const FPropertyChangedEvent* InPropertyChangedEvent) {};

	/** (optional) override to allow ops to react when a property marked "ReinitializeOnEdit" on the parent op is modified
	 * NOTE: InPropertyChangedEvent will be null at load time giving op a chance to clean based on parent state.
	 * NOTE: this is called in several places where InPropertyChangedEvent may be null */
	virtual void OnParentReinitPropertyEdited(const FIKRetargetOpBase& InParentOp, const FPropertyChangedEvent* InPropertyChangedEvent) {};

#if WITH_EDITOR
	
	/** get the nice name to display in the viewport (defaults to "DisplayName" UStruct metadata) */
	UE_API FName GetDefaultName() const;

	/** (optional) override to display a warning message in the op stack */
	UE_API virtual FText GetWarningMessage() const;

	/** (optional) override to draw debug info into the editor viewport when the Op is selected */
	virtual void DebugDraw(
		FPrimitiveDrawInterface* InPDI,
		const FTransform& InComponentTransform,
		const double InComponentScale,
		const FIKRetargetDebugDrawState& InEditorState) const {};

	/** (optional) override to support resetting all settings for a given chain */
	virtual void ResetChainSettingsToDefault(const FName& InChainName) {};

	/** (optional) override to support "Reset to Default" UI for chains */
	virtual bool AreChainSettingsAtDefault(const FName& InChainName) { return true; };
#endif

	/** Get the name of this op (may be customized by user) */
	UE_API FName GetName() const;

	/** Set the name of this op
	 * NOTE: InName is assumed to be unique in the stack (this constraint is enforced when setting the name from the controller) */
	UE_API void SetName(const FName InName);
	
	/** Set the name of the op this op is "parented" to. This enforces execution order.
	 * NOTE: InName is assumed to refer to an op that exists.
	 * NOTE: Renaming the parent op through the controller will auto-update this. */
	UE_API void SetParentOpName(const FName InName);

	/** Get the name of the op this op is parented to (None for root level ops)*/
	UE_API FName GetParentOpName() const;

	/** wholesale copy all settings from the input settings into this op.
	 * NOTE: InSettings is assumed to be castable to the type returned by GetSettingsType()
	 * NOTE: be careful calling this on an initialized op as it may invalidate it's runtime state (use SetSettings instead) */
	UE_API void CopySettingsRaw(const FIKRetargetOpSettingsBase* InSettings, const TArray<FName>& InPropertiesToIgnore);

	/** a convenience function to copy all properties from one struct to another while ignoring some */
	static UE_API void CopyStructProperties(
		const UStruct* InStructType,
		const void* InSrcStruct,
		void* InOutDestStruct,
		const TArray<FName>& InPropertiesToIgnore);

private:
	
	/** when false, execution of this op is skipped */
	UPROPERTY()
	bool bIsEnabled = true;

	/** the text label given to the op, used to refer to it from script */
	UPROPERTY()
	FName Name;

	/** (optional) some ops are considered as a group, this is the name of the group parent */
	UPROPERTY()
	FName ParentOpName;

protected:
	
	/** set to true when the op is ready to run */
	bool bIsInitialized = false;
};

/**
 * This is the base class for defining a custom controller for a given retarget op type.
 * Controllers provide an API for editing ops from blueprint or python.
 *
 * To use a controller:
 * 1. Get a controller by calling UIKRetargeterController::GetOpController(int InOpIndex)
 * 2. Cast the returned UIKRetargetOpControllerBase* to the type of controller used by the op you want to modify.
 * 3. Call the public getter/setters to modify the op as desired
 *
 * NOTE: these controllers are necessary because the op UStructs do not support UFunctions
*/ 
UCLASS(MinimalAPI, BlueprintType)
class UIKRetargetOpControllerBase : public UObject
{
	GENERATED_BODY()
	
public:
	
	// the op settings this controller controls
	FIKRetargetOpSettingsBase* OpSettingsToControl = nullptr;
};


//
// BEGIN LEGACY OP BASE
//

// NOTE: This type has been replaced by FRetargetOpBase. URetargetOpBase-based ops no longer work, please refactor into the new FRetargetOpBase struct.
UCLASS(MinimalAPI)
class URetargetOpBase : public UObject
{
	GENERATED_BODY()
	
	public:

	// this is the deprecation upgrade path for solvers that inherit from UIKRigSolver to convert them to FIKRigSolverBase
	// override this and supply a struct into OutInstancedStruct that derives from FIKRigSolverBase that implements your custom solver.
	virtual void ConvertToInstancedStruct(FInstancedStruct& OutInstancedStruct){};
	
	virtual bool Initialize(
	const FIKRetargetProcessor& Processor,
		const FRetargetSkeleton& SourceSkeleton,
		const FTargetSkeleton& TargetSkeleton,
		FIKRigLogger& Log) { return false; };
	virtual void Run(
		const FIKRetargetProcessor& Processor,
		const TArray<FTransform>& InOutSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose){};

	UPROPERTY()
	bool bIsEnabled = true;

	bool bIsInitialized = false;
	
#if WITH_EDITOR
	virtual void OnAddedToStack(const UIKRetargeter* Asset) {};
	virtual FText GetNiceName() const { return FText::FromString(TEXT("Default Op Name")); };
	virtual FText WarningMessage() const { return FText::GetEmpty(); };
#endif
};

// NOTE: This type is no longer in use except to load old stacks of UObject-based ops.
UCLASS(MinimalAPI)
class URetargetOpStack : public UObject
{
	GENERATED_BODY()
	
public:
	
	UPROPERTY()
	TArray<TObjectPtr<URetargetOpBase>> RetargetOps_DEPRECATED;
};

//
// END LEGACY OP BASE
//

#undef UE_API
