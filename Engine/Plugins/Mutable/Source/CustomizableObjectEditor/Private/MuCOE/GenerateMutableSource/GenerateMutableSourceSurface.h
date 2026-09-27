// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "HAL/Platform.h"

class UEdGraphPin;
struct FMutableGraphGenerationContext;

enum class ECustomizableObjectNumBoneInfluences:uint8;
namespace mu
{
	class FMeshBufferSet;
	class NodeSurface;
}

extern mu::Ptr<mu::NodeSurface> GenerateMutableSourceSurface(const UEdGraphPin* Pin, FMutableGraphGenerationContext & GenerationContext);

