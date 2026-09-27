// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"

#include "NiagaraStatelessModule_RotateAroundPoint.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Rotate Around Point"))
class UNiagaraStatelessModule_RotateAroundPoint : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FNiagaraStatelessRangeFloat	Rate			= FNiagaraStatelessRangeFloat(0.0f);
		FNiagaraStatelessRangeFloat	Radius			= FNiagaraStatelessRangeFloat(0.0f);
		FNiagaraStatelessRangeFloat	InitialPhase	= FNiagaraStatelessRangeFloat(0.0f);
	};

public:
	using FParameters = NiagaraStateless::FRotateAroundPointModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(DisableBindingDistribution, Units="deg/s"))
	FNiagaraDistributionRangeFloat Rate = FNiagaraDistributionRangeFloat(360.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableBindingDistribution))
	FNiagaraDistributionRangeFloat Radius = FNiagaraDistributionRangeFloat(100.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableBindingDistribution))
	FNiagaraDistributionRangeFloat InitialPhase = FNiagaraDistributionRangeFloat(0.0f);

	//-TODO: Add support for GPU once we settle on a feature set for this module
	virtual ENiagaraStatelessFeatureMask GetFeatureMask() const override { return ENiagaraStatelessFeatureMask::ExecuteGPU; }

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (!IsModuleEnabled())
		{
			return;
		}

		BuiltData->Rate			= Rate.CalculateRange();
		BuiltData->Radius		= Radius.CalculateRange();
		BuiltData->InitialPhase	= InitialPhase.CalculateRange();
	}

	virtual void BuildShaderParameters(FNiagaraStatelessShaderParametersBuilder& ShaderParametersBuilder) const override
	{
		ShaderParametersBuilder.AddParameterNestedStruct<FParameters>();
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* BuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		Parameters->RotateAroundPoint_RateScale			= FMath::DegreesToRadians(BuiltData->Rate.GetScale());
		Parameters->RotateAroundPoint_RateBias			= FMath::DegreesToRadians(BuiltData->Rate.Min);
		Parameters->RotateAroundPoint_RadiusScale		= BuiltData->Radius.GetScale();
		Parameters->RotateAroundPoint_RadiusBias		= BuiltData->Radius.Min;
		Parameters->RotateAroundPoint_InitialPhaseScale	= BuiltData->InitialPhase.GetScale();
		Parameters->RotateAroundPoint_InitialPhaseBias	= BuiltData->InitialPhase.Min;
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
	}
#endif
};
