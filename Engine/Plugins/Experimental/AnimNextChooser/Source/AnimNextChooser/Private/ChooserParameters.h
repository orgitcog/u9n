// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserParameterBool.h"
#include "IChooserParameterFloat.h"
#include "IChooserParameterEnum.h"
#include "ChooserParameters.generated.h"

USTRUCT(DisplayName = "Bool Anim Param")
struct FBoolAnimProperty :  public FChooserParameterBoolBase
{
	GENERATED_BODY()
public:

	UPROPERTY(DisplayName = "Variable", EditAnywhere, Category="Variable")
	FName VariableName;

	virtual bool GetValue(FChooserEvaluationContext& Context, bool& OutResult) const override;
	virtual bool SetValue(FChooserEvaluationContext& Context, bool InValue) const override;
	virtual void GetDisplayName(FText& OutName) const override;
};

USTRUCT(DisplayName = "Float Anim Param")
struct FFloatAnimProperty :  public FChooserParameterFloatBase
{
	GENERATED_BODY()
public:

	UPROPERTY(DisplayName = "Variable", EditAnywhere, Category="Variable")
	FName VariableName;

	virtual bool GetValue(FChooserEvaluationContext& Context, double& OutResult) const override;
	virtual bool SetValue(FChooserEvaluationContext& Context, double InValue) const override;
	virtual void GetDisplayName(FText& OutName) const override;
};

USTRUCT(DisplayName = "Enum Anim Param")
struct FEnumAnimProperty :  public FChooserParameterEnumBase
{
	GENERATED_BODY()
public:

	UPROPERTY(DisplayName = "Variable", EditAnywhere, Category="Variable")
	FName VariableName;

	UPROPERTY(EditAnywhere, Category="Variable")
	TObjectPtr<UEnum> Enum;
	
	virtual bool GetValue(FChooserEvaluationContext& Context, uint8& OutResult) const override;
	virtual bool SetValue(FChooserEvaluationContext& Context, uint8 InValue) const override;

	virtual void GetDisplayName(FText& OutName) const override;

	#if WITH_EDITOR
	virtual const UEnum* GetEnum() const override { return Enum; }
	#endif
};