// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/NiagaraStatelessParticleSimContext.h"

#include "NiagaraStatelessModule_ShapeLocation.generated.h"

UENUM()
enum class ENSM_ShapePrimitive : uint8
{
	Box,
	Cylinder,
	Plane,
	Ring,
	Sphere,
	Max UMETA(Hidden)
};

UENUM()
enum class ENSM_SurfaceExpansionMode : uint8
{
	Inner,
	Centered,
	Outside,
};

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Shape Location"))
class UNiagaraStatelessModule_ShapeLocation : public UNiagaraStatelessModule
{
	GENERATED_BODY()

public:
	using FParameters = NiagaraStateless::FShapeLocationModule_ShaderParameters;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (ShowInStackItemHeader, StackItemHeaderAlignment = "Left"))
	ENSM_ShapePrimitive ShapePrimitive = ENSM_ShapePrimitive::Sphere;

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	FNiagaraDistributionRangeVector3 BoxSize = FNiagaraDistributionRangeVector3(FVector3f(100.0f));
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box"))
	bool bBoxSurfaceOnly = false;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box && bBoxSurfaceOnly"))
	ENSM_SurfaceExpansionMode BoxSurfaceExpansion = ENSM_SurfaceExpansionMode::Centered;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Box && bBoxSurfaceOnly"))
	FNiagaraDistributionRangeFloat BoxSurfaceThickness = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Plane"))
	FNiagaraDistributionRangeVector2 PlaneSize = FNiagaraDistributionRangeVector2(FVector2f(100.0f));
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Plane"))
	bool bPlaneEdgesOnly = false;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Plane && bPlaneEdgesOnly"))
	ENSM_SurfaceExpansionMode PlaneEdgeExpansion = ENSM_SurfaceExpansionMode::Centered;
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Plane && bPlaneEdgesOnly"))
	FNiagaraDistributionRangeFloat PlaneEdgeThickness = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	FNiagaraDistributionRangeFloat CylinderHeight = FNiagaraDistributionRangeFloat(100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	FNiagaraDistributionRangeFloat CylinderRadius = FNiagaraDistributionRangeFloat(100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Cylinder"))
	FNiagaraDistributionRangeFloat CylinderHeightMidpoint = FNiagaraDistributionRangeFloat(0.5f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring"))
	FNiagaraDistributionRangeFloat RingRadius = FNiagaraDistributionRangeFloat(100.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	FNiagaraDistributionRangeFloat DiscCoverage = FNiagaraDistributionRangeFloat(0.0f);
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisableRangeDistribution, EditConditionHides, EditCondition = "ShapePrimitive == ENSM_ShapePrimitive::Ring", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	FNiagaraDistributionRangeFloat RingUDistribution = FNiagaraDistributionRangeFloat(0.0f);

	UPROPERTY(EditAnywhere, Category = "Parameters", meta=(EditConditionHides, EditCondition="ShapePrimitive == ENSM_ShapePrimitive::Sphere"))
	FNiagaraDistributionRangeFloat SphereRadius = FNiagaraDistributionRangeFloat(100.0f);

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	float SphereMin_DEPRECATED = 0.0f;
	UPROPERTY()
	float SphereMax_DEPRECATED = 100.0f;
#endif

public:
	virtual void BuildEmitterData(const FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override;
	virtual void BuildShaderParameters(FNiagaraStatelessShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const override;

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }

	virtual bool CanDebugDraw() const override { return true; }
	virtual void DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const override;
#endif
#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const override
	{
		const FNiagaraStatelessGlobals& StatelessGlobals = FNiagaraStatelessGlobals::Get();
		OutVariables.AddUnique(StatelessGlobals.PositionVariable);
		OutVariables.AddUnique(StatelessGlobals.PreviousPositionVariable);
	}

	virtual void PostLoad() override;
#endif
};
