// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMDispatch_Core.h"
#include "RigVMCore/RigVMByteCode.h"
#include "RigVMDispatch_MakeStruct.generated.h"

USTRUCT(meta=(DisplayName = "Make", Category = "Core", Keywords = "Compose,Composition,Create,Constant", NodeColor = "1,1,1,1"))
struct FRigVMDispatch_MakeStruct : public FRigVMDispatch_CoreBase
{
	GENERATED_BODY()

public:
	FRigVMDispatch_MakeStruct()
	{
		FactoryScriptStruct = StaticStruct();
	}
	
	RIGVM_API virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	RIGVM_API virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;
#if WITH_EDITOR
	RIGVM_API virtual FString GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const override;
	RIGVM_API virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	RIGVM_API virtual FString GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const override;
	RIGVM_API virtual FString GetKeywords() const override;
#endif

protected:
	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override { return &FRigVMDispatch_MakeStruct::Execute; }
	static RIGVM_API void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches);

	static inline const FLazyName ElementsName = FLazyName(TEXT("Elements"));
	static inline const FLazyName StructName = FLazyName(TEXT("Struct"));

	friend struct FRigVMDispatch_BreakStruct;
	friend class URigVMController;
};

USTRUCT(meta=(DisplayName = "Break", Keywords = "Decompose,Decomposition"))
struct FRigVMDispatch_BreakStruct : public FRigVMDispatch_MakeStruct
{
	GENERATED_BODY()

public:
	FRigVMDispatch_BreakStruct()
	{
		FactoryScriptStruct = StaticStruct();
	}

	RIGVM_API virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
#if WITH_EDITOR
	RIGVM_API virtual FText GetNodeTooltip(const FRigVMTemplateTypeMap& InTypes) const override;
	RIGVM_API virtual FString GetKeywords() const override;
#endif
};

