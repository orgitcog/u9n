// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigTrait.h"

#include "Module/AnimNextModuleInstance.h"
#include "Graph/AnimNextGraphInstance.h"
#include "AnimNextAnimGraphSettings.h"
#include "Animation/AnimSequence.h"
#include "DataInterface/DataInterfaceStructAdapter.h"
#include "TraitInterfaces/IUpdate.h"
#include "TraitInterfaces/IEvaluate.h"
#include "TraitInterfaces/ITimeline.h"
#include "TraitInterfaces/IHierarchy.h"
#include "ControlRigObjectBinding.h"
#include "ControlRigTask.h"
#include "AnimNextControlRigModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimNodeBase.h"
#include "AnimationDataSource.h"
#include "RigVMCore/RigVMMemoryStorageStruct.h"
#if WITH_EDITOR
#include "RigVMModel/RigVMController.h"
#include "ControlRigBlueprint.h" // to get the preview skeleton
#endif

//----------------------
// --- FSharedData ---
//----------------------
void FControlRigTraitSharedData::ConstructLatentProperties(const UE::AnimNext::FTraitBinding& Binding)
{
	const FControlRigTraitSharedData* SharedData = Binding.GetSharedData<FControlRigTraitSharedData>();
	if (SharedData && SharedData->ControlRigClass.Get() != nullptr)
	{
		FControlRigVariableMappings::FCustomPropertyMappings PropertyMappings;
		UE::AnimNext::FControlRigTrait::FInstanceData::GetExposedVariablesData(Binding, SharedData, PropertyMappings);
		UE::AnimNext::FControlRigTrait::FInstanceData::GetExposedControlsData(Binding, SharedData, PropertyMappings);

		const TArray<FControlRigVariableMappings::FCustomPropertyData>& Mappings = PropertyMappings.GetMappings();
		const int32 NumProperties = Mappings.Num();

		for (const FControlRigVariableMappings::FCustomPropertyData& Mapping : Mappings)
		{
			const FProperty* Property = Mapping.Property;
			const uint8* LatentPinMemory = Mapping.SourceMemory;
			if (Property && LatentPinMemory)
			{
				// Init latent memory to default value
				uint8* MutableMemory = const_cast<uint8*>(LatentPinMemory);
				Property->InitializeValue(MutableMemory);
			}
		}
	}
}

void FControlRigTraitSharedData::DestructLatentProperties(const UE::AnimNext::FTraitBinding& Binding)
{
	const FControlRigTraitSharedData* SharedData = Binding.GetSharedData<FControlRigTraitSharedData>();
	if (SharedData && SharedData->ControlRigClass.Get() != nullptr)
	{
		FControlRigVariableMappings::FCustomPropertyMappings PropertyMappings;
		UE::AnimNext::FControlRigTrait::FInstanceData::GetExposedVariablesData(Binding, SharedData, PropertyMappings);
		UE::AnimNext::FControlRigTrait::FInstanceData::GetExposedControlsData(Binding, SharedData, PropertyMappings);

		const TArray<FControlRigVariableMappings::FCustomPropertyData>& Mappings = PropertyMappings.GetMappings();
		const int32 NumProperties = Mappings.Num();

		for (const FControlRigVariableMappings::FCustomPropertyData& Mapping : Mappings)
		{
			const FProperty* Property = Mapping.Property;
			const uint8* LatentPinMemory = Mapping.SourceMemory;
			if (Property && LatentPinMemory)
			{
				uint8* MutableMemory = const_cast<uint8*>(LatentPinMemory);
				Property->DestroyValue(MutableMemory);
			}
		}
	}
}

#if WITH_EDITOR
USkeleton* FControlRigTraitSharedData::GetPreviewSkeleton() const
{
	USkeleton* PreviewSkeleton = ControlRigSkeleton;

	if (PreviewSkeleton == nullptr && ControlRigClass != nullptr)
	{
		// If the user has not provided an explicit skeleton to use, 
		// as AnimNext does not have a preview skeleton, I get the one that was used to generate the rig
		// (note that this might to be valid for some constructions and the user might have to provide the skeleton)
		if (const UControlRigBlueprint* RigVMBlueprint = Cast<UControlRigBlueprint>(ControlRigClass->ClassGeneratedBy))
		{
			if (USkeletalMesh* SkeletalMesh = RigVMBlueprint->GetPreviewMesh())
			{
				PreviewSkeleton = SkeletalMesh->GetSkeleton();
			}
		}
	}

	return PreviewSkeleton;
}
#endif //WITH_EDITOR

