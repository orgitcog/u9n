// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"

#if WITH_ENGINE
#include "ObjectTrace.h"
#endif

#if WITH_ENGINE
#define OBJECT_PROPERTY_TRACE_ENABLED OBJECT_TRACE_ENABLED
#else
#define OBJECT_PROPERTY_TRACE_ENABLED 0
#endif

#if OBJECT_PROPERTY_TRACE_ENABLED

struct FObjectPropertyTrace
{
	/** Initialize object property tracing */
	static ENGINE_API void Init();

	/** Shut down object property tracing */
	static ENGINE_API void Destroy();

	/** Check whether object property tracing is enabled */
	static ENGINE_API bool IsEnabled();

	/** Toggle registration for a UObject being traced by the system */
	static ENGINE_API void ToggleObjectRegistration(const UObject* InObject);

	/** Register a UObject to be traced by the system */
	static ENGINE_API void RegisterObject(const UObject* InObject);

	/** Unregister a UObject to be traced by the system */
	static ENGINE_API void UnregisterObject(const UObject* InObject);

	/** Check whether an object is registered */
	static ENGINE_API bool IsObjectRegistered(const UObject* InObject);
};


#define TRACE_OBJECT_PROPERTIES_BEGIN(Object) FObjectPropertyTrace::RegisterObject(Object);
#define TRACE_OBJECT_PROPERTIES_END(Object) FObjectPropertyTrace::UnregisterObject(Object);

#else

#define TRACE_OBJECT_PROPERTIES_BEGIN(Object)
#define TRACE_OBJECT_PROPERTIES_END(Object)

#endif
