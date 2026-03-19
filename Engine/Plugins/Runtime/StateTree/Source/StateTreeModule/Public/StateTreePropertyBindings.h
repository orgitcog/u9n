// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingPath.h"
#include "PropertyBindingBinding.h"
#include "PropertyBindingBindingCollection.h"
#include "StateTreeNodeBase.h"
#include "StateTreeTypes.h"
#include "StateTreePropertyRefHelpers.h"
#include "StructUtils/StructView.h"
#include "StateTreeIndexTypes.h"
#include "StateTreePropertyBindings.generated.h"

#define UE_API STATETREEMODULE_API

class FProperty;
struct FStateTreePropertyBindingCompiler;
struct FStateTreePropertyRef;
class UStateTree;
enum class EStateTreeNodeFormatting : uint8;

UENUM()
enum class EStateTreeBindableStructSource : uint8
{
	/** Source is StateTree context object */
	Context,
	/** Source is StateTree parameter */
	Parameter,
	/** Source is StateTree evaluator */
	Evaluator,
	/** Source is StateTree global task */
	GlobalTask,
	/** Source is State parameter */
	StateParameter,
	/** Source is State task */
	Task,
	/** Source is State condition */
	Condition,
	/** Source is State utility consideration */
	Consideration,
	/** Source is StateTree event used by transition */
	TransitionEvent,
	/** Source is StateTree event used by state selection */
	StateEvent,
	/** Source is Property Function */
	PropertyFunction,
	/** Source is Transition */
	Transition,
};

namespace UE::StateTree
{
	/** Can that binding type accept a task instance data for a source. */
	[[nodiscard]] UE_API bool AcceptTaskInstanceData(EStateTreeBindableStructSource Target);
}


/**
 * Describes how the copy should be performed.
 */
enum class UE_DEPRECATED(5.6, "Use EPropertyCopyType instead")  EStateTreePropertyCopyType : uint8
{
	None,						// No copying
	
	CopyPlain,					// For plain old data types, we do a simple memcpy.
	CopyComplex,				// For more complex data types, we need to call the properties copy function
	CopyBool,					// Read and write properties using bool property helpers, as source/dest could be bitfield or boolean
	CopyStruct,					// Use struct copy operation, as this needs to correctly handle CPP struct ops
	CopyObject,					// Read and write properties using object property helpers, as source/dest could be regular/weak/soft etc.
	CopyName,					// FName needs special case because its size changes between editor/compiler and runtime.
	CopyFixedArray,				// Array needs special handling for fixed size TArrays

	StructReference,			// Copies pointer to a source struct into a FStateTreeStructRef.

	/* Promote the type during the copy */

	/* Bool promotions */
	PromoteBoolToByte,
	PromoteBoolToInt32,
	PromoteBoolToUInt32,
	PromoteBoolToInt64,
	PromoteBoolToFloat,
	PromoteBoolToDouble,

	/* Byte promotions */
	PromoteByteToInt32,
	PromoteByteToUInt32,
	PromoteByteToInt64,
	PromoteByteToFloat,
	PromoteByteToDouble,

	/* Int32 promotions */
	PromoteInt32ToInt64,
	PromoteInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteInt32ToDouble,

	/* UInt32 promotions */
	PromoteUInt32ToInt64,
	PromoteUInt32ToFloat,	// This is strictly sketchy because of potential data loss, but it is usually OK in the general case
	PromoteUInt32ToDouble,

	/* Float promotions */
	PromoteFloatToInt32,
	PromoteFloatToInt64,
	PromoteFloatToDouble,

	/* Double promotions */
	DemoteDoubleToInt32,
	DemoteDoubleToInt64,
	DemoteDoubleToFloat,
};

/** Enum describing property compatibility */
enum class UE_DEPRECATED(5.6, "Use UE::PropertyBinding::EPropertyCompatibility instead") EStateTreePropertyAccessCompatibility : uint8
{
	/** Properties are incompatible */
	Incompatible,
	/** Properties are directly compatible */
	Compatible,	
	/** Properties can be copied with a simple type promotion */
	Promotable,
};

