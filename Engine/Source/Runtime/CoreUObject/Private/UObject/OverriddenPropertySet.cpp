// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/OverriddenPropertySet.h"

#include "HAL/IConsoleManager.h"
#include "UObject/OverridableManager.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"
#include "UObject/PropertyOptional.h"
#include "Misc/ScopeExit.h"
#include "UObject/InstanceDataObjectUtils.h"
#include "UObject/UObjectArchetypeHelper.h"
#include "UObject/UObjectThreadContext.h"

/*
 *************************************************************************************
 * Overridable serialization is experimental, not supported and use at your own risk *
 *************************************************************************************
 */

DEFINE_LOG_CATEGORY(LogOverridableObject);

//----------------------------------------------------------------------//
// FOverridableSerializationLogicInternalAdapter
//----------------------------------------------------------------------//

struct FOverridableSerializationLogicInternalAdapter
{
	static void SetCapability(FOverridableSerializationLogic::ECapabilities InCapability, bool bEnable)
	{
		if (bEnable)
		{
			FOverridableSerializationLogic::Capabilities = FOverridableSerializationLogic::Capabilities | InCapability;
		}
		else
		{
			FOverridableSerializationLogic::Capabilities = FOverridableSerializationLogic::Capabilities & ~InCapability;
		}
	}
};

namespace Private
{
	bool bEnableT3D = true;
	FAutoConsoleVariableRef CVar_bT3DOverrideSerializationEnabled(
		TEXT("OverridableSerializationLogic.Capabilities.T3D"),
		bEnableT3D,
		TEXT("Enables serialization of override state into/from T3D"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			FOverridableManager& Manager = FOverridableManager::Get();
			FOverridableSerializationLogicInternalAdapter::SetCapability(FOverridableSerializationLogic::ECapabilities::T3DSerialization, bEnableT3D);
		}));

	bool bEnableSubObjectsShadowSerialization = true;
	FAutoConsoleVariableRef CVar_bEnableSubObjectsShadowSerialization(
		TEXT("OverridableSerializationLogic.Capabilities.SubObjectsShadowSerialization"),
		bEnableSubObjectsShadowSerialization,
		TEXT("Enables shadow serialization of subobject"),
		FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Variable)
		{
			FOverridableManager& Manager = FOverridableManager::Get();
			FOverridableSerializationLogicInternalAdapter::SetCapability(FOverridableSerializationLogic::ECapabilities::SubObjectsShadowSerialization, bEnableSubObjectsShadowSerialization);
		}));

	struct FCapabilitiesAutoInitializer
	{
		FCapabilitiesAutoInitializer()
		{
			FOverridableSerializationLogicInternalAdapter::SetCapability(FOverridableSerializationLogic::ECapabilities::T3DSerialization, bEnableT3D);
			FOverridableSerializationLogicInternalAdapter::SetCapability(FOverridableSerializationLogic::ECapabilities::SubObjectsShadowSerialization, bEnableSubObjectsShadowSerialization);
		}
	};

	static FCapabilitiesAutoInitializer CapabilitiesAutoInitializer;
}

//----------------------------------------------------------------------//
// FOverridableSerializationLogic
//----------------------------------------------------------------------//

ENUM_CLASS_FLAGS(FOverridableSerializationLogic::ECapabilities);

FOverridableSerializationLogic::ECapabilities FOverridableSerializationLogic::Capabilities = FOverridableSerializationLogic::ECapabilities::None;
thread_local bool FOverridableSerializationLogic::bUseOverridableSerialization = false;
thread_local FOverriddenPropertySet* FOverridableSerializationLogic::OverriddenProperties = nullptr;
thread_local FPropertyVisitorPath* FOverridableSerializationLogic::OverriddenPortTextPropertyPath = nullptr;

EOverriddenPropertyOperation FOverridableSerializationLogic::GetOverriddenPropertyOperation(const int32 PortFlags, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property, const void* DataPtr, const void* DefaultValue)
{
	checkf(bUseOverridableSerialization, TEXT("Nobody should use this method if it is not setup to use overridable serialization"));
	if (!OverriddenProperties)
	{
		return EOverriddenPropertyOperation::None;
	}

	const FProperty* CurrentProperty = Property ? Property : (CurrentPropertyChain ? CurrentPropertyChain->GetPropertyFromStack(0) : nullptr);
	checkf( CurrentProperty, TEXT("Expecting a property to get OS operation on") );

	if (CurrentProperty->HasAnyPropertyFlags(CPF_ExperimentalNeverOverriden))
	{
		return EOverriddenPropertyOperation::None;
	}

	const EOverriddenPropertyOperation OverriddenOperation = OverriddenProperties->GetOverriddenPropertyOperation(CurrentPropertyChain, Property);
	if (OverriddenOperation != EOverriddenPropertyOperation::None)
	{
		return OverriddenOperation;
	}

	// In the case of a CDO owning default value, we might need to serialize it to keep its value.
	if (DataPtr && DefaultValue && OverriddenProperties->IsCDOOwningProperty(*CurrentProperty))
	{
		// Only need serialize this value if it is different from the default property value
		if (!CurrentProperty->Identical(DataPtr, DefaultValue, PortFlags))
		{
			return 	EOverriddenPropertyOperation::Replace;
		}
	}

	if (ShouldPropertyShadowSerializeSubObject(CurrentProperty))
	{
		return EOverriddenPropertyOperation::SubObjectsShadowing;
	}

	return EOverriddenPropertyOperation::None;
}

bool FOverridableSerializationLogic::ShouldPropertyShadowSerializeSubObject(TNotNull<const FProperty*> Property)
{
	// Check if the shadow serialization of subobject is enabled
	if (!HasCapabilities(ECapabilities::SubObjectsShadowSerialization))
	{
		return false;
	}

	// We shadow serialize every object property
	if (CastField<FObjectPropertyBase>(Property))
	{
		return true;
	}

	// Otherwise check if the property is in the reference linked list
	// @Todo optimized by caching the call to FProperty::ContainsObjectReference() maybe as a CPF_ContainsReferences?
	checkf(Property->GetOwnerStruct(), TEXT("Expecting an owner struct for this type of property"));
	FProperty* CurrentRefLink = Property->GetOwnerStruct()->RefLink;
	while (CurrentRefLink != nullptr)
	{
		if (CurrentRefLink == Property)
		{
			return true;
		}
		CurrentRefLink = CurrentRefLink->NextRef;
	}

	return false;
}

bool FOverridableSerializationLogic::HasCapabilities(ECapabilities InCapabilities)
{
	return (Capabilities & InCapabilities) == InCapabilities;
}

FOverriddenPropertySet* FOverridableSerializationLogic::GetOverriddenPropertiesSlow()
{
	return OverriddenProperties;
}

EOverriddenPropertyOperation FOverridableSerializationLogic::GetOverriddenPropertyOperation(const FArchive& Ar, FProperty* Property /*= nullptr*/, uint8* DataPtr /*= nullptr*/, uint8* DefaultValue /*= nullptr*/)
{
	const FArchiveSerializedPropertyChain* CurrentPropertyChain = Ar.GetSerializedPropertyChain();
	const EOverriddenPropertyOperation Operation = GetOverriddenPropertyOperation(Ar.GetPortFlags(), CurrentPropertyChain, Property, DataPtr, DefaultValue);

	// During transactions, we do not want any subobject shadow serialization
	return Operation == EOverriddenPropertyOperation::SubObjectsShadowing && Ar.IsTransacting() ? EOverriddenPropertyOperation::None : Operation;
}

EOverriddenPropertyOperation FOverridableSerializationLogic::GetOverriddenPropertyOperationForPortText(const void* DataPtr, const void* DefaultValue, int32 PortFlags)
{
	checkf(OverriddenPortTextPropertyPath, TEXT("Expecting an overridden port text path"));

	const FArchiveSerializedPropertyChain CurrentPropertyChain = OverriddenPortTextPropertyPath->ToSerializedPropertyChain();
	const EOverriddenPropertyOperation Operation = GetOverriddenPropertyOperation(PortFlags,  &CurrentPropertyChain, nullptr, DataPtr, DefaultValue);

	// For now lets not support subobject shadow serialization until the copy and paste support loose property or placeholder
	return Operation != EOverriddenPropertyOperation::SubObjectsShadowing ? Operation : EOverriddenPropertyOperation::None;
}

FPropertyVisitorPath* FOverridableSerializationLogic::GetOverriddenPortTextPropertyPath()
{
	return OverriddenPortTextPropertyPath;
}

