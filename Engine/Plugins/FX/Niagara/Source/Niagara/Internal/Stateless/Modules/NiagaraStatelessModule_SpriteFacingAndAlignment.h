// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_SpriteFacingAndAlignment.generated.h"

// Sets the sprite facing and alignment attributes
UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Sprite Facing Alignment"))
class UNiagaraStatelessModule_SpriteFacingAndAlignment : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	struct FModuleBuiltData
	{
		FNiagaraStatelessRangeVector3	SpriteFacing	= FNiagaraStatelessRangeVector3(FVector3f::XAxisVector);
		FNiagaraStatelessRangeVector3	SpriteAlignment = FNiagaraStatelessRangeVector3(FVector3f::YAxisVector);

		int32		SpriteFacingVariableOffset = INDEX_NONE;
		int32		PreviousSpriteFacingVariableOffset = INDEX_NONE;
		int32		SpriteAlignmentVariable = INDEX_NONE;
		int32		PreviousSpriteAlignmentVariable = INDEX_NONE;
	};

public:
	using FParameters = NiagaraStateless::FSpriteFacingAndAlignmentModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bSpriteFacingEnabled = true;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (InlineEditConditionToggle))
	bool bSpriteAlignmentEnabled = false;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, DisableUniformDistribution, EditCondition = "bSpriteFacingEnabled"))
	FNiagaraDistributionRangeVector3	SpriteFacing = FNiagaraDistributionRangeVector3(FVector3f::XAxisVector);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, DisableUniformDistribution, EditCondition = "bSpriteAlignmentEnabled"))
	FNiagaraDistributionRangeVector3	SpriteAlignment = FNiagaraDistributionRangeVector3(FVector3f::YAxisVector);

	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		FModuleBuiltData* BuiltData = BuildContext.AllocateBuiltData<FModuleBuiltData>();
		if (!IsModuleEnabled())
		{
			return;
		}

		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		if (bSpriteFacingEnabled)
		{
			BuiltData->SpriteFacingVariableOffset			= BuildContext.FindParticleVariableIndex(StatelessGlobals.SpriteFacingVariable);
			BuiltData->PreviousSpriteFacingVariableOffset	= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousSpriteFacingVariable);
		}
		if (bSpriteAlignmentEnabled)
		{
			BuiltData->SpriteAlignmentVariable				= BuildContext.FindParticleVariableIndex(StatelessGlobals.SpriteAlignmentVariable);
			BuiltData->PreviousSpriteAlignmentVariable		= BuildContext.FindParticleVariableIndex(StatelessGlobals.PreviousSpriteAlignmentVariable);
		}

		if ((BuiltData->SpriteFacingVariableOffset == INDEX_NONE) && (BuiltData->PreviousSpriteFacingVariableOffset == INDEX_NONE) &&
			(BuiltData->SpriteAlignmentVariable == INDEX_NONE) && (BuiltData->PreviousSpriteAlignmentVariable == INDEX_NONE))
		{
			return;
		}

		if (bSpriteFacingEnabled)
		{
			BuiltData->SpriteFacing = BuildContext.ConvertDistributionToRange(SpriteFacing, FVector3f::ZeroVector);
		}
		if (bSpriteAlignmentEnabled)
		{
			BuiltData->SpriteAlignment	= BuildContext.ConvertDistributionToRange(SpriteAlignment, FVector3f::ZeroVector);
		}

		BuildContext.AddParticleSimulationExecSimulate(&UNiagaraStatelessModule_SpriteFacingAndAlignment::ParticleSimulate);
	}

	virtual void BuildShaderParameters(FNiagaraStatelessShaderParametersBuilder& ShaderParametersBuilder) const override
	{
		ShaderParametersBuilder.AddParameterNestedStruct<FParameters>();
	}

	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override
	{
		FParameters* Parameters = SetShaderParameterContext.GetParameterNestedStruct<FParameters>();
		const FModuleBuiltData* ModuleBuiltData = SetShaderParameterContext.ReadBuiltData<FModuleBuiltData>();

		Parameters->SpriteFacingAndAlignment_SpriteFacing		= SetShaderParameterContext.ConvertRangeToValue(ModuleBuiltData->SpriteFacing);
		Parameters->SpriteFacingAndAlignment_SpriteAlignment	= SetShaderParameterContext.ConvertRangeToValue(ModuleBuiltData->SpriteAlignment);
	}

	static void ParticleSimulate(const NiagaraStateless::FParticleSimulationContext& ParticleSimulationContext)
	{
		using namespace NiagaraStateless;

		const FModuleBuiltData* ModuleBuiltData = ParticleSimulationContext.ReadBuiltData<FModuleBuiltData>();
		const FParameters* Parameters = ParticleSimulationContext.ReadParameterNestedStruct<FParameters>();

		for (uint32 i = 0; i < ParticleSimulationContext.GetNumInstances(); ++i)
		{
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->SpriteFacingVariableOffset, i, Parameters->SpriteFacingAndAlignment_SpriteFacing);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousSpriteFacingVariableOffset, i, Parameters->SpriteFacingAndAlignment_SpriteFacing);

			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->SpriteAlignmentVariable, i, Parameters->SpriteFacingAndAlignment_SpriteAlignment);
			ParticleSimulationContext.WriteParticleVariable(ModuleBuiltData->PreviousSpriteAlignmentVariable, i, Parameters->SpriteFacingAndAlignment_SpriteAlignment);
		}
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		if (bSpriteFacingEnabled)
		{
			OutVariables.AddUnique(StatelessGlobals.SpriteFacingVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousSpriteFacingVariable);
		}
		if (bSpriteAlignmentEnabled)
		{
			OutVariables.AddUnique(StatelessGlobals.SpriteAlignmentVariable);
			OutVariables.AddUnique(StatelessGlobals.PreviousSpriteAlignmentVariable);
		}
	}
#endif
};