/**
 * Descriptor for a struct or class that can be a binding source or target.
 * Each struct has unique identifier, which is used to distinguish them, and name that is mostly for debugging and UI.
 */
USTRUCT()
struct FStateTreeBindableStructDesc : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FStateTreeBindableStructDesc() = default;

#if WITH_EDITORONLY_DATA
	FStateTreeBindableStructDesc(const FString& InStatePath, const FName InName, const UStruct* InStruct, const FStateTreeDataHandle InDataHandle, const EStateTreeBindableStructSource InDataSource, const FGuid InGuid)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InGuid)
		, DataHandle(InDataHandle)
		, DataSource(InDataSource)
		, StatePath(InStatePath)
	{
	}

	UE_DEPRECATED(5.5, "Use constructor with StatePath instead.")
	FStateTreeBindableStructDesc(const FName InName, const UStruct* InStruct, const FStateTreeDataHandle InDataHandle, const EStateTreeBindableStructSource InDataSource, const FGuid InGuid)
		: FPropertyBindingBindableStructDescriptor(InName, InStruct, InGuid)
		, DataHandle(InDataHandle)
		, DataSource(InDataSource)
	{
	}

	virtual FString GetSection() const override
	{
		return StatePath;
	}
#endif // WITH_EDITORONLY_DATA

	UE_API virtual FString ToString() const override;

	/** Runtime data the struct represents. */
	UPROPERTY()
	FStateTreeDataHandle DataHandle = FStateTreeDataHandle::Invalid;

	/** Type of the source. */
	UPROPERTY()
	EStateTreeBindableStructSource DataSource = EStateTreeBindableStructSource::Context;

#if WITH_EDITORONLY_DATA
	/** In Editor path to State containing the data. */
	UPROPERTY(Transient)
	FString StatePath;
#endif
};

struct UE_DEPRECATED(5.6, "Use FPropertyBindingPathIndirection instead") FStateTreePropertyPathIndirection : FPropertyBindingPathIndirection
{
};

/**
 * Representation of a property path used for property binding in StateTree.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingPath instead") STATETREEMODULE_API FStateTreePropertyPath final : FPropertyBindingPath
{
	using FPropertyBindingPath::FPropertyBindingPath;

	//~ Intentionally not explicit to facilitate introduction of base class FPropertyBindingPath
	FStateTreePropertyPath(const FPropertyBindingPath& Other)
		: FPropertyBindingPath(Other)
	{
	}
};


USTRUCT()
struct UE_DEPRECATED(all, "Use FPropertyBindingPath instead.") STATETREEMODULE_API FStateTreeEditorPropertyPath
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA
	/** Handle of the struct this property path is relative to. */
	UPROPERTY()
	FGuid StructID;

	/** Property path segments */
	UPROPERTY()
	TArray<FString> Path;

	bool IsValid() const
	{
		return StructID.IsValid();
	}
#endif // WITH_EDITORONLY_DATA
};

/**
 * Representation of a property binding in StateTree
 */
