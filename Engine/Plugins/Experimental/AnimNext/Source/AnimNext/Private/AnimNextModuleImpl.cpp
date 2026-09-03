// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextModuleImpl.h"
#include "AnimNextConfig.h"
#include "DataRegistry.h"
#include "IUniversalObjectLocatorModule.h"
#include "RigVMRuntimeDataRegistry.h"
#include "UniversalObjectLocator.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendProfile.h"
#include "Component/AnimNextComponent.h"
#include "Curves/CurveFloat.h"
#include "Module/AnimNextModuleInstance.h"
#include "Module/ModuleHandle.h"
#include "Modules/ModuleManager.h"
#include "Param/AnimNextActorLocatorFragment.h"
#include "Param/AnimNextComponentLocatorFragment.h"
#include "Param/AnimNextObjectCastLocatorFragment.h"
#include "Param/AnimNextObjectFunctionLocatorFragment.h"
#include "Param/AnimNextObjectPropertyLocatorFragment.h"
#include "Param/AnimNextTag.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Variables/AnimNextFieldPath.h"
#include "Variables/AnimNextSoftFunctionPtr.h"
#include "RigVMCore/RigVMGraphFunctionDefinition.h"
#include "HierarchyTable.h"
#include "Features/IModularFeatures.h"
#include "RewindDebuggerRuntimeInterface/IRewindDebuggerRuntimeExtension.h"
#include "RewindDebugger/RewindDebuggerAnimNextRuntime.h"

#define LOCTEXT_NAMESPACE "AnimNextModule"

namespace UE::AnimNext
{
#if ANIMNEXT_TRACE_ENABLED
	FRewindDebuggerAnimNextRuntime GRewindDebuggerAnimNextRuntime;
#endif
	
	void FAnimNextModuleImpl::StartupModule()
	{
		GetMutableDefault<UAnimNextConfig>()->LoadConfig();

		FRigVMRegistry& RigVMRegistry = FRigVMRegistry::Get();
		static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
		{
			{ UAnimSequence::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UBlendSpace::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UScriptStruct::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UBlendProfile::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UCurveFloat::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UAnimNextComponent::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UAnimNextModule::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
			{ UHierarchyTable::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		};

		RigVMRegistry.RegisterObjectTypes(AllowedObjectTypes);

		static UScriptStruct* const AllowedStructTypes[] =
		{
			FAnimNextScope::StaticStruct(),
			FAnimNextEntryPoint::StaticStruct(),
			FUniversalObjectLocator::StaticStruct(),
			FAnimNextFieldPath::StaticStruct(),
			FAnimNextSoftFunctionPtr::StaticStruct(),
			FRigVMGraphFunctionHeader::StaticStruct(),
			TBaseStructure<FGuid>::Get(),
			FRigVMVariant::StaticStruct(),
			FAnimNextModuleHandle::StaticStruct(),
		};

		RigVMRegistry.RegisterStructTypes(AllowedStructTypes);

		FDataRegistry::Init();

		FRigVMRuntimeDataRegistry::Init();

#if ANIMNEXT_TRACE_ENABLED
		IModularFeatures::Get().RegisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, &GRewindDebuggerAnimNextRuntime);
#endif

		UE::UniversalObjectLocator::IUniversalObjectLocatorModule& UolModule = FModuleManager::Get().LoadModuleChecked<UE::UniversalObjectLocator::IUniversalObjectLocatorModule>("UniversalObjectLocator");
		FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase::ObjectSystemReady,
			[&UolModule]
			{
				{
					UE::UniversalObjectLocator::FFragmentTypeParameters FragmentTypeParams("animobjfunc", LOCTEXT("UAFObjectFunctionFragment", "Function"));
					FragmentTypeParams.PrimaryEditorType = "AnimNextObjectFunction";
					FAnimNextObjectFunctionLocatorFragment::FragmentType = UolModule.RegisterFragmentType<FAnimNextObjectFunctionLocatorFragment>(FragmentTypeParams);
				}
				{
					UE::UniversalObjectLocator::FFragmentTypeParameters FragmentTypeParams("animobjprop", LOCTEXT("UAFObjectPropertyFragment", "Property"));
					FragmentTypeParams.PrimaryEditorType = "AnimNextObjectProperty";
					FAnimNextObjectPropertyLocatorFragment::FragmentType = UolModule.RegisterFragmentType<FAnimNextObjectPropertyLocatorFragment>(FragmentTypeParams);
				}
				{
					UE::UniversalObjectLocator::FFragmentTypeParameters FragmentTypeParams("animobjcast", LOCTEXT("UAFCastFragment", "Cast"));
					FragmentTypeParams.PrimaryEditorType = "AnimNextObjectCast";
					FAnimNextObjectCastLocatorFragment::FragmentType = UolModule.RegisterFragmentType<FAnimNextObjectCastLocatorFragment>(FragmentTypeParams);
				}
				{
					UE::UniversalObjectLocator::FFragmentTypeParameters FragmentTypeParams("animcomp", LOCTEXT("UAFComponentFragment", "UAFComponent"));
					FragmentTypeParams.PrimaryEditorType = "AnimNextComponent";
					FAnimNextComponentLocatorFragment::FragmentType = UolModule.RegisterFragmentType<FAnimNextComponentLocatorFragment>(FragmentTypeParams);
				}
				{
					UE::UniversalObjectLocator::FFragmentTypeParameters FragmentTypeParams("animactor", LOCTEXT("UAFActorFragment", "UAFActor"));
					FragmentTypeParams.PrimaryEditorType = "AnimNextActor";
					FAnimNextActorLocatorFragment::FragmentType = UolModule.RegisterFragmentType<FAnimNextActorLocatorFragment>(FragmentTypeParams);
				}
			});

	}

	void FAnimNextModuleImpl::ShutdownModule()
	{
#if ANIMNEXT_TRACE_ENABLED
		IModularFeatures::Get().UnregisterModularFeature(IRewindDebuggerRuntimeExtension::ModularFeatureName, &GRewindDebuggerAnimNextRuntime);
#endif
		
		FRigVMRuntimeDataRegistry::Destroy();
		FDataRegistry::Destroy();
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(UE::AnimNext::FAnimNextModuleImpl, AnimNext)
