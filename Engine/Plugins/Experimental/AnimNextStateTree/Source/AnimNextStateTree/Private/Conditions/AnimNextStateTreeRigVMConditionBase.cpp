// Copyright Epic Games, Inc. All Rights Reserved.

#include "Conditions/AnimNextStateTreeRigVMConditionBase.h"
#include "AnimNextExecuteContext.h"
#include "Graph/AnimNextGraphContextData.h"
#include "Graph/AnimNextGraphInstance.h"
#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "StructUtils/PropertyBag.h"
#include "TraitCore/ExecutionContext.h"

#if WITH_EDITOR
#include "AnimNextStateTreeEditorOnlyTypes.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "RigVMFunctions/RigVMFunctionDefines.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "UncookedOnlyUtils.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "AnimNextStateTreeRigVMConditionBase"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextStateTreeRigVMConditionBase)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FAnimNextStateTreeRigVMConditionBase

bool FAnimNextStateTreeRigVMConditionBase::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(TraitContextHandle);
	return true;
}

bool FAnimNextStateTreeRigVMConditionBase::TestCondition(FStateTreeExecutionContext& Context) const
{
	if (FAnimNextStateTreeRigVMConditionInstanceData* InstanceData = Context.GetInstanceDataPtr<FAnimNextStateTreeRigVMConditionInstanceData>(*this))
	{
		FAnimNextStateTreeTraitContext& ExecContext = Context.GetExternalData(TraitContextHandle);
		const UE::AnimNext::FExecutionContext* AnimExecContext = ExecContext.GetAnimExecuteContext();

		// Need const cast as VM execution isn't const.
		if (TObjectPtr<UAnimNextDataInterface> RigVMInstance = const_cast<UAnimNextDataInterface*>(AnimExecContext->GetRootGraphInstance().GetDataInterface()))
		{
			// Cache RigVM variable indexes, which can vary as they are parent derived
			if (!InstanceData->VariableIndexCache.IsIndexCacheInitialized() && !InstanceData->VariableIndexCache.TryPopulateIndexCache(InstanceData->ParamData, RigVMInstance, RigVMFunctionHeader.Name, InternalResultName))
			{
				return false;
			}

			// Prior to execution, copy over ST binding values to RigVM variables
			FAnimNextGraphInstance& GraphInstance = AnimExecContext->GetRootGraphInstance();
			TArrayView<const uint8> ArgumentIndexes = InstanceData->VariableIndexCache.GetVMArgumentIndexes();
			TConstArrayView<FPropertyBagPropertyDesc> StateTreePropertyDescs = InstanceData->ParamData.GetPropertyBagStruct()->GetPropertyDescs();
			TConstArrayView<FPropertyBagPropertyDesc> AnimNextGraphPropertyDescs = GraphInstance.GetVariables().GetPropertyBagStruct()->GetPropertyDescs();
			FStructView StateTreeParamData = InstanceData->ParamData.GetMutableValue();

			for (int32 Index = 0; Index < ArgumentIndexes.Num(); Index++)
			{
				uint8 RigVMExternalVariableIndex = ArgumentIndexes[Index];

				const FPropertyBagPropertyDesc& StateTreePropertyDesc = StateTreePropertyDescs[Index];
				void* SourceAddress = StateTreePropertyDesc.CachedProperty->ContainerPtrToValuePtr<void*>(StateTreeParamData.GetMemory());

				const FPropertyBagPropertyDesc& AnimNextGraphDesc = AnimNextGraphPropertyDescs[RigVMExternalVariableIndex];
				GraphInstance.GetMutableVariables().SetValue(AnimNextGraphDesc.Name, StateTreePropertyDesc.CachedProperty, StateTreeParamData.GetMemory());
			}

			FRigVMExtendedExecuteContext& ExtendedExecuteContext = GraphInstance.GetExtendedExecuteContext();
			FAnimNextExecuteContext& AnimNextContext = GraphInstance.GetExtendedExecuteContext().GetPublicDataSafe<FAnimNextExecuteContext>();

			FAnimNextGraphContextData ContextData(GraphInstance.GetModuleInstance(), &GraphInstance);
			UE::AnimNext::FScopedExecuteContextData ContextDataScope(AnimNextContext, ContextData);

			bool bResult = false;
			RigVMInstance->GetVM()->ExecuteVM(ExtendedExecuteContext, InternalEventName);
			if (ensure(ExtendedExecuteContext.ExternalVariableRuntimeData.IsValidIndex(InstanceData->VariableIndexCache.GetVMResultIndex())))
			{
				bResult = (bool)*ExtendedExecuteContext.ExternalVariableRuntimeData[InstanceData->VariableIndexCache.GetVMResultIndex()].Memory;
			}

			return bResult;
		}
	}

	return true;
}

