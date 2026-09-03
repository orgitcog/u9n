// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectBindingCollection.h"

#include "SmartObjectDefinition.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectBindingCollection)


//----------------------------------------------------------------//
//  FSmartObjectBindingCollection
//----------------------------------------------------------------//

const FPropertyBindingBindableStructDescriptor* FSmartObjectBindingCollection::GetBindableStructDescriptorFromHandle(const FConstStructView InSourceHandleView) const
{
	return BindableStructs.FindByPredicate([SourceDataHandle = InSourceHandleView.Get<const FSmartObjectDefinitionDataHandle>()](const FSmartObjectDefinitionBindableStructDescriptor& Desc)
	{
		return Desc.DataHandle == SourceDataHandle;
	});
}

TArray<FSmartObjectDefinitionPropertyBinding>&& FSmartObjectBindingCollection::ExtractBindings()
{
	return MoveTemp(PropertyBindings);
}

FPropertyBindingBindableStructDescriptor* FSmartObjectBindingCollection::GetMutableBindableStructDescriptorFromHandle(const FSmartObjectDefinitionDataHandle InSourceHandle)
{
	return BindableStructs.FindByPredicate([InSourceHandle](const FSmartObjectDefinitionBindableStructDescriptor& Desc)
	{
		return Desc.DataHandle == InSourceHandle;
	});
}

int32 FSmartObjectBindingCollection::GetNumBindableStructDescriptors() const
{
	return BindableStructs.Num();
}

int32 FSmartObjectBindingCollection::GetNumBindings() const
{
	return PropertyBindings.Num();
}

void FSmartObjectBindingCollection::OnReset()
{
	BindableStructs.Reset();
	PropertyBindings.Reset();
}

void FSmartObjectBindingCollection::VisitSourceStructDescriptorInternal(
	TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const
{
	for (const FSmartObjectDefinitionBindableStructDescriptor& SourceStruct : BindableStructs)
	{
		if (InFunction(SourceStruct) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FSmartObjectBindingCollection::ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FPropertyBindingBinding& Binding : PropertyBindings)
	{
		InFunction(Binding);
	}
}

void FSmartObjectBindingCollection::ForEachBinding(const FPropertyBindingIndex16 InBegin, const FPropertyBindingIndex16 InEnd
	, const TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const
{
	ensureMsgf(InBegin.IsValid() && InEnd.IsValid(), TEXT("%hs expects valid indices."), __FUNCTION__);

	const TConstArrayView<FSmartObjectDefinitionPropertyBinding> Bindings = PropertyBindings;
	for (int32 BindingIndex = InBegin.Get(); BindingIndex < InEnd.Get(); ++BindingIndex)
	{
		InFunction(Bindings[BindingIndex], BindingIndex);
	}
}

void FSmartObjectBindingCollection::ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FPropertyBindingBinding& Binding : PropertyBindings)
	{
		InFunction(Binding);
	}
}

void FSmartObjectBindingCollection::VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FPropertyBindingBinding& Binding : PropertyBindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FSmartObjectBindingCollection::VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FPropertyBindingBinding& Binding : PropertyBindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

#if WITH_EDITOR
void FSmartObjectBindingCollection::AddSmartObjectBinding(FSmartObjectDefinitionPropertyBinding&& InBinding)
{
	RemoveBindings(InBinding.GetTargetPath(), ESearchMode::Exact);

	PropertyBindings.Add(MoveTemp(InBinding));
}

FPropertyBindingBinding* FSmartObjectBindingCollection::AddBindingInternal(const FPropertyBindingPath& InSourcePath
	, const FPropertyBindingPath& InTargetPath)
{
	if (const UObject* LogOwner = Cast<UObject>(GetBindingsOwner()))
	{
		UE_VLOG_UELOG(LogOwner, LogPropertyBindingUtils, Verbose, TEXT("%hs %d bindings"), __FUNCTION__, PropertyBindings.Num()+1);
	}
	else
	{
		UE_LOG(LogPropertyBindingUtils, Verbose, TEXT("%hs %d bindings"), __FUNCTION__, PropertyBindings.Num()+1);
	}
	return &PropertyBindings.Emplace_GetRef(InSourcePath, InTargetPath);
}

void FSmartObjectBindingCollection::RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate)
{
	PropertyBindings.RemoveAllSwap(InPredicate);
}

bool FSmartObjectBindingCollection::HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return PropertyBindings.ContainsByPredicate(InPredicate);
}

const FPropertyBindingBinding* FSmartObjectBindingCollection::FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return PropertyBindings.FindByPredicate(InPredicate);
}
#endif // WITH_EDITOR

//----------------------------------------------------------------//
//  FSmartObjectDefinitionPropertyBinding
//----------------------------------------------------------------//
PRAGMA_DISABLE_DEPRECATION_WARNINGS
#if WITH_EDITORONLY_DATA
void FSmartObjectDefinitionPropertyBinding::PostSerialize(const FArchive& Ar)
{
	if (!SourcePath_DEPRECATED.IsPathEmpty())
	{
		SourcePropertyPath = SourcePath_DEPRECATED;
		SourcePath_DEPRECATED.Reset();
	}

	if (!TargetPath_DEPRECATED.IsPathEmpty())
	{
		TargetPropertyPath = TargetPath_DEPRECATED;
		TargetPath_DEPRECATED.Reset();
	}
}
#endif // WITH_EDITORONLY_DATA
PRAGMA_ENABLE_DEPRECATION_WARNINGS