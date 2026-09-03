// Copyright Epic Games, Inc. All Rights Reserved.

#include "Component/AnimNextComponent.h"

#include "AnimNextComponentWorldSubsystem.h"
#include "Blueprint/BlueprintExceptionInfo.h"
#include "Engine/World.h"
#include "Module/ModuleTaskContext.h"
#include "Module/ProxyVariablesContext.h"

void UAnimNextComponent::OnRegister()
{
	using namespace UE::AnimNext;

	Super::OnRegister();

	Subsystem = GetWorld()->GetSubsystem<UAnimNextComponentWorldSubsystem>();

	if (Subsystem && Module)
	{
		check(!ModuleHandle.IsValid());

		CreatePublicVariablesProxy();
		Subsystem->Register(this);
#if UE_ENABLE_DEBUG_DRAWING
		Subsystem->ShowDebugDrawing(this, bShowDebugDrawing);
#endif
	}
}

void UAnimNextComponent::OnUnregister()
{
	Super::OnUnregister();

	if(Subsystem)
	{
		Subsystem->Unregister(this);
		Subsystem = nullptr;
		DestroyPublicVariablesProxy();
	}
}

void UAnimNextComponent::BeginPlay()
{
	Super::BeginPlay();

	UWorld* World = GetWorld();
	check(World);

	if (InitMethod == EAnimNextModuleInitMethod::InitializeAndRun
	|| (InitMethod == EAnimNextModuleInitMethod::InitializeAndPauseInEditor && World->WorldType != EWorldType::Editor && World->WorldType != EWorldType::EditorPreview))
	{
		SetEnabled(true);
	}
}

void UAnimNextComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	SetEnabled(false);
}

#if WITH_EDITOR
void UAnimNextComponent::OnModuleCompiled()
{
	CreatePublicVariablesProxy();
}
#endif

void UAnimNextComponent::CreatePublicVariablesProxy()
{
	PublicVariablesProxyMap.Reset();
	PublicVariablesProxy.Reset();
	if(Module && Module->GetPublicVariableDefaults().GetPropertyBagStruct())
	{
		PublicVariablesProxy.Data = Module->GetPublicVariableDefaults();
		TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
		PublicVariablesProxyMap.Reserve(ProxyDescs.Num());
		for(int32 DescIndex = 0; DescIndex < ProxyDescs.Num(); ++DescIndex)
		{
			PublicVariablesProxyMap.Add(ProxyDescs[DescIndex].Name, DescIndex);
		}
		PublicVariablesProxy.DirtyFlags.SetNum(ProxyDescs.Num(), false);
	}
}

void UAnimNextComponent::DestroyPublicVariablesProxy()
{
	PublicVariablesProxyMap.Empty();
	PublicVariablesProxy.Empty();
}

void UAnimNextComponent::FlipPublicVariablesProxy(const UE::AnimNext::FProxyVariablesContext& InContext)
{
	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);
	Swap(InContext.GetPublicVariablesProxy(), PublicVariablesProxy);
}

void UAnimNextComponent::SetVariable(FName Name, int32 Value)
{
	checkNoEntry();
}

