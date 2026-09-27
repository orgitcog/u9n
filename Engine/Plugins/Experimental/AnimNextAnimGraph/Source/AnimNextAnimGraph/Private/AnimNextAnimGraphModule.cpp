// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphModule.h"
#include "AnimNextAnimGraphSettings.h"
#include "Animation/BlendProfile.h"
#include "Module/AnimNextModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "RigVMCore/RigVMRegistry.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "TraitInterfaces/IUpdate.h"
#include "Graph/AnimNextAnimationGraph.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/RigVMTrait_AnimNextPublicVariables.h"
#include "Injection/InjectionRequest.h"
#include "TraitCore/NodeTemplateRegistry.h"
#include "TraitCore/TraitInterfaceRegistry.h"
#include "TraitCore/TraitRegistry.h"
#include "Traits/ModifyCurveTrait.h"

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

#include "TraitCore/NodeDescription.h"
#include "TraitCore/NodeTemplate.h"
#include "TraitCore/TraitTemplate.h"
#endif

namespace UE::AnimNext::AnimGraph
{

// Delay loading of default asset factory mappings in settings 
static FDelayedAutoRegisterHelper AutoRegisterFactoryMappings(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UAnimNextAnimGraphSettings* Settings = GetMutableDefault<UAnimNextAnimGraphSettings>();

#if WITH_EDITOR
	// Async load in the editor to avoid stalling startup
	const bool bLoadAsync = true;
#else
	// Outside the editor, we assume we are behind a loading screen of sorts
	const bool bLoadAsync = false;
#endif

	Settings->LoadAndRebuildMappings(bLoadAsync);
});

void FAnimNextAnimGraphModule::StartupModule()
{
	// Ensure that AnimNext modules are loaded so we can correctly load plugin content
	FModuleManager::LoadModuleChecked<IModuleInterface>("AnimNext");
#if WITH_EDITORONLY_DATA
	FModuleManager::LoadModuleChecked<IModuleInterface>("AnimNextUncookedOnly");
#endif

	// Setup default settings/factories
	{
		UAnimNextAnimGraphSettings* Settings = GetMutableDefault<UAnimNextAnimGraphSettings>();
		Settings->LoadConfig();
	}

	FRigVMRegistry& RigVMRegistry = FRigVMRegistry::Get();
	static TPair<UClass*, FRigVMRegistry::ERegisterObjectOperation> const AllowedObjectTypes[] =
	{
		{ UAnimNextAnimationGraph::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::Class },
		{ USkeletalMeshComponent::StaticClass(), FRigVMRegistry::ERegisterObjectOperation::ClassAndParents },
	};

	RigVMRegistry.RegisterObjectTypes(AllowedObjectTypes);
	
	static UScriptStruct* const AllowedStructTypes[] =
	{
		FAnimNextGraphReferencePose::StaticStruct(),
		FRigVMTrait_AnimNextPublicVariables::StaticStruct(),
		FAnimNextInjectionBlendSettings::StaticStruct(),
		FModifyCurveParameters::StaticStruct()
	};

	RigVMRegistry.RegisterStructTypes(AllowedStructTypes);

	FTraitRegistry::Init();
	FTraitInterfaceRegistry::Init();
	FNodeTemplateRegistry::Init();

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	if (!IsRunningCommandlet())
	{
		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("UAF.ListNodeTemplates"),
			TEXT("Dumps statistics about node templates to the log."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAnimNextAnimGraphModule::ListNodeTemplates),
			ECVF_Default
		));
		ConsoleCommands.Add(IConsoleManager::Get().RegisterConsoleCommand(
			TEXT("UAF.Module"),
			TEXT("Dumps statistics about modules to the log."),
			FConsoleCommandWithArgsDelegate::CreateRaw(this, &FAnimNextAnimGraphModule::ListAnimationGraphs),
			ECVF_Default
		));
	}
#endif
}

void FAnimNextAnimGraphModule::ShutdownModule()
{
#if WITH_ANIMNEXT_CONSOLE_COMMANDS
	for (IConsoleObject* Cmd : ConsoleCommands)
	{
		IConsoleManager::Get().UnregisterConsoleObject(Cmd);
	}
	ConsoleCommands.Empty();
#endif

	FNodeTemplateRegistry::Destroy();
	FTraitInterfaceRegistry::Destroy();
	FTraitRegistry::Destroy();

	LoadedGraphs.Reset();
}

void FAnimNextAnimGraphModule::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(LoadedGraphs);
}

FString FAnimNextAnimGraphModule::GetReferencerName() const
{
	return TEXT("AnimNextAnimGraphModule");
}

