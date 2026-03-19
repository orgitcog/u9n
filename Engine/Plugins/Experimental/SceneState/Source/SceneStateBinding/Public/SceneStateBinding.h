// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PropertyBindingBinding.h"
#include "SceneStateBindingDataHandle.h"
#include "SceneStateBinding.generated.h"

USTRUCT()
struct FSceneStateBinding : public FPropertyBindingBinding
{
	GENERATED_BODY()

	using FPropertyBindingBinding::FPropertyBindingBinding;

	//~ Begin FPropertyBindingBinding
	SCENESTATEBINDING_API virtual FConstStructView GetSourceDataHandleStruct() const override;
	//~ End FPropertyBindingBinding

	UPROPERTY()
	FSceneStateBindingDataHandle SourceDataHandle;

	UPROPERTY()
	FSceneStateBindingDataHandle TargetDataHandle;
};
