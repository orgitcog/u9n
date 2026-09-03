// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphSettings.h"

#include "AnimNextAnimGraphModule.h"
#include "AnimNextDataInterfacePayload.h"
#include "DataInterface/AnimNextNativeDataInterface.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Logging/StructuredLog.h"
#include "Misc/EnumerateRange.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryHelpers.h"
#endif // WITH_EDITOR

#if WITH_EDITOR
void UAnimNextAnimGraphSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(PropertyChangedEvent.Property)
	{
		if( PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UAnimNextAnimGraphSettings, AssetGraphMappings) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNextAssetGraphMapping, AssetType) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNextAssetGraphMapping, AnimationGraph) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNextAssetGraphMapping, Variable) ||
			PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(FAnimNextAssetGraphMapping, RequiredDataInterfaces))
		{
			LoadAndRebuildMappings(false);
		}
	}
}
#endif

const UAnimNextAnimationGraph* UAnimNextAnimGraphSettings::GetGraphFromObject(const UObject* InObject) const
{
	FAnimNextDataInterfacePayload GraphPayload;
	return GetGraphFromObject(InObject, GraphPayload);
}

const UAnimNextAnimationGraph* UAnimNextAnimGraphSettings::GetGraphFromObject(const UObject* InObject, const FAnimNextDataInterfacePayload& InGraphPayload) const
{
	check(InObject);

	const UAnimNextAnimationGraph* AnimationGraph = Cast<UAnimNextAnimationGraph>(InObject);
	if(AnimationGraph)
	{
		// Object is an animation graph, so we early out
		return AnimationGraph;
	}

	if (!ensure(bMappingsBuiltAtLeastOnce.load(std::memory_order_acquire)))
	{
		return nullptr;
	}

	// Check this class and its parent classes in turn
	const UClass* Class = InObject->GetClass();
	int32 Index = INDEX_NONE;
	while(Class)
	{
		const int32* IndexPtr = AssetGraphMap.Find(Class);
		if(IndexPtr != nullptr)
		{
			Index = *IndexPtr;
			break;
		}

		Class = Class->GetSuperClass();
	}

	if(!AssetGraphMappings.IsValidIndex(Index))
	{
		return nullptr;
	}

	TConstArrayView<FInstancedStruct> NativePayloads = InGraphPayload.GetNativePayloads();

	int32 BestNumMatchedInterfaces = -1;
	int32 BestMappingIndex = INDEX_NONE;

	while (Index != INDEX_NONE)
	{
		const FAnimNextAssetGraphMapping& Mapping = AssetGraphMappings[Index];

		bool bIsMappingValid = true;
		int32 NumMatchedInterfaces = 0;

		for (const TSoftObjectPtr<const UScriptStruct>& DataInterfacePtr : Mapping.RequiredDataInterfaces)
		{
			const UScriptStruct* DataInterface = DataInterfacePtr.Get();
			if (DataInterface == nullptr)
			{
				continue;
			}

			bool bFoundRequiredInterface = false;
			for (const FInstancedStruct& NativePayload : NativePayloads)
			{
				if (NativePayload.GetScriptStruct() == DataInterface)
				{
					bFoundRequiredInterface = true;
					break;
				}
			}

			if (bFoundRequiredInterface)
			{
				NumMatchedInterfaces++;
			}
			else
			{
				bIsMappingValid = false;
				break;
			}
		}

		// This mapping is valid, use it if its our best match
		// If the number of interfaces matched is identical, then we'll pick the first mapping seen,
		// which will be the last one found through the config files
		if (bIsMappingValid && NumMatchedInterfaces > BestNumMatchedInterfaces)
		{
			BestNumMatchedInterfaces = NumMatchedInterfaces;
			BestMappingIndex = Index;
		}

		// Try the next mapping for the same asset type
		Index = Mapping.NextMappingIndex;
	}

	// Returns our best match, if one is found
	return BestMappingIndex != INDEX_NONE ? AssetGraphMappings[BestMappingIndex].AnimationGraph.Get() : nullptr;
}

bool UAnimNextAnimGraphSettings::CanGetGraphFromAssetClass(const UClass* InClass) const
{
	if(InClass == nullptr)
	{
		return false;
	}
	
	if(InClass == UAnimNextAnimationGraph::StaticClass())
	{
		return true;
	}

	if (!ensure(bMappingsBuiltAtLeastOnce.load(std::memory_order_acquire)))
	{
		return false;
	}

	const UClass* Class = InClass;
	while(Class)
	{
		if(AssetGraphMap.Find(Class) != nullptr)
		{
			return true;
		}

		Class = Class->GetSuperClass();
	}

	return false;
}

