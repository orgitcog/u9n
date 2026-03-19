// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Hierarchy/RigUnit_Component.h"

#include "Animation/AttributeTypes.h"
#include "Units/RigUnitContext.h"
#include "Rigs/RigHierarchyController.h"
#include "UObject/UObjectIterator.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Component)

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_ComponentBase
//////////////////////////////////////////////////////////////////////////////////////////////////////

bool FRigDispatch_ComponentBase::IsTypeSupported(const TRigVMTypeIndex& InTypeIndex)
{
	const FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForRead();
	const FRigVMTemplateArgumentType& InType = Registry.GetType_NoLock(InTypeIndex);

	if (IsValid(InType.CPPTypeObject))
	{
		if (const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(InType.CPPTypeObject))
		{
			return ScriptStruct != FRigBaseComponent::StaticStruct() &&
				ScriptStruct->IsChildOf(FRigBaseComponent::StaticStruct());
		}
	}
	return false;
}

const TRigVMTypeIndex& FRigDispatch_ComponentBase::GetElementKeyType()
{
	static FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForWrite();
	static TRigVMTypeIndex RigElementKeyType = Registry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(FRigElementKey::StaticStruct()), false);
	return RigElementKeyType;
}

const TRigVMTypeIndex& FRigDispatch_ComponentBase::GetComponentKeyType()
{
	static FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForWrite();
	static TRigVMTypeIndex RigComponentKeyType = Registry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(FRigComponentKey::StaticStruct()), false);
	return RigComponentKeyType;
}

void FRigDispatch_ComponentBase::RegisterDependencyTypes_NoLock(FRigVMRegistry_NoLock& InRegistry) const
{
	Super::RegisterDependencyTypes_NoLock(InRegistry);

	FRigVMRegistry_NoLock& Registry = FRigVMRegistry_NoLock::GetForWrite();
	(void)Registry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(FRigComponentKey::StaticStruct()), true);
	
	for (TObjectIterator<UScriptStruct> ScriptStructIt; ScriptStructIt; ++ScriptStructIt)
	{
		UScriptStruct* ScriptStruct = *ScriptStructIt;
		if(ScriptStruct != FRigBaseComponent::StaticStruct() && ScriptStruct->IsChildOf(FRigBaseComponent::StaticStruct()))
		{
			(void)Registry.FindOrAddType_NoLock(FRigVMTemplateArgumentType(ScriptStruct), true);
		}
	}
}

const TArray<FRigVMExecuteArgument>& FRigDispatch_ComponentBase::GetExecuteArguments_Impl(const FRigVMDispatchContext& InContext) const
{
	if(IsMutable() && ExecuteInfos.IsEmpty())
	{
		ExecuteInfos.Emplace(ExecuteArgName, ERigVMPinDirection::IO);
	}
	return ExecuteInfos;
}

#if WITH_EDITOR

FText FRigDispatch_ComponentBase::GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == NameArgName)
	{
		return NSLOCTEXT("FRigDispatch_ComponentBase", "NameArgTooltip", "The name of the component (can be empty)");
	}
	if(InArgumentName == ItemArgName)
	{
		return NSLOCTEXT("FRigDispatch_ComponentBase", "ItemArgTooltip", "The item for this component");
	}
	if(InArgumentName == KeyArgName)
	{
		return NSLOCTEXT("FRigDispatch_ComponentBase", "KeyArgTooltip", "The key of the component");
	}
	if(InArgumentName == ComponentArgName)
	{
		return NSLOCTEXT("FRigDispatch_ComponentBase", "ComponentArgTooltip", "The actual component");
	}
	if(InArgumentName == SuccessArgName)
	{
		return NSLOCTEXT("FRigDispatch_ComponentBase", "SuccessArgTooltip", "Returns true if the operation was successful.");
	}
	return FRigDispatchFactory::GetArgumentTooltip(InArgumentName, InTypeIndex);	
}

FString FRigDispatch_ComponentBase::GetArgumentDefaultValue(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == NameArgName)
	{
		return FString();
	}
	if(InArgumentName == ItemArgName)
	{
		const FRigElementKey DefaultItem(NAME_None, ERigElementType::Bone);
		FString DefaultValue;
		FRigElementKey::StaticStruct()->ExportText(DefaultValue, &DefaultItem, &DefaultItem, nullptr, PPF_None, nullptr);
		return DefaultValue;
	}
	return FRigDispatchFactory::GetArgumentDefaultValue(InArgumentName, InTypeIndex);
}

