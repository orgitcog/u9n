// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialExpressionIO.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionOperator.generated.h"

UENUM()
enum class EMaterialExpressionOperatorKind
{
	// Unary
	BitwiseNot,
	Negate,
	Not,
	Abs,
	ACos,
	ACosh,
	ASin,
	ASinh,
	ATan,
	ATanh,
	Ceil,
	Cos,
	Cosh,
	Exponential,
	Exponential2,
	Floor,
	Frac,
	IsFinite,
	IsInf,
	IsNan,
	Length,
	Logarithm,
	Logarithm10,
	Logarithm2,
	Round,
	Saturate,
	Sign,
	Sin,
	Sinh,
	Sqrt,
	Tan,
	Tanh,
	Truncate,

	// Binary
	Equals,
	GreaterThan,
	GreaterThanOrEquals,
	LessThan,
	LessThanOrEquals,
	NotEquals,
	And,
	Or,
	Add,
	Subtract,
	Multiply,
	Divide,
	Modulo,
	BitwiseAnd,
	BitwiseOr,
	BitShiftLeft,
	BitShiftRight,
	Cross,
	Distance,
	Dot,
	Fmod,
	Max,
	Min,
	Pow,
	Step,

	// Ternary
	Clamp,
	Lerp,
	Select,
	Smoothstep,
};

UCLASS(MinimalAPI)
class UMaterialExpressionOperator : public UMaterialExpression
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category=MaterialExpressionOperator)
	EMaterialExpressionOperatorKind Operator = EMaterialExpressionOperatorKind::Add;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstA' if not specified"))
	FExpressionInput A;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstB' if not specified"))
	FExpressionInput B;

	UPROPERTY(meta = (RequiredInput = "false", ToolTip = "Defaults to 'ConstB' if not specified"))
	FExpressionInput C;

	// only used if A is not hooked up
	UPROPERTY(EditAnywhere, Category=MaterialExpressionOperator, meta=(OverridingInputProperty = "A"))
	float ConstA = 0.0f;

	// only used if B is not hooked up
	UPROPERTY(EditAnywhere, Category=MaterialExpressionOperator, meta=(OverridingInputProperty = "B"))
	float ConstB = 1.0f;
	
	// only used if B is not hooked up
	UPROPERTY(EditAnywhere, Category=MaterialExpressionOperator, meta=(OverridingInputProperty = "C"))
	float ConstC = 1.0f;

	//
	uint32 Arity = 2;

	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
	virtual FText GetKeywords() const override;
	virtual FText GetCreationName() const override;
	virtual FExpressionInput* GetInput(int32 InputIndex) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostLoad() override;
	virtual void Build(MIR::FEmitter& Emitter) override;

#endif // WITH_EDITOR
	//~ End UMaterialExpression Interface
};



