// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Nanite {

struct FIntermediateResources;
struct FInputAssemblyData;

bool BuildAssemblyData(FIntermediateResources& ParentIntermediate, const FInputAssemblyData& AssemblyData);

} // namespace Nanite