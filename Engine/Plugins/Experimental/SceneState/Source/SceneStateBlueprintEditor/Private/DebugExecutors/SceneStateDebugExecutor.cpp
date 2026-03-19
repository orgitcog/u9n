// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneStateDebugExecutor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Nodes/SceneStateMachineNode.h"
#include "SceneStateBlueprint.h"
#include "SceneStateGeneratedClass.h"

namespace UE::SceneState::Editor
{

FDebugExecutor::FDebugExecutor(USceneStateObject* InRootObject, const USceneStateMachineNode* InNode)
	: NodeKey(InNode)
{
	ExecutionContext.Setup(InRootObject);
}

FDebugExecutor::~FDebugExecutor()
{
	Exit();
}

void FDebugExecutor::Start()
{
	OnStart(ExecutionContext);
}

void FDebugExecutor::Tick(float InDeltaSeconds)
{
	OnTick(ExecutionContext, InDeltaSeconds);
}

void FDebugExecutor::Exit()
{
	OnExit(ExecutionContext);
	ExecutionContext.Reset();
}

FString FDebugExecutor::GetReferencerName() const
{
	return TEXT("SceneStateDebugExecutor");
}

void FDebugExecutor::AddReferencedObjects(FReferenceCollector& InCollector)
{
	InCollector.AddPropertyReferencesWithStructARO(FSceneStateExecutionContext::StaticStruct(), &ExecutionContext);
}

} // UE::SceneState::Editor
