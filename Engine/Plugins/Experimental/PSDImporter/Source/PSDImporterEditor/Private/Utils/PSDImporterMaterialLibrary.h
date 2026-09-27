// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

class APSDQuadMeshActor;
class UClass;
class UMaterial;
class UMaterialExpression;
class UMaterialFunctionInterface;

class FPSDImporterMaterialLibrary
{
public:
	static UMaterialFunctionInterface* GetMaterialFunction(const TCHAR* InFunctionPath);

	static UMaterialExpression* CreateExpression(UMaterial& InMaterial, UClass& InExpressionClass);

	template<typename InExpressionType>
	static InExpressionType* CreateExpression(UMaterial& InMaterial)
	{
		return Cast<InExpressionType>(CreateExpression(InMaterial, *InExpressionType::StaticClass()));
	}

	static void ResetTexture(APSDQuadMeshActor& InQuadMeshActor);
};
