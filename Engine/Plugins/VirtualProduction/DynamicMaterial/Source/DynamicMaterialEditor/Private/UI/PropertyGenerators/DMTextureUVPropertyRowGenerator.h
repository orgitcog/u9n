// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/PropertyGenerators/DMComponentPropertyRowGenerator.h"

class SWidget;

class FDMTextureUVPropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMTextureUVPropertyRowGenerator>& Get();

	virtual ~FDMTextureUVPropertyRowGenerator() override = default;

	static void AddPopoutComponentProperties(FDMComponentPropertyRowGeneratorParams& InParams);

	//~ Begin FDMComponentPropertyRowGenerator
	virtual void AddComponentProperties(FDMComponentPropertyRowGeneratorParams& InParams) override;

	virtual bool AllowKeyframeButton(UDMMaterialComponent* InComponent, FProperty* InProperty) override;
	//~ End FDMComponentPropertyRowGenerator
};
