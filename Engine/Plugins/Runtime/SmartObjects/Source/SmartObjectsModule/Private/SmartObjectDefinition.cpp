// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectDefinition.h"
#include "SmartObjectSettings.h"
#include "Engine/World.h"
#include "Misc/EnumerateRange.h"

#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#include "WorldConditions/WorldCondition_SmartObjectActorTagQuery.h"
#include "WorldConditions/SmartObjectWorldConditionObjectTagQuery.h"
#include "SmartObjectUserComponent.h"
#include "Engine/SCS_Node.h"
#include "Misc/Crc.h"
#include "Misc/DataValidation.h"
#include "SmartObjectPropertyHelpers.h"
#include "Interfaces/ITargetPlatform.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "AssetRegistry/AssetData.h"
#endif // WITH_EDITOR

#include "Logging/TokenizedMessage.h"
#include "PropertyBindingBindableStructDescriptor.h"
#include "PropertyBindingDataView.h"
#include "PropertyBindingTypes.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/Package.h"
#include "VisualLogger/VisualLogger.h"
#include "Logging/TokenizedMessage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectDefinition)

#define LOCTEXT_NAMESPACE "SmartObjectDefinition"

namespace UE::SmartObject
{
	const FVector DefaultSlotSize(40, 40, 90);

	namespace Delegates
	{
#if WITH_EDITOR
		FOnParametersChanged OnParametersChanged;
		FOnSavingDefinition OnSavingDefinition;
		FOnGetAssetRegistryTags OnGetAssetRegistryTags;
		FOnSlotDefinitionCreated OnSlotDefinitionCreated;
#endif // WITH_EDITOR
	} // Delegates

} // UE::SmartObject


const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Invalid(INDEX_NONE);
const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Root(RootIndex);
const FSmartObjectDefinitionDataHandle FSmartObjectDefinitionDataHandle::Parameters(ParametersIndex);


USmartObjectDefinition::USmartObjectDefinition(const FObjectInitializer& ObjectInitializer): UDataAsset(ObjectInitializer)
{
	UserTagsFilteringPolicy = GetDefault<USmartObjectSettings>()->DefaultUserTagsFilteringPolicy;
	ActivityTagsMergingPolicy = GetDefault<USmartObjectSettings>()->DefaultActivityTagsMergingPolicy;
	WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;

	BindingCollection.SetBindingsOwner(this);
}

void USmartObjectDefinition::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	BindingCollection.SetBindingsOwner(this);
	Super::PostDuplicate(DuplicateMode);
}

#if WITH_EDITOR
EDataValidationResult USmartObjectDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult Result = Super::IsDataValid(Context);

	TArray<TPair<EMessageSeverity::Type, FText>> ValidationMessages;
	Validate(&ValidationMessages);

	bool bAtLeastOneError = false;
	for (const TPair<EMessageSeverity::Type, FText>& MessagePair : ValidationMessages)
	{
		Context.AddMessage(this, MessagePair.Key, MessagePair.Value);
		bAtLeastOneError |= MessagePair.Key == EMessageSeverity::Error;
	}
	
	return CombineDataValidationResults(Result, bAtLeastOneError ? EDataValidationResult::Invalid : EDataValidationResult::Valid);
}

TSubclassOf<USmartObjectSlotValidationFilter> USmartObjectDefinition::GetPreviewValidationFilterClass() const
{
	if (PreviewData.UserActorClass.IsValid())
	{
		if (const UClass* UserActorClass = PreviewData.UserActorClass.Get())
		{
			// Try to get smart object user component added in the BP.
			if (const UBlueprintGeneratedClass* UserBlueprintClass = Cast<UBlueprintGeneratedClass>(UserActorClass))
			{
				const TArray<USCS_Node*>& Nodes = UserBlueprintClass->SimpleConstructionScript->GetAllNodes();
				for (USCS_Node* Node : Nodes)
				{
					UActorComponent* Component = Node->GetActualComponentTemplate(const_cast<UBlueprintGeneratedClass*>(UserBlueprintClass));
					if (const USmartObjectUserComponent* UserComponent = Cast<USmartObjectUserComponent>(Component))
					{
						return UserComponent->GetValidationFilter();
					}
				}
			}
			
			// Try to get the component from the CDO (e.g. added as default object in C++).
			if (const AActor* UserActor = Cast<AActor>(UserActorClass->GetDefaultObject()))
			{
				if (const USmartObjectUserComponent* UserComponent = UserActor->GetComponentByClass<USmartObjectUserComponent>())
				{
					return UserComponent->GetValidationFilter();
				}
			}
		}
		return nullptr;
	}

	if (PreviewData.UserValidationFilterClass.IsValid())
	{
		return PreviewData.UserValidationFilterClass.Get();
	}
	
	return nullptr;
}

#endif // WITH_EDITOR

bool USmartObjectDefinition::Validate(TArray<FText>* ErrorsToReport) const
{
	if (ErrorsToReport)
	{
		TArray<TPair<EMessageSeverity::Type, FText>> MessagesToReport;
		const bool bResult = Validate(&MessagesToReport);
		Algo::Transform(MessagesToReport, *ErrorsToReport, [](const TPair<EMessageSeverity::Type, FText>& Entry)
			{
				return Entry.Value;
			});
		return bResult;
	}

	return Validate();
}

