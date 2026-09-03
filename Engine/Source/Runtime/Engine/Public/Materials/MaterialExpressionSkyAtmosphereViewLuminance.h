// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionSkyAtmosphereViewLuminance.generated.h"

UCLASS()
class UMaterialExpressionSkyAtmosphereViewLuminance : public UMaterialExpression
{
	GENERATED_BODY()

	/** Sample the sky atmosphere using WorldDirection instead of the default camera-to-pixel V direction. */
	UPROPERTY()
	FExpressionInput WorldDirection;

public:

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};


