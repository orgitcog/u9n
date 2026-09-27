// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "AnimNextExecuteContext.h"
#include "Chooser.h"
#include "ControlRigDefines.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "Units/RigUnit.h"

#include "RigUnit_EvaluateChooser.generated.h"


/*
 * Evaluates a chooser table and outputs either an object or an array of objects depending on what the result pin is connected to
 * Compatible with both ControlRig and AnimNext graphs
 */
USTRUCT(meta = (DisplayName = "Evaluate Chooser", Category="Choosers", NodeColor = "0.8, 0, 0.2, 1"))
struct FRigVMDispatch_EvaluateChooser : public FRigVMDispatchFactory
{
	GENERATED_BODY()

	FRigVMDispatch_EvaluateChooser();
	
private:
	friend struct UE::AnimNext::UncookedOnly::FUtils;

	virtual UScriptStruct* GetExecuteContextStruct() const { return FRigVMExecuteContext::StaticStruct(); }
	virtual FName GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const override;
	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;

	virtual bool IsSingleton() const override { return true; }

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override
	{
		return &FRigVMDispatch_EvaluateChooser::Execute;
	}
	
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);
	
	static const FName ChooserName;
	static const FName ContextObjectName;
	static const FName ResultName;
};

/*
 * Evaluates a Chooser Table and outputs the selected UObject
 */
USTRUCT(meta = (Abstract, Varying))
struct FRigUnit_EvaluateChooser : public FRigVMStruct
{
	GENERATED_BODY()

	UPROPERTY(meta = (Input))
	TObjectPtr<UObject> ContextObject;
	
	UPROPERTY(EditAnywhere, Category = "Chooser", meta = (Input))
	TObjectPtr<UChooserTable> Chooser;

	UPROPERTY(meta = (Output))
	TObjectPtr<UObject> Result;
};

/*
 * Evaluates a Chooser Table in the context of ControlRig
 * deprecated in favor of RigVMDispatch_EvaluateChooser
 */
USTRUCT(meta = (ExecuteContext="FControlRigExecuteContext"))
struct FRigUnit_EvaluateChooser_ControlRig : public FRigUnit_EvaluateChooser
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;
};

/*
 * Evaluates a Chooser Table in the context of AnimNext
 * deprecated in favor of RigVMDispatch_EvaluateChooser
 */
USTRUCT(meta = (ExecuteContext="FAnimNextExecuteContext"))
struct FRigUnit_EvaluateChooser_AnimNext : public FRigUnit_EvaluateChooser
{
	GENERATED_BODY()

	/** Execute logic for this rig unit */
	RIGVM_METHOD()
	virtual void Execute() override;
};