FName UAnimNextAnimGraphSettings::GetInjectedVariableNameFromObject(const UObject* InObject) const
{
	check(InObject);

	const UAnimNextAnimationGraph* AnimationGraph = Cast<UAnimNextAnimationGraph>(InObject);
	if(AnimationGraph)
	{
		// Object is an animation graph, so we early out
		return NAME_None;
	}

	if (!ensure(bMappingsBuiltAtLeastOnce.load(std::memory_order_acquire)))
	{
		return NAME_None;
	}

	// Check this class and its parent classes in turn
	const UClass* Class = InObject->GetClass();
	int32 Index = INDEX_NONE;
	while(Class)
	{
		const int32* IndexPtr = AssetGraphMap.Find(Class);
		if(IndexPtr != nullptr)
		{
			Index = *IndexPtr;
			break;
		}

		Class = Class->GetSuperClass();
	}

	if(!AssetGraphMappings.IsValidIndex(Index))
	{
		return NAME_None;
	}

	return AssetGraphMappings[Index].Variable;
}


void UAnimNextAnimGraphSettings::GetNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextDataInterfacePayload& InOutGraphPayload) const
{
	GetNativePayloadFromGraph(InObject, InAnimationGraph, InOutGraphPayload.OwnedNativePayloads);
	InOutGraphPayload.bCombinedPayloadsDirty = true;
}

void UAnimNextAnimGraphSettings::GetNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, TArray<FInstancedStruct>& InOutGraphPayload) const
{
	if(InObject == nullptr || InAnimationGraph == nullptr)
	{
		return;
	}

	auto FindNativeInterface = [&InOutGraphPayload](const UScriptStruct* InNativeInterface)
	{
		return InOutGraphPayload.FindByPredicate([InNativeInterface](const FInstancedStruct& InInstancedStruct)
		{
			return InInstancedStruct.GetScriptStruct() == InNativeInterface;
		});
	};

	// First generate any structs for the supplied graph from the graph's native interfaces, if required
	for(const FAnimNextImplementedDataInterface& ImplementedInterface : InAnimationGraph->GetImplementedInterfaces())
	{
		if(ImplementedInterface.NativeInterface)
		{
			// Check if we have an interface already supplied
			const FInstancedStruct* NativeInterfaceInstance = FindNativeInterface(ImplementedInterface.NativeInterface);
			if(NativeInterfaceInstance == nullptr)
			{
				check(ImplementedInterface.NativeInterface->IsChildOf(FAnimNextNativeDataInterface::StaticStruct()));

				// Create a new native interface
				FInstancedStruct NewNativeInterface;
				NewNativeInterface.InitializeAs(ImplementedInterface.NativeInterface);

				FAnimNextNativeDataInterface::FBindToFactoryObjectContext Context;
				Context.FactoryObject = InObject;
				Context.DataInterface = InAnimationGraph;
				NewNativeInterface.GetMutable<FAnimNextNativeDataInterface>().BindToFactoryObject(Context);

				InOutGraphPayload.Add(MoveTemp(NewNativeInterface));
			}
		}
	}
}

void UAnimNextAnimGraphSettings::GetNonNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FAnimNextDataInterfacePayload& InOutGraphPayload) const
{
	GetNonNativePayloadFromGraph(InObject, InAnimationGraph, InOutGraphPayload.OwnedPayload);
	InOutGraphPayload.bCombinedPayloadsDirty = true;
}

void UAnimNextAnimGraphSettings::GetNonNativePayloadFromGraph(const UObject* InObject, const UAnimNextAnimationGraph* InAnimationGraph, FInstancedPropertyBag& InOutGraphPayload) const
{
	// Duplicate the public variables
	InOutGraphPayload = InAnimationGraph->GetPublicVariableDefaults();

	// Setup the factory-mapped variable
	FName VariableName = GetInjectedVariableNameFromObject(InObject);
	if(VariableName.IsNone())
	{
		return;
	}

	const FPropertyBagPropertyDesc* Desc = InOutGraphPayload.FindPropertyDescByName(VariableName);
	if(Desc == nullptr)
	{
		return;
	}

	const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Desc->CachedProperty);
	if(ObjectProperty == nullptr)
	{
		return;
	}

