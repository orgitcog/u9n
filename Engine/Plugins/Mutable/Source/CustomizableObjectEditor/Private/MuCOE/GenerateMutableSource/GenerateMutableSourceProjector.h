// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/NodeProjector.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;

/** Convert a CustomizableObject Source Graph into a mutable source graph. */
mu::Ptr<mu::NodeProjector> GenerateMutableSourceProjector(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext);