DEFINE_FUNCTION(UAnimNextComponent::execSetVariable)
{
	using namespace UE::AnimNext;

	// Read wildcard Value input.
	Stack.MostRecentPropertyAddress = nullptr;
	Stack.MostRecentPropertyContainer = nullptr;

	P_GET_PROPERTY(FNameProperty, Name);

	Stack.StepCompiledIn<FProperty>(nullptr);
	const FProperty* ValueProp = CastField<FProperty>(Stack.MostRecentProperty);
	const void* ContainerPtr = Stack.MostRecentPropertyContainer;

	P_FINISH;

	if (!ValueProp || !ContainerPtr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::AbortExecution,
			NSLOCTEXT("UAFComponent", "UAFComponent_SetVariableError", "Failed to resolve the Value for Set Variable")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	if (Name == NAME_None)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			NSLOCTEXT("UAFComponent", "UAFComponent_SetVariableInvalidWarning", "Invalid variable name supplied to Set Variable")
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	int32* IndexPtr = P_THIS->PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			FText::Format(NSLOCTEXT("UAFComponent", "UAFComponent_SetVariableNotFoundWarning", "Unknown variable name '{0}' supplied to Set Variable"), FText::FromName(Name))
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}

	bool bIsTypeMatching = false;

	P_NATIVE_BEGIN;

	{
		FRWScopeLock Lock(P_THIS->PublicVariablesLock, SLT_Write);

		TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = P_THIS->PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
		const FProperty* CachedProperty = ProxyDescs[*IndexPtr].CachedProperty;

		bIsTypeMatching = CachedProperty->GetClass() == ValueProp->GetClass();

		if (!bIsTypeMatching)
		{
			if (const FByteProperty* SourceByteProperty = CastField<FByteProperty>(ValueProp))
			{
				if (const FEnumProperty* TargetEnumProperty = CastField<FEnumProperty>(CachedProperty))
				{
					bIsTypeMatching = TargetEnumProperty->GetUnderlyingProperty()->IsA<FByteProperty>() || SourceByteProperty->Enum == TargetEnumProperty->GetEnum();
				}
			}
		}

		if (bIsTypeMatching)
		{
			const void* ValuePtr = ValueProp->ContainerPtrToValuePtr<void>(ContainerPtr);
			CachedProperty->SetValue_InContainer(P_THIS->PublicVariablesProxy.Data.GetMutableValue().GetMemory(), ValuePtr);
			P_THIS->PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
			P_THIS->PublicVariablesProxy.bIsDirty = true;
		}
	}

	P_NATIVE_END;

	if (!bIsTypeMatching)
	{
		FBlueprintExceptionInfo ExceptionInfo(
			EBlueprintExceptionType::NonFatalError,
			FText::Format(NSLOCTEXT("UAFComponent", "UAFComponent_SetVariableTypeMismatch", "Incompatible type supplied for variable '{0}'"), FText::FromName(Name))
		);

		FBlueprintCoreDelegates::ThrowScriptException(P_THIS, Stack, ExceptionInfo);
		return;
	}
}

TOptional<bool> UAnimNextComponent::GetVariableBool(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<bool>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<bool, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueBool(PropertyDesc);
	return Result.HasValue() ? TOptional<bool>(Result.GetValue()) : TOptional<bool>();
}

TOptional<uint8> UAnimNextComponent::GetVariableByte(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<uint8>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<uint8, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueByte(PropertyDesc);
	return Result.HasValue() ? TOptional<uint8>(Result.GetValue()) : TOptional<uint8>();
}

TOptional<int32> UAnimNextComponent::GetVariableInt32(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<int32>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<int32, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueInt32(PropertyDesc);
	return Result.HasValue() ? TOptional<int32>(Result.GetValue()) : TOptional<int32>();
}

TOptional<uint32> UAnimNextComponent::GetVariableUInt32(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<uint32>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<uint32, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueUInt32(PropertyDesc);
	return Result.HasValue() ? TOptional<uint32>(Result.GetValue()) : TOptional<uint32>();
}

TOptional<int64> UAnimNextComponent::GetVariableInt64(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<int64>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<int64, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueInt64(PropertyDesc);
	return Result.HasValue() ? TOptional<int64>(Result.GetValue()) : TOptional<int64>();
}

TOptional<uint64> UAnimNextComponent::GetVariableUInt64(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<uint64>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<uint64, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueUInt64(PropertyDesc);
	return Result.HasValue() ? TOptional<uint64>(Result.GetValue()) : TOptional<uint64>();
}

TOptional<float> UAnimNextComponent::GetVariableFloat(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<float>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<float, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueFloat(PropertyDesc);
	return Result.HasValue() ? TOptional<float>(Result.GetValue()) : TOptional<float>();
}

