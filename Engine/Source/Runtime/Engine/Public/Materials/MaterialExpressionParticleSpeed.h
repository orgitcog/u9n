// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
	MaterialExpressionParticleSpeed: Exposes the speed of a particle to
		the material editor.
==============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpressionExternalCodeBase.h"
#include "MaterialExpressionParticleSpeed.generated.h"

UCLASS(collapsecategories, hidecategories=Object)
class UMaterialExpressionParticleSpeed : public UMaterialExpressionExternalCodeBase
{
	GENERATED_BODY()

public:

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;

#endif
	//~ End UMaterialExpression Interface
};



