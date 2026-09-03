// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/NoExportTypes.h"

#if WITH_APPLICATION_CORE
#include "GenericPlatform/ICursor.h"
#include "GenericPlatform/IInputInterface.h"
#include "GenericPlatform/InputDeviceMappingPolicy.h"
#else
#include "../../../ApplicationCore/Public/GenericPlatform/ICursor.h" // TODO, Bad! will not work with clang vfs overlay
#include "../../../ApplicationCore/Public/GenericPlatform/IInputInterface.h" // TODO, Bad! will not work with clang vfs overlay
#include "../../../ApplicationCore/Public/GenericPlatform/InputDeviceMappingPolicy.h" // TODO, Bad! will not work with clang vfs overlay
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NoExportTypes)