#if WITH_ANIMNEXT_CONSOLE_COMMANDS
void FAnimNextAnimGraphModule::ListNodeTemplates(const TArray<FString>& Args)
{
	// Turn off log times to make diff-ing easier
	TGuardValue DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	// Make sure to log everything
	const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
	LogAnimation.SetVerbosity(ELogVerbosity::All);

	const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
	const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();

	UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Node Templates ====="));
	UE_LOG(LogAnimation, Log, TEXT("Template Buffer Size: %u bytes"), NodeTemplateRegistry.TemplateBuffer.GetAllocatedSize());

	for (auto It = NodeTemplateRegistry.TemplateUIDToHandleMap.CreateConstIterator(); It; ++It)
	{
		const FNodeTemplateRegistryHandle Handle = It.Value();
		const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(Handle);

		const uint32 NumTraits = NodeTemplate->GetNumTraits();

		UE_LOG(LogAnimation, Log, TEXT("[%x] has %u traits ..."), NodeTemplate->GetUID(), NumTraits);
		UE_LOG(LogAnimation, Log, TEXT("    Template Size: %u bytes"), NodeTemplate->GetNodeTemplateSize());
		UE_LOG(LogAnimation, Log, TEXT("    Shared Data Size: %u bytes"), NodeTemplate->GetNodeSharedDataSize());
		UE_LOG(LogAnimation, Log, TEXT("    Instance Data Size: %u bytes"), NodeTemplate->GetNodeInstanceDataSize());
		UE_LOG(LogAnimation, Log, TEXT("    Traits ..."));

		const FTraitTemplate* TraitTemplates = NodeTemplate->GetTraits();
		for (uint32 TraitIndex = 0; TraitIndex < NumTraits; ++TraitIndex)
		{
			const FTraitTemplate* TraitTemplate = TraitTemplates + TraitIndex;
			const FTrait* Trait = TraitRegistry.Find(TraitTemplate->GetRegistryHandle());
			const FString TraitName = Trait != nullptr ? Trait->GetTraitName() : TEXT("<Unknown>");

			const uint32 NextTraitIndex = TraitIndex + 1;
			const uint32 EndOfNextTraitSharedData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
			const uint32 TraitSharedDataSize = EndOfNextTraitSharedData - TraitTemplate->GetNodeSharedOffset();

			const uint32 EndOfNextTraitInstanceData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
			const uint32 TraitInstanceDataSize = EndOfNextTraitInstanceData - TraitTemplate->GetNodeInstanceOffset();

			UE_LOG(LogAnimation, Log, TEXT("            %u: [%x] %s (%s)"), TraitIndex, TraitTemplate->GetUID().GetUID(), *TraitName, TraitTemplate->GetMode() == ETraitMode::Base ? TEXT("Base") : TEXT("Additive"));
			UE_LOG(LogAnimation, Log, TEXT("                Shared Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeSharedOffset(), TraitSharedDataSize);
			if (TraitTemplate->HasLatentProperties() && Trait != nullptr)
			{
				UE_LOG(LogAnimation, Log, TEXT("                Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), TraitTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Trait->GetNumLatentTraitProperties());
			}
			UE_LOG(LogAnimation, Log, TEXT("                Instance Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeInstanceOffset(), TraitInstanceDataSize);
		}
	}

	LogAnimation.SetVerbosity(OldVerbosity);
}

void FAnimNextAnimGraphModule::ListAnimationGraphs(const TArray<FString>& Args)
{
	// Turn off log times to make diff-ing easier
	TGuardValue DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	// Make sure to log everything
	const ELogVerbosity::Type OldVerbosity = LogAnimation.GetVerbosity();
	LogAnimation.SetVerbosity(ELogVerbosity::All);

	TArray<const UAnimNextAnimationGraph*> AnimationGraphs;

	for (TObjectIterator<UAnimNextAnimationGraph> It; It; ++It)
	{
		AnimationGraphs.Add(*It);
	}

	struct FCompareObjectNames
	{
		FORCEINLINE bool operator()(const UAnimNextAnimationGraph& Lhs, const UAnimNextAnimationGraph& Rhs) const
		{
			return Lhs.GetPathName().Compare(Rhs.GetPathName()) < 0;
		}
	};
	AnimationGraphs.Sort(FCompareObjectNames());

	const FNodeTemplateRegistry& NodeTemplateRegistry = FNodeTemplateRegistry::Get();
	const FTraitRegistry& TraitRegistry = FTraitRegistry::Get();
	const bool bDetailedOutput = true;

	UE_LOG(LogAnimation, Log, TEXT("===== AnimNext Modules ====="));
	UE_LOG(LogAnimation, Log, TEXT("Num Graphs: %u"), AnimationGraphs.Num());

	for (const UAnimNextAnimationGraph* AnimationGraph : AnimationGraphs)
	{
		uint32 TotalInstanceSize = 0;
		uint32 NumNodes = 0;
		{
			// We always have a node at offset 0
			int32 NodeOffset = 0;

			while (NodeOffset < AnimationGraph->SharedDataBuffer.Num())
			{
				const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimationGraph->SharedDataBuffer[NodeOffset]);

				TotalInstanceSize += NodeDesc->GetNodeInstanceDataSize();
				NumNodes++;

				const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());
				NodeOffset += NodeTemplate->GetNodeSharedDataSize();
			}
		}

		UE_LOG(LogAnimation, Log, TEXT("    %s ..."), *AnimationGraph->GetPathName());
		UE_LOG(LogAnimation, Log, TEXT("        Shared Data Size: %.2f KB"), double(AnimationGraph->SharedDataBuffer.Num()) / 1024.0);
		UE_LOG(LogAnimation, Log, TEXT("        Max Instance Data Size: %.2f KB"), double(TotalInstanceSize) / 1024.0);
		UE_LOG(LogAnimation, Log, TEXT("        Num Nodes: %u"), NumNodes);

		if (bDetailedOutput)
		{
			// We always have a node at offset 0
			int32 NodeOffset = 0;

			while (NodeOffset < AnimationGraph->SharedDataBuffer.Num())
			{
				const FNodeDescription* NodeDesc = reinterpret_cast<const FNodeDescription*>(&AnimationGraph->SharedDataBuffer[NodeOffset]);
				const FNodeTemplate* NodeTemplate = NodeTemplateRegistry.Find(NodeDesc->GetTemplateHandle());

				const uint32 NumTraits = NodeTemplate->GetNumTraits();

				UE_LOG(LogAnimation, Log, TEXT("        Node %u: [Template %x with %u traits]"), NodeDesc->GetUID().GetNodeIndex(), NodeTemplate->GetUID(), NumTraits);
				UE_LOG(LogAnimation, Log, TEXT("            Shared Data: [Offset: %u bytes, Size: %u bytes]"), NodeOffset, NodeTemplate->GetNodeSharedDataSize());
				UE_LOG(LogAnimation, Log, TEXT("            Instance Data Size: %u bytes"), NodeDesc->GetNodeInstanceDataSize());
				UE_LOG(LogAnimation, Log, TEXT("            Traits ..."));

				const FTraitTemplate* TraitTemplates = NodeTemplate->GetTraits();
				for (uint32 TraitIndex = 0; TraitIndex < NumTraits; ++TraitIndex)
				{
					const FTraitTemplate* TraitTemplate = TraitTemplates + TraitIndex;
					const FTrait* Trait = TraitRegistry.Find(TraitTemplate->GetRegistryHandle());
					const FString TraitName = Trait != nullptr ? Trait->GetTraitName() : TEXT("<Unknown>");

					const uint32 NextTraitIndex = TraitIndex + 1;
					const uint32 EndOfNextTraitSharedData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeSharedOffset() : NodeTemplate->GetNodeSharedDataSize();
					const uint32 TraitSharedDataSize = EndOfNextTraitSharedData - TraitTemplate->GetNodeSharedOffset();

					const uint32 EndOfNextTraitInstanceData = NextTraitIndex < NumTraits ? TraitTemplates[NextTraitIndex].GetNodeInstanceOffset() : NodeTemplate->GetNodeInstanceDataSize();
					const uint32 TraitInstanceDataSize = EndOfNextTraitInstanceData - TraitTemplate->GetNodeInstanceOffset();

					UE_LOG(LogAnimation, Log, TEXT("                    %u: [%x] %s (%s)"), TraitIndex, TraitTemplate->GetUID().GetUID(), *TraitName, TraitTemplate->GetMode() == ETraitMode::Base ? TEXT("Base") : TEXT("Additive"));
					UE_LOG(LogAnimation, Log, TEXT("                        Shared Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeSharedOffset(), TraitSharedDataSize);
					if (TraitTemplate->HasLatentProperties() && Trait != nullptr)
					{
						UE_LOG(LogAnimation, Log, TEXT("                        Shared Data Latent Property Handles: [Offset: %u bytes, Count: %u]"), TraitTemplate->GetNodeSharedLatentPropertyHandlesOffset(), Trait->GetNumLatentTraitProperties());
					}
					UE_LOG(LogAnimation, Log, TEXT("                        Instance Data: [Offset: %u bytes, Size: %u bytes]"), TraitTemplate->GetNodeInstanceOffset(), TraitInstanceDataSize);
				}

				NodeOffset += NodeTemplate->GetNodeSharedDataSize();
			}
		}
	}

	LogAnimation.SetVerbosity(OldVerbosity);
}
#endif

}

IMPLEMENT_MODULE(UE::AnimNext::AnimGraph::FAnimNextAnimGraphModule, AnimNextAnimGraph)
