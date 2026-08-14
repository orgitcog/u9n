// Copyright Epic Games, Inc. All Rights Reserved.

#include "Runtime/Engine/Private/Materials/MaterialIRValueAnalyzer.h"

#if WITH_EDITOR

#include "Materials/MaterialExternalCodeRegistry.h"
#include "Materials/MaterialExpressionUtils.h"
#include "Materials/MaterialIRModule.h"
#include "Materials/MaterialIRDebug.h"
#include "Materials/MaterialIR.h"
#include "Materials/MaterialIRTypes.h"
#include "Materials/MaterialIRInternal.h"
#include "Materials/MaterialInsights.h"
#include "Materials/Material.h"
#include "MaterialShared.h"
#include "Engine/Texture.h"
#include "UObject/Package.h"

static void AnalyzeExternalInput(FMaterialIRValueAnalyzer& Analyzer, MIR::FExternalInput* ExternalInput)
{
	FMaterialIRModule::FStatistics& Statistics = Analyzer.Module->GetStatistics();
	Statistics.ExternalInputUsedMask[MIR::Stage_Vertex][(int)ExternalInput->Id] = true;
	Statistics.ExternalInputUsedMask[MIR::Stage_Pixel][(int)ExternalInput->Id] = true;

	if (MIR::IsExternalInputTexCoordOrPartialDerivative(ExternalInput->Id))
	{
		int32 TexCoordIndex = MIR::ExternalInputToTexCoordIndex(ExternalInput->Id);
		Statistics.NumVertexTexCoords = FMath::Max(Statistics.NumVertexTexCoords, TexCoordIndex + 1);
		Statistics.NumPixelTexCoords = FMath::Max(Statistics.NumPixelTexCoords, TexCoordIndex + 1);
	}
}

static EMaterialShaderFrequency MapToMaterialShaderFrequencyOrAny(MIR::EStage Stage)
{
	switch (Stage)
	{
		case MIR::EStage::Stage_Vertex: return EMaterialShaderFrequency::Vertex;
		case MIR::EStage::Stage_Pixel: return EMaterialShaderFrequency::Pixel;
		case MIR::EStage::Stage_Compute: return EMaterialShaderFrequency::Compute;
	}
	return EMaterialShaderFrequency::Any;
}

static void PropagateStateInStageInlineHLSL(FMaterialIRValueAnalyzer& Analyzer, MIR::FInlineHLSL* InlineHLSL, MIR::EStage Stage)
{
	if (!InlineHLSL->HasFlags(MIR::EValueFlags::HasDynamicHLSLCode))
	{
		check(InlineHLSL->ExternalCodeDeclaration != nullptr);
		const EMaterialShaderFrequency StageToMaterialFrequency = MapToMaterialShaderFrequencyOrAny(Stage);
		for (const FMaterialExternalCodeEnvironmentDefine& EnvironmentDefine : InlineHLSL->ExternalCodeDeclaration->EnvironmentDefines)
		{
			if ((int32)(EnvironmentDefine.ShaderFrequency & StageToMaterialFrequency) != 0)
			{
				Analyzer.EnvironmentDefines.Add(EnvironmentDefine.Name);
			}
		}
	}
}

static void AnalyzeInlineHLSL(FMaterialIRValueAnalyzer& Analyzer, MIR::FInlineHLSL* InlineHLSL)
{
	if (!InlineHLSL->HasFlags(MIR::EValueFlags::HasDynamicHLSLCode))
	{
		check(InlineHLSL->ExternalCodeDeclaration != nullptr);

		// Validate this external code can be used for the current material domain. Empty list implies no restriction on material domains.
		if (!InlineHLSL->ExternalCodeDeclaration->Domains.IsEmpty() &&
			InlineHLSL->ExternalCodeDeclaration->Domains.Find(Analyzer.Material->MaterialDomain) == INDEX_NONE)
		{
			FName AssetPathName = Analyzer.Material->GetOutermost()->GetFName();
			Analyzer.Module->AddError(nullptr, MaterialExpressionUtils::FormatUnsupportedMaterialDomainError(*InlineHLSL->ExternalCodeDeclaration, AssetPathName));
		}
	}
}

static void AnalyzeTextureObject(FMaterialIRValueAnalyzer& Analyzer, MIR::FTextureObject* TextureObject)
{
	EMaterialTextureParameterType ParamType = MIR::Internal::TextureMaterialValueTypeToParameterType(TextureObject->Texture->GetMaterialType());

	FMaterialTextureParameterInfo ParamInfo {
		.ParameterInfo = { "", EMaterialParameterAssociation::GlobalParameter, INDEX_NONE },
		.TextureIndex = Analyzer.Material->GetReferencedTextures().Find(TextureObject->Texture),
		.SamplerSource = SSM_FromTextureAsset, // TODO - Is this needed?
	};
			
	check(ParamInfo.TextureIndex != INDEX_NONE);

	TextureObject->Analysis_UniformParameterIndex = Analyzer.CompilationOutput->UniformExpressionSet.FindOrAddTextureParameter(ParamType, ParamInfo);
}

