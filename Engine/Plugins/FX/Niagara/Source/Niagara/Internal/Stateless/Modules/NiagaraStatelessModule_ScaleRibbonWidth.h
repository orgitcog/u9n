// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessCommon.h"
#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"
#include "NiagaraParameterBinding.h"

#include "NiagaraStatelessModule_ScaleRibbonWidth.generated.h"

// Multiply Particle.RibbonWidth by the module calculated scale value
// This can be a constant, random or curve indexed by Particle.NormalizedAge
UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Scale Ribbon Width"))
class UNiagaraStatelessModule_ScaleRibbonWidth : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FUintVector3	DistributionParameters = FUintVector3::ZeroValue;
		float			CurveScale = 1.0f;
		int32			CurveScaleOffset = INDEX_NONE;

		int32			RibbonWidthVariableOffset = INDEX_NONE;
		int32			PreviousRibbonWidthVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FScaleRibbonWidthModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Scale"))
	FNiagaraDistributionFloat ScaleDistribution = FNiagaraDistributionFloat(1.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "UseScaleCurveRange()"))
	FNiagaraParameterBindingWithValue ScaleCurveRange;

	virtual void PostInitProperties() override
	{
		Super::PostInitProperties();

	#if WITH_EDITORONLY_DATA
		if (HasAnyFlags(RF_ClassDefaultObject) == false)
		{
			ScaleCurveRange.SetUsage(ENiagaraParameterBindingUsage::NotParticle);
			ScaleCurveRange.SetAllowedTypeDefinitions({ FNiagaraTypeDefinition::GetVec2Def() });
			ScaleCurveRange.SetDefaultParameter(FNiagaraTypeDefinition::GetFloatDef(), 1.0f);
		}
	#endif
	}

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();

		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		BuiltData->RibbonWidthVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.RibbonWidthVariable);
		BuiltData->PreviousRibbonWidthVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousRibbonWidthVariable);

		const bool bAttributesUsed = (BuiltData->RibbonWidthVariableOffset != INDEX_NONE || BuiltData->PreviousRibbonWidthVariableOffset != INDEX_NONE);
		if (IsModuleEnabled() && bAttributesUsed)
		{
			BuiltData->DistributionParameters = BuildContext.AddDistribution(ScaleDistribution);
			if (UseScaleCurveRange())
			{
				BuiltData->CurveScaleOffset = BuildContext.AddRendererBinding(ScaleCurveRange.ResolvedParameter);
				BuiltData->CurveScale = ScaleCurveRange.GetDefaultValue<float>();
			}

			BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_ScaleRibbonWidth::ParticleSimulate);
		}
	}

	virtual void BuildShaderParameters(FNiagaraStatelessShaderParametersBuilder& ShaderParametersBuilder) const override
	{
		ShaderParametersBuilder.AddParameterNestedStruct<FParameters>();
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		FParameters* Parameters						= SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		Parameters->ScaleRibbonWidth_Distribution	= ModuleBuiltData->DistributionParameters;
		Parameters->ScaleRibbonWidth_CurveScale		= SetShaderParameterContext.GetRendererParameterValue(ModuleBuiltData->CurveScaleOffset, ModuleBuiltData->CurveScale);
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		using namespace NiagaraStateless;

		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();

		const float ScaleFactor					= ParticleSimulationContext.GetParameterBufferFloat(ModuleBuiltData->CurveScaleOffset, ModuleBuiltData->CurveScale);
		const float* NormalizedAgeData			= ParticleSimulationContext.GetParticleNormalizedAge();
		const float* PreviousNormalizedAgeData	= ParticleSimulationContext.GetParticlePreviousNormalizedAge();

		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			const FStatelessDistributionSampler<float> SpriteScaleSampler(ParticleSimulationContext, ModuleBuiltData->DistributionParameters, i, 0);

			const float NormalizedAge			= NormalizedAgeData[i];
			const float PreviousNormalizedAge	= PreviousNormalizedAgeData[i];

			const float RibbonWidth			= ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->RibbonWidthVariableOffset, i, 0.0f);
			const float PreviousRibbonWidth = ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousRibbonWidthVariableOffset, i, 0.0f);
			const float Scale				= SpriteScaleSampler.GetValue(ParticleSimulationContext, NormalizedAge) * ScaleFactor;
			const float PreviousScale		= SpriteScaleSampler.GetValue(ParticleSimulationContext, PreviousNormalizedAge) * ScaleFactor;

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->RibbonWidthVariableOffset, i, RibbonWidth * Scale);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousRibbonWidthVariableOffset, i, PreviousRibbonWidth * PreviousScale);
		}
	}

	UFUNCTION()
	bool UseScaleCurveRange() const { return ScaleDistribution.IsCurve(); }

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.RibbonWidthVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousRibbonWidthVariable);
	}
#endif
};
