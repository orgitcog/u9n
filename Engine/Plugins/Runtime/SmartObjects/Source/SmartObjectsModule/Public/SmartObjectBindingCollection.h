// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBindingCollection.h"
#include "SmartObjectDefinitionBindableStructDescriptor.h"
#include "SmartObjectDefinitionPropertyBinding.h"
#include "SmartObjectTypes.h"
#include "SmartObjectBindingCollection.generated.h"

#define UE_API SMARTOBJECTSMODULE_API

class USmartObjectDefinition;

/**
 * Representation of all property bindings in a SmartObjectDefinition
 */
USTRUCT()
struct FSmartObjectBindingCollection : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	FSmartObjectDefinitionBindableStructDescriptor& AddBindableStruct(const FSmartObjectDefinitionBindableStructDescriptor& Descriptor)
	{
		return BindableStructs.Add_GetRef(Descriptor);
	}

	/** Moves all bindings outside the collection */
	[[nodiscard]] UE_API TArray<FSmartObjectDefinitionPropertyBinding>&& ExtractBindings();

	UE_API FPropertyBindingBindableStructDescriptor* GetMutableBindableStructDescriptorFromHandle(const FSmartObjectDefinitionDataHandle InSourceHandle);

	//~ Begin FPropertyBindingBindingCollection overrides
	UE_API virtual int32 GetNumBindableStructDescriptors() const override;
	UE_API virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;

	UE_API virtual int32 GetNumBindings() const override;
	UE_API virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	UE_API virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	UE_API virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;

protected:
	UE_API virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const override;
	UE_API virtual void OnReset() override;

#if WITH_EDITOR
public:
	UE_API void AddSmartObjectBinding(FSmartObjectDefinitionPropertyBinding&& InBinding);

protected:
	UE_API virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	UE_API virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	UE_API virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	UE_API virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif // WITH_EDITOR
	//~ End FPropertyBindingBindingCollection overrides

private:

	/** Array of structs descriptors that can be used in bindings. */
	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	TArray<FSmartObjectDefinitionBindableStructDescriptor> BindableStructs;

	/** Array of property bindings, resolved into arrays of copies before use. */
	UPROPERTY(VisibleAnywhere, Category = "SmartObject")
	TArray<FSmartObjectDefinitionPropertyBinding> PropertyBindings;
};

#undef UE_API
