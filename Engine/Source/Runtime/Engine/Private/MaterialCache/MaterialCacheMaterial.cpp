// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialCache/MaterialCacheMaterial.h"

#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionWorldPosition.h"

bool MaterialCacheIsExpressionNonUVDerived(const UMaterialExpression* Expression)
{
	const bool bExpressionClassTest =
		Expression->IsA(UMaterialExpressionWorldPosition::StaticClass()) ||
		Expression->IsA(UMaterialExpressionLocalPosition::StaticClass()) ||
		Expression->IsA(UMaterialExpressionVertexColor::StaticClass()) ||
		Expression->IsA(UMaterialExpressionVertexNormalWS::StaticClass()) ||
		Expression->IsA(UMaterialExpressionVertexTangentWS::StaticClass()) ||
		Expression->IsA(UMaterialExpressionPixelNormalWS::StaticClass()) ||
		Expression->IsA(UMaterialExpressionPreSkinnedNormal::StaticClass()) ||
		Expression->IsA(UMaterialExpressionTangent::StaticClass());

	return bExpressionClassTest;
}
