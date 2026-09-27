// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "ObjectTrace.h"
#include "CoreMinimal.h"
#include "StructUtils/PropertyBag.h"
#include "Trace/Trace.h"

#define ANIMNEXT_TRACE_ENABLED (OBJECT_TRACE_ENABLED && !(UE_BUILD_SHIPPING || UE_BUILD_TEST))

#if ANIMNEXT_TRACE_ENABLED

UE_TRACE_CHANNEL_EXTERN(AnimNextChannel, ANIMNEXT_API);

struct FAnimNextModuleInstance;
struct FAnimNextDataInterfaceInstance;
struct FAnimNextGraphInstance;

namespace UE::AnimNext
{
struct FAnimNextTrace
{
	ANIMNEXT_API static const FGuid CustomVersionGUID;
	
	ANIMNEXT_API static void Reset();
	
	ANIMNEXT_API static void OutputAnimNextInstance(const FAnimNextDataInterfaceInstance* DataInterface, const UObject* OuterObject);
	ANIMNEXT_API static void OutputAnimNextVariables(const FAnimNextDataInterfaceInstance* DataInterface, const UObject* OuterObject);
};

}


#define TRACE_ANIMNEXT_INSTANCE(DataInterface, OuterObject) UE::AnimNext::FAnimNextTrace::OutputAnimNextInstance(DataInterface, OuterObject);
#define TRACE_ANIMNEXT_VARIABLES(DataInterface, OuterObject) UE::AnimNext::FAnimNextTrace::OutputAnimNextVariables(DataInterface, OuterObject);
#else
#define TRACE_ANIMNEXT_INSTANCE(DataInterface, OuterObject)
#define TRACE_ANIMNEXT_VARIABLES(DataInterface, OuterObject)
#endif
