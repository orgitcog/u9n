// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangeEditorUtilitiesBase.h"

#include "InterchangeEditorUtilities.generated.h"

UCLASS()
class INTERCHANGEEDITORUTILITIES_API UInterchangeEditorUtilities : public UInterchangeEditorUtilitiesBase
{
	GENERATED_BODY()

public:

protected:

	virtual bool SaveAsset(UObject* Asset) const override;

	virtual bool IsRuntimeOrPIE() const override;

	virtual bool ClearEditorSelection() const override;
};
