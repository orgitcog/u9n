// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "PropertyBindingExtension.h"
#include "PropertyBindingBindingCollectionOwner.h"
#include "StateTreePropertyBindings.h"
#include "StateTreeEditorNode.h"
#include "StateTreeEditorPropertyBindings.generated.h"

enum class EStateTreeNodeFormatting : uint8;
class IStateTreeEditorPropertyBindingsOwner;
enum class EStateTreeVisitor : uint8;

/**
 * Editor representation of all property bindings in a StateTree
 */
USTRUCT()
struct STATETREEEDITORMODULE_API FStateTreeEditorPropertyBindings : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	/** @return const array view to all bindings. */
	TConstArrayView<FStateTreePropertyPathBinding> GetBindings() const
	{
		return PropertyBindings;
	}

	/** @return array view to all bindings. */
	TArrayView<FStateTreePropertyPathBinding> GetMutableBindings()
	{
		return PropertyBindings;
	}

	void AddStateTreeBinding(FStateTreePropertyPathBinding&& InBinding)
	{
		RemoveBindings(InBinding.GetTargetPath(), ESearchMode::Exact);
		PropertyBindings.Add(MoveTemp(InBinding));
	}

	//~ Begin FPropertyBindingBindingCollection overrides
	virtual int32 GetNumBindableStructDescriptors() const override;
	virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;

	virtual int32 GetNumBindings() const override;
	virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void ForEachBinding(FPropertyBindingIndex16 InBegin, FPropertyBindingIndex16 InEnd, TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;
	//~ End FPropertyBindingBindingCollection overrides

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Adds binding between source and destination paths. Removes any bindings to TargetPath before adding the new one.
	 * @param SourcePath Binding source property path.
	 * @param TargetPath Binding target property path.
	 */
	UE_DEPRECATED(5.6, "Use AddBinding taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FStateTreePropertyPath& SourcePath, const FStateTreePropertyPath& TargetPath)
	{
		AddBinding(SourcePath, TargetPath);
	}
	
	/**
	 * Adds binding.
	 * @param Binding Binding to be added.
	*/
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	void AddPropertyBinding(const FStateTreePropertyPathBinding& Binding)
	{
	}

	/**
	 * Adds binding between PropertyFunction of the provided type and destination path.
	 * @param InPropertyFunctionNodeStruct Struct of PropertyFunction.
	 * @param InSourcePathSegments Binding source property path segments.
	 * @param InTargetPath Binding target property path.
	 * @return Constructed binding source property path.
	 */
	FPropertyBindingPath AddFunctionBinding(const UScriptStruct* InPropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> InSourcePathSegments, const FPropertyBindingPath& InTargetPath);
	UE_DEPRECATED(5.6, "Use AddFunctionBinding taking FPropertyBindingPath instead")
	FStateTreePropertyPath AddFunctionPropertyBinding(const UScriptStruct* InPropertyFunctionNodeStruct, TConstArrayView<FPropertyBindingPathSegment> InSourcePathSegments, const FStateTreePropertyPath& InTargetPath)
	{
		return AddFunctionBinding(InPropertyFunctionNodeStruct, InSourcePathSegments, InTargetPath);
	}

	/**
	 * Removes all bindings to target path.
	 * @param TargetPath Target property path.
	 */ 
	UE_DEPRECATED(5.6, "Use RemoveBinding taking FPropertyBindingPath instead")
	void RemovePropertyBindings(const FStateTreePropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact)
	{
		RemoveBindings(TargetPath, SearchMode);
	}
	
	/**
	 * Has any binding to the target path.
	 * @return True of the target path has any bindings.
	 */
	UE_DEPRECATED(5.6, "Use HasBinding taking FPropertyBindingPath instead")
	bool HasPropertyBinding(const FStateTreePropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact) const
	{
		return HasBinding(TargetPath, SearchMode);
	}

	/**
	 * @return binding to the target path.
	 */
	const FStateTreePropertyPathBinding* FindPropertyBinding(const FPropertyBindingPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact) const;
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	const FStateTreePropertyPathBinding* FindPropertyBinding(const FStateTreePropertyPath& TargetPath, ESearchMode SearchMode = ESearchMode::Exact) const;

	/**
	 * @return Source path for given target path, or null if binding does not exists.
	 */
	UE_DEPRECATED(5.6, "Use GetBindingSource taking FPropertyBindingPath instead")
	const FStateTreePropertyPath* GetPropertyBindingSource(const FStateTreePropertyPath& TargetPath) const
	{
		return nullptr;
	}

	/**
	 * Returns all pointers to bindings for a specified structs based in struct ID.
	 * @param StructID ID of the struct to find bindings for.
	 * @param OutBindings Bindings for specified struct.
	 */
	UE_DEPRECATED(5.6, "Use GetBindingsFor taking FPropertyBindingPath instead")
	void GetPropertyBindingsFor(const FGuid StructID, TArray<const FStateTreePropertyPathBinding*>& OutBindings) const
	{
	}

	/**
	 * Removes bindings which do not point to valid structs IDs.
	 * @param ValidStructs Set of struct IDs that are currently valid.
	 */
	UE_DEPRECATED(5.6, "Use RemoveInvalidBindings taking FPropertyBindingDataView instead")
	void RemoveUnusedBindings(const TMap<FGuid, const FStateTreeDataView>& ValidStructs)
	{
	}

	UE_DEPRECATED(5.5, "Use GetPropertyBindingsFor returning pointers instead.")
	void GetPropertyBindingsFor(const FGuid StructID, TArray<FStateTreePropertyPathBinding>& OutBindings) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

protected:
	//~ Begin FPropertyBindingBindingCollection overrides
	virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	virtual void CopyBindingsInternal(const FGuid InFromStructID, const FGuid InToStructID) override;
	virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	//~ Begin FPropertyBindingBindingCollection overrides

private:
	UPROPERTY()
	TArray<FStateTreePropertyPathBinding> PropertyBindings;
};


UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UStateTreeEditorPropertyBindingsOwner : public UPropertyBindingBindingCollectionOwner
{
	GENERATED_UINTERFACE_BODY()
};

/** Struct of Parameters used to Create a Property */
struct UE_DEPRECATED(5.6, "Use FPropertyCreationDesc instead") FStateTreeEditorPropertyCreationDesc
{
	/** Property Bag Description of the Property to Create */
	FPropertyBagPropertyDesc PropertyDesc;

	/** Optional: property to copy into the new created property */
	const FProperty* SourceProperty = nullptr;

	/** Optional: container address of the property to copy */
	const void* SourceContainerAddress = nullptr;
};

class STATETREEEDITORMODULE_API IStateTreeEditorPropertyBindingsOwner : public IPropertyBindingBindingCollectionOwner
{
	GENERATED_BODY()

public:

	/**
	 * Returns structs within the owner that are visible for target struct.
	 * @param TargetStructID Target struct ID
	 * @param OutStructDescs Result descriptors of the visible structs.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingBindableStructDescriptor instead")
	virtual void GetAccessibleStructs(const FGuid TargetStructID, TArray<FStateTreeBindableStructDesc>& OutStructDescs) const final
	{
	}

	/**
	 * Returns struct descriptor based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutStructDesc Result descriptor.
	 * @return True if struct found.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingBindableStructDescriptor instead")
	virtual bool GetStructByID(const FGuid StructID, FStateTreeBindableStructDesc& OutStructDesc) const final
	{
		return false;
	}

	/**
	 * Finds a bindable context struct based on name and type.
	 * @param ObjectType Object type to match
	 * @param ObjectNameHint Name to use if multiple context objects of same type are found. 
	 */
	virtual FStateTreeBindableStructDesc FindContextData(const UStruct* ObjectType, const FString ObjectNameHint) const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::FindContextData, return {}; );

	/**
	 * Returns data view based on struct ID.
	 * @param StructID Target struct ID
	 * @param OutDataView Result data view.
	 * @return True if struct found.
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyBindingDataView instead")
	virtual bool GetDataViewByID(const FGuid StructID, FStateTreeDataView& OutDataView) const final
	{
		return false;
	}

	/** @return Pointer to editor property bindings. */
	virtual FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetPropertyEditorBindings, return nullptr; );

	/** @return Pointer to editor property bindings. */
	virtual const FStateTreeEditorPropertyBindings* GetPropertyEditorBindings() const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::GetPropertyEditorBindings, return nullptr; );

	virtual EStateTreeVisitor EnumerateBindablePropertyFunctionNodes(TFunctionRef<EStateTreeVisitor(const UScriptStruct* NodeStruct, const FStateTreeBindableStructDesc& Desc, const FStateTreeDataView Value)> InFunc) const PURE_VIRTUAL(IStateTreeEditorPropertyBindingsOwner::EnumerateBindablePropertyFunctionNodes, return static_cast<EStateTreeVisitor>(0); );

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * Creates the given properties in the property bag of the struct matching the given struct ID
	 * @param StructID Target struct ID
	 * @param InOutCreationDescs the descriptions of the properties to create. This is modified to update the property names that actually got created
	 */
	UE_DEPRECATED(5.6, "Use version taking FPropertyCreationDesc instead")
	virtual void CreateParameters(const FGuid StructID, TArrayView<FStateTreeEditorPropertyCreationDesc> InOutCreationDescs) final
	{
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};