bool USmartObjectDefinition::Validate(TArray<TPair<EMessageSeverity::Type, FText>>* ErrorsToReport) const
{
	bValid = false;

#if WITH_EDITOR
	// Detect unbound parameters
	const UPropertyBag* ParametersScriptStruct = Parameters.GetPropertyBagStruct();
	if (ErrorsToReport && ParametersScriptStruct != nullptr)
	{
		TArray<FName> BoundParameters;
		const FPropertyBindingBindableStructDescriptor* Descriptor = BindingCollection.GetBindableStructDescriptorFromHandle(FConstStructView::Make(FSmartObjectDefinitionDataHandle::Parameters));
		BindingCollection.ForEachBinding([&BoundParameters, StructID = Descriptor->ID](const FPropertyBindingBinding& Binding)
			{
				if (!Binding.GetSourcePath().IsPathEmpty()
					&& Binding.GetSourcePath().GetStructID() == StructID)
				{
					// We only need at least one binding in the first segment to consider the parameters bound
					BoundParameters.AddUnique(Binding.GetSourcePath().GetSegments()[0].GetName());
				}
			});

		TArray<FText> UnboundParameterNames;
		for (const FPropertyBagPropertyDesc& PropertyDesc : ParametersScriptStruct->GetPropertyDescs())
		{
			if (BoundParameters.Find(PropertyDesc.Name) == INDEX_NONE)
			{
				UnboundParameterNames.Add(FText::FromName(PropertyDesc.Name));
			}
		}

		if (!UnboundParameterNames.IsEmpty())
		{
			ErrorsToReport->Emplace(EMessageSeverity::Warning
				, FText::Format(LOCTEXT("UnboundParametersWarning", "The following parameters are not bound and could be removed: {0}")
					, FText::Join(LOCTEXT("Separator", ", "), UnboundParameterNames)));
		}
	}
#endif // WITH_EDITOR

	// Detect null entries in default definitions
	int32 NullEntryIndex;
	if (DefaultBehaviorDefinitions.Find(nullptr, NullEntryIndex))
	{
		if (ErrorsToReport)
		{
			ErrorsToReport->Emplace(EMessageSeverity::Error
				, FText::Format(LOCTEXT("NullDefaultBehaviorEntryError", "Null entry found at index {0} in default behavior definition list"), NullEntryIndex));
		}
		else
		{
			return false;
		}
	}

	// Detect null entries in slot definitions
	for (int i = 0; i < Slots.Num(); ++i)
	{
		const FSmartObjectSlotDefinition& Slot = Slots[i];
		if (Slot.BehaviorDefinitions.Find(nullptr, NullEntryIndex))
		{
			if (ErrorsToReport)
			{
				ErrorsToReport->Emplace(EMessageSeverity::Error
					, FText::Format(LOCTEXT("NullSlotBehaviorEntryError", "Null entry found at index {0} in default behavior definition list"), NullEntryIndex));
			}
			else
			{
				return false;
			}
		}
	}

	// Detect missing definitions in slots if no default one are provided
	if (DefaultBehaviorDefinitions.Num() == 0)
	{
		for (int i = 0; i < Slots.Num(); ++i)
		{
			const FSmartObjectSlotDefinition& Slot = Slots[i];
			if (Slot.BehaviorDefinitions.Num() == 0)
			{
				if (ErrorsToReport)
				{
					ErrorsToReport->Emplace(EMessageSeverity::Error
						, FText::Format(LOCTEXT("MissingSlotBehaviorError", "Slot at index {0} needs to provide a behavior definition since there is no default one in the SmartObject definition"), i));
				}
				else
				{
					return false;
				}
			}
		}
	}

	bValid = ErrorsToReport == nullptr || ErrorsToReport->IsEmpty();
	return bValid.GetValue();
}

FBox USmartObjectDefinition::GetBounds() const
{
	FBox BoundingBox(ForceInitToZero);
	for (const FSmartObjectSlotDefinition& Slot : GetSlots())
	{
		BoundingBox += FVector(Slot.Offset) + UE::SmartObject::DefaultSlotSize;
		BoundingBox += FVector(Slot.Offset) - UE::SmartObject::DefaultSlotSize;
	}
	return BoundingBox;
}

void USmartObjectDefinition::GetSlotActivityTags(const int32 SlotIndex, FGameplayTagContainer& OutActivityTags) const
{
	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting activity tags for an out of range slot index: %s"), *LexToString(SlotIndex)))
	{
		GetSlotActivityTags(Slots[SlotIndex], OutActivityTags);
	}
}

void USmartObjectDefinition::GetSlotActivityTags(const FSmartObjectSlotDefinition& SlotDefinition, FGameplayTagContainer& OutActivityTags) const
{
	OutActivityTags = ActivityTags;

	if (ActivityTagsMergingPolicy == ESmartObjectTagMergingPolicy::Combine)
	{
		OutActivityTags.AppendTags(SlotDefinition.ActivityTags);
	}
	else if (ActivityTagsMergingPolicy == ESmartObjectTagMergingPolicy::Override && !SlotDefinition.ActivityTags.IsEmpty())
	{
		OutActivityTags = SlotDefinition.ActivityTags;
	}
}

FTransform USmartObjectDefinition::GetSlotWorldTransform(const int32 SlotIndex, const FTransform& OwnerTransform) const
{
	if (ensureMsgf(Slots.IsValidIndex(SlotIndex), TEXT("Requesting slot transform for an out of range index: %s"), *LexToString(SlotIndex)))
	{
		const FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];
		return FTransform(FRotator(Slot.Rotation), FVector(Slot.Offset)) * OwnerTransform;
	}
	return OwnerTransform;
}

const USmartObjectBehaviorDefinition* USmartObjectDefinition::GetBehaviorDefinition(const int32 SlotIndex,
																					const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass) const
{
	const USmartObjectBehaviorDefinition* Definition = nullptr;
	if (Slots.IsValidIndex(SlotIndex))
	{
		Definition = GetBehaviorDefinitionByType(Slots[SlotIndex].BehaviorDefinitions, DefinitionClass);
	}

	if (Definition == nullptr)
	{
		Definition = GetBehaviorDefinitionByType(DefaultBehaviorDefinitions, DefinitionClass);
	}

	return Definition;
}


const USmartObjectBehaviorDefinition* USmartObjectDefinition::GetBehaviorDefinitionByType(const TArray<USmartObjectBehaviorDefinition*>& BehaviorDefinitions,
																				 const TSubclassOf<USmartObjectBehaviorDefinition>& DefinitionClass)
{
	USmartObjectBehaviorDefinition* const* BehaviorDefinition = BehaviorDefinitions.FindByPredicate([&DefinitionClass](const USmartObjectBehaviorDefinition* SlotBehaviorDefinition)
		{
			return SlotBehaviorDefinition != nullptr && SlotBehaviorDefinition->GetClass()->IsChildOf(*DefinitionClass);
		});

	return BehaviorDefinition != nullptr ? *BehaviorDefinition : nullptr;
}

#if WITH_EDITOR
int32 USmartObjectDefinition::FindSlotByID(const FGuid ID) const
{
	const int32 Slot = Slots.IndexOfByPredicate([&ID](const FSmartObjectSlotDefinition& Slot) { return Slot.ID == ID; });
	return Slot;
}

