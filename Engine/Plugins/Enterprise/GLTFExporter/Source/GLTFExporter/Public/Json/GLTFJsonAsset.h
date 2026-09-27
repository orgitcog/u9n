// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"

#define UE_API GLTFEXPORTER_API

struct FGLTFJsonAsset : IGLTFJsonObject
{
	FString Version;
	FString Generator;
	FString Copyright;

	FGLTFJsonAsset()
		: Version(TEXT("2.0"))
	{
	}

	UE_API virtual void WriteObject(IGLTFJsonWriter& Writer) const override;
};

#undef UE_API
