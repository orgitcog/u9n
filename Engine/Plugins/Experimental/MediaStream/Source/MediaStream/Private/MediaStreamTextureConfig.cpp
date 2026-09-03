// Copyright Epic Games, Inc. All Rights Reserved.

#include "MediaStreamTextureConfig.h"

bool FMediaStreamTextureConfig::operator==(const FMediaStreamTextureConfig& InOther) const
{
	return bEnableMipGen == InOther.bEnableMipGen;
}

void FMediaStreamTextureConfig::ApplyConfig(UMediaTexture& InMediaTexture) const
{
	if (InMediaTexture.EnableGenMips == bEnableMipGen)
	{
		return;
	}

	InMediaTexture.EnableGenMips = bEnableMipGen;
	InMediaTexture.UpdateResource();
}