#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_SpawnComponent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_SpawnComponent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		ItemArgIndex = Infos.Emplace(ItemArgName, ERigVMPinDirection::Input, GetElementKeyType());
		NameArgIndex = Infos.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Input, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_SpawnComponent::IsTypeSupported));
		KeyArgIndex = Infos.Emplace(KeyArgName, ERigVMPinDirection::Output, GetComponentKeyType());
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Infos;
}

#if WITH_EDITOR

FString FRigDispatch_SpawnComponent::GetKeywords() const
{
	return FRigDispatch_ComponentBase::GetKeywords() + TEXT(",AddComponent,CreateComponent");
}

#endif

void FRigDispatch_SpawnComponent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray /* not needed */)
{
	const FRigDispatch_SpawnComponent* Factory = static_cast<const FRigDispatch_SpawnComponent*>(InContext.Factory);
		
	// unpack the memory
	const FRigElementKey& ElementKey = *(const FRigElementKey*)Handles[Factory->ItemArgIndex].GetData();
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const uint8* SourceComponent = Handles[Factory->ComponentArgIndex].GetData();
	FRigComponentKey& ComponentKey = *(FRigComponentKey*)Handles[Factory->KeyArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	ComponentKey = FRigComponentKey();
	bSuccess = false;

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	if(!Context.Hierarchy->Contains(ElementKey))
	{
#if WITH_EDITOR
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), FString::Printf(TEXT("Item %s not found."), *ElementKey.ToString()));
#endif
		return;
	}

	if(URigHierarchyController* Controller = Context.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, Context.GetInstructionIndex());
		ComponentKey = Controller->AddComponent(ComponentStruct, Name, ElementKey);
		if(ComponentKey.IsValid())
		{
			FRigBaseComponent* TargetComponent = Context.Hierarchy->FindComponent(ComponentKey);
			check(TargetComponent);
			
			// copy the public content over (members which are not UProperty will be skipped) 
			for (TFieldIterator<FProperty> It(ComponentStruct); It; ++It)
			{
				It->CopyCompleteValue_InContainer(TargetComponent, SourceComponent);
			}

			bSuccess = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_SpawnTopLevelComponent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_SpawnTopLevelComponent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		NameArgIndex = Infos.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Input, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_SpawnTopLevelComponent::IsTypeSupported));
		KeyArgIndex = Infos.Emplace(KeyArgName, ERigVMPinDirection::Output, GetComponentKeyType());
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	return Infos;
}

#if WITH_EDITOR

FString FRigDispatch_SpawnTopLevelComponent::GetKeywords() const
{
	return FRigDispatch_ComponentBase::GetKeywords() + TEXT(",AddComponent,CreateComponent,TopLevel,RootComponent");
}

#endif

