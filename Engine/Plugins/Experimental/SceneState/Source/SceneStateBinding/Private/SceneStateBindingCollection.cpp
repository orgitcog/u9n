// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateBindingCollection.h"

const FSceneStateBindingDesc* FSceneStateBindingCollection::FindBindingDesc(FSceneStateBindingDataHandle InDataHandle) const
{
	return BindingDescs.FindByPredicate(
		[&InDataHandle](const FSceneStateBindingDesc& InDesc)
		{
			return InDesc.DataHandle == InDataHandle;
		});
}

#if WITH_EDITOR
FPropertyBindingBinding* FSceneStateBindingCollection::AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
{
	return &Bindings.Emplace_GetRef(InSourcePath, InTargetPath);
}

void FSceneStateBindingCollection::RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate)
{
	Bindings.RemoveAllSwap(InPredicate);
}

bool FSceneStateBindingCollection::HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.ContainsByPredicate(InPredicate);
}

const FPropertyBindingBinding* FSceneStateBindingCollection::FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const
{
	return Bindings.FindByPredicate(InPredicate);
}
#endif

int32 FSceneStateBindingCollection::GetNumBindings() const
{
	return Bindings.Num();
}

int32 FSceneStateBindingCollection::GetNumBindableStructDescriptors() const
{
	return BindingDescs.Num();
}

const FPropertyBindingBindableStructDescriptor* FSceneStateBindingCollection::GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const
{
	return FindBindingDesc(InSourceHandleView.Get<const FSceneStateBindingDataHandle>());
}

void FSceneStateBindingCollection::ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FSceneStateBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FSceneStateBindingCollection::ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const
{
	checkf(InBegin.IsValid() && InEnd.IsValid(), TEXT("Begin and end indices are not valid!"));

	for (int32 BindingIndex = InBegin.Get(); BindingIndex < InEnd.Get(); ++BindingIndex)
	{
		checkf(Bindings.IsValidIndex(BindingIndex), TEXT("Index %d out of bounds! Bindings Num: %d"), BindingIndex, Bindings.Num());
		InFunction(Bindings[BindingIndex], BindingIndex);
	}
}

void FSceneStateBindingCollection::ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FSceneStateBinding& Binding : Bindings)
	{
		InFunction(Binding);
	}
}

void FSceneStateBindingCollection::VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const
{
	for (const FSceneStateBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FSceneStateBindingCollection::VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction)
{
	for (FSceneStateBinding& Binding : Bindings)
	{
		if (InFunction(Binding) == EVisitResult::Break)
		{
			break;
		}
	}
}

void FSceneStateBindingCollection::OnReset()
{
	BindingDescs.Reset();
	Bindings.Reset();
}

void FSceneStateBindingCollection::VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor&)> InFunction) const
{
	for (const FSceneStateBindingDesc& BindingDesc : BindingDescs)
	{
		if (InFunction(BindingDesc) == EVisitResult::Break)
		{
			break;
		}
	}
}