#if WITH_EDITOR
	// Remove editor visibility for the mapped property as UI doesnt want to display it twice
	const_cast<FObjectProperty*>(ObjectProperty)->PropertyFlags &= ~CPF_Edit;
#endif

	uint8* ValuePtr = ObjectProperty->ContainerPtrToValuePtr<uint8>(InOutGraphPayload.GetMutableValue().GetMemory());
	ObjectProperty->SetObjectPropertyValue(ValuePtr, const_cast<UObject*>(InObject));
}

void UAnimNextAnimGraphSettings::OnDefaultRunGraphHostLoaded(const UAnimNextAnimationGraph* AnimationGraph, bool bLoadAsync)
{
	if (AnimationGraph == nullptr)
	{
		UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Could not load default host animation graph '{GraphName}'", DefaultRunGraphHost.ToString());
		return;
	}

	using namespace UE::AnimNext::AnimGraph;
	FAnimNextAnimGraphModule& AnimGraphModule = FModuleManager::GetModuleChecked<FAnimNextAnimGraphModule>("AnimNextAnimGraph");
	AnimGraphModule.LoadedGraphs.Add(AnimationGraph);

	if (AssetGraphMappings.IsEmpty())
	{
		FinalizeAsyncLoad();
		return;
	}

	for (TEnumerateRef<FAnimNextAssetGraphMapping> Mapping : EnumerateRange(AssetGraphMappings))
	{
		FSoftObjectPath AnimationGraphSoftPath = Mapping->AnimationGraph.ToSoftObjectPath();

#if WITH_EDITOR
		UAssetRegistryHelpers::FixupRedirectedAssetPath(IN OUT AnimationGraphSoftPath);
#endif

		int32 PackageID = AnimationGraphSoftPath.LoadAsync(FLoadSoftObjectPathAsyncDelegate::CreateLambda(
			[WeakThis = TWeakObjectPtr<UAnimNextAnimGraphSettings>(this),
			&Mapping = *Mapping,
			MappingIndex = Mapping.GetIndex()]
			(const FSoftObjectPath& InSoftObjectPath, UObject* InObject)
			{
				if (UAnimNextAnimGraphSettings* ThisPin = WeakThis.Get())
				{
					ThisPin->NumMappingsLoaded++;

					ThisPin->OnMappingAnimationGraphLoaded(CastChecked<const UAnimNextAnimationGraph>(InObject, ECastCheckedType::NullAllowed), Mapping, MappingIndex);

					if (ThisPin->NumMappingsLoaded == ThisPin->AssetGraphMappings.Num())
					{
						ThisPin->FinalizeAsyncLoad();
					}
				}
			}));

		if (!bLoadAsync)
		{
			FlushAsyncLoading(PackageID);
		}
	}
}

