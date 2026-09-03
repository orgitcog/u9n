// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareContext.h"

/**
 * TextureShare context for WorldSubsystem based implementation
 */
class ITextureShareWorldSubsystemContext
	: public ITextureShareContext
{
public:
	virtual ~ITextureShareWorldSubsystemContext() = default;

	/** A quick and dirty way to determine which TS data (sub)class this is. */
	virtual FName GetRTTI() const { return TEXT("TextureShareWorldSubsystem"); }
};
