// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionIfThenElse.generated.h"


UCLASS(collapsecategories, hidecategories = Object, MinimalAPI)
class UMaterialExpressionIfThenElse : public UMaterialExpression
{
	GENERATED_BODY()

public:
	
	UPROPERTY()
	FExpressionInput Condition;

	UPROPERTY()
	FExpressionInput True;

	UPROPERTY()
	FExpressionInput False;
	
	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void Build(MIR::FEmitter& Emitter) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual EMaterialValueType GetInputValueType(int32 InputIndex) override;
#endif
	//~ End UMaterialExpression Interface
};
