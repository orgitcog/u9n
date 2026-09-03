// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "StateTreeState.h"
#include "StateTreeEditorPropertyBindings.h"
#include "StateTreeEditorTypes.h"
#include "Debugger/StateTreeDebuggerTypes.h"
#include "StateTreeEditorData.generated.h"

struct FStateTreeBindableStructDesc;

class UStateTreeSchema;

namespace UE::StateTree::Editor
{
	// Name used to describe container of global items (other items use the path to the container State).  
	extern STATETREEEDITORMODULE_API const FString GlobalStateName;

	// Name used to describe container of property functions.
	extern STATETREEEDITORMODULE_API const FString PropertyFunctionStateName;
}


USTRUCT()
struct FStateTreeEditorBreakpoint
{
	GENERATED_BODY()

	FStateTreeEditorBreakpoint() = default;
	explicit FStateTreeEditorBreakpoint(const FGuid& ID, const EStateTreeBreakpointType BreakpointType)
		: ID(ID)
		, BreakpointType(BreakpointType)
	{
	}

	/** Unique Id of the Node or State associated to the breakpoint. */
	UPROPERTY()
	FGuid ID;

	/** The event type that should trigger the breakpoint (e.g. OnEnter, OnExit, etc.). */
	UPROPERTY()
	EStateTreeBreakpointType BreakpointType = EStateTreeBreakpointType::Unset;
};

UENUM()
enum class EStateTreeVisitor : uint8
{
	Continue,
	Break,
};

/**
 * Edit time data for StateTree asset. This data gets baked into runtime format before being used by the StateTreeInstance.
 */
UCLASS(BlueprintType, EditInlineNew, CollapseCategories, Within = "StateTree")
class STATETREEEDITORMODULE_API UStateTreeEditorData : public UObject, public IStateTreeEditorPropertyBindingsOwner
{
	GENERATED_BODY()
	
public:
	UStateTreeEditorData();

	virtual void PostInitProperties() override;
	virtual void Serialize(FArchive& Ar) override;
	
