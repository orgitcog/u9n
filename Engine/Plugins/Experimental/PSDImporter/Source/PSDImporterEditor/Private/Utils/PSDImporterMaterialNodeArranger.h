// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/MathFwd.h"

class UMaterialExpression;
struct FExpressionInput;

class FPSDImporterMaterialNodeArranger
{
public:
	static void ArrangeNodes(const FExpressionInput& InMaterialChannelInput);

private:
	static void ArrangeNodes(UMaterialExpression& InExpression, FIntPoint InPosition);
};
