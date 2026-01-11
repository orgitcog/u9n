// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"

class CAPTUREDATACONVERTER_API FCaptureDataConverterError
{
public:

	explicit FCaptureDataConverterError(TArray<FText> InErrors);
	~FCaptureDataConverterError();

	TArray<FText> GetErrors() const;

private:

	TArray<FText> Errors;
};
