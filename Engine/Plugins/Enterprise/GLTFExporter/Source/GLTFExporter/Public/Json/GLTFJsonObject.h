// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonValue.h"

#define UE_API GLTFEXPORTER_API

struct IGLTFJsonObject : IGLTFJsonValue
{
	UE_API virtual void WriteValue(IGLTFJsonWriter& Writer) const override final;

	virtual void WriteObject(IGLTFJsonWriter& Writer) const = 0;
};

#undef UE_API