void FRigDispatch_SpawnTopLevelComponent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray /* not needed */)
{
	const FRigDispatch_SpawnTopLevelComponent* Factory = static_cast<const FRigDispatch_SpawnTopLevelComponent*>(InContext.Factory);
		
	// unpack the memory
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const uint8* SourceComponent = Handles[Factory->ComponentArgIndex].GetData();
	FRigComponentKey& ComponentKey = *(FRigComponentKey*)Handles[Factory->KeyArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	ComponentKey = FRigComponentKey();
	bSuccess = false;

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	if(URigHierarchyController* Controller = Context.Hierarchy->GetController())
	{
		FRigHierarchyControllerInstructionBracket InstructionBracket(Controller, Context.GetInstructionIndex());
		ComponentKey = Controller->AddTopLevelComponent(ComponentStruct, Name);
		if(ComponentKey.IsValid())
		{
			FRigBaseComponent* TargetComponent = Context.Hierarchy->FindComponent(ComponentKey);
			check(TargetComponent);
			
			// copy the public content over (members which are not UProperty will be skipped) 
			for (TFieldIterator<FProperty> It(ComponentStruct); It; ++It)
			{
				It->CopyCompleteValue_InContainer(TargetComponent, SourceComponent);
			}

			bSuccess = true;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_GetComponentContent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_GetComponentContent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		KeyArgIndex = Infos.Emplace(KeyArgName, ERigVMPinDirection::Input, GetComponentKeyType());
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Output, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_GetComponentContent::IsTypeSupported));
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Infos;
}

void FRigDispatch_GetComponentContent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray)
{
	const FRigDispatch_GetComponentContent* Factory = static_cast<const FRigDispatch_GetComponentContent*>(InContext.Factory);
			
	// unpack the memory
	const FRigComponentKey& ComponentKey = *(const FRigComponentKey*)Handles[Factory->KeyArgIndex].GetData();
	uint8* TargetComponent = Handles[Factory->ComponentArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	const UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	const FRigBaseComponent* SourceComponent = Context.Hierarchy->FindComponent(ComponentKey);
	if(SourceComponent == nullptr)
	{
		ComponentStruct->InitializeDefaultValue(TargetComponent);
		// Since we return success/failure, this is not an error/warning
		bSuccess = false;
		return;
	}

	if(SourceComponent->GetScriptStruct() != ComponentStruct)
	{
		ComponentStruct->InitializeDefaultValue(TargetComponent);
#if WITH_EDITOR
		const FString Message = FString::Printf(TEXT("Component pin type (%s) doesn't match component in hierarchy (%s)."), *ComponentStruct->GetName(), *SourceComponent->GetScriptStruct()->GetName());
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), Message);
#endif
		bSuccess = false;
		return;
	}

	ComponentStruct->CopyScriptStruct(TargetComponent, SourceComponent, 1);
	bSuccess = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_SetComponentContent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_SetComponentContent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		KeyArgIndex = Infos.Emplace(KeyArgName, ERigVMPinDirection::Input, GetComponentKeyType());
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Input, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_SetComponentContent::IsTypeSupported));
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Infos;
}

void FRigDispatch_SetComponentContent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray)
{
	const FRigDispatch_SetComponentContent* Factory = static_cast<const FRigDispatch_SetComponentContent*>(InContext.Factory);
			
	// unpack the memory
	const FRigComponentKey& ComponentKey = *(const FRigComponentKey*)Handles[Factory->KeyArgIndex].GetData();
	const  uint8* SourceComponent = Handles[Factory->ComponentArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	const UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	FRigBaseComponent* TargetComponent = Context.Hierarchy->FindComponent(ComponentKey);
	if(TargetComponent == nullptr)
	{
#if WITH_EDITOR
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), FString::Printf(TEXT("%s not found."), *ComponentKey.ToString()));
#endif
		bSuccess = false;
		return;
	}

	if(TargetComponent->GetScriptStruct() != ComponentStruct)
	{
#if WITH_EDITOR
		const FString Message = FString::Printf(TEXT("Component pin type (%s) doesn't match component in hierarchy (%s)."), *ComponentStruct->GetName(), *TargetComponent->GetScriptStruct()->GetName());
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), Message);
#endif
		bSuccess = false;
		return;
	}

	// copy the public content over (members which are not UProperty will be skipped) 
	for (TFieldIterator<FProperty> It(ComponentStruct); It; ++It)
	{
		It->CopyCompleteValue_InContainer(TargetComponent, SourceComponent);
	}
	bSuccess = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_GetTopLevelComponentContent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_GetTopLevelComponentContent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		NameArgIndex = Infos.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Output, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_GetTopLevelComponentContent::IsTypeSupported));
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Infos;
}

#if WITH_EDITOR

FString FRigDispatch_GetTopLevelComponentContent::GetKeywords() const
{
	return FRigDispatch_ComponentBase::GetKeywords() + TEXT(",GetComponent,ReadComponent");
}

#endif