static void AnalyzeTextureUniformParameter(FMaterialIRValueAnalyzer& Analyzer, MIR::FUniformParameter* Parameter)
{
	UTexture* Texture = Analyzer.Module->GetParameterMetadata(Parameter->ParameterIdInModule).Value.Texture;

	FMaterialTextureParameterInfo ParamInfo {
		.ParameterInfo = Analyzer.Module->GetParameterInfo(Parameter->ParameterIdInModule),
		.TextureIndex = Analyzer.Material->GetReferencedTextures().Find(Texture),
		.SamplerSource = SSM_FromTextureAsset, // TODO - Is this needed?
		.VirtualTextureLayerIndex = 0xff,
	};

	EMaterialTextureParameterType ParamType = MIR::Internal::TextureMaterialValueTypeToParameterType(Texture->GetMaterialType());
	
	Parameter->Analysis_UniformParameterIndex = Analyzer.CompilationOutput->UniformExpressionSet.FindOrAddTextureParameter(ParamType, ParamInfo);
}

static void AnalyzePrimitiveUniformParameter(FMaterialIRValueAnalyzer& Analyzer, MIR::FUniformParameter* Parameter)
{
	FUniformExpressionSet& UniformExpressionSet = Analyzer.CompilationOutput->UniformExpressionSet;
	FMaterialParameterInfo ParameterInfo = Analyzer.Module->GetParameterInfo(Parameter->ParameterIdInModule);
	FMaterialParameterMetadata ParameterMetadata = Analyzer.Module->GetParameterMetadata(Parameter->ParameterIdInModule);

	UE::Shader::FValue DefaultValue;
	switch (ParameterMetadata.Value.Type)
	{
		case EMaterialParameterType::Scalar: DefaultValue = ParameterMetadata.Value.AsScalar(); break;
		case EMaterialParameterType::Vector: DefaultValue = ParameterMetadata.Value.AsLinearColor(); break;
		default: UE_MIR_TODO();
	}

	// TODO: GetParameterOverrideValueForCurrentFunction?

	uint32 DefaultValueOffset;
	if (!MIR::Internal::Find(Analyzer.UniformDefaultValueOffsets, DefaultValue, DefaultValueOffset))
	{
		DefaultValueOffset = UniformExpressionSet.AddDefaultParameterValue(DefaultValue);
		Analyzer.UniformDefaultValueOffsets.Add(DefaultValue, DefaultValueOffset);
	}

	Parameter->Analysis_UniformParameterIndex = UniformExpressionSet.FindOrAddNumericParameter(ParameterMetadata.Value.Type, ParameterInfo, DefaultValueOffset);

	// Get the parameter type as primitive
	const MIR::FPrimitiveType* ParameterType = Parameter->Type->AsPrimitive();

	// Only int and float parameters supported for now (TODO)
	check(ParameterType->ScalarKind == MIR::ScalarKind_Int || ParameterType->ScalarKind == MIR::ScalarKind_Float);

	// Following code calculates GlobalComponentOffset.
	// GlobalComponentOffset is the i-th component in the array of float4s that make the uniform buffer.
	// For example a GlobalComponentOffset of 13 references PreshaderBuffer[3].y.
	// First, try to find an available sequence of free components in any previous allocation, in
	// order to reduce the number of allocations and thus preshader buffer memory footprint.
	// If the parameter type is too large and we can't find space for it in previous allocations,
	// allocate a new uniform buffer slot (a float4, 16 bytes) and put any unused component
	// in the appropriate freelist.

	uint32 GlobalComponentOffset = UINT32_MAX;
	uint32 NumComponents = ParameterType->GetNumComponents();
	check(NumComponents > 0);
	check(NumComponents <= 4); // Only vectors supported for now

	uint32 UsedNumComponents;

	for (UsedNumComponents = NumComponents; UsedNumComponents < 4; ++UsedNumComponents)
	{
		auto& FreeOffsetsForCurrentNumComponents = Analyzer.FreeOffsetsPerNumComponents[UsedNumComponents - 1];

		// If there are no available slots at this nubmer of components, try with a larger one
		if (FreeOffsetsForCurrentNumComponents.IsEmpty())
		{
			continue;
		}
		
		// There is space for this number `NumComponents` in a a chunk with `i` free components
		GlobalComponentOffset = FreeOffsetsForCurrentNumComponents.Last();
		FreeOffsetsForCurrentNumComponents.Pop();

		break;
	}
	
	// If UsedNumComponents is 4, it means we looked for all possible components sizes (1, 2 and 3) and
	// could not find a chunk that would fit this parameter. Allocate a new chunk.
	if (UsedNumComponents >= 4)
	{
		check(UsedNumComponents == 4); 
		GlobalComponentOffset = UniformExpressionSet.AllocateFromUniformBuffer(16) / 4;
	}

	// Finally, check whether in the used slot there's any slack left, and if so, record it in the appropriate free-list.
	int NumComponentsLeft = UsedNumComponents - NumComponents;
	if (NumComponentsLeft > 0)
	{
		int LeftOverOffset = GlobalComponentOffset + NumComponents;
		Analyzer.FreeOffsetsPerNumComponents[NumComponentsLeft - 1].Push(LeftOverOffset);
	}

	// Add the parameter evaluation to the uniform data
	UniformExpressionSet.AddNumericParameterEvaluation(Parameter->Analysis_UniformParameterIndex, GlobalComponentOffset);

	if (Analyzer.Insights)
	{
		// Push information about this parameter allocation to the insights.
		FMaterialInsights::FUniformParameterAllocationInsight ParamInsight;
		ParamInsight.BufferSlotIndex = GlobalComponentOffset / 4;
		ParamInsight.BufferSlotOffset = GlobalComponentOffset % 4;
		ParamInsight.ComponentsCount = NumComponents;
		ParamInsight.ParameterName = ParameterInfo.Name;

		switch (ParameterType->ScalarKind)
		{
			case MIR::ScalarKind_Int: ParamInsight.ComponentType = FMaterialInsights::FUniformBufferSlotComponentType::CT_Int; break;
			case MIR::ScalarKind_Float: ParamInsight.ComponentType = FMaterialInsights::FUniformBufferSlotComponentType::CT_Float; break;
			default: UE_MIR_UNREACHABLE();
		}

		Analyzer.Insights->UniformParameterAllocationInsights.Push(ParamInsight);
	}
}