// TODO: We should merge this with IStateTreeEditorPropertyBindingsOwner and FStateTreeEditorPropertyBindings.
// Currently FStateTreeEditorPropertyBindings is meant to be used as a member for just to store things,
// IStateTreeEditorPropertyBindingsOwner is meant return model specific stuff,
// and IStateTreeBindingLookup is used in non-editor code and it cannot be in FStateTreeEditorPropertyBindings because bindings don't know about the owner.
struct STATETREEEDITORMODULE_API FStateTreeBindingLookup : public IStateTreeBindingLookup
{
	FStateTreeBindingLookup(const IStateTreeEditorPropertyBindingsOwner* InBindingOwner);

	const IStateTreeEditorPropertyBindingsOwner* BindingOwner = nullptr;

	virtual const FPropertyBindingPath* GetPropertyBindingSource(const FPropertyBindingPath& InTargetPath) const override;
	virtual FText GetPropertyPathDisplayName(const FPropertyBindingPath& InTargetPath, EStateTreeNodeFormatting Formatting) const override;
	virtual FText GetBindingSourceDisplayName(const FPropertyBindingPath& InTargetPath, EStateTreeNodeFormatting Formatting) const override;
	virtual const FProperty* GetPropertyPathLeafProperty(const FPropertyBindingPath& InPath) const override;

};