bool USmartObjectDefinition::FindSlotAndDefinitionDataIndexByID(const FGuid ID, int32& OutSlotIndex, int32& OutDefinitionDataIndex) const
{
	OutSlotIndex = INDEX_NONE;
	OutDefinitionDataIndex = INDEX_NONE;
	
	// First try to find direct match on a slot.
	for (TConstEnumerateRef<const FSmartObjectSlotDefinition> SlotDefinition : EnumerateRange(Slots))
	{
		if (SlotDefinition->ID == ID)
		{
			OutSlotIndex = SlotDefinition.GetIndex();
			return true;
		}

		// Next try to find slot index based on definition data.
		const int32 DefinitionDataIndex = SlotDefinition->DefinitionData.IndexOfByPredicate([&ID](const FSmartObjectDefinitionDataProxy& DataProxy)
		{
			return DataProxy.ID == ID;
		});
		if (DefinitionDataIndex != INDEX_NONE)
		{
			OutSlotIndex = SlotDefinition.GetIndex();
			OutDefinitionDataIndex = DefinitionDataIndex;
			return true;
		}
	}

	return false;
}

void USmartObjectDefinition::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	(void)UE::SmartObject::Delegates::OnGetAssetRegistryTags.ExecuteIfBound(*this, Context);
}

void USmartObjectDefinition::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	const FSmartObjectEditPropertyPath ChangePropertyPath(PropertyChangedEvent);

	static const FSmartObjectEditPropertyPath ParametersPath(USmartObjectDefinition::StaticClass(), TEXT("Parameters"));
	static const FSmartObjectEditPropertyPath SlotsPath(USmartObjectDefinition::StaticClass(), TEXT("Slots"));
	static const FSmartObjectEditPropertyPath WorldConditionSchemaClassPath(USmartObjectDefinition::StaticClass(), TEXT("WorldConditionSchemaClass"));
	static const FSmartObjectEditPropertyPath SlotsDefinitionDataPath(USmartObjectDefinition::StaticClass(), TEXT("Slots.DefinitionData"));

	// Ensure unique Slot ID on added or duplicated items.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd
		|| PropertyChangedEvent.ChangeType == EPropertyChangeType::Duplicate)
	{
		if (ChangePropertyPath.IsPathExact(SlotsPath))
		{
			const int32 SlotIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsPath);
			if (Slots.IsValidIndex(SlotIndex))
			{
				FSmartObjectSlotDefinition& SlotDefinition = Slots[SlotIndex];
				SlotDefinition.ID = FGuid::NewGuid();
				SlotDefinition.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
				
				// Set new IDs to all duplicated data too
				for (FSmartObjectDefinitionDataProxy& DataProxy : SlotDefinition.DefinitionData)
				{
					DataProxy.ID = FGuid::NewGuid();
				}

				// Call delegate only when a new definition is created (not called when duplicating an existing one)
				if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ArrayAdd)
				{
					(void)UE::SmartObject::Delegates::OnSlotDefinitionCreated.ExecuteIfBound(*this, Slots[SlotIndex]);
				}
			}
		}

		if (ChangePropertyPath.IsPathExact(SlotsDefinitionDataPath))
		{
			const int32 SlotIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsPath);
			if (Slots.IsValidIndex(SlotIndex))
			{
				FSmartObjectSlotDefinition& SlotDefinition = Slots[SlotIndex];
				const int32 DataIndex = ChangePropertyPath.GetPropertyArrayIndex(SlotsDefinitionDataPath);
				if (SlotDefinition.DefinitionData.IsValidIndex(DataIndex))
				{
					FSmartObjectDefinitionDataProxy& DataProxy = SlotDefinition.DefinitionData[DataIndex];
					DataProxy.ID = FGuid::NewGuid();
				}
			}
		}
	}

	// There are many changes that might require path to be invalidated and segments out of date so
	// always update them.
	UpdateBindingPaths();
	bool bParametersUpdateRequired = false;

	// Anything in the parameters change, notify.
	if (ChangePropertyPath.ContainsPath(ParametersPath))
	{
		bParametersUpdateRequired = true;
		UE::SmartObject::Delegates::OnParametersChanged.Broadcast(*this);
	}

	// Anything in the slots changed, update references.
	if (ChangePropertyPath.ContainsPath(SlotsPath))
	{
		UpdateSlotReferences();
	}

	// If schema changes, update preconditions too.
	if (ChangePropertyPath.IsPathExact(WorldConditionSchemaClassPath))
	{
		for (FSmartObjectSlotDefinition& Slot : Slots)
		{
			Slot.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
			Slot.SelectionPreconditions.Initialize(this);
		}
	}

	UpdatePropertyBindings();
	if (bParametersUpdateRequired)
	{
		ApplyParameters();
	}

	Validate();
}

void USmartObjectDefinition::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);

	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		for (USmartObjectBehaviorDefinition* BehaviorDefinition : Slot.BehaviorDefinitions)
		{
			if (BehaviorDefinition)
			{
				OutDeps.Add(BehaviorDefinition);
			}
		}
	}
}

void USmartObjectDefinition::PreSave(FObjectPreSaveContext SaveContext)
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		Slot.SelectionPreconditions.Initialize(this);
	}

	UpdateSlotReferences();
	Super::PreSave(SaveContext);

	// In cooking we don't want to update bindings again since there was a
	// call in PostLoad and no data modifications are expected in the process.
	// During that call we also dropped the Picked paths so we can't call it again.
	if (!IsRunningCookCommandlet())
	{
		UpdatePropertyBindings();
	}

	// Invalidate variations since they are using a copy of the previous version of the asset.
	// Also send notification so loaded references can be refreshed.
	Variations.Reset();
	UE::SmartObject::Delegates::OnSavingDefinition.Broadcast(*this);
}