USTRUCT()
struct FStateTreePropertyPathBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	FStateTreePropertyPathBinding() = default;

	FStateTreePropertyPathBinding(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
	{
	}

	FStateTreePropertyPathBinding(const FStateTreeDataHandle InSourceDataHandle, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InSourcePath, InTargetPath)
		, SourceDataHandle(InSourceDataHandle)
	{
	}

#if WITH_EDITOR
	FStateTreePropertyPathBinding(FConstStructView InFunctionNodeStruct, const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath)
		: FPropertyBindingBinding(InFunctionNodeStruct, InSourcePath, InTargetPath)
	{
	}
#endif // WITH_EDITOR

UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

#if WITH_EDITORONLY_DATA
	void PostSerialize(const FArchive& Ar);
#endif // WITH_EDITORONLY_DATA

	void SetSourceDataHandle(const FStateTreeDataHandle NewSourceDataHandle)
	{
		SourceDataHandle = NewSourceDataHandle;
	}

	FStateTreeDataHandle GetSourceDataHandle() const
	{
		return SourceDataHandle;
	}

protected:
	virtual FConstStructView GetSourceDataHandleStruct() const override
	{
		return FConstStructView::Make(SourceDataHandle);
	}

private:
	/** Describes how to get the source data pointer for the binding. */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UPROPERTY()
	FStateTreeEditorPropertyPath SourcePath_DEPRECATED;

	UPROPERTY()
	FStateTreeEditorPropertyPath TargetPath_DEPRECATED;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITORONLY_DATA
template<>
struct TStructOpsTypeTraits<FStateTreePropertyPathBinding> : public TStructOpsTypeTraitsBase2<FStateTreePropertyPathBinding>
{
	enum 
	{
		WithPostSerialize = true,
	};
};
#endif // WITH_EDITORONLY_DATA

/**
 * Representation of a property reference binding in StateTree.
 */
USTRUCT()
struct FStateTreePropertyRefPath
{
	GENERATED_BODY()

	FStateTreePropertyRefPath() = default;

	FStateTreePropertyRefPath(FStateTreeDataHandle InSourceDataHandle, const FPropertyBindingPath& InSourcePath)
		: SourcePropertyPath(InSourcePath)
		, SourceDataHandle(InSourceDataHandle)
	{
	}

	const FPropertyBindingPath& GetSourcePath() const { return SourcePropertyPath; }

	FPropertyBindingPath& GetMutableSourcePath() { return SourcePropertyPath; }

	void SetSourceDataHandle(const FStateTreeDataHandle NewSourceDataHandle) { SourceDataHandle = NewSourceDataHandle; }
	FStateTreeDataHandle GetSourceDataHandle() const { return SourceDataHandle; }

private:
	/** Source property path of the reference */
	UPROPERTY()
	FPropertyBindingPath SourcePropertyPath;

	/** Describes how to get the source data pointer */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;
};

PRAGMA_DISABLE_DEPRECATION_WARNINGS
/**
 * Used internally.
 * Property indirection is a resolved property path segment, used for accessing properties in structs.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingPropertyIndirection instead") STATETREEMODULE_API FStateTreePropertyIndirection
{
	/** Index in the array the property points at. */
	FStateTreeIndex16 ArrayIndex = FStateTreeIndex16::Invalid;

	/** Cached offset of the property */
	uint16 Offset = 0;

	/** Cached offset of the property */
	FStateTreeIndex16 NextIndex = FStateTreeIndex16::Invalid;

	/** Type of access/indirection. */
	EPropertyBindingPropertyAccessType Type = EPropertyBindingPropertyAccessType::Offset;

	/** Type of the struct or object instance in case the segment is pointing into an instanced data. */
	TObjectPtr<const UStruct> InstanceStruct = nullptr;

	/** Cached array property. */
	const FArrayProperty* ArrayProperty = nullptr;
};


/**
 * Used internally.
 * Describes property copy, the property from source is copied into the property at the target.
 * Copy target struct is described in the property copy batch.
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingCopyInfo instead") STATETREEMODULE_API FStateTreePropertyCopy
{
	/** Source property access. */
	FStateTreePropertyIndirection SourceIndirection;

	/** Target property access. */
	FStateTreePropertyIndirection TargetIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* TargetLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Cached property element size * dim. */
	int32 CopySize = 0;

	/** Describes how to get the source data pointer for the copy. */
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;

	/** Type of the copy */
	EStateTreePropertyCopyType Type = EStateTreePropertyCopyType::None;
};


/**
 * Describes a batch of property copies from many sources to one target struct.
 * Note: The batch is used to reference both bindings and copies (a binding turns into copy when resolved).
 */
struct UE_DEPRECATED(5.6, "Use FPropertyBindingCopyInfoBatch instead") STATETREEMODULE_API FStateTreePropertyCopyBatch
{
	/** Expected target struct */
	FStateTreeBindableStructDesc TargetStruct;