namespace UE::AnimNext
{

AUTO_REGISTER_ANIM_TRAIT(FControlRigTrait)

// Trait implementation boilerplate
#define TRAIT_INTERFACE_ENUMERATOR(GeneratorMacro) \
	GeneratorMacro(IEvaluate) \
	GeneratorMacro(IUpdate) \
	GeneratorMacro(IHierarchy) \
	GeneratorMacro(IGarbageCollection) \

ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT(FControlRigTrait)
ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT_GET_INTERFACE(FControlRigTrait, TRAIT_INTERFACE_ENUMERATOR)
ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT_GET_INTERFACES(FControlRigTrait, TRAIT_INTERFACE_ENUMERATOR)
ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT_GET_REQUIRED_INTERFACES(FControlRigTrait, NULL_ANIM_TRAIT_INTERFACE_ENUMERATOR)
ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT_ON_TRAIT_EVENT(FControlRigTrait, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
ANIM_NEXT_IMPL_DEFINE_ANIM_TRAIT_GET_TRAIT_EVENTS(FControlRigTrait, NULL_ANIM_TRAIT_EVENT_ENUMERATOR)
#undef TRAIT_INTERFACE_ENUMERATOR

//----------------------
// --- FInstanceData ---
//----------------------

void FControlRigTrait::FInstanceData::Construct(const FExecutionContext& Context, const FTraitBinding& Binding)
{
	FTrait::FInstanceData::Construct(Context, Binding);

	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

	IGarbageCollection::RegisterWithGC(Context, Binding);

#if WITH_EDITOR
	{
		OnObjectsReinstancedHandle = UE::AnimNext::ControlRig::FAnimNextControlRigModule::OnObjectsReinstanced.AddRaw(this, &FControlRigTrait::FInstanceData::OnObjectsReinstanced);
	}
#endif

	if (UClass* ControlRigClass = GetTargetClass(SharedData))
	{
		if (UObject* AnimContext = GetAnimContext(Context))
		{
			USkeletalMeshComponent* BindableObject = const_cast<USkeletalMeshComponent*>(FInstanceData::GetBindableObject(Context));
			if (ensure(CreateControlRig(GetAnimContext(Context), BindableObject, SharedData->ControlRigClass, this)))
			{
				InitializeControlRig(Context, Binding);
			}
		}
	}

#if UE_ENABLE_DEBUG_DRAWING
	DebugDrawInterface = Context.GetDebugDrawInterface();
#endif
}

void FControlRigTrait::FInstanceData::Destruct(const FExecutionContext& Context, const FTraitBinding& Binding)
{
	FTrait::FInstanceData::Destruct(Context, Binding);

	IGarbageCollection::UnregisterWithGC(Context, Binding);

#if WITH_EDITOR
	UE::AnimNext::ControlRig::FAnimNextControlRigModule::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
#endif

	DestroyControlRig(Context, Binding);
}

void FControlRigTrait::FInstanceData::InitializeControlRig(const FExecutionContext& Context, const FTraitBinding& Binding)
{
	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();

	if (ControlRig)
	{
		// Provide available properties to the construction event
		InitializeCustomProperties(Binding, SharedData);
		ControlRigVariableMappings.PropagateCustomInputProperties(ControlRig);

		ControlRig->Initialize(true);
		ControlRig->RequestInit();

		ControlRigHierarchyMappings.ResetRefPoseSetterHash();

		ControlRigVariableMappings.ResetCurvesInputToControlCache();
		ControlRigHierarchyMappings.ResetRefPoseSetterHash();
		ControlRigVariableMappings.CacheCurveMappings(SharedData->InputMapping, SharedData->OutputMapping, ControlRig->GetHierarchy());

		// Re-init Custom Properties after construction, as new controls could be created and might have to be remapped
		InitializeCustomProperties(Binding, SharedData);
		ControlRigVariableMappings.PropagateCustomInputProperties(ControlRig);

		//UpdateInputOutputMappingIfRequired(ControlRig, InRequiredBones);

		ControlRig->OnInitialized_AnyThread().RemoveAll(this);
		OnInitializedHandle = ControlRig->OnInitialized_AnyThread().AddRaw(this, &FControlRigTrait::FInstanceData::HandleOnInitialized_AnyThread);
	}
}

void FControlRigTrait::FInstanceData::DestroyControlRig(const FExecutionContext& Context, const FTraitBinding& Binding)
{
	if (ControlRig.Get() != nullptr)
	{
		if (OnInitializedHandle.IsValid())
		{
			ControlRig->OnInitialized_AnyThread().Remove(OnInitializedHandle);
			OnInitializedHandle.Reset();
		}
		ControlRig->MarkAsGarbage();
		ControlRig = nullptr;
	}
}

UObject* FControlRigTrait::FInstanceData::GetAnimContext(const FExecutionContext& Context)
{
	UObject* AnimContext = nullptr;

	if (const FAnimNextModuleInstance* ModuleInstance = Context.GetRootGraphInstance().GetModuleInstance())
	{
		AnimContext = ModuleInstance->GetObject();
	}

	return AnimContext;
}

const USkeletalMeshComponent* FControlRigTrait::FInstanceData::GetBindableObject(const FExecutionContext& Context)
{
	const USkeletalMeshComponent* BindableObject = Context.GetBindingObject().Get();
	
	return BindableObject;
}

UClass* FControlRigTrait::FInstanceData::GetTargetClass(const FSharedData* SharedData)
{
	if (SharedData != nullptr)
	{
		return SharedData->ControlRigClass;
	}
	return nullptr;
}

int32 FControlRigTrait::FInstanceData::GetExposedVariablesData(const UE::AnimNext::FTraitBinding& Binding, const FSharedData* SharedData, FControlRigVariableMappings::FCustomPropertyMappings& OutMappings)
{
	int32 NumElementsAdded = 0;

	if (SharedData && SharedData->ControlRigClass.Get() != nullptr && SharedData->ExposedPropertyVariableNames.Num() > 0)
	{
		if (const UControlRig* CDO = SharedData->ControlRigClass->GetDefaultObject<UControlRig>())
		{
			const int32 NumLatentProperties = SharedData->LatentProperties.Num();
			if (ensure(NumLatentProperties == SharedData->LatentPropertyMemoryLayouts.Num()))
			{
				const TArray<FRigVMExternalVariable> PublicVariables = CDO->GetPublicVariables();
				for (int32 LatentPropertyIndex = 0; LatentPropertyIndex < NumLatentProperties; LatentPropertyIndex++)
				{
					const FName& LatentPropertyName = SharedData->LatentProperties[LatentPropertyIndex];

					if (!SharedData->ExposedPropertyVariableNames.Contains(LatentPropertyName))	// Only process exposed public variables
					{
						continue;
					}

					const FRigVMExternalVariable* Variable = PublicVariables.FindByPredicate([&LatentPropertyName](const FRigVMExternalVariable& In)
						{
							return In.Name == LatentPropertyName;
						});

					if (Variable != nullptr)
					{
						uint32 PropertyAlignment = 0;
						uint32 PropertySize = 0;
						UE::AnimNext::FControlRigTrait::GetVariableSizeAndAlignment(*Variable, PropertySize, PropertyAlignment);
						check(PropertyAlignment < MAX_uint16);
						check(PropertySize < MAX_uint16);

						if (ensure(SharedData->LatentPropertyMemoryLayouts[LatentPropertyIndex] == ((PropertySize << 16) | PropertyAlignment)))
						{
							const UE::AnimNext::FLatentPropertyHandle* TraitLatentPropertyHandles = Binding.GetLatentPropertyHandles();

							const UE::AnimNext::FLatentPropertyHandle& LatentPropertyHandle = TraitLatentPropertyHandles[LatentPropertyIndex];
							if (LatentPropertyHandle.IsOffsetValid())
							{
								const uint8* LatentPropertyMemory = const_cast<uint8*>(Binding.GetLatentProperty<uint8>(LatentPropertyHandle));
								OutMappings.AddVariable(Variable->Name, Variable->Memory, Variable->Property, LatentPropertyMemory);
							}
						}
					}
				}
			}
		}
	}

	return NumElementsAdded;
}

int32 FControlRigTrait::FInstanceData::GetExposedControlsData(const UE::AnimNext::FTraitBinding& Binding, const FSharedData* SharedData, FControlRigVariableMappings::FCustomPropertyMappings& OutMappings)
{
	int32 NumElementsAdded = 0;

	if (SharedData && SharedData->ControlRigClass.Get() != nullptr)
	{
		const int32 NumLatentProperties = SharedData->LatentProperties.Num();
		if (ensure(NumLatentProperties == SharedData->LatentPropertyMemoryLayouts.Num()))
		{
			for (int32 LatentPropertyIndex = 0; LatentPropertyIndex < NumLatentProperties; LatentPropertyIndex++)
			{
				const FName& LatentPropertyName = SharedData->LatentProperties[LatentPropertyIndex];

				const int32 ControlIndex = SharedData->ExposedPropertyControlNames.IndexOfByKey(LatentPropertyName);
				if (ControlIndex == INDEX_NONE)	// Only process exposed controls
				{
					continue;
				}

				// Note : I can not check here if the controls exist, as I would have to instantiate the rig
				//        Using the exposed data to fill the information
				if (ensure(SharedData->ExposedPropertyControlNames.Num() == SharedData->ExposedPropertyControlTypes.Num()))
				{
					const FName ControlName = SharedData->ExposedPropertyControlNames[ControlIndex];
					const ERigControlType ControlType = SharedData->ExposedPropertyControlTypes[ControlIndex];

					uint32 PropertyAlignment = 0;
					uint32 PropertySize = 0;
					UE::AnimNext::FControlRigTrait::GetControlSizeAndAlignment(ControlType, PropertySize, PropertyAlignment);
					ensure(PropertyAlignment < MAX_uint16);
					ensure(PropertySize < MAX_uint16);

					if (ensure(SharedData->LatentPropertyMemoryLayouts[LatentPropertyIndex] == ((PropertySize << 16) | PropertyAlignment)))
					{
						const UE::AnimNext::FLatentPropertyHandle* TraitLatentPropertyHandles = Binding.GetLatentPropertyHandles();

						const UE::AnimNext::FLatentPropertyHandle& LatentPropertyHandle = TraitLatentPropertyHandles[LatentPropertyIndex];
						if (LatentPropertyHandle.IsOffsetValid())
						{
							const uint8* LatentPropertyMemory = const_cast<uint8*>(Binding.GetLatentProperty<uint8>(LatentPropertyHandle));
							OutMappings.AddControl(ControlType, ControlName, nullptr, nullptr, LatentPropertyMemory);
						}
					}
				}
			}
		}
	}

	return NumElementsAdded;
}

void FControlRigTrait::FInstanceData::HandleOnInitialized_AnyThread(URigVMHost*, const FName&)
{
	ControlRigHierarchyMappings.ResetRefPoseSetterHash();
	#if WITH_EDITOR
		bRegenerateVariableMappings = true;	// required as FRigVMEditorModule::PreChange (UUSerStructs) recreates VM memory and requests a re-init, which recreates controls
	#endif
}

void FControlRigTrait::FInstanceData::InitializeCustomProperties(const FTraitBinding& Binding, const FSharedData* SharedData)
{
	// Obtain exposed variables and controls data, in order to init the variable mappings
	FControlRigVariableMappings::FCustomPropertyMappings PropertyMappings;
	GetExposedVariablesData(Binding, SharedData, PropertyMappings);
	GetExposedControlsData(Binding, SharedData, PropertyMappings);

	// Setup mappings using the latent pin memory as source (we have to copy from latent pin to external variable / rig control)
	ControlRigVariableMappings.InitializeCustomProperties(ControlRig, PropertyMappings);
}

#if WITH_EDITOR
void FControlRigTrait::FInstanceData::OnObjectsReinstanced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	if (ControlRig)
	{
		for (const TPair<UObject*, UObject*>& Pair : OldToNewInstanceMap)
		{
			const UObject* NewObject = Pair.Value;
			if((NewObject == nullptr) || (NewObject->GetOuter() != ControlRig->GetOuter()) || (!NewObject->IsA<UControlRig>()))
			{
				continue;
			}

			if (NewObject->GetClass() == ControlRig->GetClass())
			{
				bRefreshBindableObject = true;
				bReinitializeControlRig = true;
				break;
			}
		}
	}
}
#endif

//-------------------------
// --- FControlRigTrait ---
//-------------------------
// --- Custom Latent Pin Support impl ---
void FControlRigTrait::SerializeTraitSharedData(FArchive& Ar, FAnimNextTraitSharedData& SharedData) const
{
	FControlRigTraitSharedData& ControlRigSharedData = static_cast<FControlRigTraitSharedData&>(SharedData);

	TraitSuper::SerializeTraitSharedData(Ar, SharedData);

	if (Ar.IsLoading())
	{
		// Compute our latent property data based on our ControlRig class
		if (UClass* ControlRigClass = ControlRigSharedData.ControlRigClass)
		{
			// We build the size/alignment map for each property even if their pin isn't hooked to anything
			// since handles are reserved for every one of them
			const int32 NumLatentProperties = ControlRigSharedData.ExposedPropertyVariableNames.Num() + ControlRigSharedData.ExposedPropertyControlNames.Num();
			ControlRigSharedData.LatentProperties.Reserve(NumLatentProperties);
			ControlRigSharedData.LatentPropertyMemoryLayouts.Reserve(NumLatentProperties);

			if (ControlRigSharedData.ExposedPropertyVariableNames.Num() > 0)
			{
				if (const UControlRig* CDO = ControlRigClass->GetDefaultObject<UControlRig>())
				{
					const TArray<FRigVMExternalVariable> PublicVariables = CDO->GetPublicVariables();
					if (!ensureMsgf(PublicVariables.Num() <= (int32)GetNumLatentTraitProperties(), TEXT("The ControlRig Trait only supports up to %u input variables"), GetNumLatentTraitProperties()))
					{
						return;
					}

					for (const FRigVMExternalVariable& Variable : PublicVariables)
					{
						if (!ControlRigSharedData.ExposedPropertyVariableNames.Contains(Variable.Name))	// Only process exposed public variables
						{
							continue;
						}

						uint32 PropertyAlignment = 0;
						uint32 PropertySize = 0;
						if (GetVariableSizeAndAlignment(Variable, PropertySize, PropertyAlignment))
						{
							check(PropertyAlignment < MAX_uint16);
							check(PropertySize < MAX_uint16);

							ControlRigSharedData.LatentProperties.Add(Variable.Name);
							ControlRigSharedData.LatentPropertyMemoryLayouts.Add((PropertySize << 16) | PropertyAlignment);
						}
					}
				}
			}

			if (ControlRigSharedData.ExposedPropertyControlNames.Num() > 0)
			{
				// Here I can not get the controls list, so I just use the exposed control names and types
				const int32 NumControls = ControlRigSharedData.ExposedPropertyControlNames.Num();
				check(NumControls == ControlRigSharedData.ExposedPropertyControlTypes.Num());

				for (int32 i = 0; i < NumControls; ++i)
				{
					uint32 PropertyAlignment = 0;
					uint32 PropertySize = 0;
					if (GetControlSizeAndAlignment(ControlRigSharedData.ExposedPropertyControlTypes[i], PropertySize, PropertyAlignment))
					{
						check(PropertyAlignment < MAX_uint16);
						check(PropertySize < MAX_uint16);

						ControlRigSharedData.LatentProperties.Add(ControlRigSharedData.ExposedPropertyControlNames[i]);
						ControlRigSharedData.LatentPropertyMemoryLayouts.Add((PropertySize << 16) | PropertyAlignment);
					}
				}
			}
		}
	}
}

uint32 FControlRigTrait::GetNumLatentTraitProperties() const
{
	// Number of latent trait properties must be known ahead of time to reserve space
	// We support a maximum number of input properties, each one will need a 2-byte handle in the shared data for each trait
	return 64;
}

FTraitLatentPropertyMemoryLayout FControlRigTrait::GetLatentPropertyMemoryLayout(const FAnimNextTraitSharedData& SharedData, FName PropertyName, uint32 PropertyIndex) const
{
	const FControlRigTraitSharedData& ControlRigSharedData = static_cast<const FControlRigTraitSharedData&>(SharedData);

	const int32 LatentPropertyIndex = ControlRigSharedData.LatentProperties.IndexOfByKey(PropertyName);
	if (LatentPropertyIndex == INDEX_NONE)
	{
		// This property isn't being tracked, ignore it
		return FTraitLatentPropertyMemoryLayout{ 0, 1 };
	}

	const uint32 PropertyLayout = ControlRigSharedData.LatentPropertyMemoryLayouts[LatentPropertyIndex];
	const uint32 PropertySize = PropertyLayout >> 16;
	const uint32 PropertyAlignment = PropertyLayout & MAX_uint16;

	return FTraitLatentPropertyMemoryLayout{ PropertySize, PropertyAlignment };
}

// --- IUpdate impl --- 
void FControlRigTrait::OnBecomeRelevant(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
{
	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
	check(SharedData);

	FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
	check(InstanceData);

	if (UControlRig* ControlRig = GetControlRig(InstanceData))
	{
		ControlRig->RequestInit();

		InstanceData->bControlRigRequiresInitialization = true;
		InstanceData->LastBonesSerialNumberForCacheBones = 0;
	}

	InstanceData->ControlRigHierarchyMappings.InitializeInstance();
	InstanceData->ControlRigHierarchyMappings.ResetRefPoseSetterHash();

	InstanceData->bUpdateInputOutputMapping = true;
}

void FControlRigTrait::PreUpdate(FUpdateTraversalContext& Context, const TTraitBinding<IUpdate>& Binding, const FTraitUpdateState& TraitState) const
{
	IUpdate::PreUpdate(Context, Binding, TraitState);

	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
	FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

	if (!InstanceData->Input.IsValid())
	{
		InstanceData->Input = Context.AllocateNodeInstance(Binding, SharedData->Input);
	}

#if WITH_EDITOR
	if (InstanceData->bRefreshBindableObject) // AnimNext full recompile Thaw function does not set a binding object
	{
		UObject* AnimContext = InstanceData->GetAnimContext(Context);
		USkeletalMeshComponent* BindableObject = const_cast<USkeletalMeshComponent*>(FInstanceData::GetBindableObject(Context));
		InstanceData->bRefreshBindableObject = !SetBindableObject(InstanceData->ControlRig, AnimContext, BindableObject);
	}
	if (InstanceData->bReinitializeControlRig)
	{
		InstanceData->InitializeControlRig(Context, Binding);
		InstanceData->bReinitializeControlRig = false;
	}
#endif

	if (UControlRig* ControlRig = GetControlRig(InstanceData))
	{
#if WITH_EDITOR
		if (InstanceData->bRegenerateVariableMappings)
		{
			InstanceData->InitializeCustomProperties(Binding, SharedData);
			InstanceData->bRegenerateVariableMappings = false;
		}
#endif

		const float DeltaTime = TraitState.GetDeltaTime();
		ControlRig->SetDeltaTime(DeltaTime);

		InstanceData->ControlRigVariableMappings.PropagateCustomInputProperties(ControlRig);
	}
}

// --- IEvaluate impl --- 
void FControlRigTrait::PostEvaluate(FEvaluateTraversalContext& Context, const TTraitBinding<IEvaluate>& Binding) const
{
	const FSharedData* SharedData = Binding.GetSharedData<FSharedData>();
	FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();

	if (UControlRig* ControlRig = GetControlRig(InstanceData))
	{
		// The transform is used by the task to "transform" the debug drawings inside Control Rig
#if UE_ENABLE_DEBUG_DRAWING
		if (const USkeletalMeshComponent* BindableObject = FInstanceData::GetBindableObject(Context))
		{
			InstanceData->ComponentTransform = BindableObject->GetComponentTransform();
		}
#endif

		Context.AppendTask(FAnimNextControlRigTask::Make(SharedData, InstanceData));
	}
}

// --- IHierarchy impl --- 
uint32 FControlRigTrait::GetNumChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding) const
{
	return 1;
}

void FControlRigTrait::GetChildren(const FExecutionContext& Context, const TTraitBinding<IHierarchy>& Binding, FChildrenArray& Children) const
{
	const FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>();
	Children.Add(InstanceData->Input);
}

// --- IGarbageCollection impl --- 
void FControlRigTrait::AddReferencedObjects(const FExecutionContext& Context, const TTraitBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const
{
	IGarbageCollection::AddReferencedObjects(Context, Binding, Collector);

	if (FInstanceData* InstanceData = Binding.GetInstanceData<FInstanceData>())
	{
		if (InstanceData->ControlRig.Get() != nullptr)
		{
			Collector.AddReferencedObject(InstanceData->ControlRig);
		}
	}
}

// --- Utility --- 

UControlRig* FControlRigTrait::GetControlRig(FInstanceData* InstanceData)
{
	if (InstanceData)
	{
		return InstanceData->ControlRig.Get();
	}

	return nullptr;
}

bool FControlRigTrait::CreateControlRig(UObject* InAnimContext, UObject* InBindableObject, UClass* InControlRigClass, FInstanceData* InstanceData)
{
	if (InstanceData->ControlRig == nullptr && InControlRigClass != nullptr)
	{
		// Let's make sure the GC isn't running when we try to create a new Control Rig.
		{
			FGCScopeGuard GCGuard;
			InstanceData->ControlRig = NewObject<UControlRig>(InAnimContext, InControlRigClass);
			InstanceData->ControlRig->AtomicallyClearInternalFlags(EInternalObjectFlags::Async);
		}

		const bool bSuccess = SetBindableObject(InstanceData->ControlRig, InAnimContext, InBindableObject);
#if WITH_EDITOR
		InstanceData->bRefreshBindableObject = !bSuccess;
#endif
	}

	return InstanceData->ControlRig != nullptr;
}


bool FControlRigTrait::SetBindableObject(UControlRig* ControlRig, UObject* InAnimContext, UObject* InBindableObject)
{
	if (ControlRig != nullptr && ensure(InAnimContext) && InBindableObject)
	{
		ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());

		UObject* ObjectToBind = InBindableObject != nullptr ? InBindableObject : FControlRigObjectBinding::GetBindableObject(InAnimContext);
		check(ObjectToBind != nullptr);

		ControlRig->GetObjectBinding()->BindToObject(ObjectToBind);

		// register bindable object as data source (used for To World / From World transformations)
		ControlRig->GetDataSourceRegistry()->RegisterDataSource(UControlRig::OwnerComponent, ObjectToBind);

		return true;
	}
	return false;
}

#if WITH_EDITOR
void FControlRigTrait::GetProgrammaticPins(FAnimNextTraitSharedData* InSharedData, URigVMController* InController, int32 InParentPinIndex, const URigVMPin* InTraitPin, const FString& InDefaultValue, struct FRigVMPinInfoArray& OutPinArray) const
{
	if (FControlRigTraitSharedData* SharedData = (FControlRigTraitSharedData*)InSharedData)
	{
		if (SharedData->ControlRigClass != nullptr)
		{
			// --- Exposed Public Variables ---
			if (SharedData->ExposedPropertyVariableNames.Num() > 0)
			{
				if (const UControlRig* CDO = SharedData->ControlRigClass->GetDefaultObject<UControlRig>())
				{
					const TArray<FRigVMExternalVariable> PublicVariables = CDO->GetPublicVariables();

					for (const FRigVMExternalVariable& Variable : PublicVariables)
					{
						if (!SharedData->ExposedPropertyVariableNames.Contains(Variable.Name))	// Only process exposed public variables
						{
							continue;
						}

						if (Variable.Memory == nullptr) // if we make a variable public but don't recompile, we have to skip, as it comes without memory
						{
							continue;
						}

						if (const URigVMPin* SubPin = InTraitPin->FindSubPin(Variable.Name.ToString()))
						{
							const FString PinDefaultValue = SubPin->GetDefaultValue();
							FRigVMMemoryStorageStruct StorageDefaultValue(ERigVMMemoryType::External, {FRigVMPropertyDescription(Variable.Property, PinDefaultValue, Variable.Name)});

							const uint8* DefaultMemory = StorageDefaultValue.GetDataByName<const uint8>(Variable.Name);
							
							OutPinArray.AddPin(const_cast<FProperty*>(Variable.Property), InController, ERigVMPinDirection::Input, InParentPinIndex, ERigVMPinDefaultValueType::AutoDetect, DefaultMemory, true);
						}
						else
						{
							OutPinArray.AddPin(const_cast<FProperty*>(Variable.Property), InController, ERigVMPinDirection::Input, InParentPinIndex, ERigVMPinDefaultValueType::AutoDetect, Variable.Memory, true);
						}
					}
				}
			}

			// --- Exposed Controls ---
			const int32 NumExposedControls = SharedData->ExposedPropertyControlNames.Num();
			if (NumExposedControls > 0)
			{
				if (NumExposedControls == SharedData->ExposedPropertyControlDefaultValues.Num() && NumExposedControls == SharedData->ExposedPropertyControlTypes.Num())
				{
					for (int32 ControlIndex = 0; ControlIndex < NumExposedControls; ControlIndex++)
					{
						const FName& ControlName = SharedData->ExposedPropertyControlNames[ControlIndex];
						const FString& ControlDefaultValue = SharedData->ExposedPropertyControlDefaultValues[ControlIndex];
						const TRigVMTypeIndex TypeIndex = RigVMTypeUtils::TypeIndexFromPinType(URigHierarchy::GetControlPinType(SharedData->ExposedPropertyControlTypes[ControlIndex]));

						if (ensure(TypeIndex != INDEX_NONE))
						{
							OutPinArray.AddPin(InController, InParentPinIndex, ControlName, ERigVMPinDirection::Input, TypeIndex, ControlDefaultValue, ERigVMPinDefaultValueType::AutoDetect, nullptr, nullptr, true);
						}
					}
				}
				else
				{
					if (USkeleton* PreviewSkeleton = SharedData->GetPreviewSkeleton())
					{
						// Obtain the controls from the RigControlsData helper. This will instantiate a rig using the provided class and cache the controls until the class changes
						const TArray<FControlRigIOMapping::FControlsInfo>& RigControls = RigControlsData.GetControls(SharedData->ControlRigClass, PreviewSkeleton);

						for (const FControlRigIOMapping::FControlsInfo& Control : RigControls)
						{
							const FName& ControlName = Control.Name;
							if (!SharedData->ExposedPropertyControlNames.Contains(ControlName))	// Only process exposed controls
							{
								continue;
							}

							const FString& ControlDefaultValue = Control.DefaultValue;
							const TRigVMTypeIndex TypeIndex = RigVMTypeUtils::TypeIndexFromPinType(Control.PinType);
							if (ensure(TypeIndex != INDEX_NONE))
							{
								OutPinArray.AddPin(InController, InParentPinIndex, ControlName, ERigVMPinDirection::Input, TypeIndex, ControlDefaultValue, ERigVMPinDefaultValueType::AutoDetect, nullptr, nullptr, true);
							}
						}
					}
				}
			}
		}
	}
}

bool FControlRigTrait::IsPropertyLatent(const FAnimNextTraitSharedData& InSharedData, FName PropertyName) const
{
	const FControlRigTraitSharedData& SharedData = (const FControlRigTraitSharedData&)InSharedData;

	// --- Exposed Public Variables ---
	if (SharedData.ExposedPropertyVariableNames.Num() > 0)
	{
		if (SharedData.ExposedPropertyVariableNames.Contains(PropertyName))
		{
			return true;
		}
	}

	// --- Exposed Controls ---
	if (SharedData.ExposedPropertyControlNames.Num() > 0)
	{
		if (SharedData.ExposedPropertyControlNames.Contains(PropertyName))
		{
			return true;
		}
	}

	return false;
}

uint32 FControlRigTrait::GetLatentPropertyHandles(
	const FAnimNextTraitSharedData* InSharedData,
	TArray<FLatentPropertyMetadata>& OutLatentPropertyHandles,
	bool bFilterEditorOnly,
	const TFunction<uint16(FName PropertyName)>& GetTraitLatentPropertyIndex) const
{
	// Get shared data latent properties
	uint32 NumLatentPinsAdded = Super::GetLatentPropertyHandles(InSharedData, OutLatentPropertyHandles, bFilterEditorOnly, GetTraitLatentPropertyIndex);

	// Generate Control Rig exposed pins
	if (FControlRigTraitSharedData* SharedData = (FControlRigTraitSharedData*)InSharedData)
	{
		if (SharedData->ControlRigClass != nullptr)
		{
			// --- Iterate over public variables --- 
			if (SharedData->ExposedPropertyVariableNames.Num() > 0)
			{
				if (const UControlRig* CDO = SharedData->ControlRigClass->GetDefaultObject<UControlRig>())
				{
					const TArray<FRigVMExternalVariable> PublicVariables = CDO->GetPublicVariables();

					for (const FRigVMExternalVariable& Variable : PublicVariables)
					{
						if (!SharedData->ExposedPropertyVariableNames.Contains(Variable.Name))	// Only process exposed public variables
						{
							continue;
						}

						const FProperty* Property = Variable.Property;

						FLatentPropertyMetadata Metadata;
						Metadata.Name = Property->GetFName();
						Metadata.RigVMIndex = GetTraitLatentPropertyIndex(Property->GetFName());

						// Always false for now, we don't support freezing yet
						Metadata.bCanFreeze = false;

						OutLatentPropertyHandles.Add(Metadata);
						NumLatentPinsAdded++;
					}
				}
			}

			// --- Iterate over exposed controls ---
			if (SharedData->ExposedPropertyControlNames.Num() > 0)
			{
				if (USkeleton* PreviewSkeleton = SharedData->GetPreviewSkeleton())
				{
					// Obtain the controls from the RigControlsData helper. This will instantiate a rig using the provided class and cache the controls until the class changes
					const TArray<FControlRigIOMapping::FControlsInfo>& RigControls = RigControlsData.GetControls(SharedData->ControlRigClass, PreviewSkeleton);

					for (const FControlRigIOMapping::FControlsInfo& Control : RigControls)
					{
						const FName& ControlName = Control.Name;
						if (!SharedData->ExposedPropertyControlNames.Contains(ControlName))	// Only process exposed controls
						{
							continue;
						}

						FLatentPropertyMetadata Metadata;
						Metadata.Name = ControlName;
						Metadata.RigVMIndex = GetTraitLatentPropertyIndex(ControlName);

						// Always false for now, we don't support freezing yet
						Metadata.bCanFreeze = false;

						OutLatentPropertyHandles.Add(Metadata);
						NumLatentPinsAdded++;
					}
				}
			}
		}
	}

	return NumLatentPinsAdded;
}

#endif // WITH_EDITOR

bool FControlRigTrait::GetVariableSizeAndAlignment(const FRigVMExternalVariable& Variable, uint32& PropertySize, uint32& PropertyAlignment)
{
	bool bValidType = false;

	if (Variable.TypeObject != nullptr)
	{
		if (const UScriptStruct* Struct = Cast<UScriptStruct>(Variable.TypeObject))
		{
			PropertySize = Struct->GetStructureSize();
			PropertyAlignment = Struct->GetMinAlignment();
			bValidType = true;
		}
		else if (const UEnum* Enum = Cast<UEnum>(Variable.TypeObject))
		{
			PropertySize = Variable.Property->GetSize();
			PropertyAlignment = Variable.Property->GetMinAlignment();
			bValidType = true;
		}
		else if (const UClass* Class = Cast<UClass>(Variable.TypeObject))
		{
			PropertySize = Class->GetStructureSize();
			PropertyAlignment = Class->GetMinAlignment();
			bValidType = true;
		}
		else
		{
			ensureMsgf(false, TEXT("Unsupported ControlRig public variable type: %s"), *Variable.TypeName.ToString());
			PropertySize = Variable.Property->GetSize();
			PropertyAlignment = Variable.Property->GetMinAlignment();
		}
	}
	else
	{
		PropertySize = Variable.Property->GetSize();
		PropertyAlignment = Variable.Property->GetMinAlignment();
		bValidType = true;
	}

	return bValidType;
}

bool FControlRigTrait::GetControlSizeAndAlignment(ERigControlType ControlType, uint32& PropertySize, uint32& PropertyAlignment)
{
	switch (ControlType)
	{
		case ERigControlType::Bool:
		{
			PropertySize = sizeof(bool);
			PropertyAlignment = PropertySize;
			return true;
		}
		case ERigControlType::Float:
		case ERigControlType::ScaleFloat:
		{
			PropertySize = sizeof(float);
			PropertyAlignment = PropertySize;
			return true;
		}
		case ERigControlType::Integer:
		{
			PropertySize = sizeof(int32);
			PropertyAlignment = PropertySize;
			return true;
		}
		case ERigControlType::Vector2D:
		{
			if (const UScriptStruct* Struct = TBaseStructure<FVector2D>::Get())
			{
				PropertySize = Struct->GetStructureSize();
				PropertyAlignment = Struct->GetMinAlignment();
				return true;
			}
		}
		case ERigControlType::Position:
		case ERigControlType::Scale:
		{
			PropertySize = sizeof(FVector3f);
			PropertyAlignment = PropertySize;
			return true;
		}
		case ERigControlType::Rotator:
		{
			if (const UScriptStruct* Struct = TBaseStructure<FRotator>::Get())
			{
				PropertySize = Struct->GetStructureSize();
				PropertyAlignment = Struct->GetMinAlignment();
				return true;
			}
			break;
		}
		case ERigControlType::Transform:
		case ERigControlType::TransformNoScale:
		case ERigControlType::EulerTransform:
		{
			if (const UScriptStruct* Struct = TBaseStructure<FTransform>::Get())
			{
				PropertySize = Struct->GetStructureSize();
				PropertyAlignment = Struct->GetMinAlignment();
				return true;
			}
			break;
		}
		default:
		{
			ensureMsgf(false, TEXT("Unsupported ControlRig control type."));
			break;
		}
	}
	return false;
}

} // end namespace UE::AnimNext
