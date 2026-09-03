// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_MeshRotationRate.generated.h"

// Applies a constant rotation rate to mesh orientation
UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Mesh Rotation Rate"))
class UNiagaraStatelessModule_MeshRotationRate : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		bool							ModuleEnabled = false;
		FNiagaraStatelessRangeVector3	RotationRange;
		FUintVector3					RateScaleParameters = FUintVector3::ZeroValue;
		int32							MeshOrientationVariableOffset = INDEX_NONE;
		int32							PreviousMeshOrientationVariableOffset = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FMeshRotationRateModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bUseRateScale = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Rotation Rate", Units="deg"))
	FNiagaraDistributionRangeVector3 RotationRateDistribution = FNiagaraDistributionRangeVector3(FVector3f::ZeroVector);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Rate Scale", EditCondition = "bUseRateScale"))
	FNiagaraDistributionCurveVector3 RateScaleDistribution = FNiagaraDistributionCurveVector3(ENiagaraDistributionCurveLUTMode::Accumulate);

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (!IsModuleEnabled())
		{
			return;
		}

		const FNiagaraStatelessGlobals& StatelessGlobals	= FNiagaraStatelessGlobals::Get();
		BuiltData->MeshOrientationVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.MeshOrientationVariable);
		BuiltData->PreviousMeshOrientationVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousMeshOrientationVariable);

		if (BuiltData->MeshOrientationVariableOffset != INDEX_NONE || BuiltData->PreviousMeshOrientationVariableOffset != INDEX_NONE)
		{
			BuiltData->ModuleEnabled = true;
			BuiltData->RotationRange = BuildContext.ConvertDistributionToRange(RotationRateDistribution, FVector3f::ZeroVector);
			BuiltData->RotationRange.Min *= 1.0f / 360.0f;
			BuiltData->RotationRange.Max *= 1.0f / 360.0f;
			if (bUseRateScale)
			{
				BuiltData->RateScaleParameters = BuildContext.AddDistributionAsCurve(RateScaleDistribution, 1.0f);
			}
			else
			{
				BuiltData->RateScaleParameters = BuildContext.AddDistributionAsCurve(FNiagaraDistributionCurveVector3(ENiagaraDistributionCurveLUTMode::Accumulate), 1.0f);
			}

			BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_MeshRotationRate::ParticleSimulate);
		}
	}

	virtual void BuildShaderParameters(FNiagaraStatelessShaderParametersBuilder& ShaderParametersBuilder) const override
	{
		ShaderParametersBuilder.AddParameterNestedStruct<FParameters>();
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();
		Parameters->MeshRotationRate_ModuleEnabled = ModuleBuiltData->ModuleEnabled;
		SetShaderParameterContext.ConvertRangeToScaleBias(ModuleBuiltData->RotationRange, Parameters->MeshRotationRate_Scale, Parameters->MeshRotationRate_Bias);
		Parameters->MeshRotationRate_RateScaleParameters = ModuleBuiltData->RateScaleParameters;
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		using namespace NiagaraStateless;

		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();
		const FParameters* ShaderParameters = ParticleSimulationContext.ReadParameterNestedStruct<FParameters>();

		const float* LifetimeData = ParticleSimulationContext.GetParticleLifetime();
		const float* AgeData = ParticleSimulationContext.GetParticleNormalizedAge();
		const float* PreviousAgeData = ParticleSimulationContext.GetParticlePreviousNormalizedAge();

		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			const FVector3f RotationRate = ParticleSimulationContext.RandomScaleBiasFloat(i, 0, ShaderParameters->MeshRotationRate_Scale, ShaderParameters->MeshRotationRate_Bias);
			const FVector3f MultiplyRate = ParticleSimulationContext.SampleCurve<FVector3f>(ShaderParameters->MeshRotationRate_RateScaleParameters, AgeData[i]);
			const FVector3f PreviousMultiplyRate = ParticleSimulationContext.SampleCurve<FVector3f>(ShaderParameters->MeshRotationRate_RateScaleParameters, PreviousAgeData[i]);

			FQuat4f MeshOrientation = ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->MeshOrientationVariableOffset, i, FQuat4f::Identity);
			FQuat4f PreviousMeshOrientation = ParticleSimulationContext.ReadParticleVariable(ModuleBuiltData->PreviousMeshOrientationVariableOffset, i, FQuat4f::Identity);

			MeshOrientation *= ParticleSimulationContext.RotatorToQuat(LifetimeData[i] * RotationRate * MultiplyRate);
			PreviousMeshOrientation *= ParticleSimulationContext.RotatorToQuat(LifetimeData[i] * RotationRate * PreviousMultiplyRate);

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->MeshOrientationVariableOffset, i, MeshOrientation);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousMeshOrientationVariableOffset, i, PreviousMeshOrientation);
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.MeshOrientationVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousMeshOrientationVariable);
	}
#endif
};
