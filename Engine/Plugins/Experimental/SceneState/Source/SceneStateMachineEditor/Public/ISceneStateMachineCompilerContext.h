// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "UObject/NameTypes.h"

class UBlueprint;
class USceneStateGeneratedClass;
class USceneStateTransitionGraph;

namespace UE::SceneState::Editor
{

enum class ETransitionGraphCompileReturnCode : uint8
{
	/** Succeeded compiling transition graph */
	Success,

	/** Failed to compile transition graph due to errors */
	Failed,

	/** Transition graph not compiled because it's always going to evaluate to false */
	SkippedAlwaysFalse,

	/** Transition graph not compiled because it's always going to evaluate to true */
	SkippedAlwaysTrue,
};

struct FTransitionGraphCompileResult
{
	/** Return code of the graph compilation, whether it succeeded, failed, etc. */
	ETransitionGraphCompileReturnCode ReturnCode;
	/** Compiled event name to call (if any) */
	FName EventName;
	/** Name of the result property that the event name will write to when called */
	FName ResultPropertyName;
};

/** Interface to get data or execute functionality out of the scope of the compiler */
class IStateMachineCompilerContext
{
public:
	virtual UBlueprint* GetBlueprint() const = 0;

	virtual USceneStateGeneratedClass* GetGeneratedClass() const = 0;

	virtual FTransitionGraphCompileResult CompileTransitionGraph(USceneStateTransitionGraph* InTransitionGraph) = 0;
};

} // UE::SceneState::Editor