void FOverridableSerializationLogic::SetOverriddenPortTextPropertyPath(FPropertyVisitorPath& Path)
{
	checkf(OverriddenPortTextPropertyPath == nullptr, TEXT("Should not set a path on top of an existing one"));
	OverriddenPortTextPropertyPath = &Path;
}

void FOverridableSerializationLogic::ResetOverriddenPortTextPropertyPath()
{
	OverriddenPortTextPropertyPath = nullptr;
}

//----------------------------------------------------------------------//
// FOverridableSerializationScope
//----------------------------------------------------------------------//
FEnableOverridableSerializationScope::FEnableOverridableSerializationScope(bool bEnableOverridableSerialization, FOverriddenPropertySet* OverriddenProperties)
{
	if (bEnableOverridableSerialization)
	{
		if (FOverridableSerializationLogic::IsEnabled())
		{
			bWasOverridableSerializationEnabled = true;
			SavedOverriddenProperties = FOverridableSerializationLogic::GetOverriddenProperties();
			FOverridableSerializationLogic::Disable();
		}
		FOverridableSerializationLogic::Enable(OverriddenProperties);
		bOverridableSerializationEnabled = true;
	}
}

FEnableOverridableSerializationScope::~FEnableOverridableSerializationScope()
{
	if (bOverridableSerializationEnabled)
	{
		FOverridableSerializationLogic::Disable();
		if (bWasOverridableSerializationEnabled)
		{
			FOverridableSerializationLogic::Enable(SavedOverriddenProperties);
		}
	}
}

//----------------------------------------------------------------------//
// FOverridableTextImportPropertyPathScope
//----------------------------------------------------------------------//
FOverridableTextPortPropertyPathScope::FOverridableTextPortPropertyPathScope(const FProperty* InProperty, int32 InIndex/* = INDEX_NONE*/, EPropertyVisitorInfoType InPropertyInfo /*= EPropertyVisitorInfoType::None*/)
{
	if (!FOverridableSerializationLogic::IsEnabled())
	{
		return;
	}

	checkf(InProperty, TEXT("Expecting a valid property ptr"));

	// Save property for comparison in the destructor
	Property = InProperty;

	FPropertyVisitorPath* Path = FOverridableSerializationLogic::GetOverriddenPortTextPropertyPath();
	if (!Path)
	{
		FOverridableSerializationLogic::SetOverriddenPortTextPropertyPath(DefaultPath);
		Path = &DefaultPath;
	}

	Path->Push(FPropertyVisitorInfo(InProperty, InIndex, InPropertyInfo));
}

FOverridableTextPortPropertyPathScope::~FOverridableTextPortPropertyPathScope()
{
	if (Property)
	{
		FPropertyVisitorPath* Path = FOverridableSerializationLogic::GetOverriddenPortTextPropertyPath();
		checkf(Path, TEXT("Expecting a valid path "));
		checkf(Path->Num(), TEXT("Expecting at least one property in the path"));
		verifyf(Path->Pop().Property == Property, TEXT("Expecting at the top property to match the one we pushed in the constructor"));
		if (!Path->Num())
		{
			FOverridableSerializationLogic::ResetOverriddenPortTextPropertyPath();
		}
	}
}

//----------------------------------------------------------------------//
// FOverriddenPropertyNodeID
//----------------------------------------------------------------------//
FOverriddenPropertyNodeID::FOverriddenPropertyNodeID(const FProperty* Property)
	: Object(nullptr)
{
	if (Property)
	{
		// append typename to the end of the property ID
		UE::FPropertyTypeNameBuilder TypeNameBuilder;
#if WITH_EDITORONLY_DATA
		{
			// use property impersonation for SaveTypeName so that keys don't change when classes die
			FUObjectSerializeContext* SerializeContext = FUObjectThreadContext::Get().GetSerializeContext();
            TGuardValue<bool> ScopedImpersonateProperties(SerializeContext->bImpersonateProperties, true);
            Property->SaveTypeName(TypeNameBuilder);
		}
#endif
		UE::FPropertyTypeName TypeName = TypeNameBuilder.Build();
		TStringBuilder<256> StringBuilder;
		StringBuilder << Property->GetFName();
		StringBuilder << " - ";
		StringBuilder << TypeName;
		Path = FName(StringBuilder.ToView());
	}
}

FOverriddenPropertyNodeID::FOverriddenPropertyNodeID(const FOverriddenPropertyNodeID& ParentNodeID, const FOverriddenPropertyNodeID& NodeID)
{
	// Combine the 2 node ids
	FStringBuilderBase SubPropertyKeyBuilder;
	SubPropertyKeyBuilder = *ParentNodeID.ToString();
	SubPropertyKeyBuilder.Append(TEXT("."));
	SubPropertyKeyBuilder.Append(*NodeID.ToString());
	Path = FName(SubPropertyKeyBuilder.ToString());
	Object = NodeID.Object;
}

FOverriddenPropertyNodeID FOverriddenPropertyNodeID::RootNodeId()
{
	FOverriddenPropertyNodeID Result;
	Result.Path = FName(TEXT("root"));
	return Result;
}

FOverriddenPropertyNodeID FOverriddenPropertyNodeID::FromMapKey(const FProperty* KeyProperty, const void* KeyData)
{
	if (const FObjectPropertyBase* KeyObjectProperty = CastField<FObjectPropertyBase>(KeyProperty))
	{
		if (const UObject* Object = KeyObjectProperty->GetObjectPropertyValue(KeyData))
		{
			return FOverriddenPropertyNodeID(Object);
		}
	}
	else
	{
		FString KeyString;
		KeyProperty->ExportTextItem_Direct(KeyString, KeyData, /*DefaultValue*/nullptr, /*Parent*/nullptr, PPF_None);
		FOverriddenPropertyNodeID Result;
		Result.Path = FName(KeyString);
		return Result;
	}
		
	checkf(false, TEXT("This case is not handled"))
	return FOverriddenPropertyNodeID();
}

int32 FOverriddenPropertyNodeID::ToMapInternalIndex(FScriptMapHelper& MapHelper) const
{
	// Special case for object we didn't use the pointer to create the key
	if (const FObjectPropertyBase* KeyObjectProperty = CastField<FObjectPropertyBase>(MapHelper.KeyProp))
	{
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It) 
		{
			if (UObject* CurrentObject = KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(It)))
			{
				if ((*this) == FOverriddenPropertyNodeID(CurrentObject))
				{
					return It.GetInternalIndex();
				}
			}
		}
	}
	else
	{
		// Default case, just import the text as key value for comparison
		void* TempKeyValueStorage = FMemory_Alloca(MapHelper.MapLayout.SetLayout.Size);
		MapHelper.KeyProp->InitializeValue(TempKeyValueStorage);

		FString KeyToFind(ToString());
		MapHelper.KeyProp->ImportText_Direct(*KeyToFind, TempKeyValueStorage, nullptr, PPF_None);

		const int32 InternalIndex = MapHelper.FindMapPairIndexFromHash(TempKeyValueStorage);

		MapHelper.KeyProp->DestroyValue(TempKeyValueStorage);

		return InternalIndex;
	}
	return INDEX_NONE;
}

bool FOverriddenPropertyNodeID::operator==(const FOverriddenPropertyNodeID& Other) const
{
	if (Path == Other.Path)
	{
		return true;
	}

	// After reinstanciation, we do not change the path we only patch the ptr.
	// Unfortunately we do not have a stable id that is stable through reinstantiation,
	// so are only way if to compare ptr.
	if (Object && Other.Object && Object == Other.Object)
	{
		return true;
	}

	return false;
}

void FOverriddenPropertyNodeID::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map)
{
	if (!Object)
	{
		return;
	}

	if (UObject*const* ReplacedObject = Map.Find(Object))
	{
		Object = *ReplacedObject;
	}
}

void FOverriddenPropertyNodeID::AddReferencedObjects(FReferenceCollector& Collector, UObject* Owner)
{
	Collector.AddReferencedObject(Object, Owner);
}

void FOverriddenPropertyNodeID::HandleDeadObjectReferences(const TSet<UObject*>& ActiveInstances, const TSet<UObject*>& TemplateInstances)
{
	if (!Object)
	{
		return;
	}

	if (ActiveInstances.Find(Object) || TemplateInstances.Find(Object) )
	{
		Object = nullptr;
	}
}

void FOverriddenPropertySet::RestoreOverriddenState(const FOverriddenPropertySet& FromOverriddenProperties)
{
	bWasAdded = FromOverriddenProperties.bWasAdded;
}

