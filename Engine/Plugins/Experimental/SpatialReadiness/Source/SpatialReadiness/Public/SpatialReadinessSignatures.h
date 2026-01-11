// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"

// Declare internal delegate types
DECLARE_DELEGATE_RetVal_TwoParams(int32, FAddUnreadyVolume_Delegate, const FBox&, const FString&);
DECLARE_DELEGATE_OneParam(FRemoveUnreadyVolume_Delegate, int32);

// Declare TFunction types which can be bound to the delegates
using FAddUnreadyVolume_Function = TFunction<int32(const FBox&, const FString&)>;
using FRemoveUnreadyVolume_Function = TFunction<void(int32)>;

// Declare member function types which can be bound to delegates
template <typename ObjectT>
using TAddUnreadyVolume_Member = int32(ObjectT::*)(const FBox&, const FString&);
template <typename ObjectT>
using TRemoveUnreadyVolume_Member = void(ObjectT::*)(int32);
