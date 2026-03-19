// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNextStateTreeContext.h"
#include "AnimNextStateTreeTypes.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "RigVMCore/RigVMExternalVariable.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "RigVMCore/RigVMMemoryStorageStruct.h"
#include "RigVMUtils/RigVariableIndexCache.h"
#include "StateTreeTaskBase.h"
#include "StateTreeReference.h"
#include "Templates/SubclassOf.h"

#include "AnimNextStateTreeRigVMTaskBase.generated.h"

class UAnimNextDataInterface;
class UAnimNextDataInterface_EditorData;
class URigVMGraph;
class UStateTreeState;

struct FAnimNextStateTreeProgrammaticFunctionHeaderParams;
struct FStateTreeBindableStructDesc;
struct FStateTreeExecutionContext;
struct FRigVMClient;

USTRUCT()
struct ANIMNEXTSTATETREE_API FAnimNextStateTreeRigVMTaskInstanceData
{
	GENERATED_BODY()

public:

	/** Per instance copy of ST function arguments we will copy into our RigVM before execution */
	UPROPERTY(EditAnywhere, Category = "Parameter", Meta = (FixedLayout, ShowOnlyInnerProperties))
	FRigVMMemoryStorageStruct ParamData = FRigVMMemoryStorageStruct();

public:

	/** Helper struct to store parent RigVM variable indicies, */
	FRigVariableIndexCache VariableIndexCache = FRigVariableIndexCache();
};

/**
 * Wrapper for RigVM based Tasks. 
 */
USTRUCT()
struct ANIMNEXTSTATETREE_API FAnimNextStateTreeRigVMTaskBase : public FAnimNextStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FAnimNextStateTreeRigVMTaskInstanceData;

public:

	//~ Begin FStateTreeTaskBase Interface
	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	//~ End FStateTreeTaskBase Interface

	//~ Begin FStateTreeNodeBase Interface
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
	virtual void PostEditNodeChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView) override;
#endif
	//~ End FStateTreeNodeBase Interface
	
#if WITH_EDITOR
	/** Adds Task function headers to parent StateTree RigVM */
	virtual void GetProgrammaticFunctionHeaders(FAnimNextStateTreeProgrammaticFunctionHeaderParams& InProgrammaticFunctionHeaderParams, const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc);
#endif

public:

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Parameter", Meta = (GetOptions = "AnimNextStateTreeUncookedOnly.AnimNextStateTreeFunctionLibraryHelper.GetExposedAnimNextFunctionNames"))
	FName TaskFunctionName = NAME_None;

	/** Name of the Result of function */
	UPROPERTY()
	FName ResultName = NAME_None;
#endif

	UPROPERTY()
	FRigVMGraphFunctionHeader RigVMFunctionHeader = FRigVMGraphFunctionHeader();

	/** Owning state name, populated during programmatic graph creation */
	UPROPERTY()
	FName StateName = NAME_None;

	/** External node ID defined by owning state tree, populated during programmatic graph creation */
	UPROPERTY()
	FGuid NodeId = FGuid();

	UPROPERTY()
	FName InternalEventName = NAME_None;

	UPROPERTY()
	FName InternalResultName = NAME_None;

public:
	TStateTreeExternalDataHandle<FAnimNextStateTreeTraitContext> TraitContextHandle;
};