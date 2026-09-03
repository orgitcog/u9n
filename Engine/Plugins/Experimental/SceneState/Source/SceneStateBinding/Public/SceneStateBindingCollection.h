// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBindingCollection.h"
#include "SceneStateBinding.h"
#include "SceneStateBindingDesc.h"
#include "SceneStateBindingCollection.generated.h"

namespace UE::SceneState::Editor
{
	class FBindingCompiler;
}

USTRUCT()
struct FSceneStateBindingCollection : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	TArrayView<FSceneStateBindingDesc> GetMutableBindingDescs()
	{
		return BindingDescs;
	}

	TArrayView<FSceneStateBinding> GetMutableBindings()
	{
		return Bindings;
	}

	/** Finds the binding desc matching the given data handle */
	SCENESTATEBINDING_API const FSceneStateBindingDesc* FindBindingDesc(FSceneStateBindingDataHandle InDataHandle) const;

#if WITH_EDITOR
	//~ Begin FPropertyBindingBindingCollection
	SCENESTATEBINDING_API virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	SCENESTATEBINDING_API virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	SCENESTATEBINDING_API virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	SCENESTATEBINDING_API virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif
	SCENESTATEBINDING_API virtual int32 GetNumBindings() const override;
	SCENESTATEBINDING_API virtual int32 GetNumBindableStructDescriptors() const override;
	SCENESTATEBINDING_API virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;
	SCENESTATEBINDING_API virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	SCENESTATEBINDING_API virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	SCENESTATEBINDING_API virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	SCENESTATEBINDING_API virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	SCENESTATEBINDING_API virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;
	SCENESTATEBINDING_API virtual void OnReset() override;
	SCENESTATEBINDING_API virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor&)> InFunction) const override;
	//~ End FPropertyBindingBindingCollection

private:
	UPROPERTY()
	TArray<FSceneStateBindingDesc> BindingDescs;

	UPROPERTY()
	TArray<FSceneStateBinding> Bindings;

	friend UE::SceneState::Editor::FBindingCompiler;
};