void USmartObjectDefinition::CollectSaveOverrides(FObjectCollectSaveOverridesContext SaveContext)
{
	Super::CollectSaveOverrides(SaveContext);

	if (SaveContext.IsCooking()
		&& SaveContext.GetTargetPlatform()->IsClientOnly()
		&& GetDefault<USmartObjectSettings>()->bShouldExcludePreConditionsOnDedicatedClient
		&& !HasAnyFlags(RF_ArchetypeObject | RF_ClassDefaultObject))
	{
		FObjectSaveOverride ObjSaveOverride;

		// Add path to the conditions within the main definition
		FProperty* OverrideProperty = FindFProperty<FProperty>(GetClass(), GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Preconditions));
		check(OverrideProperty);
		FPropertySaveOverride PropOverride;
		PropOverride.PropertyPath = FFieldPath(OverrideProperty);
		PropOverride.bMarkTransient = true;
		
		ObjSaveOverride.PropOverrides.Add(PropOverride);

		// Add path to the conditions within the slot definition struct
		OverrideProperty = FindFProperty<FProperty>(FSmartObjectSlotDefinition::StaticStruct(), GET_MEMBER_NAME_CHECKED(FSmartObjectSlotDefinition, SelectionPreconditions));
		check(OverrideProperty);
		PropOverride.PropertyPath = FFieldPath(OverrideProperty);
		ObjSaveOverride.PropOverrides.Add(PropOverride);

		SaveContext.AddSaveOverride(this, ObjSaveOverride);
	}
}

void USmartObjectDefinition::UpdateSlotReferences()
{
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		for (FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (!DataProxy.Data.IsValid())
			{
				continue;
			}
			const UScriptStruct* ScriptStruct = DataProxy.Data.GetScriptStruct();
			uint8* Memory = DataProxy.Data.GetMutableMemory();
			
			for (TFieldIterator<FProperty> It(ScriptStruct); It; ++It)
			{
				if (const FStructProperty* StructProp = CastField<FStructProperty>(*It))
				{
					if (StructProp->Struct == TBaseStructure<FSmartObjectSlotReference>::Get())
					{
						FSmartObjectSlotReference& Ref = *StructProp->ContainerPtrToValuePtr<FSmartObjectSlotReference>(Memory);
						const int32 Index = FindSlotByID(Ref.GetSlotID());
						Ref.SetIndex(Index);
					}
				}
			}
		}
	}
}

void USmartObjectDefinition::UpdateBindingPaths()
{
	BindingCollection.RemoveBindings([this](FPropertyBindingBinding& Binding)
	{
		return !UpdateAndValidatePath(Binding.GetMutableTargetPath())
			|| !UpdateAndValidatePath(Binding.GetMutableSourcePath());
	});
}

bool USmartObjectDefinition::UpdateAndValidatePath(FPropertyBindingPath& Path) const
{
	FPropertyBindingDataView DataView;
	if (!GetBindingDataViewByID(Path.GetStructID(), DataView))
	{
		return false;
	}
	if (!Path.UpdateSegmentsFromValue(DataView))
	{
		return false;
	}
	return true;
}
#endif // WITH_EDITOR

void USmartObjectDefinition::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	EnsureValidGuids();
#endif	
}

void USmartObjectDefinition::PostLoad()
{
	Super::PostLoad();

	// Fill in missing world condition schema for old data.
	if (!WorldConditionSchemaClass)
	{
		WorldConditionSchemaClass = GetDefault<USmartObjectSettings>()->DefaultWorldConditionSchemaClass;
	}

	if (Preconditions.GetSchemaClass().Get() != nullptr)
	{
		Preconditions.GetSchemaClass()->ConditionalPostLoad();
	}
	else
	{
		Preconditions.SetSchemaClass(WorldConditionSchemaClass);
	}

#if WITH_EDITOR
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (!ObjectTagFilter.IsEmpty())
	{
		FWorldCondition_SmartObjectActorTagQuery NewActorTagQueryCondition;
		NewActorTagQueryCondition.TagQuery = ObjectTagFilter;
		Preconditions.AddCondition(FWorldConditionEditable(0, EWorldConditionOperator::And, FConstStructView::Make(NewActorTagQueryCondition)));
		ObjectTagFilter.Clear();
		UE_ASSET_LOG(LogSmartObject, Log, this, TEXT("Deprecated object tag filter has been replaced by a %s precondition to validate tags on the smart object actor."
			" If the intent was to validate against instance runtime tags then the condition should be replaced by %s."),
			*FWorldCondition_SmartObjectActorTagQuery::StaticStruct()->GetName(),
			*FSmartObjectWorldConditionObjectTagQuery::StaticStruct()->GetName());
	}

	if (PreviewClass_DEPRECATED.IsValid())
	{
		PreviewData.ObjectActorClass = PreviewClass_DEPRECATED;
		PreviewClass_DEPRECATED.Reset();
	}
	if (PreviewMeshPath_DEPRECATED.IsValid())
	{
		PreviewData.ObjectMeshPath = PreviewMeshPath_DEPRECATED;
		PreviewMeshPath_DEPRECATED.Reset();
	}

	for (TEnumerateRef<FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		if (Slot->Data_DEPRECATED.Num() > 0)
		{
			Slot->DefinitionData.Reserve(Slot->Data_DEPRECATED.Num());

			for (TEnumerateRef<const FInstancedStruct> Data : EnumerateRange(Slot->Data_DEPRECATED))
			{
				FSmartObjectDefinitionDataProxy& DataProxy = Slot->DefinitionData.AddDefaulted_GetRef();
				DataProxy.Data.InitializeAsScriptStruct(Data->GetScriptStruct(), Data->GetMemory());

				static FName DataProxyName(TEXT("DataProxy"));
				const uint32 Hashes[] = {
					GetTypeHash(DataProxyName),
					GetTypeHash(Slot.GetIndex()),
					GetTypeHash(Data.GetIndex())
				}; 
				const uint64 Hash = CityHash64((const char*)Hashes, sizeof Hashes);
				DataProxy.ID = FGuid::NewDeterministicGuid(GetPathName(), Hash);
			}
			Slot->Data_DEPRECATED.Reset();
		}
	}

	// Transfer existing bindings to the collection
	if (!PropertyBindings_DEPRECATED.IsEmpty())
	{
		for (FSmartObjectDefinitionPropertyBinding& Binding : PropertyBindings_DEPRECATED)
		{
			BindingCollection.AddSmartObjectBinding(MoveTemp(Binding));
		}
		PropertyBindings_DEPRECATED.Empty();
	}

	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	// Preload dependencies
	TArray<UObject*> Dependencies;
	GetPreloadDependencies(Dependencies);
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
		for (USmartObjectBehaviorDefinition* BehaviorDefinition : Slot.BehaviorDefinitions)
		{
			if (BehaviorDefinition)
			{
				BehaviorDefinition->ConditionalPostLoad();
				BehaviorDefinition->GetPreloadDependencies(Dependencies);
			}
		}
	}
	for (UObject* Dependency : Dependencies)
	{
		if (Dependency)
		{
			Dependency->ConditionalPostLoad();
		}
	}

	EnsureValidGuids();