//----------------------------------------------------------------------//
// FOverriddenPropertySet
//----------------------------------------------------------------------//
FOverriddenPropertyNode& FOverriddenPropertySet::FindOrAddNode(FOverriddenPropertyNode& ParentNode, FOverriddenPropertyNodeID NodeID)
{
	FOverriddenPropertyNodeID& SubNodeID = ParentNode.SubPropertyNodeKeys.FindOrAdd(NodeID, FOverriddenPropertyNodeID());
	if (SubNodeID.IsValid())
	{
		FOverriddenPropertyNode* FoundNode = OverriddenPropertyNodes.Find(SubNodeID);
		checkf(FoundNode, TEXT("Expecting a node"));
		return *FoundNode;
	}

	// We can safely assume that the parent node is at least modified from now on
	if (ParentNode.Operation == EOverriddenPropertyOperation::None)
	{
		ParentNode.Operation = EOverriddenPropertyOperation::Modified;
	}

	// Not found add the node
	SubNodeID = FOverriddenPropertyNodeID(ParentNode.NodeID, NodeID);
	const FSetElementId NewID = OverriddenPropertyNodes.Emplace(SubNodeID);
	return OverriddenPropertyNodes.Get(NewID);
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FOverriddenPropertyNode* ParentPropertyNode, FPropertyVisitorPath::Iterator PropertyIterator, bool* bOutInheritedOperation, const void* Data) const
{
	FOverridableManager& OverridableManager = FOverridableManager::Get();

	const void* SubValuePtr = Data;
	const FOverriddenPropertyNode* OverriddenPropertyNode = ParentPropertyNode;
	int32 ArrayIndex = INDEX_NONE;
	while (PropertyIterator && (!OverriddenPropertyNode || OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace))
	{
		ArrayIndex = INDEX_NONE;

		const FProperty* CurrentProperty = PropertyIterator->Property;
		SubValuePtr = CurrentProperty->ContainerPtrToValuePtr<void>(SubValuePtr, 0); //@todo support static arrays

		const FOverriddenPropertyNode* CurrentOverriddenPropertyNode = nullptr;
		if (OverriddenPropertyNode)
		{
			if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentProperty))
			{
				CurrentOverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
				checkf(CurrentOverriddenPropertyNode, TEXT("Expecting a node"));
			}
		}

		FPropertyVisitorPath::Iterator NextPropertyIterator = PropertyIterator+1;
		// Special handling for instanced subobjects 
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(CurrentProperty))
		{
			if (NextPropertyIterator)
			{
				// Forward any sub queries to the subobject
				if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
				{
					// This should not be needed in the property grid, as it should already been called on the subobject.
					return OverridableManager.GetOverriddenPropertyOperation(SubObject, NextPropertyIterator, bOutInheritedOperation);
				}
			}
		}
		// Special handling for array of instanced subobjects 
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyIterator->Index;
			checkf(ArrayIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index"));

			// Only special case is instanced subobjects, otherwise we fallback to full array override
			checkf(ArrayProperty->Inner, TEXT("Expecting an inner type for Arrays"));
			if (const FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);
				if(ArrayHelper.IsValidIndex(ArrayIndex))
				{
					if (UObject* SubObject = TryGetInstancedSubObjectValue(InnerObjectProperty, ArrayHelper.GetElementPtr(ArrayIndex)))
					{
						if (NextPropertyIterator)
						{
							// Forward any sub queries to the subobject
							return OverridableManager.GetOverriddenPropertyOperation(SubObject, NextPropertyIterator, bOutInheritedOperation);
						}
						else if(CurrentOverriddenPropertyNode)
						{
							// Caller wants to know about any override state on the reference of the subobject itself
							const FOverriddenPropertyNodeID  SubObjectID(SubObject);
							if (const FOverriddenPropertyNodeID* CurrentPropKey = CurrentOverriddenPropertyNode->SubPropertyNodeKeys.Find(SubObjectID))
							{
								const FOverriddenPropertyNode* SubObjectOverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
								checkf(SubObjectOverriddenPropertyNode, TEXT("Expecting a node"));
								if (bOutInheritedOperation)
								{
									*bOutInheritedOperation = false;
								}
								return SubObjectOverriddenPropertyNode->Operation;
							}
						}
					}
				}
			}
		}
		// Special handling for maps and values of instance subobjects
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyIterator->Index;
			checkf(ArrayIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index"));

			checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
			FScriptMapHelper MapHelper(MapProperty, SubValuePtr);

			const int32 InternalMapIndex = ArrayIndex != INDEX_NONE ? MapHelper.FindInternalIndex(ArrayIndex) : INDEX_NONE;
			if(MapHelper.IsValidIndex(InternalMapIndex))
			{
				if (NextPropertyIterator)
				{
					// Forward any sub queries to the subobject
					if (const FObjectPropertyBase* ValueObjectProperty = CastField<FObjectPropertyBase>(MapProperty->ValueProp))
					{
						if (UObject* ValueSubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(InternalMapIndex)))
						{
							return OverridableManager.GetOverriddenPropertyOperation(ValueSubObject, NextPropertyIterator, bOutInheritedOperation);
						}
					}
				}
				else if(CurrentOverriddenPropertyNode)
				{
					// Caller wants to know about any override state on the reference of the map pair itself
					checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
					FOverriddenPropertyNodeID OverriddenKeyID = FOverriddenPropertyNodeID::FromMapKey(MapProperty->KeyProp, MapHelper.GetKeyPtr(InternalMapIndex));

					if (const FOverriddenPropertyNodeID* CurrentPropKey = CurrentOverriddenPropertyNode->SubPropertyNodeKeys.Find(OverriddenKeyID))
					{
						const FOverriddenPropertyNode* SubObjectOverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
						checkf(SubObjectOverriddenPropertyNode, TEXT("Expecting a node"));
						if (bOutInheritedOperation)
						{
							*bOutInheritedOperation = false;
						}
						return SubObjectOverriddenPropertyNode->Operation;
					}
				}
			}
		}

		OverriddenPropertyNode = CurrentOverriddenPropertyNode;
		// While digging down the path, if there is one property that is always overridden
		// stop there and return replace
		if (CurrentProperty->HasAnyPropertyFlags(CPF_ExperimentalAlwaysOverriden))
		{
			if (bOutInheritedOperation)
			{
				*bOutInheritedOperation = NextPropertyIterator ? true : false;
			}
			return EOverriddenPropertyOperation::Replace;
		}

		++PropertyIterator;
	}

	if (bOutInheritedOperation)
	{
		*bOutInheritedOperation = PropertyIterator || ArrayIndex != INDEX_NONE;
	}
	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

