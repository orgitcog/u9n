// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonValue.h"

#define UE_API GLTFEXPORTER_API

struct IGLTFJsonArray : IGLTFJsonValue
{
	UE_API virtual void WriteValue(IGLTFJsonWriter& Writer) const override final;

	virtual void WriteArray(IGLTFJsonWriter& Writer) const = 0;
};

#undef UE_API
