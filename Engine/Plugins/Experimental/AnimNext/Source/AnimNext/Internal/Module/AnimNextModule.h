// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterface/AnimNextDataInterface.h"
#include "RigVMCore/RigVM.h"
#include "RigVMHost.h"
#include "Graph/AnimNextGraphState.h"
#include "Module/RigVMTrait_ModuleEventDependency.h"

#include "AnimNextModule.generated.h"

#define UE_API ANIMNEXT_API

class UEdGraph;
class UAnimNextModule;
class UAnimGraphNode_AnimNextGraph;
struct FAnimNode_AnimNextGraph;
struct FRigUnit_AnimNextGraphEvaluator;
struct FAnimNextGraphInstance;
struct FAnimNextScheduleGraphTask;
struct FAnimNextEditorParam;
struct FAnimNextParam;

namespace UE::AnimNext
{
	struct FContext;
	struct FExecutionContext;
	class FAnimNextModuleImpl;
	struct FTestUtils;
	struct FParametersProxy;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
}

namespace UE::AnimNext::Editor
{
	class FModuleEditor;
	class FVariableCustomization;
}

// Root asset represented by a component when instantiated
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextModule : public UAnimNextDataInterface
{
	GENERATED_BODY()

public:
	UE_API UAnimNextModule(const FObjectInitializer& ObjectInitializer);

	// UObject interface
	UE_API virtual void Serialize(FArchive& Ar) override;
	UE_API virtual void PostLoad() override;

#if WITH_EDITORONLY_DATA
	// Delegate called in editor when a module is compiled
	using FOnModuleCompiled = TTSMulticastDelegate<void(UAnimNextModule*)>;
	static UE_API FOnModuleCompiled& OnModuleCompiled();
#endif

protected:
	friend class UAnimNextModuleFactory;
	friend class UAnimNextModule_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend class UE::AnimNext::Editor::FModuleEditor;
	friend struct UE::AnimNext::FTestUtils;
	friend FAnimNextGraphInstance;
	friend class UAnimGraphNode_AnimNextGraph;
	friend UE::AnimNext::FExecutionContext;
	friend struct FAnimNextScheduleGraphTask;
	friend UE::AnimNext::FAnimNextModuleImpl;
	friend class UE::AnimNext::Editor::FVariableCustomization;
	friend struct UE::AnimNext::FParametersProxy;
	friend FAnimNextModuleInstance;

	// All components that are required on startup for this module
	UPROPERTY()
	TArray<TObjectPtr<UScriptStruct>> RequiredComponents;

	// All dependencies that should be set up when the module initializes
	UPROPERTY()
	TArray<TInstancedStruct<FRigVMTrait_ModuleEventDependency>> Dependencies;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FAnimNextGraphState DefaultState_DEPRECATED;
	
	UPROPERTY()
	FInstancedPropertyBag PropertyBag_DEPRECATED;
#endif
};

#undef UE_API