bool FOverriddenPropertySet::ClearOverriddenProperty(FOverriddenPropertyNode& ParentPropertyNode, FPropertyVisitorPath::Iterator PropertyIterator, const void* Data)
{
	FOverridableManager& OverridableManager = FOverridableManager::Get();
	if (!PropertyIterator)
	{
		// if no property iterator is provided, clear all overrides
		OverridableManager.ClearOverrides(Owner);
		return true;
	}

	bool bClearedOverrides = false;
	const void* SubValuePtr = Data;
	FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	int32 ArrayIndex = INDEX_NONE;
	TArray<FOverriddenPropertyNodeID> TraversedNodes;
	TraversedNodes.Push(OverriddenPropertyNode->NodeID);
	while (PropertyIterator && (!OverriddenPropertyNode || OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace))
	{
		ArrayIndex = INDEX_NONE;

		const FProperty* CurrentProperty = PropertyIterator->Property;
		SubValuePtr = CurrentProperty->ContainerPtrToValuePtr<void>(SubValuePtr, 0); //@todo support static arrays

		FOverriddenPropertyNode* CurrentOverriddenPropertyNode = nullptr;
		if (OverriddenPropertyNode)
		{
			if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentProperty))
			{
				CurrentOverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
				checkf(CurrentOverriddenPropertyNode, TEXT("Expecting a node"));
				TraversedNodes.Push(CurrentOverriddenPropertyNode->NodeID);
			}
		}

		// Special handling for instanced subobjects 
		FPropertyVisitorPath::Iterator NextPropertyIterator = PropertyIterator+1;
		if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(CurrentProperty))
		{
			if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
			{
				if (NextPropertyIterator)
				{
					return OverridableManager.ClearOverriddenProperty(SubObject, NextPropertyIterator);
				}
				else
				{
					OverridableManager.ClearOverrides(SubObject);
					bClearedOverrides = true;
				}
			}
		}
		// Special handling for array of instanced subobjects 
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyIterator->Index;
			checkf(ArrayIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index"));

			// Only special case is instanced subobjects, otherwise we fallback to full array override
			if (FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);

				if(ArrayIndex == INDEX_NONE)
				{
					// This is a case of the entire array needs to be cleared
					// Need to loop through every sub object and clear them
					for (int i = 0; i < ArrayHelper.Num(); ++i)
					{
						if (UObject* SubObject = TryGetInstancedSubObjectValue(InnerObjectProperty, ArrayHelper.GetElementPtr(i)))
						{
							OverridableManager.ClearInstancedSubObjectOverrides(Owner, SubObject);
						}
					}
					bClearedOverrides = true;
				}
				else if(ArrayHelper.IsValidIndex(ArrayIndex))
				{
					if (UObject* SubObject = TryGetInstancedSubObjectValue(InnerObjectProperty, ArrayHelper.GetElementPtr(ArrayIndex)))
					{
						if (NextPropertyIterator)
						{
							return OverridableManager.ClearOverriddenProperty(SubObject, NextPropertyIterator);
						}
						else if (CurrentOverriddenPropertyNode)
						{
							const FOverriddenPropertyNodeID  SubObjectID(SubObject);
							FOverriddenPropertyNodeID CurrentPropKey;
							if (CurrentOverriddenPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(SubObjectID, CurrentPropKey))
							{
								verifyf(OverriddenPropertyNodes.Remove(CurrentPropKey), TEXT("Expecting a node to be removed"));
								OverridableManager.ClearInstancedSubObjectOverrides(Owner, SubObject);
								return true;
							}
						}
					}
				}
			}
		}
		// Special handling for maps and values of instance subobjects 
		else if (const FMapProperty* MapProperty = CastField<FMapProperty>(CurrentProperty))
		{
			ArrayIndex = PropertyIterator->Index;
			checkf(ArrayIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index"));

			FScriptMapHelper MapHelper(MapProperty, SubValuePtr);

			const int32 InternalMapIndex = ArrayIndex != INDEX_NONE ? MapHelper.FindInternalIndex(ArrayIndex) : INDEX_NONE;
			const FObjectPropertyBase* ValueObjectProperty = CastField<FObjectPropertyBase>(MapProperty->ValueProp);

			// If there is a next node, it is probably because the map value is holding a instanced subobject and the user is changing value on it.
			// So forward the call to the instanced subobject
			if (NextPropertyIterator)
			{
				if(MapHelper.IsValidIndex(InternalMapIndex))
				{
					checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
					if (UObject* ValueSubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(InternalMapIndex)))
					{
						return OverridableManager.ClearOverriddenProperty(ValueSubObject, NextPropertyIterator);
					}
				}
			}
			else if(InternalMapIndex == INDEX_NONE)
			{
				// Users want to clear all of the overrides on the array, but in the case of instanced subobject, we need to clear the overrides on them as well.
				if (ValueObjectProperty)
				{
					// This is a case of the entire array needs to be cleared
					// Need to loop through every sub object and clear them
					for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
					{
						if (UObject* ValueSubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(It.GetInternalIndex())))
						{
							OverridableManager.ClearInstancedSubObjectOverrides(Owner, ValueSubObject);
						}
					}
				}
				bClearedOverrides = true;
			}
			else if (MapHelper.IsValidIndex(InternalMapIndex) && CurrentOverriddenPropertyNode)
			{
				checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
				FOverriddenPropertyNodeID OverriddenKeyID = FOverriddenPropertyNodeID::FromMapKey(MapProperty->KeyProp, MapHelper.GetKeyPtr(InternalMapIndex));

				FOverriddenPropertyNodeID CurrentPropKey;
				if (CurrentOverriddenPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(OverriddenKeyID, CurrentPropKey))
				{
					verifyf(OverriddenPropertyNodes.Remove(CurrentPropKey), TEXT("Expecting a node to be removed"));

					if (UObject* ValueSubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(InternalMapIndex)))
					{
						// In the case of a instanced subobject, clear all the overrides on the subobject as well
						OverridableManager.ClearInstancedSubObjectOverrides(Owner, ValueSubObject);
					}

					return true;
				}
			}
		}

		OverriddenPropertyNode = CurrentOverriddenPropertyNode;
		++PropertyIterator;
	}

	auto CleanupClearedNodes = [this, &TraversedNodes]()
	{
		// Go through each traversed property in reversed order to do cleanup
		// We need to continue the cleanup until there is more overrides than just the one we are removing
		FOverriddenPropertyNodeID LastCleanedNode;
		while (FOverriddenPropertyNode* CurrentNode = !TraversedNodes.IsEmpty() ? OverriddenPropertyNodes.Find(TraversedNodes.Top()) : nullptr)
		{
			TraversedNodes.Pop();

			if (LastCleanedNode.IsValid())
			{
				const FOverriddenPropertyNodeID* NodeToRemove = CurrentNode->SubPropertyNodeKeys.FindKey(LastCleanedNode);
				checkf(NodeToRemove, TEXT("Expecting to find the last cleaned node"));

				// In the case there are other overrides, just cleanup that node and stop.
				if (CurrentNode->SubPropertyNodeKeys.Num() > 1)
				{
					CurrentNode->SubPropertyNodeKeys.Remove(*NodeToRemove);
					verifyf(OverriddenPropertyNodes.Remove(LastCleanedNode), TEXT("Expecting the node to be removed"));
					break;
				}
			}

			RemoveOverriddenSubProperties(*CurrentNode);
			LastCleanedNode = CurrentNode->NodeID;
		}
	};

	if (PropertyIterator || OverriddenPropertyNode == nullptr)
	{
		if (bClearedOverrides)
		{
			CleanupClearedNodes();
		}

		return bClearedOverrides;
	}

	if (ArrayIndex != INDEX_NONE)
	{
		return false;
	}

	CleanupClearedNodes();
	return true;
}