	/** Index to first binding/copy. */
	FStateTreeIndex16 BindingsBegin;

	/** Index to one past the last binding/copy. */
	FStateTreeIndex16 BindingsEnd;

	/** Index to first property function. */
	FStateTreeIndex16 PropertyFunctionsBegin;

	/** Index to one past the last property function. */
	FStateTreeIndex16 PropertyFunctionsEnd;
};
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/**
 * Describes access to referenced property.
 */
USTRUCT()
struct FStateTreePropertyAccess
{
	GENERATED_BODY()

	/** Source property access. */
	UPROPERTY()
	FPropertyBindingPropertyIndirection SourceIndirection;

	/** Cached pointer to the leaf property of the access. */
	const FProperty* SourceLeafProperty = nullptr;

	/** Type of the source data, used for validation. */
	UPROPERTY(Transient)
	TObjectPtr<const UStruct> SourceStructType = nullptr;

	/** Describes how to get the source data pointer. */
	UPROPERTY()
	FStateTreeDataHandle SourceDataHandle = FStateTreeDataHandle::Invalid;
};

/**
 * Runtime storage and execution of property bindings.
 */
USTRUCT()
struct FStateTreePropertyBindings : public FPropertyBindingBindingCollection
{
	GENERATED_BODY()

	UE_API FStateTreePropertyBindings();

	/**
	 * Clears all bindings.
	 */
	UE_API virtual void OnReset() override;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	/**
	 * @return Number of source structs the copy expects.
	 */
	UE_DEPRECATED(5.6, "Use GetNumBindableStructDescriptors instead")
	UE_API int32 GetSourceStructNum() const;

	/**
	 * Copies a property from Source to Target based on the provided Copy.
	 * @param Copy Describes which parameter and how is copied.
	 * @param SourceStructView Pointer and type for the source containing the property to be copied.
	 * @param TargetStructView Pointer and type for the target containing the property to be copied.
	 * @return true if the property was copied successfully.
	 */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	UE_API bool CopyProperty(const FStateTreePropertyCopy& Copy, FStateTreeDataView SourceStructView, FStateTreeDataView TargetStructView) const;

	/** @return copy batch at specified index. */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	UE_API const FStateTreePropertyCopyBatch& GetBatch(const FStateTreeIndex16 TargetBatchIndex) const;

	/** @return All the property copies for a specific batch. */
	UE_DEPRECATED(5.6, "Use the overload return FPropertyBindingCopyInfo and taking FPropertyBindingIndex16 instead")
	UE_API TConstArrayView<FStateTreePropertyCopy> GetBatchCopies(const FStateTreeIndex16 TargetBatchIndex) const;

	/** @return All the property copies for a specific batch. */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfoBatch instead")
	UE_API TConstArrayView<FStateTreePropertyCopy> GetBatchCopies(const FStateTreePropertyCopyBatch& Batch) const;
UE_API PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/**
	 * @return Referenced property access for provided PropertyRef.
	 */
	const FStateTreePropertyAccess* GetPropertyAccess(const FStateTreePropertyRef& Reference) const;

	/**
	 * Pointer to referenced property 
	 * @param SourceView Data view to referenced property's owner.
	 * @param PropertyAccess Access to the property for which we want to obtain a pointer.
	 * @return Pointer to referenced property if it's type match, nullptr otherwise.
	 */
	template<class T>
	T* GetMutablePropertyPtr(FStateTreeDataView SourceView, const FStateTreePropertyAccess& PropertyAccess) const;

	//~ Begin FPropertyBindingBindingCollection overrides
	UE_API virtual int32 GetNumBindableStructDescriptors() const override;
	UE_API virtual const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FConstStructView InSourceHandleView) const override;
	UE_API virtual void VisitSourceStructDescriptorInternal(TFunctionRef<EVisitResult(const FPropertyBindingBindableStructDescriptor& Descriptor)> InFunction) const override;
	//~ End FPropertyBindingBindingCollection overrides