#if WITH_EDITOR
FText FAnimNextStateTreeRigVMConditionBase::GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const
{
	return LOCTEXT("AnimNextStateTreeConditon_Desc", "RigVM function driven condition");
}

void FAnimNextStateTreeRigVMConditionBase::PostEditNodeChangeChainProperty(const FPropertyChangedChainEvent& PropertyChangedEvent, FStateTreeDataView InstanceDataView)
{
	if (PropertyChangedEvent.GetMemberPropertyName() == GET_MEMBER_NAME_CHECKED(FAnimNextStateTreeRigVMConditionBase, ConditionFunctionName))
	{
		// Function Name selected has changed. Update param struct & result / event names used during execution.
		if (FAnimNextStateTreeRigVMConditionInstanceData* InstanceData = InstanceDataView.GetMutablePtr<FAnimNextStateTreeRigVMConditionInstanceData>())
		{
			// @TODO: This relies on the function name being unique (Ex: In a workspace). For now that's okay. Later on however we will want to use a more robust function picker
			auto GetRigVMFunctionHeader = [](FName InName)
			{
				FRigVMGraphFunctionHeader Result = FRigVMGraphFunctionHeader();

				TMap<FAssetData, FRigVMGraphFunctionHeaderArray> FunctionExports;
				UE::AnimNext::UncookedOnly::FUtils::GetExportedFunctionsFromAssetRegistry(UE::AnimNext::AnimNextPublicGraphFunctionsExportsRegistryTag, FunctionExports);
				UE::AnimNext::UncookedOnly::FUtils::GetExportedFunctionsFromAssetRegistry(UE::AnimNext::ControlRigAssetPublicGraphFunctionsExportsRegistryTag, FunctionExports);

				for (const auto& Export : FunctionExports.Array())
				{
					for (const FRigVMGraphFunctionHeader& FunctionHeader : Export.Value.Headers)
					{
						if (InName == FunctionHeader.Name)
						{
							Result = FunctionHeader;
							return Result;
						}
					}
				}

				return Result;
			};

			RigVMFunctionHeader = GetRigVMFunctionHeader(ConditionFunctionName);
			InstanceData->ParamData = FRigVMMemoryStorageStruct();

			TArray<FRigVMPropertyDescription> RigVMPropertyDescriptions;
			for (const FRigVMGraphFunctionArgument& Argument : RigVMFunctionHeader.Arguments)
			{
				if (Argument.Direction == ERigVMPinDirection::Input)
				{
					FString CppTypeString = Argument.CPPType.ToString();
					UObject* CppTypeObject = Argument.CPPTypeObject.Get();
					FRigVMPropertyDescription RigVMPropertyDescription = FRigVMPropertyDescription(Argument.Name, CppTypeString, CppTypeObject, Argument.DefaultValue);
					RigVMPropertyDescriptions.Add(RigVMPropertyDescription);
				}
				else if (Argument.Direction == ERigVMPinDirection::Output)
				{
					ensure(Argument.CPPType == RigVMTypeUtils::BoolTypeName);
					ResultName = Argument.Name;
				}
			}
		
			InstanceData->ParamData.AddProperties(RigVMPropertyDescriptions);
		}
	}
}

void FAnimNextStateTreeRigVMConditionBase::GetProgrammaticFunctionHeaders(FAnimNextStateTreeProgrammaticFunctionHeaderParams& InProgrammaticFunctionHeaderParams, const UStateTreeState* State, const FStateTreeBindableStructDesc& Desc)
{
	using namespace UE::AnimNext::UncookedOnly;

	FAnimNextGetFunctionHeaderCompileContext& OutCompileContext = InProgrammaticFunctionHeaderParams.OutCompileContext;
	
	StateName = State->Name;
	NodeId = Desc.ID;
	InternalResultName = FName(FUtils::MakeFunctionWrapperVariableName(RigVMFunctionHeader.Name, ResultName));
	InternalEventName = FName(FUtils::MakeFunctionWrapperEventName(RigVMFunctionHeader.Name));

	FAnimNextProgrammaticFunctionHeader AnimNextFunctionHeader = {};
	AnimNextFunctionHeader.Wrapped = RigVMFunctionHeader;
	AnimNextFunctionHeader.bGenerateParamVariables = true;
	AnimNextFunctionHeader.bGenerateReturnVariables = true;
	OutCompileContext.GetMutableFunctionHeaders().Add(AnimNextFunctionHeader);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