TOptional<double> UAnimNextComponent::GetVariableDouble(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<double>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<double, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueDouble(PropertyDesc);
	return Result.HasValue() ? TOptional<double>(Result.GetValue()) : TOptional<double>();
}

TOptional<FName> UAnimNextComponent::GetVariableName(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<FName>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FName, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueName(PropertyDesc);
	return Result.HasValue() ? TOptional<FName>(Result.GetValue()) : TOptional<FName>();
}

TOptional<FString> UAnimNextComponent::GetVariableString(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<FString>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FString, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueString(PropertyDesc);
	return Result.HasValue() ? TOptional<FString>(Result.GetValue()) : TOptional<FString>();
}

TOptional<uint8> UAnimNextComponent::GetVariableEnum(const FName Name, const UEnum* RequestedEnum) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<uint8>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<uint8, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueEnum(PropertyDesc, RequestedEnum);
	return Result.HasValue() ? TOptional<uint8>(Result.GetValue()) : TOptional<uint8>();
}

TOptional<FConstStructView> UAnimNextComponent::GetVariableStruct(const FName Name, const UScriptStruct* RequestedStruct) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<FConstStructView>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FStructView, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueStruct(PropertyDesc, RequestedStruct);
	return Result.HasValue() ? TOptional<FConstStructView>(Result.GetValue()) : TOptional<FConstStructView>();
}

TOptional<const UObject*> UAnimNextComponent::GetVariableObject(const FName Name, const UClass* RequestedClass) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<const UObject*>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<UObject*, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueObject(PropertyDesc, RequestedClass);
	return Result.HasValue() ? TOptional<const UObject*>(Result.GetValue()) : TOptional<const UObject*>();
}

TOptional<const UClass*> UAnimNextComponent::GetVariableClass(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<const UClass*>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<UClass*, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueClass(PropertyDesc);
	return Result.HasValue() ? TOptional<const UClass*>(Result.GetValue()) : TOptional<const UClass*>();
}

TOptional<FSoftObjectPath> UAnimNextComponent::GetVariableSoftPath(const FName Name) const
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return TOptional<FSoftObjectPath>();
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_ReadOnly);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FSoftObjectPath, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueSoftPath(PropertyDesc);
	return Result.HasValue() ? TOptional<FSoftObjectPath>(Result.GetValue()) : TOptional<FSoftObjectPath>();
}