	//~ Begin IStateTreeEditorPropertyBindingsOwner interface
	virtual void GetBindableStructs(const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const override;
	virtual bool GetBindableStructByID(const FGuid StructID, TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutStructDesc) const override;
	virtual bool GetBindingDataViewByID(const FGuid StructID, FPropertyBindingDataView& OutDataView) const override;
	virtual const FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() const override
	{
		return &EditorBindings;
	}

	virtual FPropertyBindingBindingCollection* GetEditorPropertyBindings() override
	{
		return &EditorBindings;
	}

	virtual const FPropertyBindingBindingCollection* GetEditorPropertyBindings() const override
	{
		return &EditorBindings;
	}

	virtual FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() override
	{
		return &EditorBindings;
	}

	virtual FStateTreeBindableStructDesc FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const override;

	virtual bool CanCreateParameter(const FGuid InStructID) const override;
	virtual void CreateParametersForStruct(const FGuid InStructID, TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) override;
	virtual void OnPropertyBindingChanged(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	virtual void AppendBindablePropertyFunctionStructs(TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& InOutStructs) const override;
	//~ End IStateTreeEditorPropertyBindingsOwner interface

	/**
	 * Returns the description for the node for UI.
	 * Handles the name override logic, figures out required data for the GetDescription() call, and handles the fallbacks.
	 * @return description for the node.
	 */
	FText GetNodeDescription(const FStateTreeEditorNode& Node, const EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const;

#if WITH_EDITOR
	using FReplacementObjectMap = TMap<UObject*, UObject*>;
	void OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap);
	void OnUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct);
	void OnParametersChanged(const UStateTree& StateTree);
	void OnStateParametersChanged(const UStateTree& StateTree, const FGuid StateID);
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
#endif

	/** @return the public parameters ID that could be used for bindings within the Tree. */
	FGuid GetRootParametersGuid() const
	{
		return RootParametersGuid;
	}

	/** @return the public parameters that could be used for bindings within the Tree. */
	virtual const FInstancedPropertyBag& GetRootParametersPropertyBag() const
	{
		return RootParameterPropertyBag;
	}

	/** @returns parent state of a struct, or nullptr if not found. */
	const UStateTreeState* GetStateByStructID(const FGuid TargetStructID) const;

	/** @returns state based on its ID, or nullptr if not found. */
	const UStateTreeState* GetStateByID(const FGuid StateID) const;

	/** @returns mutable state based on its ID, or nullptr if not found. */
	UStateTreeState* GetMutableStateByID(const FGuid StateID);

	/** @returns the IDs and instance values of all bindable structs in the StateTree. */
	void GetAllStructValues(TMap<FGuid, const FStateTreeDataView>& OutAllValues) const;

	/** @returns the IDs and instance values of all bindable structs in the StateTree. */
	void GetAllStructValues(TMap<FGuid, const FPropertyBindingDataView>& OutAllValues) const;

	/**
	* Iterates over all structs that are related to binding
	* @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	*/
	EStateTreeVisitor VisitHierarchy(TFunctionRef<EStateTreeVisitor(UStateTreeState& State, UStateTreeState* ParentState)> InFunc) const;

	/**
	 * Iterates over all structs at the global level (context, tree parameters, evaluators, global tasks) that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	EStateTreeVisitor VisitGlobalNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const;

	/**
	 * Iterates over all structs in the state hierarchy that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	EStateTreeVisitor VisitHierarchyNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const;

	/**
	 * Iterates over all structs that are related to binding.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	EStateTreeVisitor VisitAllNodes(TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const;

	/**
	 * Iterates over all nodes in a given state.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	EStateTreeVisitor VisitStateNodes(const UStateTreeState& State, TFunctionRef<EStateTreeVisitor(const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const;

	/**
	 * Iterates recursively over all property functions of the provided node. Also nested ones.
	 * @param InFunc function called at each node, should return Continue if visiting is continued or Break to stop.
	 */
	EStateTreeVisitor VisitStructBoundPropertyFunctions(FGuid StructID, const FString& StatePath, TFunctionRef<EStateTreeVisitor(const FStateTreeEditorNode& EditorNode, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const;

	/**
	 * Returns array of nodes along the execution path, up to the TargetStruct.
	 * @param Path The states to visit during the check
	 * @param TargetStructID The ID of the node where to stop.
	 * @param OutStructDescs Array of nodes accessible on the given path.
	 */
	void GetAccessibleStructsInExecutionPath(const TConstArrayView<const UStateTreeState*> Path, const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const;

	UE_DEPRECATED(5.6, "Use GetAccessibleStructsInExecutionPath instead")
	void GetAccessibleStruct(const TConstArrayView<const UStateTreeState*> Path, const FGuid TargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
	{
		GetAccessibleStructsInExecutionPath(Path, TargetStructID, OutStructDescs);
	}

	void ReparentStates();
	
	// StateTree Builder API

	/**
	 * Adds new Subtree with specified name.
	 * @return Pointer to the new Subtree.
	 */
	UStateTreeState& AddSubTree(const FName Name)
	{
		UStateTreeState* SubTreeState = NewObject<UStateTreeState>(this, FName(), RF_Transactional);
		check(SubTreeState);
		SubTreeState->Name = Name;
		SubTrees.Add(SubTreeState);
		return *SubTreeState;
	}

	/**
	 * Adds new Subtree named "Root".
	 * @return Pointer to the new Subtree.
	 */
	UStateTreeState& AddRootState()
	{
		return AddSubTree(FName(TEXT("Root")));
	}

	/**
	 * Adds Evaluator of specified type.
	 * @return reference to the new Evaluator. 
	 */
	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddEvaluator(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& EditorNode = Evaluators.AddDefaulted_GetRef();
		EditorNode.ID = FGuid::NewGuid();
		EditorNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		T& Eval = EditorNode.Node.GetMutable<T>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Eval.GetInstanceDataType()))
		{
			EditorNode.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(EditorNode);
	}

	/**
	 * Adds Global Task of specified type.
	 * @return reference to the new task. 
	 */
	template<typename T, typename... TArgs>
	TStateTreeEditorNode<T>& AddGlobalTask(TArgs&&... InArgs)
	{
		FStateTreeEditorNode& EditorNode = GlobalTasks.AddDefaulted_GetRef();
		EditorNode.ID = FGuid::NewGuid();
		EditorNode.Node.InitializeAs<T>(Forward<TArgs>(InArgs)...);
		const FStateTreeNodeBase& Task = EditorNode.Node.Get<FStateTreeNodeBase>();
		if (const UScriptStruct* InstanceType = Cast<const UScriptStruct>(Task.GetInstanceDataType()))
		{
			EditorNode.Instance.InitializeAs(InstanceType);
		}
		return static_cast<TStateTreeEditorNode<T>&>(EditorNode);
	}

	/**
	 * Adds property binding between two structs.
	 */
	void AddPropertyBinding(const FPropertyBindingPath& SourcePath, const FPropertyBindingPath& TargetPath)
	{
		EditorBindings.AddBinding(SourcePath, TargetPath);
	}

	/**
	 * Adds property binding to PropertyFunction of provided type.
	 */
	void AddPropertyBinding(const UScriptStruct* PropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> SourcePathSegments, const FPropertyBindingPath& TargetPath)
	{
		EditorBindings.AddFunctionBinding(PropertyFunctionNodeStruct, SourcePathSegments, TargetPath);
	}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath)
	{
		EditorBindings.AddBinding(SourcePath, TargetPath);
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const UScriptStruct* PropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> SourcePathSegments, const FStateTreePropertyPath& TargetPath)
	{
		EditorBindings.AddFunctionBinding(PropertyFunctionNodeStruct, SourcePathSegments, TargetPath);
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * Adds property binding between two structs.
	 */
	bool AddPropertyBinding(const FStateTreeEditorNode& SourceNode, const FString SourcePathStr, const FStateTreeEditorNode& TargetNode, const FString TargetPathStr)
	{
		FPropertyBindingPath SourcePath;
		FPropertyBindingPath TargetPath;
		SourcePath.SetStructID(SourceNode.ID);
		TargetPath.SetStructID(TargetNode.ID);
		if (SourcePath.FromString(SourcePathStr) && TargetPath.FromString(TargetPathStr))
		{
			EditorBindings.AddBinding(SourcePath, TargetPath);
			return true;
		}
		return false;
	}

#if WITH_STATETREE_TRACE_DEBUGGER
	bool HasAnyBreakpoint(FGuid ID) const;
	bool HasBreakpoint(FGuid ID, EStateTreeBreakpointType BreakpointType) const;
	const FStateTreeEditorBreakpoint* GetBreakpoint(FGuid ID, EStateTreeBreakpointType BreakpointType) const;
	void AddBreakpoint(FGuid ID, EStateTreeBreakpointType BreakpointType);
	bool RemoveBreakpoint(FGuid ID, EStateTreeBreakpointType BreakpointType);
#endif // WITH_STATETREE_TRACE_DEBUGGER

	// ~StateTree Builder API

	/**
	 * Attempts to find a Color matching the provided Color Key
	 */
	const FStateTreeEditorColor* FindColor(const FStateTreeEditorColorRef& ColorRef) const
	{
		return Colors.Find(FStateTreeEditorColor(ColorRef));
	}

	virtual void CreateRootProperties(TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs) { UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(InOutCreationDescs, RootParameterPropertyBag); }

private:
	virtual EStateTreeVisitor EnumerateBindablePropertyFunctionNodes(TFunctionRef<EStateTreeVisitor(const UScriptStruct* NodeStruct, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const override;

	void FixObjectInstance(TSet<UObject*>& SeenObjects, UObject& Outer, FStateTreeEditorNode& Node);
	void FixObjectNodes();
	void FixDuplicateIDs();
	void DuplicateIDs();
	void UpdateBindingsInstanceStructs();
	void CallPostLoadOnNodes();

#if WITH_EDITORONLY_DATA
	FDelegateHandle OnObjectsReinstancedHandle;
	FDelegateHandle OnUserDefinedStructReinstancedHandle;
	FDelegateHandle OnParametersChangedHandle;
	FDelegateHandle OnStateParametersChangedHandle;
#endif

public:
	/** Schema describing which inputs, evaluators, and tasks a StateTree can contain */	
	UPROPERTY(EditDefaultsOnly, Instanced, Category = Common)
	TObjectPtr<UStateTreeSchema> Schema = nullptr;

	/** Public parameters that could be used for bindings within the Tree. */
	UE_DEPRECATED(5.6, "Public access to RootParameters is deprecated. Use GetRootParametersPropertyBag")
	UPROPERTY(meta = (DeprecatedProperty))
	FStateTreeStateParameters RootParameters;

private:
	/** Public parameters ID that could be used for bindings within the Tree. */
	UPROPERTY()
	FGuid RootParametersGuid;

	/** Public parameters property bag that could be used for bindings within the Tree. */
	UPROPERTY(EditDefaultsOnly, Category = Parameters)
	FInstancedPropertyBag RootParameterPropertyBag;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Evaluators", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeEvaluatorBase", BaseClass = "/Script/StateTreeModule.StateTreeEvaluatorBlueprintBase"))
	TArray<FStateTreeEditorNode> Evaluators;

	UPROPERTY(EditDefaultsOnly, Category = "Global Tasks", meta = (BaseStruct = "/Script/StateTreeModule.StateTreeTaskBase", BaseClass = "/Script/StateTreeModule.StateTreeTaskBlueprintBase"))
	TArray<FStateTreeEditorNode> GlobalTasks;

	UPROPERTY(EditDefaultsOnly, Category = "Global Tasks")
	EStateTreeTaskCompletionType GlobalTasksCompletion = EStateTreeTaskCompletionType::Any;

	UPROPERTY()
	FStateTreeEditorPropertyBindings EditorBindings;

	/** Color Options to assign to a State */
	UPROPERTY(EditDefaultsOnly, Category = "Theme")
	TSet<FStateTreeEditorColor> Colors;

	/** Top level States. */
	UPROPERTY(Instanced)
	TArray<TObjectPtr<UStateTreeState>> SubTrees;

	/**
	 * Transient list of breakpoints added in the debugging session.
	 * These will be lost if the asset gets reloaded.
	 * If there is eventually a change to make those persist with the asset
	 * we need to prune all dangling breakpoints after states/tasks got removed.
	 */
	UPROPERTY(Transient)
	TArray<FStateTreeEditorBreakpoint> Breakpoints;

	/**
	 * List of the previous compiled delegate dispatchers.
	 * Saved in the editor data to be duplicated transient.
	 */
	UPROPERTY(DuplicateTransient)
	TArray<FStateTreeEditorDelegateDispatcherCompiledBinding> CompiledDispatchers;

	friend class FStateTreeEditorDataDetails;
};


UCLASS()
class UQAStateTreeEditorData : public UStateTreeEditorData
{
	GENERATED_BODY()
};