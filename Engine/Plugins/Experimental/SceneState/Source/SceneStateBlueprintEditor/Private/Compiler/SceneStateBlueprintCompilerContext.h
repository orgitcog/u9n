// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISceneStateMachineCompilerContext.h"
#include "KismetCompiler.h"
#include "SceneStateMachineCompiler.h"

class USceneStateBlueprint;
class USceneStateGeneratedClass;

namespace UE::SceneState::Editor
{

class FBlueprintCompilerContext : public FKismetCompilerContext, public IStateMachineCompilerContext
{
public:
	explicit FBlueprintCompilerContext(USceneStateBlueprint* InBlueprint, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompilerOptions);

private:
	//~ Begin FKismetCompilerContext
	virtual void SpawnNewClass(const FString& InNewClassName) override;
	virtual void OnNewClassSet(UBlueprintGeneratedClass* InClassToUse) override;
	virtual void EnsureProperGeneratedClass(UClass*& InOutTargetClass) override;
	virtual void CleanAndSanitizeClass(UBlueprintGeneratedClass* InClassToClean, UObject*& InOutOldCDO) override;
	virtual void SaveSubObjectsFromCleanAndSanitizeClass(FSubobjectCollection& OutSubObjectsToSave, UBlueprintGeneratedClass* InClassToClean) override;
	virtual void MergeUbergraphPagesIn(UEdGraph* InUbergraph) override;
	virtual void OnPostCDOCompiled(const UObject::FPostCDOCompiledContext& InContext) override;
	//~ End FKismetCompilerContext

	//~ Begin ISceneStateMachineCompilerContext
	virtual UBlueprint* GetBlueprint() const override;
	virtual USceneStateGeneratedClass* GetGeneratedClass() const override;
	virtual FTransitionGraphCompileResult CompileTransitionGraph(USceneStateTransitionGraph* InTransitionGraph) override;
	//~ End ISceneStateMachineCompilerContext

	/** Sets the Task Index and Parent State Index of each Task */
	void UpdateTaskIndices();

	void CompileBindings();

	USceneStateGeneratedClass* NewGeneratedClass = nullptr;

	friend class FTransitionGraphCompiler;
};

} // UE::SceneState::Editor