bool UAnimNextComponent::SetVariableBool(const FName Name, const bool bInValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueBool(PropertyDesc, bInValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableByte(const FName Name, const uint8 InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueByte(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableInt32(const FName Name, const int32 InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueInt32(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableUInt32(const FName Name, const uint32 InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueUInt32(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableInt64(const FName Name, const int64 InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueInt64(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableUInt64(const FName Name, const uint64 InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueUInt64(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableFloat(const FName Name, const float InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueFloat(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableDouble(const FName Name, const double InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueDouble(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableName(const FName Name, const FName InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueName(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableString(const FName Name, const FString& InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueString(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableEnum(const FName Name, const uint8 InValue, const UEnum* Enum)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueEnum(PropertyDesc, InValue, Enum);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableStruct(const FName Name, FConstStructView InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueStruct(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableStructRef(const FName Name, TFunctionRef<void(FStructView)> InStructRefSetter, const UScriptStruct* RequestedStruct)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FStructView, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueStruct(PropertyDesc, RequestedStruct);
	if (Result.HasValue())
	{
		InStructRefSetter(Result.GetValue());
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
		return true;
	}

	return false;
}

bool UAnimNextComponent::SetVariableObject(const FName Name, UObject* InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueObject(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableObjectRef(const FName Name, TFunctionRef<void(UObject*)> InObjectRefSetter, const UClass* RequestedClass)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<UObject*, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetValueObject(PropertyDesc, RequestedClass);
	if (Result.HasValue())
	{
		InObjectRefSetter(Result.GetValue());
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
		return true;
	}

	return false;
}

bool UAnimNextComponent::SetVariableClass(const FName Name, UClass* InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueClass(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableSoftPath(const FName Name, const FSoftObjectPath& InValue)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	EPropertyBagResult Result = PublicVariablesProxy.Data.SetValueSoftPath(PropertyDesc, InValue);
	if (Result == EPropertyBagResult::Success)
	{
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
	}

	return Result == EPropertyBagResult::Success;
}

bool UAnimNextComponent::SetVariableArrayRef(const FName Name, TFunctionRef<void(FPropertyBagArrayRef&)> InArrayRefSetter)
{
	const int32* IndexPtr = PublicVariablesProxyMap.Find(Name);
	if (IndexPtr == nullptr)
	{
		return false;
	}

	FRWScopeLock Lock(PublicVariablesLock, SLT_Write);

	TConstArrayView<FPropertyBagPropertyDesc> ProxyDescs = PublicVariablesProxy.Data.GetPropertyBagStruct()->GetPropertyDescs();
	const FPropertyBagPropertyDesc& PropertyDesc = ProxyDescs[*IndexPtr];

	TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> Result = PublicVariablesProxy.Data.GetMutableArrayRef(PropertyDesc);
	if (Result.HasValue())
	{
		InArrayRefSetter(Result.GetValue());
		PublicVariablesProxy.DirtyFlags[*IndexPtr] = true;
		PublicVariablesProxy.bIsDirty = true;
		return true;
	}

	return false;
}

bool UAnimNextComponent::IsEnabled() const
{
	if (Subsystem)
	{
		return Subsystem->IsEnabled(this);
	}
	return false;
}

void UAnimNextComponent::SetEnabled(bool bEnabled)
{
	if(Subsystem)
	{
		Subsystem->SetEnabled(this, bEnabled);
	}
}

void UAnimNextComponent::ShowDebugDrawing(bool bInShowDebugDrawing)
{
#if UE_ENABLE_DEBUG_DRAWING
	bShowDebugDrawing = bInShowDebugDrawing;
	if(Subsystem)
	{
		Subsystem->ShowDebugDrawing(this, bInShowDebugDrawing);
	}
#endif
}

void UAnimNextComponent::QueueTask(FName InModuleEventName, TUniqueFunction<void(const UE::AnimNext::FModuleTaskContext&)>&& InTaskFunction, UE::AnimNext::ETaskRunLocation InLocation)
{
	using namespace UE::AnimNext;

	if(Subsystem)
	{
		Subsystem->QueueTask(this, InModuleEventName, MoveTemp(InTaskFunction), InLocation);
	}
}

void UAnimNextComponent::QueueInputTraitEvent(FAnimNextTraitEventPtr Event)
{
	using namespace UE::AnimNext;

	if(Subsystem)
	{
		Subsystem->QueueInputTraitEvent(this, Event);
	}
}

const FTickFunction* UAnimNextComponent::FindTickFunction(FName InEventName) const
{
	if(Subsystem)
	{
		return Subsystem->FindTickFunction(this, InEventName);
	}
	return nullptr;
}

void UAnimNextComponent::AddPrerequisite(UObject* InObject, FTickFunction& InTickFunction, FName InEventName)
{
	if(Subsystem)
	{
		Subsystem->AddDependency(this, InObject, InTickFunction, InEventName, UAnimNextWorldSubsystem::EDependency::Prerequisite);
	}
}

void UAnimNextComponent::AddComponentPrerequisite(UActorComponent* InComponent, FName InEventName)
{
	AddPrerequisite(InComponent, InComponent->PrimaryComponentTick, InEventName);
}

void UAnimNextComponent::AddSubsequent(UObject* InObject, FTickFunction& InTickFunction, FName InEventName)
{
	if(Subsystem)
	{
		Subsystem->AddDependency(this, InObject, InTickFunction, InEventName, UAnimNextWorldSubsystem::EDependency::Subsequent);
	}
}

void UAnimNextComponent::AddComponentSubsequent(UActorComponent* InComponent, FName InEventName)
{
	AddSubsequent(InComponent, InComponent->PrimaryComponentTick, InEventName);
}

void UAnimNextComponent::RemovePrerequisite(UObject* InObject, FTickFunction& InTickFunction, FName InEventName)
{
	if(Subsystem)
	{
		Subsystem->RemoveDependency(this, InObject, InTickFunction, InEventName, UAnimNextWorldSubsystem::EDependency::Prerequisite);
	}
}

void UAnimNextComponent::RemoveComponentPrerequisite(UActorComponent* InComponent, FName InEventName)
{
	RemovePrerequisite(InComponent, InComponent->PrimaryComponentTick, InEventName);
}

void UAnimNextComponent::RemoveSubsequent(UObject* InObject, FTickFunction& InTickFunction, FName InEventName)
{
	if(Subsystem)
	{
		Subsystem->RemoveDependency(this, InObject, InTickFunction, InEventName, UAnimNextWorldSubsystem::EDependency::Subsequent);
	}
}

void UAnimNextComponent::RemoveComponentSubsequent(UActorComponent* InComponent, FName InEventName)
{
	RemoveSubsequent(InComponent, InComponent->PrimaryComponentTick, InEventName);
}

void UAnimNextComponent::AddModuleEventPrerequisite(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName)
{
	if (!OtherAnimNextComponent)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAFComponent::AddModuleEventPrerequisite called with null OtherAnimNextComponent"));
	}
	else if (OtherAnimNextComponent == this)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAFComponent::AddModuleEventPrerequisite called using the same component"));
	}
	else if (Subsystem)
	{
		Subsystem->AddModuleEventDependency(this, InEventName, OtherAnimNextComponent, OtherEventName, UAnimNextWorldSubsystem::EDependency::Prerequisite);
	}
}

void UAnimNextComponent::AddModuleEventSubsequent(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName)
{
	if (!OtherAnimNextComponent)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAFComponent::AddModuleEventSubsequent called with null OtherAnimNextComponent"));
	}
	else if (OtherAnimNextComponent == this)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAFComponent::AddModuleEventPrerequisite called using the same component"));
	}
	else if (Subsystem)
	{
		Subsystem->AddModuleEventDependency(this, InEventName, OtherAnimNextComponent, OtherEventName, UAnimNextWorldSubsystem::EDependency::Subsequent);
	}
}

void UAnimNextComponent::RemoveModuleEventPrerequisite(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName)
{
	if (!OtherAnimNextComponent)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAnimNextComponent::RemoveModuleEventPrerequisite called with null OtherAnimNextComponent"));
	}
	else if (OtherAnimNextComponent == this)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAnimNextComponent::AddModuleEventPrerequisite called using the same component"));
	}
	else if (Subsystem)
	{
		Subsystem->RemoveModuleEventDependency(this, InEventName, OtherAnimNextComponent, OtherEventName, UAnimNextWorldSubsystem::EDependency::Prerequisite);
	}
}

void UAnimNextComponent::RemoveModuleEventSubsequent(FName InEventName, UAnimNextComponent* OtherAnimNextComponent, FName OtherEventName)
{
	if (!OtherAnimNextComponent)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAnimNextComponent::RemoveModuleEventSubsequent called with null OtherAnimNextComponent"));
	}
	else if (OtherAnimNextComponent == this)
	{
		UE_LOG(LogAnimation, Warning, TEXT("UAnimNextComponent::AddModuleEventPrerequisite called using the same component"));
	}
	else if (Subsystem)
	{
		Subsystem->RemoveModuleEventDependency(this, InEventName, OtherAnimNextComponent, OtherEventName, UAnimNextWorldSubsystem::EDependency::Subsequent);
	}
}

FAnimNextModuleHandle UAnimNextComponent::BlueprintGetModuleHandle() const
{
	return FAnimNextModuleHandle(ModuleHandle);
}