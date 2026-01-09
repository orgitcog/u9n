// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variables/RigVMDispatch_CallObjectAccessorFunction.h"
#include "RigVMCore/RigVMStruct.h"
#include "Variables/AnimNextSoftFunctionPtr.h"

FRigVMDispatch_CallObjectAccessorFunctionBase::FRigVMDispatch_CallObjectAccessorFunctionBase()
{
	FactoryScriptStruct = StaticStruct();
}

FName FRigVMDispatch_CallObjectAccessorFunctionBase::GetArgumentNameForOperandIndex(int32 InOperandIndex, int32 InTotalOperands) const
{
	static const FName ArgumentNames[] =
	{
		ObjectName,
		FunctionName,
		ValueName,
	};
	check(InTotalOperands == UE_ARRAY_COUNT(ArgumentNames));
	return ArgumentNames[InOperandIndex];
}

void FRigVMDispatch_CallObjectAccessorFunctionBase::RegisterDependencyTypes_NoLock(FRigVMRegistry_NoLock& InRegistry) const
{
	static UScriptStruct* const AllowedStructTypes[] =
	{
		FAnimNextSoftFunctionPtr::StaticStruct(),
	};

	InRegistry.RegisterStructTypes_NoLock(AllowedStructTypes);
	for(UScriptStruct* const ScriptStruct : AllowedStructTypes)
	{
		InRegistry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(ScriptStruct));
	}

	static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
	{
		{ UObject::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
	};

	InRegistry.RegisterObjectTypes_NoLock(AllowedObjectTypes);

	for(const TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation>& ObjectType : AllowedObjectTypes)
	{
		InRegistry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(ObjectType.Key));
	}
}

const TArray<FRigVMTemplateArgumentInfo>& FRigVMDispatch_CallObjectAccessorFunctionBase::GetArgumentInfos() const
{
	static TArray<FRigVMTemplateArgumentInfo> Infos;
	if(Infos.IsEmpty())
	{
		static const TArray<FRigVMTemplateArgument::ETypeCategory> ValueCategories =
		{
			FRigVMTemplateArgument::ETypeCategory_SingleAnyValue,
			FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue
		};

		const FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForRead();
		Infos.Emplace(ObjectName, ERigVMPinDirection::Input, Registry.GetTypeIndex_NoLock<UObject>());
		Infos.Emplace(FunctionName, ERigVMPinDirection::Input, Registry.GetTypeIndex_NoLock<FAnimNextSoftFunctionPtr>());
		Infos.Emplace(ValueName, ERigVMPinDirection::Output, ValueCategories);
	}

	return Infos;
}

FRigVMTemplateTypeMap FRigVMDispatch_CallObjectAccessorFunctionBase::OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	const FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForRead();
	
	FRigVMTemplateTypeMap Types;
	Types.Add(ObjectName, Registry.GetTypeIndex_NoLock<UObject>());
	Types.Add(FunctionName, Registry.GetTypeIndex_NoLock<FAnimNextSoftFunctionPtr>());
	Types.Add(ValueName, InTypeIndex);
	return Types;
}

FRigVMDispatch_CallObjectAccessorFunctionNative::FRigVMDispatch_CallObjectAccessorFunctionNative()
{
	FactoryScriptStruct = StaticStruct();
}

void FRigVMDispatch_CallObjectAccessorFunctionNative::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	UObject* ObjectPtr = *reinterpret_cast<UObject**>(Handles[0].GetData());
	if(ObjectPtr == nullptr)
	{
		// Something failed to resolve upstream, OK to just skip this work
		return;
	}

	FAnimNextSoftFunctionPtr* SoftFunctionPtr = reinterpret_cast<FAnimNextSoftFunctionPtr*>(Handles[1].GetData());
	UFunction* Function = SoftFunctionPtr->SoftObjectPtr.Get();
	if(Function == nullptr || Function->NumParms != 1)
	{
		return;
	}

	const FProperty* ReturnValueProperty = CastField<FProperty>(Function->GetReturnProperty());
	if(ReturnValueProperty == nullptr)
	{
		return;
	}

	const FProperty* TargetProperty = Handles[2].GetResolvedProperty();
	checkSlow(TargetProperty);
	check(TargetProperty->GetClass() == ReturnValueProperty->GetClass());
	uint8* TargetAddress = Handles[2].GetData();
	checkSlow(TargetAddress);

	FFrame Stack(ObjectPtr, Function, nullptr, nullptr, Function->ChildProperties);
	Function->Invoke(ObjectPtr, Stack, TargetAddress);
}

FRigVMDispatch_CallObjectAccessorFunctionScript::FRigVMDispatch_CallObjectAccessorFunctionScript()
{
	FactoryScriptStruct = StaticStruct();
}

void FRigVMDispatch_CallObjectAccessorFunctionScript::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray RigVMBranches)
{
	UObject* ObjectPtr = *reinterpret_cast<UObject**>(Handles[0].GetData());
	if(ObjectPtr == nullptr)
	{
		// Something failed to resolve upstream, OK to just skip this work
		return;
	}

	FAnimNextSoftFunctionPtr* SoftFunctionPtr = reinterpret_cast<FAnimNextSoftFunctionPtr*>(Handles[1].GetData());
	UFunction* Function = SoftFunctionPtr->SoftObjectPtr.Get();
	if(Function == nullptr || Function->NumParms != 1)
	{
		return;
	}

	const FProperty* ReturnValueProperty = CastField<FProperty>(Function->GetReturnProperty());
	if(ReturnValueProperty == nullptr)
	{
		return;
	}

	const FProperty* TargetProperty = Handles[2].GetResolvedProperty();
	checkSlow(TargetProperty);
	check(TargetProperty->GetClass() == ReturnValueProperty->GetClass());
	uint8* TargetAddress = Handles[2].GetData();
	checkSlow(TargetAddress);

	check(ObjectPtr->GetClass()->IsChildOf(Function->GetOuterUClass()));
	ObjectPtr->ProcessEvent(Function, TargetAddress);
}

