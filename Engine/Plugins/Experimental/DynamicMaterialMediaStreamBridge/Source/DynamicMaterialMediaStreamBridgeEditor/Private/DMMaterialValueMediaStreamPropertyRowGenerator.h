// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/PropertyGenerators/DMComponentPropertyRowGenerator.h"

class UMediaStream;

class FDMMaterialValueMediaStreamPropertyRowGenerator : public FDMComponentPropertyRowGenerator
{
public:
	static const TSharedRef<FDMMaterialValueMediaStreamPropertyRowGenerator>& Get();

	virtual ~FDMMaterialValueMediaStreamPropertyRowGenerator() override = default;

	//~ Begin FDMComponentPropertyRowGenerator
	virtual void AddComponentProperties(FDMComponentPropertyRowGeneratorParams& InParams) override;
	//~ End FDMComponentPropertyRowGenerator

	void AddControlCategory(FDMComponentPropertyRowGeneratorParams& InParams);

	void AddSourceCategory(FDMComponentPropertyRowGeneratorParams& InParams);

	void AddDetailsCategory(FDMComponentPropertyRowGeneratorParams& InParams);

	void AddTextureCategory(FDMComponentPropertyRowGeneratorParams& InParams, bool bInPreview);

	void AddCacheCategory(FDMComponentPropertyRowGeneratorParams& InParams);

	void AddPlayerProperty(FDMComponentPropertyRowGeneratorParams& InParams);
};