protected:

	UE_API const FPropertyBindingBindableStructDescriptor* GetBindableStructDescriptorFromHandle(FStateTreeDataHandle InSourceHandle) const;

	//~ Begin FPropertyBindingBindingCollection overrides
	[[nodiscard]] UE_API virtual bool OnResolvingPaths() override;

	UE_API virtual int32 GetNumBindings() const override;
	UE_API virtual void ForEachBinding(TFunctionRef<void(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void ForEachBinding(const FPropertyBindingIndex16 InBegin, const FPropertyBindingIndex16 InEnd, const TFunctionRef<void(const FPropertyBindingBinding& Binding, const int32 BindingIndex)> InFunction) const override;
	UE_API virtual void ForEachMutableBinding(TFunctionRef<void(FPropertyBindingBinding& Binding)> InFunction) override;
	UE_API virtual void VisitBindings(TFunctionRef<EVisitResult(const FPropertyBindingBinding& Binding)> InFunction) const override;
	UE_API virtual void VisitMutableBindings(TFunctionRef<EVisitResult(FPropertyBindingBinding& Binding)> InFunction) override;

#if WITH_EDITOR
	UE_API virtual FPropertyBindingBinding* AddBindingInternal(const FPropertyBindingPath& InSourcePath, const FPropertyBindingPath& InTargetPath) override;
	UE_API virtual void RemoveBindingsInternal(TFunctionRef<bool(FPropertyBindingBinding&)> InPredicate) override;
	UE_API virtual bool HasBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
	UE_API virtual const FPropertyBindingBinding* FindBindingInternal(TFunctionRef<bool(const FPropertyBindingBinding&)> InPredicate) const override;
#endif // WITH_EDITOR
	//~ End FPropertyBindingBindingCollection overrides

PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingIndex16 and FPropertyBindingDataView instead")
	UE_API bool ResetObjects(const FStateTreeIndex16 TargetBatchIndex, FStateTreeDataView TargetStructView) const;

	/**
	 * Resolves what kind of copy type to use between specified property indirections.
	 * @param SourceIndirection Property path indirections of the copy source,
	 * @param TargetIndirection Property path indirections of the copy target.
	 * @param OutCopy Resulting copy type.
	 * @return true if copy was resolved, or false if no copy could be resolved between paths.
	 */
	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingCopyInfo instead")
	[[nodiscard]] static UE_API bool ResolveCopyType(const FPropertyBindingPathIndirection& SourceIndirection, const FPropertyBindingPathIndirection& TargetIndirection, FStateTreePropertyCopy& OutCopy);

	UE_DEPRECATED(5.6, "Use UE::PropertyBinding::GetPropertyCompatibility instead")
	static UE_API EStateTreePropertyAccessCompatibility GetPropertyCompatibility(const FProperty* FromProperty, const FProperty* ToProperty);

	UE_DEPRECATED(5.6, "Use the overload taking FPropertyBindingPathIndirection instead")
	[[nodiscard]] static UE_API bool ResolveCopyType(const FStateTreePropertyPathIndirection& SourceIndirection, const FStateTreePropertyPathIndirection& TargetIndirection, FStateTreePropertyCopy& OutCopy);

	UE_DEPRECATED(5.5, "DebugPrintInternalLayout is deprecated. Use DebugAsString instead.")
	UE_API void DebugPrintInternalLayout(FString& OutString) const;

private:
	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPropertyIndirection instead.")
	[[nodiscard]] UE_API bool ResolvePath(const UStruct* Struct, const FPropertyBindingPath& Path, FStateTreePropertyIndirection& OutFirstIndirection, FPropertyBindingPathIndirection& OutLeafIndirection);

	UE_DEPRECATED(5.6, "Use GetBindableStructDescriptorFromHandle instead.")
	UE_API const FStateTreeBindableStructDesc* GetSourceDescByHandle(const FStateTreeDataHandle SourceDataHandle);

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingCopyInfo instead.")
	UE_API void PerformCopy(const FStateTreePropertyCopy& Copy, uint8* SourceAddress, uint8* TargetAddress) const;

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingCopyInfo instead.")
	UE_API void PerformResetObjects(const FStateTreePropertyCopy& Copy, uint8* TargetAddress) const;

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBinding types instead.")
	UE_API uint8* GetAddress(FStateTreeDataView InStructView, const FStateTreePropertyIndirection& FirstIndirection, const FProperty* LeafProperty) const;
PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** Array of expected source structs. */
	UPROPERTY()
	TArray<FStateTreeBindableStructDesc> SourceStructs;

	/** Array of property bindings, resolved into arrays of copies before use. */
	UPROPERTY()
	TArray<FStateTreePropertyPathBinding> PropertyPathBindings;

	/** Array of referenced property paths */
	UPROPERTY()
	TArray<FStateTreePropertyRefPath> PropertyReferencePaths;

	/** Array of individually accessed properties */
	UPROPERTY()
	TArray<FStateTreePropertyAccess> PropertyAccesses;

	friend FStateTreePropertyBindingCompiler;
	friend UStateTree;
};

template <class T>
T* FStateTreePropertyBindings::GetMutablePropertyPtr(FStateTreeDataView SourceView, const FStateTreePropertyAccess& PropertyAccess) const
{
	check(SourceView.GetStruct() == PropertyAccess.SourceStructType);

	if (!UE::StateTree::PropertyRefHelpers::Validator<std::remove_cv_t<T>>::IsValid(*PropertyAccess.SourceLeafProperty))
	{
		return nullptr;
	}

	return reinterpret_cast<T*>(Super::GetAddress(SourceView, PropertyAccess.SourceIndirection, PropertyAccess.SourceLeafProperty));
}

/**
 * Helper interface to reason about bound properties. The implementation is in the editor plugin.
 */
struct IStateTreeBindingLookup
{
	virtual ~IStateTreeBindingLookup() {} 

	/** @return Source path for given target path, or null if binding does not exists. */
	virtual const FPropertyBindingPath* GetPropertyBindingSource(const FPropertyBindingPath& InTargetPath) const = 0;

	/** @return Display name given property path. */
	virtual FText GetPropertyPathDisplayName(const FPropertyBindingPath& InPath, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const = 0;

	/** @return Leaf property based on property path. */
	virtual const FProperty* GetPropertyPathLeafProperty(const FPropertyBindingPath& InPath) const = 0;

	/** @return Display name of binding source, or empty if binding does not exists. */
	virtual FText GetBindingSourceDisplayName(const FPropertyBindingPath& InTargetPath, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const = 0;

PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual const FStateTreePropertyPath* GetPropertyBindingSource(const FStateTreePropertyPath& InTargetPath) const final
	{
		return nullptr;
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual FText GetPropertyPathDisplayName(const FStateTreePropertyPath& InPath, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const final
	{
		return FText::GetEmpty();
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual FText GetBindingSourceDisplayName(const FStateTreePropertyPath& InTargetPath, EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const final
	{
		return FText::GetEmpty();
	}

	UE_DEPRECATED(5.6, "Use the version taking FPropertyBindingPath instead")
	virtual const FProperty* GetPropertyPathLeafProperty(const FStateTreePropertyPath& InPath) const final
	{
		return nullptr;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
};


namespace UE::StateTree
{
	/** @return desc and path as a display string. */
	extern STATETREEMODULE_API FString GetDescAndPathAsString(const FStateTreeBindableStructDesc& Desc, const FPropertyBindingPath& Path);

#if WITH_EDITOR
	/**
	 * Returns property usage based on the Category metadata of given property.
	 * @param Property Handle to property where value is got from.
	 * @return found usage type, or EStateTreePropertyUsage::Invalid if not found.
	 */
	STATETREEMODULE_API EStateTreePropertyUsage GetUsageFromMetaData(const FProperty* Property);

	/** @return struct's property which is the only one marked as Output. Returns null otherwise. */
	STATETREEMODULE_API const FProperty* GetStructSingleOutputProperty(const UStruct& InStruct);
#endif
} // UE::StateTree

#undef UE_API