void FOverriddenPropertySet::NotifyPropertyChange(FOverriddenPropertyNode* ParentPropertyNode, const EPropertyNotificationType Notification, FPropertyVisitorPath::Iterator PropertyIterator, const EPropertyChangeType::Type ChangeType, const void* Data, bool& bNeedsCleanup)
{
	checkf(IsValid(Owner), TEXT("Expecting a valid overridable owner"));

	if (ChangeType == EPropertyChangeType::ResetToDefault)
	{
		if (ParentPropertyNode && Notification == EPropertyNotificationType::PostEdit)
		{
			this->ClearOverriddenProperty(*ParentPropertyNode, PropertyIterator, Data);
		}
		return;
	}

	FOverridableManager& OverridableManager = FOverridableManager::Get();
	if (!PropertyIterator)
	{
		if (ParentPropertyNode && Notification == EPropertyNotificationType::PostEdit)
		{
			// Sub-property overrides are not needed from now on, so clear them
			RemoveOverriddenSubProperties(*ParentPropertyNode);

			// Replacing this entire property
			ParentPropertyNode->Operation = EOverriddenPropertyOperation::Replace;

			// If we are overriding the root node, need to propagate the overrides to all instanced sub object
			const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.Find(RootNodeID);
			checkf(RootNode, TEXT("Expecting to always have a "));
			if (RootNode == ParentPropertyNode)
			{
				OverridableManager.PropagateOverrideToInstancedSubObjects(Owner);
			}
		}
		return;
	}

	const FProperty* Property = PropertyIterator->Property;
	checkf(Property, TEXT("Expecting a valid property"));

	const void* SubValuePtr = Property->ContainerPtrToValuePtr<void>(Data, 0); //@todo support static arrays

	FOverriddenPropertyNode* SubPropertyNode = nullptr;
	if (ParentPropertyNode)
	{
		FOverriddenPropertyNode& SubPropertyNodeRef = FindOrAddNode(*ParentPropertyNode, Property);
		SubPropertyNode = SubPropertyNodeRef.Operation != EOverriddenPropertyOperation::Replace ? &SubPropertyNodeRef : nullptr;
	}

	ON_SCOPE_EXIT
	{
		if (!ParentPropertyNode || Notification != EPropertyNotificationType::PostEdit)
		{
			return;
		}

		if (SubPropertyNode && SubPropertyNode->SubPropertyNodeKeys.IsEmpty() &&
			(bNeedsCleanup || 
			 SubPropertyNode->Operation == EOverriddenPropertyOperation::None || 
			 SubPropertyNode->Operation == EOverriddenPropertyOperation::Modified))
		{
			FOverriddenPropertyNodeID RemovedNodeID;
			if (ParentPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(Property, RemovedNodeID))
			{
				verifyf(OverriddenPropertyNodes.Remove(RemovedNodeID), TEXT("Expecting the node to be removed"));
			}
			if (ParentPropertyNode->Operation == EOverriddenPropertyOperation::Modified && ParentPropertyNode->SubPropertyNodeKeys.IsEmpty())
			{
				ParentPropertyNode->Operation = EOverriddenPropertyOperation::None;
			}
		}
	};

	// Because the PreEdit API doesn't provide Index info, we need to snapshot changed containers during pre-edit
	// so we can intuit which element was removed during PostEdit. This is a map-like structure that stores the latest snapshot for each contianer property
	static struct
	{
		uint8* Find(const FProperty* ContainerProperty) const
		{
			for (const TPair<const FProperty*, uint8*>& Element : Data)
			{
				if (Element.Key == ContainerProperty)
				{
					return Element.Value;
				}
			}
			return nullptr;
		}

		void Free(const FProperty* ContainerProperty)
		{
			for (int32 I = 0; I < Data.Num(); ++I)
			{
				if (Data[I].Key == ContainerProperty)
				{
					ContainerProperty->DestroyValue(Data[I].Value);
					FMemory::Free(Data[I].Value);
					Data.RemoveAtSwap(I);
					return;
				}
			}
			checkf(false, TEXT("Expecting a matching property to the allocated memory"));
		}

		uint8* FindOrAdd(const FProperty* ContainerProperty)
		{
			if (uint8* Found = Find(ContainerProperty))
			{
				return Found;
			}
			int32 I = Data.Add({ ContainerProperty, (uint8*)FMemory::Malloc(ContainerProperty->GetSize(), ContainerProperty->GetMinAlignment()) });
			ContainerProperty->InitializeValue(Data[I].Value);
			return Data[I].Value;
		}

		// There's not many elements so we're using an array of TPairs for cache friendliness. A TMap would work fine here as well.
		TArray<TPair<const FProperty*, uint8*>> Data;
	} SavedPreEditContainers;

	FPropertyVisitorPath::Iterator NextPropertyIterator = PropertyIterator+1;
	if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
	{
		// Only special case is instanced subobjects, otherwise we fallback to full array override
		if (FObjectPropertyBase* InnerObjectProperty = CastField<FObjectPropertyBase>(ArrayProperty->Inner))
		{
			FScriptArrayHelper ArrayHelper(ArrayProperty, SubValuePtr);
			int32 ArrayIndex = PropertyIterator->Index;
			checkf(ArrayIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index"));

			if (!NextPropertyIterator)
			{
				checkf(ArrayProperty->Inner, TEXT("Expecting an inner type for Arrays"));

				if (Notification == EPropertyNotificationType::PreEdit)
				{
					FScriptArrayHelper PreEditArrayHelper(ArrayProperty, SavedPreEditContainers.FindOrAdd(ArrayProperty));
					PreEditArrayHelper.EmptyAndAddValues(ArrayHelper.Num());
					for (int32 i = 0; i < ArrayHelper.Num(); i++)
					{
						InnerObjectProperty->SetObjectPropertyValue(PreEditArrayHelper.GetElementPtr(i), InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(i)));
					}
					return;
				}

				FScriptArrayHelper PreEditArrayHelper(ArrayProperty, SavedPreEditContainers.Find(ArrayProperty));

				auto ArrayReplace = [&]
				{
					if (SubPropertyNode)
					{
						// Overriding all entry in the array
						SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
					}

					// This is a case of the entire array is overridden
					// Need to loop through every sub object and setup them up as overridden
					for (int i = 0; i < ArrayHelper.Num(); ++i)
					{
						if (UObject* SubObject = TryGetInstancedSubObjectValue(InnerObjectProperty, ArrayHelper.GetElementPtr(i)))
						{
							if(SubPropertyNode)
							{
								const FOverriddenPropertyNodeID SubObjectID(SubObject);
								FOverriddenPropertyNode& SubObjectNode = FindOrAddNode(*SubPropertyNode, SubObjectID);
								SubObjectNode.Operation = EOverriddenPropertyOperation::Replace;
							}

							OverridableManager.OverrideInstancedSubObject(Owner, SubObject);
						}
					}
				};

				auto ArrayAddImpl = [&]()
				{
					checkf(ArrayHelper.IsValidIndex(ArrayIndex), TEXT("ArrayAdd change type expected to have an valid index"));
					if (UObject* AddedSubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
					{
						if(SubPropertyNode)
						{
							const FOverriddenPropertyNodeID  AddedSubObjectID(AddedSubObject);
							FOverriddenPropertyNode& AddedSubObjectNode = FindOrAddNode(*SubPropertyNode, AddedSubObjectID);
							AddedSubObjectNode.Operation = EOverriddenPropertyOperation::Add;

							// Notify the subobject that it was added
							if (FOverriddenPropertySet* AddedSubObjectOverriddenProperties = OverridableManager.GetOverriddenProperties(AddedSubObject))
							{
								AddedSubObjectOverriddenProperties->bWasAdded = true;
							}
						}
					}
				};

				auto ArrayRemoveImpl = [&]()
				{
					checkf(PreEditArrayHelper.IsValidIndex(ArrayIndex), TEXT("ArrayRemove change type expected to have an valid index"));
					if (UObject* RemovedSubObject = InnerObjectProperty->GetObjectPropertyValue(PreEditArrayHelper.GetElementPtr(ArrayIndex)))
					{
						if(SubPropertyNode)
						{
							// Check if there is a matching archetype for this object
							UObject* RemovedSubObjectArchetype = RemovedSubObject->GetArchetype();
							if (RemovedSubObjectArchetype && !RemovedSubObjectArchetype->HasAnyFlags(RF_ClassDefaultObject))
							{
								const FOverriddenPropertyNodeID RemovedSubObjectID (RemovedSubObjectArchetype);
								FOverriddenPropertyNode& RemovedSubObjectNode = FindOrAddNode(*SubPropertyNode, RemovedSubObjectID);
								if (RemovedSubObjectNode.Operation == EOverriddenPropertyOperation::Add)
								{
									// An add then a remove becomes no opt
									FOverriddenPropertyNodeID RemovedNodeID;
									if (SubPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(RemovedSubObjectID, RemovedNodeID))
									{
										verifyf(OverriddenPropertyNodes.Remove(RemovedNodeID), TEXT("Expecting the node to be removed"));
										bNeedsCleanup = true;
									}
								}
								else
								{
									RemovedSubObjectNode.Operation = EOverriddenPropertyOperation::Remove;
								}
							}
							else
							{
								// Figure out if it is a remove of a previously added element
								const FOverriddenPropertyNodeID RemovedSubObjectID (RemovedSubObject);
								if (const FOverriddenPropertyNodeID* AddedSubObjectID = SubPropertyNode->SubPropertyNodeKeys.Find(RemovedSubObjectID))
								{
									FOverriddenPropertyNode* AddedSubObjectNode = OverriddenPropertyNodes.Find(*AddedSubObjectID);
									checkf(AddedSubObjectNode, TEXT("Expecting a node"));
									if (AddedSubObjectNode->Operation != EOverriddenPropertyOperation::Add)
									{
										UE_LOG(LogOverridableObject, Warning, TEXT("This removed object:%s(0x%p) was not tracked as an add in the overridden properties"), *GetNameSafe(RemovedSubObject), RemovedSubObject);
									}

									// An add then a remove becomes no opt
									FOverriddenPropertyNodeID RemovedNodeID;
									if (SubPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(RemovedSubObjectID, RemovedNodeID))
									{
										verifyf(OverriddenPropertyNodes.Remove(RemovedNodeID), TEXT("Expecting the node to be removed"));
										bNeedsCleanup = true;
									}
								}
								else
								{
									UE_LOG(LogOverridableObject, Log, TEXT("This removed object:%s(0x%p) was not tracked in the overridden properties"), *GetNameSafe(RemovedSubObject), RemovedSubObject);
								}
							}
						}
					}
				};

				// Only arrays flagged overridable logic can record deltas, for now just override entire array
				if (!ArrayProperty->HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic))
				{
					if(ChangeType == EPropertyChangeType::Unspecified && ArrayIndex == INDEX_NONE)
					{
						// Overriding all entry in the array + override instanced sub objects
						ArrayReplace();
					}
					else if (SubPropertyNode)
					{
						// Overriding all entry in the array
						SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
					}
					return;
				}

				// Note: Currently, if CPF_ExperimentalOverridableLogic is set, we also require the property to be explicitly marked as an instanced subobject.
				checkf(InnerObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance), TEXT("Only instanced array properties support experimental overridable logic"));

				if (ChangeType & EPropertyChangeType::ValueSet)
				{
					checkf(ArrayIndex != INDEX_NONE, TEXT("ValueSet change type should have associated indexes"));
				}

				if (ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::Unspecified))
				{
					if (ArrayIndex != INDEX_NONE)
					{
						// Overriding a single entry in the array
						ArrayRemoveImpl();
						ArrayAddImpl();
					}
					else
					{
						ArrayReplace();
					}
					return;
				}

				if (ChangeType & EPropertyChangeType::ArrayAdd)
				{
					ArrayAddImpl();
					return;
				}

				if (ChangeType & EPropertyChangeType::ArrayRemove)
				{
					ArrayRemoveImpl();
					return;
				}
			
				if (ChangeType & EPropertyChangeType::ArrayClear)
				{
					checkf(ArrayIndex == INDEX_NONE, TEXT("ArrayClear change type should not have associated indexes"));

					for (int i = 0; i < PreEditArrayHelper.Num(); ++i)
					{
						ArrayIndex = i;
						ArrayRemoveImpl();
					}
					return;
				}
			
				if (ChangeType & EPropertyChangeType::ArrayMove)
				{
					UE_LOG(LogOverridableObject, Log, TEXT("ArrayMove change type is not going to change anything as ordering of object isn't supported yet"));
					return;
				}

				UE_LOG(LogOverridableObject, Verbose, TEXT("Property change type is not supported will default to full array override"));
			}
			// Can only forward to subobject if we have a valid index
			else if (ArrayHelper.IsValidIndex(ArrayIndex))
			{
				if (UObject* SubObject = InnerObjectProperty->GetObjectPropertyValue(ArrayHelper.GetElementPtr(ArrayIndex)))
				{
					// This should not be needed in the property grid, as it should already been called on the subobject itself.
					OverridableManager.NotifyPropertyChange(Notification, SubObject, NextPropertyIterator, ChangeType);
					return;
				}
			}
		}
	}
	// @todo support set in the overridable serialization
	//else if (const FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	//{
	//	
	//}
	else if (const FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		// Special handling of instanced subobjects
		checkf(MapProperty->KeyProp, TEXT("Expecting a key type for Maps"));
		FObjectPropertyBase* KeyObjectProperty = CastField<FObjectPropertyBase>(MapProperty->KeyProp);

		// SubObjects
		checkf(MapProperty->ValueProp, TEXT("Expecting a value type for Maps"));
		FObjectPropertyBase* ValueObjectProperty = CastField<FObjectPropertyBase>(MapProperty->ValueProp);

		FScriptMapHelper MapHelper(MapProperty, SubValuePtr);
		int32 LogicalMapIndex = PropertyIterator->Index;
		checkf(LogicalMapIndex == INDEX_NONE || PropertyIterator->PropertyInfo == EPropertyVisitorInfoType::ContainerIndex, TEXT("Expecting a container index type"));

		int32 InternalMapIndex = LogicalMapIndex != INDEX_NONE ? MapHelper.FindInternalIndex(LogicalMapIndex) : INDEX_NONE;
		if (!NextPropertyIterator)
		{
			if (Notification == EPropertyNotificationType::PreEdit)
			{
				FScriptMapHelper PreEditMapHelper(MapProperty, SavedPreEditContainers.FindOrAdd(MapProperty));
				PreEditMapHelper.EmptyValues();
				for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
				{
					PreEditMapHelper.AddPair(MapHelper.GetKeyPtr(It.GetInternalIndex()), MapHelper.GetValuePtr(It.GetInternalIndex()));
				}
				return;
			}

			uint8* SavedPreEditMap = SavedPreEditContainers.Find(MapProperty);
			checkf(SavedPreEditMap, TEXT("Expecting the same property as the pre edit flow"));
			FScriptMapHelper PreEditMapHelper(MapProperty, SavedPreEditMap);
			// The logical should map directly to the pre edit map internal index as we skipped all of the invalid entries
			int32 InternalPreEditMapIndex = LogicalMapIndex;

			ON_SCOPE_EXIT
			{
				SavedPreEditContainers.Free(MapProperty);
			};

			auto MapReplace = [&]()
			{
				// Overriding a all entries in the map
				if (SubPropertyNode)
				{
					SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
				}

				// This is a case of the entire array is overridden
				// Need to loop through every sub object and setup them up as overridden
				for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
				{
					if(SubPropertyNode)
					{
						FOverriddenPropertyNodeID OverriddenKeyID = FOverriddenPropertyNodeID::FromMapKey(MapProperty->KeyProp, MapHelper.GetKeyPtr(It.GetInternalIndex()));
						FOverriddenPropertyNode& OverriddenKeyNode = FindOrAddNode(*SubPropertyNode, OverriddenKeyID);
						OverriddenKeyNode.Operation = EOverriddenPropertyOperation::Replace;
					}

					// @todo support instanced object as a key in maps
					//if (UObject* KeySubObject = TryGetInstancedSubObjectValue(KeyObjectProperty, MapHelper.GetKeyPtr(It.GetInternalIndex())))
					//{
					//	checkf(false, TEXT("Keys as an instanced subobject is not supported yet"));
					//	OverridableManager.OverrideInstancedSubObject(Owner, KeySubObject);
					//}
					if (UObject* ValueSubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(It.GetInternalIndex())))
					{
						OverridableManager.OverrideInstancedSubObject(Owner, ValueSubObject);
					}
				}
			};

			auto MapAddImpl = [&]()
			{
				checkf(MapHelper.IsValidIndex(InternalMapIndex), TEXT("ArrayAdd change type expected to have an valid index"));

				if(SubPropertyNode)
				{
					FOverriddenPropertyNodeID AddedKeyID = FOverriddenPropertyNodeID::FromMapKey(MapProperty->KeyProp, MapHelper.GetKeyPtr(InternalMapIndex));
					FOverriddenPropertyNode& AddedKeyNode = FindOrAddNode(*SubPropertyNode, AddedKeyID);
					AddedKeyNode.Operation = EOverriddenPropertyOperation::Add;
				}
			};

			auto MapRemoveImpl = [&]()
			{
				checkf(PreEditMapHelper.IsValidIndex(InternalPreEditMapIndex), TEXT("ArrayRemove change type expected to have an valid index"));

				if(SubPropertyNode)
				{
					FOverriddenPropertyNodeID RemovedKeyID = FOverriddenPropertyNodeID::FromMapKey(MapProperty->KeyProp, PreEditMapHelper.GetKeyPtr(InternalPreEditMapIndex));
					FOverriddenPropertyNode& RemovedKeyNode = FindOrAddNode(*SubPropertyNode, RemovedKeyID);
					if (RemovedKeyNode.Operation == EOverriddenPropertyOperation::Add)
					{
						// @Todo support remove/add/remove
						FOverriddenPropertyNodeID RemovedNodeID;
						if (SubPropertyNode->SubPropertyNodeKeys.RemoveAndCopyValue(RemovedKeyID, RemovedNodeID))
						{
							verifyf(OverriddenPropertyNodes.Remove(RemovedNodeID), TEXT("Expecting the node to be removed"));
							bNeedsCleanup = true;
						}
					}
					else
					{
						RemovedKeyNode.Operation = EOverriddenPropertyOperation::Remove;
					}
				}
			};

			// Only maps flagged overridable logic can be handled here
			if (!MapProperty->HasAnyPropertyFlags(CPF_ExperimentalOverridableLogic))
			{
				if (ChangeType == EPropertyChangeType::Unspecified && InternalMapIndex == INDEX_NONE)
				{
					// Overriding all entry in the array + override instanced sub obejects
					MapReplace();
				}
				else if(SubPropertyNode)
				{
					// Overriding all entry in the array
					SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
				}
				return;
			}

			// Ensure that an object key type is not explicitly marked as an instanced subobject. This is not supported yet.
			checkf(!KeyObjectProperty || !KeyObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance), TEXT("Keys as an instanced subobject is not supported yet"));
			// Note: Currently, if CPF_ExperimentalOverridableLogic is set on the map, we require its value type to be explicitly marked as an instanced subobject.
			checkf(!ValueObjectProperty || ValueObjectProperty->HasAllPropertyFlags(CPF_PersistentInstance), TEXT("Values must be instanced to support map overrides"));

			if (ChangeType & EPropertyChangeType::ValueSet)
			{
				checkf(LogicalMapIndex != INDEX_NONE, TEXT("ValueSet change type should have associated indexes"));
			}

			if (ChangeType & (EPropertyChangeType::ValueSet | EPropertyChangeType::Unspecified))
			{
				if(LogicalMapIndex != INDEX_NONE)
				{
					// Overriding a single entry in the map
					MapRemoveImpl();
					MapAddImpl();
				}
				else
				{
					MapReplace();
				}
				return;
			}

			if (ChangeType & EPropertyChangeType::ArrayAdd)
			{
				MapAddImpl();
				return;
			}

			if (ChangeType & EPropertyChangeType::ArrayRemove)
			{
				MapRemoveImpl();
				return;
			}
			
			if (ChangeType & EPropertyChangeType::ArrayClear)
			{
				checkf(InternalPreEditMapIndex == INDEX_NONE, TEXT("ArrayClear change type should not have associated indexes"));

				for (FScriptMapHelper::FIterator It(PreEditMapHelper); It; ++It)
				{
					InternalPreEditMapIndex = It.GetInternalIndex();
					MapRemoveImpl();
				}
				return;
			}
			
			if (ChangeType & EPropertyChangeType::ArrayMove)
			{
				UE_LOG(LogOverridableObject, Log, TEXT("ArrayMove change type is not going to change anything as ordering of object isn't supported yet"));
				return;
			}

			if (ChangeType & EPropertyChangeType::ArrayAdd)
			{
				MapAddImpl();
				return;
			}

			UE_LOG(LogOverridableObject, Verbose, TEXT("Property change type is not supported will default to full array override"));;
		}
		// Can only forward to subobject if we have a valid index
		else if (MapHelper.IsValidIndex(InternalMapIndex))
		{
			// @todo support instanced object as a key in maps
			//if (UObject* SubObject = TryGetInstancedSubObjectValue(KeyObjectProperty, MapHelper.GetValuePtr(InternalMapIndex)))
			//{
			//	checkf(false, TEXT("Keys as an instanced subobject is not supported yet"));
			//	// This should not be needed in the property grid, as it should already been called on the subobject.
			//	OverridableManager.NotifyPropertyChange(Notification, *SubObject, NextPropertyIterator, ChangeType);
			//	return;
			//}

			if (UObject* SubObject = TryGetInstancedSubObjectValue(ValueObjectProperty, MapHelper.GetValuePtr(InternalMapIndex)))
			{
				// This should not be needed in the property grid, as it should already been called on the subobject.
				OverridableManager.NotifyPropertyChange(Notification, SubObject, NextPropertyIterator, ChangeType);
				return;
			}
		}
	}
	else if (Property->IsA<FStructProperty>())
	{
		if (!NextPropertyIterator)
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else
		{
			NotifyPropertyChange(SubPropertyNode, Notification, NextPropertyIterator, ChangeType, SubValuePtr, bNeedsCleanup);
		}
		return;
	}
	else if (const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
	{
		if (!NextPropertyIterator)
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else if (UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValuePtr))
		{
			// This should not be needed in the property grid, as it should already been called on the subobject.
			OverridableManager.NotifyPropertyChange(Notification, SubObject, NextPropertyIterator, ChangeType);
		}
		return;
	}
	else if (const FOptionalProperty* OptionalProperty = CastField<FOptionalProperty>(Property))
	{
		if (!NextPropertyIterator)
		{
			if (Notification == EPropertyNotificationType::PostEdit && SubPropertyNode)
			{
				SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
			}
		}
		else if (OptionalProperty->IsSet(Data))
		{
			NotifyPropertyChange(SubPropertyNode, Notification, NextPropertyIterator, ChangeType, OptionalProperty->GetValuePointerForRead(SubValuePtr), bNeedsCleanup);
		}
		return;
	}

	UE_CLOG(NextPropertyIterator, LogOverridableObject, Verbose, TEXT("Unsupported property type(%s), fallback to overriding entire property"), *Property->GetName());
	if (Notification == EPropertyNotificationType::PostEdit)
	{
		if (SubPropertyNode)
		{
			// Replacing this entire property
			SubPropertyNode->Operation = EOverriddenPropertyOperation::Replace;
		}
	}
}

