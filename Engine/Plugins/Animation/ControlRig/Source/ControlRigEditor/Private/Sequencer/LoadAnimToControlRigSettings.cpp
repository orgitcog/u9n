// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadAnimToControlRigSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LoadAnimToControlRigSettings)

ULoadAnimToControlRigSettings::ULoadAnimToControlRigSettings(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	Reset();
}

void ULoadAnimToControlRigSettings::Reset()
{
	bReduceKeys = false;
	SmartReduce.Reset();
	bUseCustomTimeRange = false;
	bOntoSelectedControls = false;
}

