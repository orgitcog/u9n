// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SceneStateBindingDataHandle.h"
#include "PropertyBindingBindableStructDescriptor.h"
#include "SceneStateBindingDesc.generated.h"

USTRUCT()
struct FSceneStateBindingDesc : public FPropertyBindingBindableStructDescriptor
{
	GENERATED_BODY()

	FSceneStateBindingDesc() = default;

#if WITH_EDITOR
	explicit FSceneStateBindingDesc(const FName InName, const UStruct* InStruct, const FGuid InGuid, const FSceneStateBindingDataHandle& InDataHandle)
		: Super(InName, InStruct, InGuid)
		, DataHandle(InDataHandle)
	{
	}
#endif

	UPROPERTY()
	FSceneStateBindingDataHandle DataHandle;
};