void FOverriddenPropertySet::RemoveOverriddenSubProperties(FOverriddenPropertyNode& PropertyNode)
{
	for (const auto& Pair : PropertyNode.SubPropertyNodeKeys)
	{
		FOverriddenPropertyNode* RemovedPropertyNode = OverriddenPropertyNodes.Find(Pair.Value);
		checkf(RemovedPropertyNode, TEXT("Expecting a node"));
		RemoveOverriddenSubProperties(*RemovedPropertyNode);
		verifyf(OverriddenPropertyNodes.Remove(Pair.Value), TEXT("Expecting the node to be removed"));
	}
	PropertyNode.Operation = EOverriddenPropertyOperation::None;
	PropertyNode.SubPropertyNodeKeys.Empty();
}

UObject* FOverriddenPropertySet::TryGetInstancedSubObjectValue(const FObjectPropertyBase* FromProperty, void* ValuePtr) const
{
	// Property can be NULL - in that case there is no value.
	if (!FromProperty)
	{
		return nullptr;
	}

	// subobject pointers in IDOs point to the instance subobjects. For this purpose we need to redirect them to IDO subobjects
	UObject* SubObject = FromProperty->GetObjectPropertyValue(ValuePtr);
	const UObject* ExpectedOuter = Owner;
	TFunction<UObject* (UObject*)> RedirectMethod = [](UObject* Obj) {return Obj; };
#if WITH_EDITORONLY_DATA
	if (const UObject* Instance = UE::FPropertyBagRepository::Get().FindInstanceForDataObject(Owner))
	{
		RedirectMethod = UE::ResolveInstanceDataObject;
		ExpectedOuter = Instance;
	}
#endif

	if (FromProperty->HasAnyPropertyFlags(CPF_PersistentInstance)
		|| (FromProperty->IsA<FObjectProperty>() && SubObject && SubObject->IsIn(ExpectedOuter)))
	{
		return RedirectMethod(SubObject);
	}

	return nullptr;
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(FPropertyVisitorPath::Iterator PropertyIterator, bool* bOutInheritedOperation /*= nullptr*/) const
{
	return GetOverriddenPropertyOperation(OverriddenPropertyNodes.Find(RootNodeID), PropertyIterator, bOutInheritedOperation, Owner);
}

bool FOverriddenPropertySet::ClearOverriddenProperty(FPropertyVisitorPath::Iterator PropertyIterator)
{
	if (FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.Find(RootNodeID))
	{
		return ClearOverriddenProperty(*RootNode, PropertyIterator, Owner);
	}
	return true;
}

void FOverriddenPropertySet::OverrideProperty(FPropertyVisitorPath::Iterator PropertyIterator, const void* Data)
{
	FOverriddenPropertyNode& RootPropertyNode = OverriddenPropertyNodes.FindOrAdd(RootNodeID);
	bool bNeedsCleanup = false;
	NotifyPropertyChange(&RootPropertyNode, EPropertyNotificationType::PreEdit, PropertyIterator, EPropertyChangeType::Unspecified, Data, bNeedsCleanup);
	NotifyPropertyChange(&RootPropertyNode, EPropertyNotificationType::PostEdit, PropertyIterator, EPropertyChangeType::Unspecified, Data, bNeedsCleanup);
}

void FOverriddenPropertySet::NotifyPropertyChange(const EPropertyNotificationType Notification, FPropertyVisitorPath::Iterator PropertyIterator, const EPropertyChangeType::Type ChangeType, const void* Data)
{
	bool bNeedsCleanup = false;
	NotifyPropertyChange(&OverriddenPropertyNodes.FindOrAdd(RootNodeID), Notification, PropertyIterator, ChangeType, Data, bNeedsCleanup);
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const
{
	return GetOverriddenPropertyOperation(OverriddenPropertyNodes.Find(RootNodeID), CurrentPropertyChain, Property);
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property)
{
	return SetOverriddenPropertyOperation(Operation, OverriddenPropertyNodes.FindOrAdd(RootNodeID), CurrentPropertyChain, Property);
}

FOverriddenPropertyNode* FOverriddenPropertySet::RestoreOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property)
{
	// 'None', 'Modified' and 'SubObjectShadowing' operations are not needed to be restored on a property,
	// because 'None' is equal to the node not existing and
	// 'Modified' will be restored when the sub property overrides will be restored successfully
	if (Operation != EOverriddenPropertyOperation::None && 
		Operation != EOverriddenPropertyOperation::Modified &&
		Operation != EOverriddenPropertyOperation::SubObjectsShadowing)
	{
		// Prevent marking as replaced the properties that are always overridden
		if (!Property || Operation != EOverriddenPropertyOperation::Replace || !Property->HasAnyPropertyFlags(CPF_ExperimentalAlwaysOverriden))
		{
			return SetOverriddenPropertyOperation(Operation, CurrentPropertyChain, Property);
		}
	}

	return nullptr;
}

const FOverriddenPropertyNode* FOverriddenPropertySet::GetOverriddenPropertyNode(const FArchiveSerializedPropertyChain* CurrentPropertyChain) const
{
	if (const FOverriddenPropertyNode* RootNode = OverriddenPropertyNodes.Find(RootNodeID))
	{
		return GetOverriddenPropertyNode(*RootNode, CurrentPropertyChain);
	}
	return nullptr;
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetOverriddenPropertyOperation(const FOverriddenPropertyNode* ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property) const
{
	// No need to look further
	// if it is the entire property is replaced or
	// if it is the FOverriddenPropertySet struct which is always Overridden
	if (ParentPropertyNode && ParentPropertyNode->Operation == EOverriddenPropertyOperation::Replace)
	{
		return EOverriddenPropertyOperation::Replace;
	}

	// @Todo optimize find a way to not have to copy the property chain here.
	FArchiveSerializedPropertyChain PropertyChain(CurrentPropertyChain ? *CurrentPropertyChain : FArchiveSerializedPropertyChain());
	if(Property)
	{
		PropertyChain.PushProperty(Property, Property->IsEditorOnlyProperty());
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = PropertyChain.GetRootIterator();
	const FOverriddenPropertyNode* OverriddenPropertyNode = ParentPropertyNode;
	while (PropertyIterator && (!OverriddenPropertyNode || (OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace)))
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		if (OverriddenPropertyNode)
		{
			if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentProperty))
			{
				OverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
				checkf(OverriddenPropertyNode, TEXT("Expecting a node"));
			}
			else
			{
				OverriddenPropertyNode = nullptr;
			}
		}
		// While digging down the path, if there is one property that is always overridden
		// stop there and return replace
		if (CurrentProperty->HasAnyPropertyFlags(CPF_ExperimentalAlwaysOverriden))
		{
			return EOverriddenPropertyOperation::Replace;
		}
		++PropertyIterator;
	}

	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetOverriddenPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain, FProperty* Property)
{
	// No need to look further
	// if it is the entire property is replaced or
	// if it is the FOverriddenPropertySet struct which is always Overridden
	if (ParentPropertyNode.Operation == EOverriddenPropertyOperation::Replace)
	{
		return nullptr;
	}

	// @Todo optimize find a way to not have to copy the property chain here.
	FArchiveSerializedPropertyChain PropertyChain(CurrentPropertyChain ? *CurrentPropertyChain : FArchiveSerializedPropertyChain());
	if (Property)
	{
		PropertyChain.PushProperty(Property, Property->IsEditorOnlyProperty());
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = PropertyChain.GetRootIterator();
	FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	while (PropertyIterator && OverriddenPropertyNode->Operation != EOverriddenPropertyOperation::Replace)
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		// While digging down the path, if the operation is replace and one of the property is always overridden
		// then there isn't anything to do
		if (Operation == EOverriddenPropertyOperation::Replace && CurrentProperty->HasAnyPropertyFlags(CPF_ExperimentalAlwaysOverriden))
		{
			return nullptr;
		}
		OverriddenPropertyNode = &FindOrAddNode(*OverriddenPropertyNode, CurrentProperty);
		++PropertyIterator;
	}

	// Might have stop before as one of the parent property was completely replaced.
	if (!PropertyIterator)
	{
		OverriddenPropertyNode->Operation = Operation;
		return OverriddenPropertyNode;
	}

	return nullptr;
}