static void AnalyzeUniformParameter(FMaterialIRValueAnalyzer& Analyzer, MIR::FUniformParameter* Parameter)
{
	if (Parameter->Type->IsObjectOfKind(MIR::ObjectKind_Texture2D))
	{
		AnalyzeTextureUniformParameter(Analyzer, Parameter);
	}
	else
	{
		check(Parameter->Type->AsPrimitive());
		AnalyzePrimitiveUniformParameter(Analyzer, Parameter);
	}
}

static void AnalyzeSetMaterialOutput(FMaterialIRValueAnalyzer& Analyzer, MIR::FSetMaterialOutput* SetMaterialOutput)
{
	if (SetMaterialOutput->Property == EMaterialProperty::MP_Normal)
	{
		if (SetMaterialOutput->HasSubgraphProperties(MIR::EGraphProperties::ReadsPixelNormal))
		{
			Analyzer.Module->AddError(nullptr, TEXT("Cannot set material attribute Normal to a value that depends on reading the pixel normal, as that would create a circular dependency."));
		}
	}
}


void FMaterialIRValueAnalyzer::Setup(UMaterial* InMaterial, FMaterialIRModule* InModule, FMaterialCompilationOutput* InCompilationOutput, FMaterialInsights* InInsights)
{
	Material = InMaterial;
	Module = InModule;
	CompilationOutput = InCompilationOutput;
	Insights = InInsights;
}

void FMaterialIRValueAnalyzer::Analyze(MIR::FValue* Value)
{
	for (MIR::FValue* Use : Value->GetUses())
	{
		if (Use)
		{
			Value->GraphProperties |= Use->GraphProperties;
		}
	}

	#define EXPAND_CASE(ValueType, ...) \
	case MIR::VK_##ValueType: \
	{ \
		Analyze##ValueType(*this, static_cast<MIR::F##ValueType*>(Value) , ## __VA_ARGS__); \
		break; \
	}

	switch (Value->Kind)
	{
		EXPAND_CASE(ExternalInput);
		EXPAND_CASE(TextureObject);
		EXPAND_CASE(UniformParameter);
		EXPAND_CASE(SetMaterialOutput);
		EXPAND_CASE(InlineHLSL);
		default: break;
	}

	#undef EXPAND_CASE
}

void FMaterialIRValueAnalyzer::PropagateStateInStage(MIR::FValue* Value, MIR::EStage Stage)
{
	#define EXPAND_CASE(ValueType) \
	case MIR::VK_##ValueType: \
	{ \
		PropagateStateInStage##ValueType(*this, static_cast<MIR::F##ValueType*>(Value), Stage); \
		break; \
	}

	switch (Value->Kind)
	{
		EXPAND_CASE(InlineHLSL);
		default: break;
	}

	#undef EXPAND_CASE
}
	
#endif // #if WITH_EDITOR