#endif // WITH_EDITOR

	Preconditions.Initialize(this);
	
	for (FSmartObjectSlotDefinition& Slot : Slots)
	{
#if WITH_EDITOR
		// Fill in missing slot ID for old data.
		if (!Slot.ID.IsValid())
		{
			Slot.ID = FGuid::NewGuid();
		}
#endif // WITH_EDITOR

		// Fill in missing world condition schema for old data.
		if (Slot.SelectionPreconditions.GetSchemaClass().Get() != nullptr)
		{
			Slot.SelectionPreconditions.GetSchemaClass()->ConditionalPostLoad();
		}
		else
		{
			Slot.SelectionPreconditions.SetSchemaClass(WorldConditionSchemaClass);
		}

		Slot.SelectionPreconditions.Initialize(this);
	}

#if WITH_EDITOR
	UpdateBindingPaths();
	UpdatePropertyBindings();
	UpdateSlotReferences();
	ApplyParameters();
#else
	// The parameters property bag struct is not cooked so we need to update its struct descriptor
	if (Parameters.GetPropertyBagStruct())
	{
		FPropertyBindingBindableStructDescriptor* Descriptor = BindingCollection.GetMutableBindableStructDescriptorFromHandle(FSmartObjectDefinitionDataHandle::Parameters);
		if (ensureMsgf(Descriptor, TEXT("The binding collection is expected to contain a bindable struct descriptor for the parameters"))
			&& Descriptor->Struct == nullptr)
		{
			Descriptor->Struct = Parameters.GetPropertyBagStruct();
		}
	}
#endif // WITH_EDITOR

	Validate();
}

uint64 USmartObjectDefinition::GetVariationParametersHash(const FInstancedPropertyBag& Parameters)
{
	if (UPropertyBag* ParametersBag = const_cast<UPropertyBag*>(Parameters.GetPropertyBagStruct()))
	{
		TArray<uint8> Data;
		FMemoryWriter Writer(Data);
		FObjectAndNameAsStringProxyArchive WriterProxy(Writer, /*bInLoadIfFindFails*/true);
		ParametersBag->SerializeItem(WriterProxy, const_cast<uint8*>(Parameters.GetValue().GetMemory()), /* Defaults */ nullptr);

		return CityHash64(reinterpret_cast<const char*>(Data.GetData()), Data.Num());
	}

	return 0;
}

USmartObjectDefinition* USmartObjectDefinition::GetAssetVariation(const FInstancedPropertyBag& VariationParameters, UWorld* World)
{
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs %s"), __FUNCTION__, *GetFullNameSafe(this));

	// If no parameters, return this asset.
	if (!VariationParameters.IsValid())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs: no parameters -> returning base asset"), __FUNCTION__);
		return this;
	}

	// Remove unused variations
	for (auto It = Variations.CreateIterator(); It; ++It)
	{
		if (!It->DefinitionAsset.IsValid())
		{
			It.RemoveCurrentSwap();
		}
	}
	
	// Expect correct bag if provided.
	UPropertyBag* VariationParametersBag = const_cast<UPropertyBag*>(VariationParameters.GetPropertyBagStruct());
	if (!VariationParametersBag || VariationParametersBag != Parameters.GetPropertyBagStruct())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs %s: Expecting matching variation parameters."), __FUNCTION__, *GetFullNameSafe(this));
		return nullptr;
	}

	// Calculate hash of the parameters, will be used to look up an existing variation.
	const uint64 VariationParametersHash = GetVariationParametersHash(VariationParameters);

	const FSmartObjectDefinitionAssetVariation* ExistingVariation = Variations.FindByPredicate([VariationParametersHash, World](const FSmartObjectDefinitionAssetVariation& Variation)
		{
			// DefinitionAsset has been validated above in the 'Remove unused variations' section
			return Variation.ParametersHash == VariationParametersHash && Variation.DefinitionAsset->GetOuter() == World;
		});
	if (ExistingVariation)
	{
		return ExistingVariation->DefinitionAsset.Get();
	}

	// Not the same, create a new one.
	const FName UniqueName = MakeUniqueObjectName(
		GetTransientPackage(),
		USmartObjectDefinition::StaticClass(),
		FName(FString::Printf(TEXT("%s_Var%llx"), *GetNameSafe(this), VariationParametersHash))
	);

	// Create asset variation using provided world as Outer so it gets properly GC'ed when changing world.
	// This is required since Parameters can have pointers to objects in the level (e.g., actors, components, etc.)
	USmartObjectDefinition* AssetVariation = DuplicateObject(this, World, UniqueName);
	check(AssetVariation);
	AssetVariation->SetFlags(RF_Transient);

	// Apply parameters
	UE_SUPPRESS(LogSmartObject, Verbose,
	{
		FString AsText;
		VariationParametersBag->ExportText(AsText, VariationParameters.GetValue().GetMemory(), /*Defaults*/nullptr, VariationParametersBag, PPF_None, /*ExportRootScope*/nullptr);
		UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs %s: Assigning parameters: %s."), __FUNCTION__, *GetFullNameSafe(AssetVariation), *AsText);
	});

	AssetVariation->Parameters = VariationParameters;

#if WITH_EDITOR
	AssetVariation->UpdatePropertyBindings();
#endif

	AssetVariation->ApplyParameters();

	// Keep track of variations.
	Variations.Emplace(AssetVariation, VariationParametersHash);
	
	return AssetVariation;
}