EOverriddenPropertyOperation FOverriddenPropertySet::GetSubPropertyOperation(FOverriddenPropertyNodeID NodeID) const
{
	const FOverriddenPropertyNode* OverriddenPropertyNode = OverriddenPropertyNodes.Find(NodeID);
	return OverriddenPropertyNode ? OverriddenPropertyNode->Operation : EOverriddenPropertyOperation::None;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetSubPropertyOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& Node, FOverriddenPropertyNodeID NodeID)
{
	FOverriddenPropertyNode& OverriddenPropertyNode = FindOrAddNode(Node, NodeID);
	OverriddenPropertyNode.Operation = Operation;
	return &OverriddenPropertyNode;
}

FOverriddenPropertyNode* FOverriddenPropertySet::SetSubObjectOperation(EOverriddenPropertyOperation Operation, FOverriddenPropertyNode& Node, TNotNull<UObject*> SubObject)
{
	const FOverriddenPropertyNodeID SubObjectID(SubObject);
	FOverriddenPropertyNode* SubObjectNode = SetSubPropertyOperation(Operation, Node, SubObjectID);

	if (Operation == EOverriddenPropertyOperation::Add)
	{
		// Notify the subobject that it was added
		if (FOverriddenPropertySet* AddedSubObjectOverriddenProperties = FOverridableManager::Get().GetOverriddenProperties(SubObject))
		{
			AddedSubObjectOverriddenProperties->bWasAdded = true;
		}
	}

	return SubObjectNode;
}

