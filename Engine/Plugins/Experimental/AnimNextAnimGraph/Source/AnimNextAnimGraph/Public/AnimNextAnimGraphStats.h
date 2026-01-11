// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stats/Stats.h"
#include "AnimNextStats.h"

DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF: Allocate Graph Instance"), STAT_AnimNext_Graph_AllocateInstance, STATGROUP_AnimNext, ANIMNEXTANIMGRAPH_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF: Graph RigVM"), STAT_AnimNext_Graph_RigVM, STATGROUP_AnimNext, ANIMNEXTANIMGRAPH_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF: Execute Evaluation Program"), STAT_AnimNext_EvaluationProgram_Execute, STATGROUP_AnimNext, ANIMNEXTANIMGRAPH_API);

DECLARE_CYCLE_STAT_EXTERN(TEXT("UAF: Call Function"), STAT_AnimNext_CallFunction, STATGROUP_AnimNext, ANIMNEXTANIMGRAPH_API);