void USmartObjectDefinition::ApplyParameters()
{
	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs %s."), __FUNCTION__, *GetFullNameSafe(this));

	if (!BindingCollection.ResolvePaths())
	{
		UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs for '%s' failed: Unable to resolve binding paths")
			, __FUNCTION__, *GetFullNameSafe(this));
		return;
	}

	bool bSucceeded = true;
	for (const FPropertyBindingCopyInfoBatch& Batch : BindingCollection.GetCopyBatches())
	{
		ensureMsgf((Batch.BindingsEnd.AsInt32() - Batch.BindingsBegin.AsInt32()) == 1
			, TEXT("SmartObject bindings are not currently using batches so we expect one binding per batch, "
			"if that assumption changed we need to adapt the following code to fetch the TargetView only once per batch"));

		BindingCollection.ForEachBinding(Batch.BindingsBegin, Batch.BindingsEnd
			,[this, &Batch, &bSucceeded](const FPropertyBindingBinding& Binding, const int32 /*BindingIndex*/)
			{
				const FSmartObjectDefinitionPropertyBinding& SmartObjectBinding = static_cast<const FSmartObjectDefinitionPropertyBinding&>(Binding);

				FPropertyBindingDataView SourceDataView;
				if (!GetDataView(SmartObjectBinding.SourceDataHandle, SourceDataView))
				{
					UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("Could not find data view for property copy source %s."), *Binding.GetSourcePath().ToString());
					bSucceeded = false;
					return;
				}

				FPropertyBindingDataView TargetDataView;
				if (!GetDataView(SmartObjectBinding.TargetDataHandle, TargetDataView))
				{
					UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("Could not find data view for property copy target %s."), *Binding.GetTargetPath().ToString());
					bSucceeded = false;
					return;
				}

				for (const FPropertyBindingCopyInfo& Copy : BindingCollection.GetBatchCopies(Batch))
				{
					if (!BindingCollection.CopyProperty(Copy, SourceDataView, TargetDataView))
					{
						UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("Could not copy property for binding %s."), *Binding.ToString());
						bSucceeded = false;
					}
				}
			});
	}

	UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs for '%s': %s")
		, __FUNCTION__, *GetFullNameSafe(this), bSucceeded ? TEXT("Succeeded") : TEXT("Failed"));
}

// Deprecated
bool USmartObjectDefinition::ArePropertiesCompatible(const FProperty* SourceProperty, const FProperty* TargetProperty)
{
	return UE::PropertyBinding::GetPropertyCompatibility(SourceProperty, TargetProperty) == UE::PropertyBinding::EPropertyCompatibility::Compatible;
}

#if WITH_EDITOR
void USmartObjectDefinition::EnsureValidGuids()
{
	if (!RootID.IsValid())
	{
		RootID = FGuid::NewDeterministicGuid(GetPathName(), FCrc::StrCrc32<TCHAR>(TEXT("RootID")));
	}
	if (!ParametersID.IsValid())
	{
		ParametersID = FGuid::NewDeterministicGuid(GetPathName(), FCrc::StrCrc32<TCHAR>(TEXT("ParametersID")));
	}
}