bool FOverriddenPropertySet::IsCDOOwningProperty(const FProperty& Property) const
{
	checkf(Owner, TEXT("Expecting a valid overridable owner"));
	if (!Owner->HasAnyFlags(RF_ClassDefaultObject))
	{
		return false;
	}

	// We need to serialize only if the property owner is the current CDO class
	// Otherwise on derived class, this is done in parent CDO or it should be explicitly overridden if it is different than the parent value
	// This is sort of like saying it overrides the default property initialization value.
	return Property.GetOwnerClass() == Owner->GetClass();
}

void FOverriddenPropertySet::Reset()
{
	OverriddenPropertyNodes.Reset();
}

void FOverriddenPropertySet::HandleObjectsReInstantiated(const TMap<UObject*, UObject*>& Map)
{
#if WITH_EDITOR
	// When there is a cached archetype, it is an indicator this object is about to be replaced
	// So no need to replace any ptr, otherwise we might not be able to reconstitute the right information
	if (FEditorCacheArchetypeManager::Get().GetCachedArchetype(Owner))
	{
		return;
	}
#endif // WITH_EDITOR

	for (FOverriddenPropertyNode& Node : OverriddenPropertyNodes)
	{
		Node.NodeID.HandleObjectsReInstantiated(Map);
		for (auto& Pair : Node.SubPropertyNodeKeys)
		{
			Pair.Key.HandleObjectsReInstantiated(Map);
			Pair.Value.HandleObjectsReInstantiated(Map);
		}
	}
}

void FOverriddenPropertySet::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (FOverriddenPropertyNode& Node : OverriddenPropertyNodes)
	{
		Node.NodeID.AddReferencedObjects(Collector, Owner);
		for (auto& Pair : Node.SubPropertyNodeKeys)
		{
			Pair.Key.AddReferencedObjects(Collector, Owner);
			Pair.Value.AddReferencedObjects(Collector, Owner);
		}
	}
}

void FOverriddenPropertySet::HandleDeadObjectReferences(const TSet<UObject*>& ActiveInstances, const TSet<UObject*>& TemplateInstances)
{
	for (FOverriddenPropertyNode& Node : OverriddenPropertyNodes)
	{
		Node.NodeID.HandleDeadObjectReferences(ActiveInstances, TemplateInstances);
		for (auto& Pair : Node.SubPropertyNodeKeys)
		{
			Pair.Key.HandleDeadObjectReferences(ActiveInstances, TemplateInstances);
			Pair.Value.HandleDeadObjectReferences(ActiveInstances, TemplateInstances);
		}
	}
}

const FOverriddenPropertyNode* FOverriddenPropertySet::GetOverriddenPropertyNode(const FOverriddenPropertyNode& ParentPropertyNode, const FArchiveSerializedPropertyChain* CurrentPropertyChain) const
{
	if (!CurrentPropertyChain)
	{
		return &ParentPropertyNode;
	}

	TArray<class FProperty*, TInlineAllocator<8>>::TConstIterator PropertyIterator = CurrentPropertyChain->GetRootIterator();
	const FOverriddenPropertyNode* OverriddenPropertyNode = &ParentPropertyNode;
	while (PropertyIterator && OverriddenPropertyNode)
	{
		const FProperty* CurrentProperty = (*PropertyIterator);
		if (const FOverriddenPropertyNodeID* CurrentPropKey = OverriddenPropertyNode->SubPropertyNodeKeys.Find(CurrentProperty))
		{
			OverriddenPropertyNode = OverriddenPropertyNodes.Find(*CurrentPropKey);
			checkf(OverriddenPropertyNode, TEXT("Expecting a node"));
		}
		else
		{
			OverriddenPropertyNode = nullptr;
			break;
		}
		++PropertyIterator;
	}

	return OverriddenPropertyNode;
}
