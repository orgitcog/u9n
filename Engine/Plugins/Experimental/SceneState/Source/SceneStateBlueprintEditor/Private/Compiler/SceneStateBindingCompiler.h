// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "PropertyBindingDataView.h"
#include "UObject/ObjectPtr.h"

class USceneStateBlueprint;
class USceneStateGeneratedClass;
struct FPropertyBindingIndex16;
struct FSceneStateBinding;
struct FSceneStateBindingDataHandle;
struct FSceneStateBindingDesc;

namespace UE::SceneState::Editor
{
	class FBlueprintCompilerContext;
}

namespace UE::SceneState::Editor
{

class FBindingCompiler
{
public:
	explicit FBindingCompiler(FBlueprintCompilerContext& InContext, USceneStateBlueprint* InBlueprint, USceneStateGeneratedClass* InGeneratedClass)
		: Context(InContext)
		, Blueprint(InBlueprint)
		, Class(InGeneratedClass)
	{
	}

	void Compile();

private:
	void AddBindingDesc(FSceneStateBindingDesc&& InBindingDesc);

	void AddRootBindingDesc();
	void AddStateMachineBindingDescs();
	void AddTransitionBindingDescs();
	void AddTaskBindingDescs();
	void AddEventHandlerBindingDescs();

	bool ValidateBinding(const FSceneStateBinding& InBinding) const;

	void ResolveBindingDataHandles();
	void RemoveInvalidBindings();
	void BatchCopies();

	void OnBindingsBatchCompiled(FPropertyBindingIndex16 InBindingsBatch, const FSceneStateBindingDataHandle& InTargetDataHandle);

	FSceneStateBindingDataHandle GetDataHandleById(const FGuid& InStructId);

	/** Compiler context used for error logging */
	FBlueprintCompilerContext& Context;

	/** The blueprint that generated the class */
	TObjectPtr<USceneStateBlueprint> Blueprint;

	/** The generated class to compile bindings for */
	TObjectPtr<USceneStateGeneratedClass> Class;

	/** Map of struct ids to their binding data view */
	TMap<FGuid, const FPropertyBindingDataView> ValidBindingMap;
};

} // UE::SceneState::Editor
