// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraStatelessSetShaderParameterContext.h"

FNiagaraStatelessSetShaderParameterContext::FNiagaraStatelessSetShaderParameterContext(const FNiagaraStatelessSpaceTransforms& InSpaceTransforms, TConstArrayView<uint8> InRendererParameterData, TConstArrayView<uint8> InBuiltData, const FShaderParametersMetadata* InShaderParametersMetadata, uint8* InShaderParameters)
	: SpaceTransforms(InSpaceTransforms)
	, RendererParameterData(InRendererParameterData)
	, BuiltData(InBuiltData)
	, ShaderParametersBase(InShaderParameters)
	, ParameterOffset(0)
	, ShaderParametersMetadata(InShaderParametersMetadata)
{
}

void FNiagaraStatelessSetShaderParameterContext::TransformVectorRangeToScaleBias(const FNiagaraStatelessRangeVector3& Range, ENiagaraCoordinateSpace SourceSpace, FVector3f& OutScale, FVector3f& OutBias) const
{
	if (Range.ParameterOffset == INDEX_NONE)
	{
		OutScale = SpaceTransforms.TransformVector(SourceSpace, Range.GetScale());
		OutBias  = SpaceTransforms.TransformVector(SourceSpace, Range.Min);
	}
	else
	{
		OutScale = FNiagaraStatelessRangeDefaultValue<FVector3f>::Zero();
		OutBias  = SpaceTransforms.TransformVector(SourceSpace, GetRendererParameterValue(Range.ParameterOffset, Range.Min));
	}
}

FVector3f FNiagaraStatelessSetShaderParameterContext::TransformPositionRangeToValue(const FNiagaraStatelessRangeVector3& Range, ENiagaraCoordinateSpace SourceSpace) const
{
	return SpaceTransforms.TransformPosition(SourceSpace, GetRendererParameterValue(Range.ParameterOffset, Range.Min));
}

#if DO_CHECK
void FNiagaraStatelessSetShaderParameterContext::ValidateIncludeStructType(uint32 StructOffset, const FShaderParametersMetadata* StructMetaData) const
{
	for (const FShaderParametersMetadata::FMember& Member : ShaderParametersMetadata->GetMembers())
	{
		if (Member.GetOffset() != StructOffset)
		{
			continue;
		}

		if (Member.GetBaseType() == UBMT_INCLUDED_STRUCT && Member.GetStructMetadata() && Member.GetStructMetadata()->GetLayout() == StructMetaData->GetLayout())
		{
			return;
		}

		const TCHAR* StructType = Member.GetStructMetadata() ? Member.GetStructMetadata()->GetStructTypeName() : TEXT("null");
		UE_LOG(LogNiagara, Fatal, TEXT("Shader parameter struct member (%s) at offset (%u) is not of type (%s) struct type is (%s)"), Member.GetName(), StructOffset, StructMetaData->GetStructTypeName(), StructType);
		return;
	}

	UE_LOG(LogNiagara, Fatal, TEXT("Failed to find shader parameter struct member type (%s) at offset (%u)"), StructMetaData->GetStructTypeName(), StructOffset);
}
#endif //DO_CHECK