void USmartObjectDefinition::UpdatePropertyBindings()
{
	// SmartObjectDefinition uses the same collection for Editor operations and runtime so we
	// rebuild the collection be reusing the current bindings that might only need to map to
	// new bindable structs or relocated structs.
	TArray<FSmartObjectDefinitionPropertyBinding> Bindings(BindingCollection.ExtractBindings());
	BindingCollection.Reset();

	// Setup all struct descriptors:
	// ------------------------------
	BindingCollection.AddBindableStruct({TEXT("Parameters"), Parameters.GetPropertyBagStruct(), ParametersID, FSmartObjectDefinitionDataHandle::Parameters});
	BindingCollection.AddBindableStruct({TEXT("Root"), GetClass(), RootID, FSmartObjectDefinitionDataHandle::Root});

	// Slots
	int32 SlotIndex = 0;
	for (const FSmartObjectSlotDefinition& Slot : Slots)
	{
		BindingCollection.AddBindableStruct({TEXT("Slot"), FSmartObjectSlotDefinition::StaticStruct(), Slot.ID, FSmartObjectDefinitionDataHandle(SlotIndex)});

		// SlotDefinitionData
		int32 DataIndex = 0;
		for (const FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			BindingCollection.AddBindableStruct({TEXT("DefinitionData"), DataProxy.Data.GetScriptStruct(), DataProxy.ID, FSmartObjectDefinitionDataHandle(SlotIndex, DataIndex)});
			DataIndex++;
		}

		SlotIndex++;
	}

	// Note that copy batches optimization is currently not used for SmartObject definition
	// since all copies are done once per asset on load and usually on a small amount or properties.
	// For now each binding will use its own batch but, if eventually required, bindings could be sorted and put in batches.
	for (FSmartObjectDefinitionPropertyBinding& Binding : Bindings)
	{
		// Ignore binding with retargeted path in normal editor mode, if required they will be recreated below from the picked path
		if (Binding.TargetPathRetargetingStatus == ESmartObjectPropertyPathRetargetingStatus::RetargetedPath)
		{
			UE_VLOG_UELOG(this, LogSmartObject, Verbose, TEXT("%hs: skipping binding with retargeted path '%s'."), __FUNCTION__, *Binding.ToString());
			continue;
		}

		Binding.SourceDataHandle = GetDataHandleByID(Binding.GetSourcePath().GetStructID());
		Binding.TargetDataHandle = GetDataHandleByID(Binding.GetTargetPath().GetStructID());

		const FPropertyBindingBindableStructDescriptor* SourceDesc = BindingCollection.GetBindableStructDescriptorFromHandle(FConstStructView::Make(Binding.SourceDataHandle));
		const FPropertyBindingBindableStructDescriptor* TargetDesc = BindingCollection.GetBindableStructDescriptorFromHandle(FConstStructView::Make(Binding.TargetDataHandle));

		// Source must be in the source array
		if (SourceDesc == nullptr)
		{
			UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs %s: Could not find a struct descriptor for Source '%s'.")
				, __FUNCTION__, *GetFullNameSafe(this), *Binding.GetSourcePath().ToString());
			return;
		}

		// Target must be in the source array
		if (TargetDesc == nullptr)
		{
			UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs %s: Could not find a struct descriptor for Target '%s'.")
				, __FUNCTION__, *GetFullNameSafe(this), *Binding.GetTargetPath().ToString());
			return;
		}

		if (!ensureMsgf(Binding.GetTargetPath().GetStructID() == TargetDesc->ID, TEXT("StructID of the Target struct descriptor is expected to match the struct Id of the TargetPath")))
		{
			return;
		}

		FPropertyBindingDataView SourceDataView;
		if (!GetDataView(Binding.SourceDataHandle, SourceDataView))
		{
			UE_VLOG_UELOG(this ,LogSmartObject, Error, TEXT("%hs %s: Could not find data view for property copy source %s.")
				, __FUNCTION__, *GetFullNameSafe(this), *Binding.GetSourcePath().ToString());
			return;
		}

		FPropertyBindingDataView TargetDataView;
		if (!GetDataView(Binding.TargetDataHandle, TargetDataView))
		{
			UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs %s: Could not find data view for property copy target %s.")
				, __FUNCTION__, *GetFullNameSafe(this), *Binding.GetTargetPath().ToString());
			return;
		}

		FString Error;
		TArray<FPropertyBindingPathIndirection> SourceIndirections;
		TArray<FPropertyBindingPathIndirection> TargetIndirections;
		
		if (!Binding.GetSourcePath().ResolveIndirectionsWithValue(SourceDataView, SourceIndirections, &Error))
		{
			UE_VLOG_UELOG(this, LogSmartObject, Error, TEXT("%hs %s: Resolving path in %s: %s")
				, __FUNCTION__, *GetFullNameSafe(this),	*SourceDesc->ToString(), *Error);
			return;
		}

		auto ResolveFunc = [&BindingCollection = BindingCollection, LogOwner = this, SourceDataView, TargetDataView, SourceDesc, TargetDesc, &SourceIndirections, &TargetIndirections, &Error]
		(FSmartObjectDefinitionPropertyBinding& BindingToResolve)
		{
			if (!BindingToResolve.GetTargetPath().ResolveIndirectionsWithValue(TargetDataView, TargetIndirections, &Error))
			{
				UE_VLOG_UELOG(LogOwner, LogSmartObject, Error, TEXT("ResolveIndirectionsWithValue failed to resolve path in %s: %s")	, *TargetDesc->ToString(), *Error);
				return false;
			}

			FPropertyBindingCopyInfo DummyCopy;
			FPropertyBindingPathIndirection LastSourceIndirection = !SourceIndirections.IsEmpty() ? SourceIndirections.Last() : FPropertyBindingPathIndirection(SourceDataView.GetStruct());
			FPropertyBindingPathIndirection LastTargetIndirection = !TargetIndirections.IsEmpty() ? TargetIndirections.Last() : FPropertyBindingPathIndirection(TargetDataView.GetStruct());
			if (!BindingCollection.ResolveCopyType(LastSourceIndirection, LastTargetIndirection, DummyCopy))
			{
				UE_VLOG_UELOG(LogOwner, LogSmartObject, Error, TEXT("ResolveCopyType %s failed to copy properties between %s and %s: types are incompatible.")
					, *GetFullNameSafe(LogOwner)
					, *UE::PropertyBinding::GetDescriptorAndPathAsString(*SourceDesc, BindingToResolve.GetSourcePath())
					, *UE::PropertyBinding::GetDescriptorAndPathAsString(*TargetDesc, BindingToResolve.GetTargetPath()));
				return false;
			}

			return true;
		};

		// Special case for bindings that also have an additional Editor only binding
		FSmartObjectDefinitionPropertyBinding BindingUsingRetargetedPath(Binding);

		FPropertyBindingPath RedirectedPath(Binding.GetTargetPath());
		const bool bRetargeted = FWorldConditionQueryDefinition::TryRetargetingPathToConditions(RedirectedPath);
		if (bRetargeted)
		{
			BindingUsingRetargetedPath.GetMutableTargetPath() = RedirectedPath;
			BindingUsingRetargetedPath.TargetPathRetargetingStatus = ESmartObjectPropertyPathRetargetingStatus::RetargetedPath;
			Binding.TargetPathRetargetingStatus = ESmartObjectPropertyPathRetargetingStatus::PickedPath;
		}
		
		// Make sure we can resolve the binding
		if (!ResolveFunc(Binding))
		{
			continue;
		}

		if (bRetargeted)
		{
			// Make sure we can resolve the binding that uses the retargeted path
			if (!ResolveFunc(BindingUsingRetargetedPath))
			{
				continue;
			}
		}

		auto AddBindingToCollectionFunc = [&BindingCollection = BindingCollection, TargetDesc](FSmartObjectDefinitionPropertyBinding& BindingToAdd)
		{
			// When cooking we discard the Editor picked path since we only want to use the retargeted path
			if (IsRunningCookCommandlet() && BindingToAdd.TargetPathRetargetingStatus == ESmartObjectPropertyPathRetargetingStatus::PickedPath)
			{
				return;
			}

			FPropertyBindingCopyInfoBatch& Batch = BindingCollection.AddCopyBatch();
			Batch.TargetStruct = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(*TargetDesc);

			const int32 NumBindings = BindingCollection.GetNumBindings();
			Batch.BindingsBegin = FPropertyBindingIndex16(NumBindings);
			Batch.BindingsEnd = FPropertyBindingIndex16(NumBindings + 1);

			// PropertyFunctions are not used by SmartObject definitions
			Batch.PropertyFunctionsBegin = FPropertyBindingIndex16();
			Batch.PropertyFunctionsEnd = FPropertyBindingIndex16();

			// Add the validated binding to the collection.
			BindingCollection.AddSmartObjectBinding(MoveTemp(BindingToAdd));
		};

		// Add the main binding to the collection
		AddBindingToCollectionFunc(Binding);

		// Retargeted path indicates that we need an Editor-only variation for this binding
		if (bRetargeted)
		{
			AddBindingToCollectionFunc(BindingUsingRetargetedPath);
		}
	}
}

void USmartObjectDefinition::GetBindableStructs(const FGuid InTargetStructID, TArray<TInstancedStruct<FPropertyBindingBindableStructDescriptor>>& OutStructDescs) const
{
	FPropertyBindingBindableStructDescriptor& ParametersDesc = OutStructDescs.Add_GetRef(TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make()).GetMutable();
	ParametersDesc.Name = FName(TEXT("Parameters"));
	ParametersDesc.ID = ParametersID;
	ParametersDesc.Struct = Parameters.GetPropertyBagStruct();
}

FPropertyBindingBindingCollection* USmartObjectDefinition::GetEditorPropertyBindings()
{
	return &BindingCollection;
}

const FPropertyBindingBindingCollection* USmartObjectDefinition::GetEditorPropertyBindings() const
{
	return &BindingCollection;
}

void USmartObjectDefinition::OnPropertyBindingChanged(const FPropertyBindingPath & InSourcePath, const FPropertyBindingPath & InTargetPath)
{
	UpdateBindingPaths();
	UpdatePropertyBindings();
	ApplyParameters();
}