void FRigDispatch_GetTopLevelComponentContent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray)
{
	const FRigDispatch_GetTopLevelComponentContent* Factory = static_cast<const FRigDispatch_GetTopLevelComponentContent*>(InContext.Factory);
			
	// unpack the memory
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const FRigComponentKey ComponentKey(URigHierarchy::GetTopLevelComponentElementKey(), Name);
	uint8* TargetComponent = Handles[Factory->ComponentArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	const UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	const FRigBaseComponent* SourceComponent = Context.Hierarchy->FindComponent(ComponentKey);
	if(SourceComponent == nullptr)
	{
		ComponentStruct->InitializeDefaultValue(TargetComponent);
#if WITH_EDITOR
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), FString::Printf(TEXT("%s not found."), *ComponentKey.ToString()));
#endif
		bSuccess = false;
		return;
	}

	if(SourceComponent->GetScriptStruct() != ComponentStruct)
	{
		ComponentStruct->InitializeDefaultValue(TargetComponent);
#if WITH_EDITOR
		const FString Message = FString::Printf(TEXT("Component pin type (%s) doesn't match component in hierarchy (%s)."), *ComponentStruct->GetName(), *SourceComponent->GetScriptStruct()->GetName());
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), Message);
#endif
		bSuccess = false;
		return;
	}

	ComponentStruct->CopyScriptStruct(TargetComponent, SourceComponent, 1);
	bSuccess = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
// FRigDispatch_SetTopLevelComponentContent
//////////////////////////////////////////////////////////////////////////////////////////////////////

const TArray<FRigVMTemplateArgumentInfo>& FRigDispatch_SetTopLevelComponentContent::GetArgumentInfos() const
{
	if (Infos.IsEmpty())
	{
		NameArgIndex = Infos.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		ComponentArgIndex = Infos.Add(FRigVMTemplateArgumentInfo(ComponentArgName, ERigVMPinDirection::Input, {FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue}, &FRigDispatch_SetTopLevelComponentContent::IsTypeSupported));
		SuccessArgIndex = Infos.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	}
	
	return Infos;
}

#if WITH_EDITOR

FString FRigDispatch_SetTopLevelComponentContent::GetKeywords() const
{
	return FRigDispatch_ComponentBase::GetKeywords() + TEXT(",SetComponent,WriteComponent");
}

#endif

void FRigDispatch_SetTopLevelComponentContent::Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray)
{
	const FRigDispatch_SetTopLevelComponentContent* Factory = static_cast<const FRigDispatch_SetTopLevelComponentContent*>(InContext.Factory);
			
	// unpack the memory
	const FName& Name = *(const FName*)Handles[Factory->NameArgIndex].GetData();
	const FRigComponentKey ComponentKey(URigHierarchy::GetTopLevelComponentElementKey(), Name);
	const  uint8* SourceComponent = Handles[Factory->ComponentArgIndex].GetData();
	bool& bSuccess = *(bool*)Handles[Factory->SuccessArgIndex].GetData();

	const FStructProperty* StructProperty = CastField<FStructProperty>(Handles[Factory->ComponentArgIndex].GetResolvedProperty());
	check(StructProperty);
	const UScriptStruct* ComponentStruct = StructProperty->Struct;
	check(ComponentStruct && ComponentStruct->IsChildOf(FRigBaseComponent::StaticStruct()));

	// extract the component from the hierarchy
	const FControlRigExecuteContext& Context = InContext.GetPublicDataSafe<FControlRigExecuteContext>();
	FRigBaseComponent* TargetComponent = Context.Hierarchy->FindComponent(ComponentKey);
	if(TargetComponent == nullptr)
	{
#if WITH_EDITOR
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), FString::Printf(TEXT("%s not found."), *ComponentKey.ToString()));
#endif
		bSuccess = false;
		return;
	}

	if(TargetComponent->GetScriptStruct() != ComponentStruct)
	{
#if WITH_EDITOR
		const FString Message = FString::Printf(TEXT("Component pin type (%s) doesn't match component in hierarchy (%s)."), *ComponentStruct->GetName(), *TargetComponent->GetScriptStruct()->GetName());
		Context.Report(FRigVMLogSettings(EMessageSeverity::Warning), Context.GetFunctionName(), Context.GetInstructionIndex(), Message);
#endif
		bSuccess = false;
		return;
	}

	// copy the public content over (members which are not UProperty will be skipped) 
	for (TFieldIterator<FProperty> It(ComponentStruct); It; ++It)
	{
		It->CopyCompleteValue_InContainer(TargetComponent, SourceComponent);
	}
	bSuccess = true;
}