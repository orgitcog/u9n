// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuR/Ptr.h"
#include "HAL/Platform.h"

class FCustomizableObjectCompiler;
class UCustomizableObjectNode;
class UEdGraphPin;
class UTexture2D;
struct FMutableGraphGenerationContext;

namespace mu
{
	class FImage;
	class NodeImage;
}


//TSharedPtr<mu::FImage> ConvertTextureUnrealToMutable(UTexture2D* Texture, const UCustomizableObjectNode* Node, FCustomizableObjectCompiler* Compiler, bool bIsNormalComposite);

mu::Ptr<mu::NodeImage> ResizeTextureByNumMips(const mu::Ptr<mu::NodeImage>& Image, int32 MipsToSkip);

/** Convert a CustomizableObject Source Graph from an Image pin into a mutable source graph. */
mu::Ptr<mu::NodeImage> GenerateMutableSourceImage(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext, int32 ReferenceTextureSize);