bool USmartObjectDefinition::GetBindingDataViewByID(const FGuid InStructID, FPropertyBindingDataView& OutDataView) const
{
	if (InStructID == ParametersID)
	{
		OutDataView = FPropertyBindingDataView(const_cast<FInstancedPropertyBag&>(Parameters).GetMutableValue());
		return true;
	}
	if (InStructID == RootID)
	{
		OutDataView = FPropertyBindingDataView(const_cast<UObject*>(static_cast<const UObject*>(this)));
		return true;
	}

	for (const FSmartObjectSlotDefinition& Slot : Slots)
	{
		if (InStructID == Slot.ID)
		{
			OutDataView = FPropertyBindingDataView(FStructView::Make(const_cast<FSmartObjectSlotDefinition&>(Slot)));
			return true;
		}
		for (const FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (InStructID == DataProxy.ID)
			{
				OutDataView = FPropertyBindingDataView(DataProxy.Data.GetScriptStruct(),  const_cast<TInstancedStruct<FSmartObjectDefinitionData>&>(DataProxy.Data).GetMutableMemory());
				return true;
			}
		}
	}
	
	return false;
}

bool USmartObjectDefinition::GetBindableStructByID(const FGuid InStructID, TInstancedStruct<FPropertyBindingBindableStructDescriptor>& OutDesc) const
{
	if (InStructID == ParametersID)
	{
		OutDesc = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(FName(TEXT("Parameters")), const_cast<FInstancedPropertyBag&>(Parameters).GetMutableValue().GetScriptStruct(), ParametersID);
		return true;
	}
	if (InStructID == RootID)
	{
		OutDesc = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(FName(TEXT("Root")), StaticClass(), RootID);
		return true;
	}

	for (const FSmartObjectSlotDefinition& Slot : Slots)
	{
		if (InStructID == Slot.ID)
		{
			OutDesc = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(Slot.Name, TBaseStructure<FSmartObjectSlotDefinition>::Get(), Slot.ID);
			return true;
		}
		for (const FSmartObjectDefinitionDataProxy& DataProxy : Slot.DefinitionData)
		{
			if (InStructID == DataProxy.ID)
			{
				FString DataName = Slot.Name.ToString();
				const UScriptStruct* ScriptStruct = DataProxy.Data.GetScriptStruct();
				if (ScriptStruct)
				{
					DataName += TEXT(" ");
					DataName += ScriptStruct->GetDisplayNameText().ToString();
				}
				OutDesc = TInstancedStruct<FPropertyBindingBindableStructDescriptor>::Make(FName(DataName), ScriptStruct, DataProxy.ID);
				return true;
			}
		}
	}
	
	return false;
}

FSmartObjectDefinitionDataHandle USmartObjectDefinition::GetDataHandleByID(const FGuid StructID)
{
	if (StructID == ParametersID)
	{
		return FSmartObjectDefinitionDataHandle::Parameters;
	}
	if (StructID == RootID)
	{
		return FSmartObjectDefinitionDataHandle::Root;
	}

	for (const TEnumerateRef<const FSmartObjectSlotDefinition> Slot : EnumerateRange(Slots))
	{
		if (StructID == Slot->ID)
		{
			return FSmartObjectDefinitionDataHandle(Slot.GetIndex());
		}
		for (const TEnumerateRef<const FSmartObjectDefinitionDataProxy> DataProxy : EnumerateRange(Slot->DefinitionData))
		{
			if (StructID == DataProxy->ID)
			{
				return FSmartObjectDefinitionDataHandle(Slot.GetIndex(), DataProxy.GetIndex());
			}
		}
	}
	
	return {};
}

FGuid USmartObjectDefinition::GetFallbackStructID() const
{
	return RootID;
}

void USmartObjectDefinition::CreateParametersForStruct(const FGuid InStructID, const TArrayView<UE::PropertyBinding::FPropertyCreationDescriptor> InOutCreationDescs)
{
	if (InOutCreationDescs.IsEmpty())
	{
		return;
	}

	UE::PropertyBinding::CreateUniquelyNamedPropertiesInPropertyBag(InOutCreationDescs, Parameters);

	// Update UI
	UE::SmartObject::Delegates::OnParametersChanged.Broadcast(*this);
}

#endif // WITH_EDITOR

bool USmartObjectDefinition::GetBindingDataView(const FPropertyBindingBinding& InBinding, const EBindingSide InSide, FPropertyBindingDataView& OutDataView)
{
	const FSmartObjectDefinitionPropertyBinding& SmartObjectBinding = reinterpret_cast<const FSmartObjectDefinitionPropertyBinding&>(InBinding);
	switch (InSide)
	{
	case EBindingSide::Source:
		return GetDataView(SmartObjectBinding.SourceDataHandle, OutDataView);
	case EBindingSide::Target:
		return GetDataView(SmartObjectBinding.TargetDataHandle, OutDataView);
	}

	return false;
}

bool USmartObjectDefinition::GetDataView(const FSmartObjectDefinitionDataHandle DataHandle, FPropertyBindingDataView& OutDataView)
{
	if (!DataHandle.IsSlotValid())
	{
		return false;
	}
	
	if (DataHandle.IsParameters())
	{
		OutDataView = FPropertyBindingDataView(Parameters.GetMutableValue());
		return true;
	}
	if (DataHandle.IsRoot())
	{
		OutDataView = FPropertyBindingDataView(this);
		return true;
	}

	const int32 SlotIndex = DataHandle.GetSlotIndex();
	if (Slots.IsValidIndex(SlotIndex))
	{
		FSmartObjectSlotDefinition& Slot = Slots[SlotIndex];

		if (DataHandle.IsDataValid())
		{
			// Slot data definition
			const int32 DataDefinitionIndex = DataHandle.GetDataIndex();
			if (Slot.DefinitionData.IsValidIndex(DataDefinitionIndex))
			{
				FSmartObjectDefinitionDataProxy& DataProxy = Slot.DefinitionData[DataDefinitionIndex];
				OutDataView = FPropertyBindingDataView(DataProxy.Data.GetScriptStruct(), DataProxy.Data.GetMutableMemory());
				return true;
			}
		}
		else
		{
			// Just a slot
			OutDataView = FPropertyBindingDataView(FStructView::Make(Slot));
			return true;
		}
	}

	return false;
}


#undef LOCTEXT_NAMESPACE