void UAnimNextAnimGraphSettings::OnMappingAnimationGraphLoaded(const UAnimNextAnimationGraph* AnimationGraph, FAnimNextAssetGraphMapping& Mapping, int32 MappingIndex)
{
	if (AnimationGraph == nullptr)
	{
		UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Could not load animation graph '{GraphName}'", Mapping.AnimationGraph.ToString());
		return;
	}

	using namespace UE::AnimNext::AnimGraph;
	FAnimNextAnimGraphModule& AnimGraphModule = FModuleManager::GetModuleChecked<FAnimNextAnimGraphModule>("AnimNextAnimGraph");
	AnimGraphModule.LoadedGraphs.Add(AnimationGraph);

	const UClass* AssetType = Mapping.AssetType.LoadSynchronous();
	if (AssetType == nullptr)
	{
		UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Could not load asset class '{ClassName}'", Mapping.AssetType.ToString());
		return;
	}

	bool bAreRequiredInterfacesValid = true;
	for (const TSoftObjectPtr<const UScriptStruct>& DataInterfacePtr : Mapping.RequiredDataInterfaces)
	{
		const UScriptStruct* DataInterface = DataInterfacePtr.LoadSynchronous();
		if (DataInterface == nullptr)
		{
			UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Could not load required data interface class '{ClassName}'", DataInterfacePtr.ToString());
			bAreRequiredInterfacesValid = false;
			continue;
		}
	}

	if (!bAreRequiredInterfacesValid)
	{
		return;
	}

	// Find or add our new entry
	int32& FirstAssetMappingIndex = AssetGraphMap.FindOrAdd(AssetType, INDEX_NONE);

	// Update the next mapping index to point to our old entry (or INDEX_NONE if we are the first)
	Mapping.NextMappingIndex = FirstAssetMappingIndex;

	// Our new first mapping index is the current one
	FirstAssetMappingIndex = MappingIndex;

	// Warn if the variable is not settable in the public interface of the graph
	if (AnimationGraph != nullptr && AssetType != nullptr && !Mapping.Variable.IsNone())
	{
		const FInstancedPropertyBag& PublicVariableDefaults = AnimationGraph->GetPublicVariableDefaults();
		const FPropertyBagPropertyDesc* Desc = PublicVariableDefaults.FindPropertyDescByName(Mapping.Variable);
		if (Desc == nullptr)
		{
			UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Could not find public variable '{VariableName}' in graph '{GraphName}'", Mapping.Variable, AnimationGraph->GetPathName());
			return;
		}

		const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Desc->CachedProperty);
		if (ObjectProperty == nullptr)
		{
			UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Variable '{VariableName}' in graph '{GraphName}' is not of object type", Mapping.Variable, AnimationGraph->GetPathName());
			return;
		}

		if (!AssetType->IsChildOf(ObjectProperty->PropertyClass))
		{
			UE_LOGFMT(LogAnimation, Error, "UAnimNextAnimGraphSettings::LoadAndRebuildMappings: Variable '{VariableName}' in graph '{GraphName}' is not of a compatible object type: '{TargetType}' vs '{SourceType}'", Mapping.Variable, AnimationGraph->GetPathName(), ObjectProperty->PropertyClass->GetFullName(), AssetType->GetFullName());
			return;
		}
	}
}

void UAnimNextAnimGraphSettings::FinalizeAsyncLoad()
{
	// Release memory order to ensure all previous writes complete before other threads can see this value change
	// Queries to this value must use the acquire memory order to ensure loads are not re-ordered before the value
	// is read and tested
	bMappingsBuiltAtLeastOnce.store(true, std::memory_order_release);
}

void UAnimNextAnimGraphSettings::LoadAndRebuildMappings(bool bLoadAsync)
{
	using namespace UE::AnimNext::AnimGraph;

	// To avoid loading every asset synchronously at engine init, we load them async in multiple steps
	//   - We first load the default run graph host
	//   - Followed by the mapping graph assets concurrently
	//   - Once everything has loaded, the graph factories are usable
	
	FAnimNextAnimGraphModule& AnimGraphModule = FModuleManager::GetModuleChecked<FAnimNextAnimGraphModule>("AnimNextAnimGraph");
	AnimGraphModule.LoadedGraphs.Reset();
	AssetGraphMap.Reset();

	int32 PackageID = DefaultRunGraphHost.ToSoftObjectPath().LoadAsync(FLoadSoftObjectPathAsyncDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UAnimNextAnimGraphSettings>(this), bLoadAsync](const FSoftObjectPath& InSoftObjectPath, UObject* InObject)
		{
			if (TStrongObjectPtr<UAnimNextAnimGraphSettings> ThisPin = WeakThis.Pin())
			{
				ThisPin->OnDefaultRunGraphHostLoaded(CastChecked<const UAnimNextAnimationGraph>(InObject), bLoadAsync);
			}
		}));

	if (!bLoadAsync)
	{
		FlushAsyncLoading(PackageID);
	}
}

TArray<UClass*> UAnimNextAnimGraphSettings::GetAllowedAssetClasses()
{
	return GetDefault<UAnimNextAnimGraphSettings>()->GetAllowedAssetClassesImpl();
}

TArray<UClass*> UAnimNextAnimGraphSettings::GetAllowedAssetClassesImpl() const
{
	TArray<UClass*> AllowedClasses;

	if (!ensure(bMappingsBuiltAtLeastOnce.load(std::memory_order_acquire)))
	{
		return AllowedClasses;
	}

	// Add all derived classes of UAnimNextAnimationGraph
	AllowedClasses.Add(UAnimNextAnimationGraph::StaticClass());
	GetDerivedClasses(UAnimNextAnimationGraph::StaticClass(), AllowedClasses);

	// Add all mappings
	for(const FAnimNextAssetGraphMapping& Mapping : AssetGraphMappings)
	{
		if(UClass* AssetClass = Mapping.AssetType.Get())
		{
			AllowedClasses.Add(AssetClass);
		}
	}

	return AllowedClasses;
}
