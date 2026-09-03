// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Curves/CurveLinearColorAtlas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "MaterialDomain.h"
#include "MaterialExpressionIO.h"
#include "Materials/Material.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionAbsorptionMediumMaterialOutput.h"
#include "Materials/MaterialExpressionActorPositionWS.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionAntialiasedTextureMask.h"
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionArccosine.h"
#include "Materials/MaterialExpressionArccosineFast.h"
#include "Materials/MaterialExpressionArcsine.h"
#include "Materials/MaterialExpressionArcsineFast.h"
#include "Materials/MaterialExpressionArctangent.h"
#include "Materials/MaterialExpressionArctangent2.h"
#include "Materials/MaterialExpressionArctangent2Fast.h"
#include "Materials/MaterialExpressionArctangentFast.h"
#include "Materials/MaterialExpressionAtmosphericLightColor.h"
#include "Materials/MaterialExpressionAtmosphericLightVector.h"
#include "Materials/MaterialExpressionBentNormalCustomOutput.h"
#include "Materials/MaterialExpressionBlackBody.h"
#include "Materials/MaterialExpressionBlendMaterialAttributes.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionBumpOffset.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCeil.h"
#include "Materials/MaterialExpressionChannelMaskParameter.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionClearCoatNormalCustomOutput.h"
#include "Materials/MaterialExpressionCloudLayer.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionColorRamp.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionConstantBiasScale.h"
#include "Materials/MaterialExpressionConvert.h"
#include "Materials/MaterialExpressionCosine.h"
#include "Materials/MaterialExpressionCrossProduct.h"
#include "Materials/MaterialExpressionCurveAtlasRowParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionDataDrivenShaderPlatformInfoSwitch.h"
#include "Materials/MaterialExpressionDBufferTexture.h"
#include "Materials/MaterialExpressionDDX.h"
#include "Materials/MaterialExpressionDDY.h"
#include "Materials/MaterialExpressionDecalColor.h"
#include "Materials/MaterialExpressionDecalDerivative.h"
#include "Materials/MaterialExpressionDecalLifetimeOpacity.h"
#include "Materials/MaterialExpressionDecalMipmapLevel.h"
#include "Materials/MaterialExpressionDeltaTime.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionDepthOfFieldFunction.h"
#include "Materials/MaterialExpressionDeriveNormalZ.h"
#include "Materials/MaterialExpressionDesaturation.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDistanceCullFade.h"
#include "Materials/MaterialExpressionDistanceFieldApproxAO.h"
#include "Materials/MaterialExpressionDistanceFieldGradient.h"
#include "Materials/MaterialExpressionDistanceFieldsRenderingSwitch.h"
#include "Materials/MaterialExpressionDistanceToNearestSurface.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionDoubleVectorParameter.h"
#include "Materials/MaterialExpressionDynamicParameter.h"
#include "Materials/MaterialExpressionExponential.h"
#include "Materials/MaterialExpressionExponential2.h"
#include "Materials/MaterialExpressionEyeAdaptation.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionFeatureLevelSwitch.h"
#include "Materials/MaterialExpressionFloor.h"
#include "Materials/MaterialExpressionFmod.h"
#include "Materials/MaterialExpressionAtmosphericFogColor.h"
#include "Materials/MaterialExpressionFontSample.h"
#include "Materials/MaterialExpressionFrac.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionGenericConstant.h"
#include "Materials/MaterialExpressionGetMaterialAttributes.h"
#include "Materials/MaterialExpressionGIReplace.h"
#include "Materials/MaterialExpressionHairAttributes.h"
#include "Materials/MaterialExpressionHairColor.h"
#include "Materials/MaterialExpressionHsvToRgb.h"
#include "Materials/MaterialExpressionIf.h"
#include "Materials/MaterialExpressionIfThenElse.h"
#include "Materials/MaterialExpressionInverseLinearInterpolate.h"
#include "Materials/MaterialExpressionIsOrthographic.h"
#include "Materials/MaterialExpressionLength.h"
#include "Materials/MaterialExpressionLightmapUVs.h"
#include "Materials/MaterialExpressionLightVector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionLightmassReplace.h"
#include "Materials/MaterialExpressionLocalPosition.h"
#include "Materials/MaterialExpressionLogarithm.h"
#include "Materials/MaterialExpressionLogarithm10.h"
#include "Materials/MaterialExpressionLogarithm2.h"
#include "Materials/MaterialExpressionMakeMaterialAttributes.h"
#include "Materials/MaterialExpressionMapARPassthroughCameraUV.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionMaterialProxyReplace.h"
#include "Materials/MaterialExpressionMax.h"
#include "Materials/MaterialExpressionMin.h"
#include "Materials/MaterialExpressionModulo.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionNaniteReplace.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionNormalize.h"
#include "Materials/MaterialExpressionNeuralPostProcessNode.h"
#include "Materials/MaterialExpressionObjectBounds.h"
#include "Materials/MaterialExpressionObjectLocalBounds.h"
#include "Materials/MaterialExpressionBounds.h"
#include "Materials/MaterialExpressionObjectOrientation.h"
#include "Materials/MaterialExpressionObjectPositionWS.h"
#include "Materials/MaterialExpressionObjectRadius.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionParticleColor.h"
#include "Materials/MaterialExpressionParticleDirection.h"
#include "Materials/MaterialExpressionParticleMacroUV.h"
#include "Materials/MaterialExpressionParticleMotionBlurFade.h"
#include "Materials/MaterialExpressionParticlePositionWS.h"
#include "Materials/MaterialExpressionParticleRadius.h"
#include "Materials/MaterialExpressionParticleRandom.h"
#include "Materials/MaterialExpressionParticleRelativeTime.h"
#include "Materials/MaterialExpressionParticleSize.h"
#include "Materials/MaterialExpressionParticleSpeed.h"
#include "Materials/MaterialExpressionParticleSubUVProperties.h"
#include "Materials/MaterialExpressionPathTracingBufferTexture.h"
#include "Materials/MaterialExpressionPathTracingQualitySwitch.h"
#include "Materials/MaterialExpressionPathTracingRayTypeSwitch.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionPerInstanceFadeAmount.h"
#include "Materials/MaterialExpressionPerInstanceRandom.h"
#include "Materials/MaterialExpressionPixelDepth.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionPrecomputedAOMask.h"
#include "Materials/MaterialExpressionPreSkinnedLocalBounds.h"
#include "Materials/MaterialExpressionPreSkinnedNormal.h"
#include "Materials/MaterialExpressionPreSkinnedPosition.h"
#include "Materials/MaterialExpressionPreviousFrameSwitch.h"
#include "Materials/MaterialExpressionSamplePhysicsField.h"
#include "Materials/MaterialExpressionSphericalParticleOpacity.h"
#include "Materials/MaterialExpressionQualitySwitch.h"
#include "Materials/MaterialExpressionRayTracingQualitySwitch.h"
#include "Materials/MaterialExpressionReflectionCapturePassSwitch.h"
#include "Materials/MaterialExpressionReflectionVectorWS.h"
#include "Materials/MaterialExpressionReroute.h"
#include "Materials/MaterialExpressionRgbToHsv.h"
#include "Materials/MaterialExpressionRotateAboutAxis.h"
#include "Materials/MaterialExpressionRotator.h"
#include "Materials/MaterialExpressionRound.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureOutput.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureReplace.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSample.h"
#include "Materials/MaterialExpressionRuntimeVirtualTextureSampleParameter.h"
#include "Materials/MaterialExpressionSaturate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSceneColor.h"
#include "Materials/MaterialExpressionSceneDepth.h"
#include "Materials/MaterialExpressionSceneDepthWithoutWater.h"
#include "Materials/MaterialExpressionSceneTexelSize.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionShaderStageSwitch.h"
#include "Materials/MaterialExpressionShadingModel.h"
#include "Materials/MaterialExpressionShadingPathSwitch.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionSign.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightDirection.h"
#include "Materials/MaterialExpressionSkyAtmosphereLightIlluminance.h"
#include "Materials/MaterialExpressionSkyAtmosphereViewLuminance.h"
#include "Materials/MaterialExpressionSkyLightEnvMapSample.h"
#include "Materials/MaterialExpressionSmoothStep.h"
#include "Materials/MaterialExpressionSobol.h"
#include "Materials/MaterialExpressionSpeedTree.h"
#include "Materials/MaterialExpressionSphereMask.h"
#include "Materials/MaterialExpressionSquareRoot.h"
#include "Materials/MaterialExpressionSRGBColorToWorkingColorSpace.h"
#include "Materials/MaterialExpressionStaticBool.h"
#include "Materials/MaterialExpressionStaticComponentMaskParameter.h"
#include "Materials/MaterialExpressionStaticSwitch.h"
#include "Materials/MaterialExpressionStaticSwitchParameter.h"
#include "Materials/MaterialExpressionStep.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionSwitch.h"
#include "Materials/MaterialExpressionTangent.h"
#include "Materials/MaterialExpressionTangentOutput.h"
#include "Materials/MaterialExpressionTemporalSobol.h"
#include "Materials/MaterialExpressionTextureCollection.h"
#include "Materials/MaterialExpressionTextureCollectionParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObject.h"
#include "Materials/MaterialExpressionTextureObjectFromCollection.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureProperty.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameterCube.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionTransformPosition.h"
#include "Materials/MaterialExpressionTruncate.h"
#include "Materials/MaterialExpressionTruncateLWC.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionVertexInterpolator.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Materials/MaterialExpressionVertexTangentWS.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionViewSize.h"
#include "Materials/MaterialExpressionVirtualTextureFeatureSwitch.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialInput.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExternalCodeRegistry.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialExpressionOperator.h"
#include "MaterialShared.h"
#include "Misc/MemStackUtility.h"
#include "RenderUtils.h"
#include "ColorManagement/ColorSpace.h"

#include "Materials/MaterialIREmitter.h"

using FValueRef = MIR::FValueRef;

/* Constants */

void UMaterialExpression::Build(MIR::FEmitter& Em)
{
	Em.Error(TEXT("Unsupported material expression."));
} 

void UMaterialExpressionFunctionInput::Build(MIR::FEmitter& Em)
{
	FValueRef OutputValue = Em.TryInput(&Preview);
	if (OutputValue)
	{
		Em.Output(0, OutputValue);
		return;
	}

	switch (InputType)
	{
		case FunctionInput_Scalar:
			OutputValue = Em.ConstantFloat(PreviewValue.X);
			break;

		case FunctionInput_Vector2:
			OutputValue = Em.ConstantFloat2({ PreviewValue.X, PreviewValue.Y });
			break;

		case FunctionInput_Vector3:
			OutputValue = Em.ConstantFloat3({ PreviewValue.X, PreviewValue.Y, PreviewValue.Z });
			break;

		case FunctionInput_Vector4:
			OutputValue = Em.ConstantFloat4(PreviewValue);
			break;

		case FunctionInput_Bool:
		case FunctionInput_StaticBool:
			OutputValue = Em.ConstantBool(PreviewValue.X != 0.0f);

		case FunctionInput_Texture2D:
		case FunctionInput_TextureCube:
		case FunctionInput_Texture2DArray:
		case FunctionInput_VolumeTexture:
		case FunctionInput_MaterialAttributes:
		case FunctionInput_TextureExternal:
		case FunctionInput_Substrate:
			Em.Error(TEXT("Function input of object type requires preview input to be provided."));
			return;

		default:
			UE_MIR_UNREACHABLE();
	}

	Em.Output(0, OutputValue);
}

void UMaterialExpressionFunctionOutput::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Input(&A));
}

void UMaterialExpressionConstant::Build(MIR::FEmitter& Em)
{
	FValueRef Value = Em.ConstantFloat(R);
	Em.Output(0, Value);
}

void UMaterialExpressionConstant2Vector::Build(MIR::FEmitter& Em)
{
	FValueRef Value = Em.ConstantFloat2({ R, G });
	Em.Output(0, Value);
	for (int i = 0; i < 2; ++i)
	{
		Em.Output(i + 1, Em.Subscript(Value, i));
	}
}

void UMaterialExpressionConstant3Vector::Build(MIR::FEmitter& Em)
{
	FValueRef Value = Em.ConstantFloat3({ Constant.R, Constant.G, Constant.B });
	Em.Output(0, Value);
	for (int i = 0; i < 3; ++i)
	{
		Em.Output(i + 1, Em.Subscript(Value, i));
	}
}

void UMaterialExpressionConstant4Vector::Build(MIR::FEmitter& Em)
{
	FValueRef Value = Em.ConstantFloat4(Constant);
	Em.Output(0, Value);
	for (int i = 0; i < 4; ++i)
	{
		Em.Output(i + 1, Em.Subscript(Value, i));
	}
}

void UMaterialExpressionStaticBool::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.ConstantBool(Value));
}

static FValueRef BuildMaterialExpressionParameter(MIR::FEmitter& Em, UMaterialExpressionParameter* ParameterExpr)
{
	FMaterialParameterMetadata Metadata;
	if (!ParameterExpr->GetParameterValue(Metadata))
	{
		Em.Error(TEXT("Could not get parameter value."));
		return Em.Poison();
	}

	return Em.Parameter(ParameterExpr->GetParameterName(), Metadata);
}

void UMaterialExpressionParameter::Build(MIR::FEmitter& Em)
{
	Em.Output(0, BuildMaterialExpressionParameter(Em, this));
}

void UMaterialExpressionVectorParameter::Build(MIR::FEmitter& Em)
{
	FValueRef Value = BuildMaterialExpressionParameter(Em, this);
	Em.Output(0, Value);
	Em.Output(1, Em.Subscript(Value, 0));
	Em.Output(2, Em.Subscript(Value, 1));
	Em.Output(3, Em.Subscript(Value, 2));
	Em.Output(4, Em.Subscript(Value, 3));
}

void UMaterialExpressionChannelMaskParameter::Build(MIR::FEmitter& Em)
{
	FValueRef DotResult = Em.Dot(
		Em.CastToFloat(Em.Input(&Input), 4),
		BuildMaterialExpressionParameter(Em, this));

	Em.Output(0, DotResult);
	Em.Output(1, Em.Subscript(DotResult, 1));
	Em.Output(2, Em.Subscript(DotResult, 2));
	Em.Output(3, Em.Subscript(DotResult, 3));
	Em.Output(4, Em.Subscript(DotResult, 4));
}

void UMaterialExpressionStaticBoolParameter::Build(MIR::FEmitter& Em)
{
	FValueRef Value = BuildMaterialExpressionParameter(Em, this);
	Em.ToConstantBool(Value); // Check that it is a constant boolean
	Em.Output(0, Value);
}

void UMaterialExpressionStaticSwitch::Build(MIR::FEmitter& Em)
{
	bool bCondition = Em.ConstantBool(Em.InputDefaultBool(&Value, DefaultValue));
	UE_MIR_CHECKPOINT(Em); // Make sure that evaluating bCondition didn't raise an error

	Em.Output(0, Em.Input(bCondition ? &A : &B));
}

void UMaterialExpressionStaticSwitchParameter::Build(MIR::FEmitter& Em)
{
	bool bCondition = Em.ToConstantBool(BuildMaterialExpressionParameter(Em, this));
	UE_MIR_CHECKPOINT(Em); // Make sure that evaluating bCondition didn't raise an error

	Em.Output(0, Em.Input(bCondition ? &A : &B));
}

void UMaterialExpressionAppendVector::Build(MIR::FEmitter& Em)
{
	FValueRef AVal = Em.CheckIsScalarOrVector(Em.Input(&A));
	FValueRef BVal = Em.CheckIsScalarOrVector(Em.TryInput(&B));

	UE_MIR_CHECKPOINT(Em);

	const MIR::FPrimitiveType* AType = AVal->Type->AsPrimitive();
	const MIR::FPrimitiveType* BType = BVal ? BVal->Type->AsPrimitive() : nullptr;

	int Dimensions = AType->NumRows + (BType ? BType->NumRows : 0);
	if (Dimensions > 4)
	{
		Em.Errorf(TEXT("The resulting vector would have %d component (it can have at most 4)."), Dimensions);
		return;
	}

	check(Dimensions >= 2 && Dimensions <= 4);

	// Construct the output vector type.
	const MIR::FPrimitiveType* ResultType = MIR::FPrimitiveType::GetVector(MIR::ScalarKind_Float, Dimensions);

	// Set up each output vector component 
	FValueRef Components[4] = { nullptr, nullptr, nullptr, nullptr };
	int ComponentIndex = 0;
	for (int i = 0; i < AType->NumRows; ++i, ++ComponentIndex)
	{
		Components[ComponentIndex] = Em.Subscript(AVal, i);
	}

	if (BType)
	{
		for (int i = 0; i < BType->NumRows; ++i, ++ComponentIndex)
		{
			Components[ComponentIndex] = Em.Subscript(BVal, i);
		}
	}

	// Create the vector value and output it.
	FValueRef Output{};
	if (Dimensions == 2)
	{
		Output = Em.Vector2(Components[0], Components[1]);
	}
	else if (Dimensions == 3)
	{
		Output = Em.Vector3(Components[0], Components[1], Components[2]);
	}
	else
	{
		Output = Em.Vector4(Components[0], Components[1], Components[2], Components[3]);
	}

	Em.Output(0, Output);
}

/* Unary Operators */

void UMaterialExpressionAbs::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Abs(Em.Input(&Input)));
}

void UMaterialExpressionCeil::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Ceil(Em.Input(&Input)));
}

void UMaterialExpressionFloor::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Floor(Em.Input(&Input)));
}

void UMaterialExpressionFrac::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Frac(Em.Input(&Input)));
}

void UMaterialExpressionLength::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Length(Em.Input(&Input)));
}

void UMaterialExpressionRound::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Round(Em.Input(&Input)));
}

void UMaterialExpressionExponential::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Exponential(Em.Input(&Input)));
}

void UMaterialExpressionExponential2::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Exponential2(Em.Input(&Input)));
}

void UMaterialExpressionLogarithm::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Logarithm(Em.Input(&Input)));
}

void UMaterialExpressionLogarithm2::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Logarithm2(Em.Input(&X)));
}

void UMaterialExpressionLogarithm10::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Logarithm10(Em.Input(&X)));
}

void UMaterialExpressionTruncate::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Truncate(Em.Input(&Input)));
}

void UMaterialExpressionArccosine::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.ACos(Em.Input(&Input)));
}

void UMaterialExpressionArcsine::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.ASin(Em.Input(&Input)));
}

void UMaterialExpressionArctangent::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.ATan(Em.Input(&Input)));
}

void UMaterialExpressionComponentMask::Build(MIR::FEmitter& Emitter)
{
	FValueRef Value = Emitter.Input(&Input);

	MIR::FSwizzleMask Mask;
	if (R)
	{
		Mask.Components[Mask.NumComponents++] = MIR::EVectorComponent::X;
	}
	if (G)
	{
		Mask.Components[Mask.NumComponents++] = MIR::EVectorComponent::Y;
	}
	if (B)
	{
		Mask.Components[Mask.NumComponents++] = MIR::EVectorComponent::Z;
	}
	if (A)
	{
		Mask.Components[Mask.NumComponents++] = MIR::EVectorComponent::W;
	}

	Emitter.Output(0, Emitter.Swizzle(Value, Mask));
}


static FValueRef GetTrigonometricInputWithPeriod(MIR::FEmitter& Em, const FExpressionInput* Input, float Period)
{
	// Get input after checking it has primitive type.
	FValueRef Value = Em.CheckIsArithmetic(Em.Input(Input));
	if (Period > 0.0f)
	{
		Value = Em.Multiply(Value, Em.ConstantFloat(2.0f * (float)UE_PI / Period));
	}
	return Value;
}

void UMaterialExpressionCosine::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Cos(GetTrigonometricInputWithPeriod(Em, &Input, Period)));
}

void UMaterialExpressionSine::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Sin(GetTrigonometricInputWithPeriod(Em, &Input, Period)));
}

void UMaterialExpressionTangent::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Tan(GetTrigonometricInputWithPeriod(Em, &Input, Period)));
}

void UMaterialExpressionSaturate::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Saturate(Em.Input(&Input)));
}

void UMaterialExpressionSquareRoot::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Sqrt(Em.Input(&Input)));
}

static FValueRef EmitInlineHLSL(MIR::FEmitter& Em, const UMaterialExpressionExternalCodeBase& InExternalCodeExpression, int32 InExternalCodeIdentifierIndex, TConstArrayView<FValueRef> InArguments)
{
	checkf(InExternalCodeIdentifierIndex >= 0 && InExternalCodeIdentifierIndex < InExternalCodeExpression.ExternalCodeIdentifiers.Num(),
		TEXT("External code identifier index (%d) out of bounds; Upper bound is %d"), InExternalCodeIdentifierIndex, InExternalCodeExpression.ExternalCodeIdentifiers.Num());

	const FName& ExternalCodeIdentifier = InExternalCodeExpression.ExternalCodeIdentifiers[InExternalCodeIdentifierIndex];
	const FMaterialExternalCodeDeclaration* ExternalCodeDeclaration = MaterialExternalCodeRegistry::Get().FindExternalCode(ExternalCodeIdentifier);
	if (!ExternalCodeDeclaration)
	{
		Em.Errorf(TEXT("Missing external code declaration for '%s' [Index=%d]"), *ExternalCodeIdentifier.ToString(), InExternalCodeIdentifierIndex);
		return {};
	}

	return Em.InlineHLSL(ExternalCodeDeclaration, InArguments);
}

static void BuildInlineHLSLOutput(MIR::FEmitter& Em, const UMaterialExpressionExternalCodeBase& InExternalCodeExpression, TConstArrayView<FValueRef> InArguments)
{
	for (int32 OutputIndex = 0; OutputIndex < InExternalCodeExpression.ExternalCodeIdentifiers.Num(); ++OutputIndex)
	{
		Em.Output(OutputIndex, EmitInlineHLSL(Em, InExternalCodeExpression, OutputIndex, InArguments));
	}
}

void UMaterialExpressionExternalCodeBase::Build(MIR::FEmitter& Em)
{
	BuildInlineHLSLOutput(Em, *this, {});
}

/* Binary Operators */

void UMaterialExpressionDesaturation::Build(MIR::FEmitter& Em)
{
	FValueRef ColorValue = Em.CastToFloat(Em.Input(&Input), 3);
	FValueRef GreyOrLerpValue = Em.Dot(ColorValue, Em.ConstantFloat3(FVector3f(LuminanceFactors))); // todo: check
	FValueRef FractionValue = Em.TryInput(&Fraction);
	if (FractionValue)
	{
		GreyOrLerpValue = Em.Lerp(ColorValue, GreyOrLerpValue, FractionValue);
	}
	Em.Output(0, GreyOrLerpValue);
}

void UMaterialExpressionDistance::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Length(Em.Subtract(Em.Input(&A), Em.Input(&B))));
}

void UMaterialExpressionFmod::Build(MIR::FEmitter& Em)
{ 
	Em.Output(0, Em.Fmod(Em.Input(&A), Em.Input(&B)));
}

static void BuildBinaryOperatorWithDefaults(MIR::FEmitter& Em, MIR::EOperator Op, const FExpressionInput* A, float ConstA, const FExpressionInput* B, float ConstB)
{
	FValueRef AVal = Em.InputDefaultFloat(A, ConstA);
	FValueRef BVal = Em.InputDefaultFloat(B, ConstB);
	Em.Output(0, Em.Operator(Op, AVal, BVal));
}

void UMaterialExpressionAdd::Build(MIR::FEmitter& Em)
{ 
	BuildBinaryOperatorWithDefaults(Em, MIR::BO_Add, &A, ConstA, &B, ConstB);
}

void UMaterialExpressionSubtract::Build(MIR::FEmitter& Em)
{
	BuildBinaryOperatorWithDefaults(Em, MIR::BO_Subtract, &A, ConstA, &B, ConstB);
}

void UMaterialExpressionMultiply::Build(MIR::FEmitter& Em)
{
	BuildBinaryOperatorWithDefaults(Em, MIR::BO_Multiply, &A, ConstA, &B, ConstB);
}

void UMaterialExpressionDivide::Build(MIR::FEmitter& Em)
{
	BuildBinaryOperatorWithDefaults(Em, MIR::BO_Divide, &A, ConstA, &B, ConstB);
}

void UMaterialExpressionMin::Build(MIR::FEmitter& Em)
{ 
	BuildBinaryOperatorWithDefaults(Em, MIR::BO_Min, &A, ConstA, &B, ConstB);
}

void UMaterialExpressionDotProduct::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Dot(Em.Input(&A), Em.Input(&B)));
}

void UMaterialExpressionCrossProduct::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.Cross(Em.Input(&A), Em.Input(&B)));
}

void UMaterialExpressionEyeAdaptationInverse::Build(MIR::FEmitter& Em)
{
	check(ExternalCodeIdentifiers.Num() == 1);
	FValueRef LightValue = Em.CastToFloat(Em.InputDefaultFloat(&LightValueInput, 1.0f), 3);
	FValueRef AlphaValue = Em.CastToFloat(Em.InputDefaultFloat(&AlphaInput, 1.0f), 1);
	FValueRef MultiplierValue = EmitInlineHLSL(Em, *this, 0, { AlphaValue });
	Em.Output(0, Em.Multiply(LightValue, MultiplierValue));
}

void UMaterialExpressionOneMinus::Build(MIR::FEmitter& Em)
{
	// Default input to zero if not connected, then get it as a primitive.
	FValueRef Value = Em.InputDefaultFloat(&Input, 0.0f);

	// Make a "One" value of the same type and dimension as input's.
	FValueRef One = Em.ConstantOne(Value->Type->AsPrimitive()->ScalarKind);

	// And flow the subtraction out of the expression's only output.
	Em.Output(0, Em.Subtract(One, Value));
}

void UMaterialExpressionIfThenElse::Build(MIR::FEmitter& Em)
{
	// Get the condition value checking it is a bool scalar
	FValueRef ConditionValue = Em.CastToBool(Em.InputDefaultBool(&Condition, false), 1);

	UE_MIR_CHECKPOINT(Em); // Make sure the condition value is valid
	
	// If condition boolean is constant, select which input is active and simply
	// bypass its value to our output.
	if (MIR::FConstant* Constant = ConditionValue->As<MIR::FConstant>())
	{
		FExpressionInput* ActiveInput = Constant->Boolean ? &True : &False;
		Em.Output(0, Em.Input(ActiveInput));
		return;
	}

	// The condition isn't static; Get the true and false values.
	// If any is disconnected, the emitter will report an error.
	FValueRef ThenValue = Em.Input(&True);
	FValueRef ElseValue = Em.Input(&False);
	
	const MIR::FType* CommonType = Em.GetCommonType(ThenValue->Type, ElseValue->Type);

	UE_MIR_CHECKPOINT(Em); // Make sure the common type is valid

	// Cast the "then" and "else" values to the common type.
	ThenValue = Em.Cast(ThenValue, CommonType);
	ElseValue = Em.Cast(ElseValue, CommonType);

	// Emit the branch instruction
	FValueRef OutputValue = Em.Branch(ConditionValue, ThenValue, ElseValue);

	Em.Output(0, OutputValue);
}

static FValueRef EmitAlmostEquals(MIR::FEmitter& Em, FValueRef A, FValueRef B, float Threshold)
{
	// abs(A - B) <= Threshold
	return Em.LessThanOrEquals(Em.Abs(Em.Subtract(A, B)), Em.ConstantFloat(Threshold));
}

void UMaterialExpressionIf::Build(MIR::FEmitter& Em)
{
	// Get input values and check their types are what we expect.
	FValueRef AValue = Em.CheckIsScalar(Em.InputDefaultFloat(&A, 0.f));
	FValueRef BValue = Em.CheckIsScalar(Em.InputDefaultFloat(&B, ConstB));
	FValueRef AGreaterThanBValue = Em.InputDefaultFloat(&AGreaterThanB, 0.f);
	FValueRef AEqualsBValue = Em.InputDefaultFloat(&AEqualsB, 0.f);
	FValueRef ALessThanBValue = Em.InputDefaultFloat(&ALessThanB, 0.f);

	// Emit the comparison expressions.
	FValueRef ALessThanBConditionValue = Em.LessThan(AValue, BValue);
	FValueRef AEqualsBConditionValue = EmitAlmostEquals(Em, AValue, BValue, EqualsThreshold);

	// And finally emit the full conditional expression.
	FValueRef OutputValue = Em.Branch(AEqualsBConditionValue, AEqualsBValue, AGreaterThanBValue);
	OutputValue = Em.Branch(ALessThanBConditionValue, ALessThanBValue, OutputValue);

	Em.Output(0, OutputValue);
}

void UMaterialExpressionSphericalParticleOpacity::Build(MIR::FEmitter& Em)
{
	FValueRef DensityValue = Em.InputDefaultFloat(&Density, ConstantDensity);
	UE_MIR_CHECKPOINT(Em); // Early out in case of errors
	BuildInlineHLSLOutput(Em, *this, { DensityValue });
}

void UMaterialExpressionTextureObject::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.TextureObject(Texture, SamplerType));
}

static MIR::ETextureReadMode TextureGatherModeToMIR(::ETextureGatherMode Mode)
{
	switch (Mode)
	{
		case TGM_Red: return MIR::ETextureReadMode::GatherRed;
		case TGM_Green: return MIR::ETextureReadMode::GatherGreen;
		case TGM_Blue: return MIR::ETextureReadMode::GatherBlue;
		case TGM_Alpha: return MIR::ETextureReadMode::GatherAlpha;
		default: UE_MIR_UNREACHABLE();
	}
}

static void BuildTextureSampleExpression(MIR::FEmitter& Em, UMaterialExpressionTextureSample* Expr, FValueRef Texture)
{
	FValueRef TexCoords = Em.TryInput(&Expr->Coordinates);
	if (!TexCoords)
	{
		TexCoords = Em.ExternalInput(MIR::TexCoordIndexToExternalInput(Expr->ConstCoordinate));
	}

	FValueRef TextureRead{};
	if (Expr->GatherMode != TGM_None)
	{
		if (Expr->MipValueMode != TMVM_None)
		{
			Em.Errorf(TEXT("Texture gather does not support mipmap overrides (it implicitly accesses a specific mip)."));
			return;
		}

		TextureRead = Em.TextureGather(Texture, TexCoords, TextureGatherModeToMIR(Expr->GatherMode), Expr->SamplerSource, Expr->SamplerType);
	} 
	else
	{
		// Determine if automatic view mip bias should be used, by trying ot acquire a its input as a static boolean.
		bool bAutomaticViewMipBias = Em.ToConstantBool(Em.InputDefaultBool(&Expr->AutomaticViewMipBiasValue, Expr->AutomaticViewMipBias));
	
		UE_MIR_CHECKPOINT(Em);

		// Get the mip value level (either through the expression input or using the given constant if disconnected).
		FValueRef MipValue = (Expr->MipValueMode == TMVM_MipLevel || Expr->MipValueMode == TMVM_MipBias)
			? Em.CheckIsScalar(Em.InputDefaultInt(&Expr->MipValue, Expr->ConstMipValue))
			: Em.Poison();

		switch (Expr->MipValueMode)
		{
			case TMVM_None:
				TextureRead = Em.TextureSample(Texture, TexCoords, bAutomaticViewMipBias, Expr->SamplerSource, Expr->SamplerType);
				break;
		
			case TMVM_MipBias:
				TextureRead = Em.TextureSampleBias(Texture, TexCoords, MipValue, bAutomaticViewMipBias, Expr->SamplerSource, Expr->SamplerType);
				break;
		
			case TMVM_MipLevel:
				TextureRead = Em.TextureSampleLevel(Texture, TexCoords, MipValue, bAutomaticViewMipBias, Expr->SamplerSource, Expr->SamplerType);
				break;

			case TMVM_Derivative:
			{
				FValueRef TexCoordsDdx = Em.Cast(Em.Input(&Expr->CoordinatesDX), TexCoords->Type);
				FValueRef TexCoordsDdy = Em.Cast(Em.Input(&Expr->CoordinatesDY), TexCoords->Type);
				TextureRead = Em.TextureSampleGrad(Texture, TexCoords, TexCoordsDdx, TexCoordsDdy, bAutomaticViewMipBias, Expr->SamplerSource, Expr->SamplerType);
				break;
			}
		}
	}

	Em.Output(0, Em.Swizzle(TextureRead, MIR::FSwizzleMask::XYZ()));
	Em.Output(1, Em.Subscript(TextureRead, 0));
	Em.Output(2, Em.Subscript(TextureRead, 1));
	Em.Output(3, Em.Subscript(TextureRead, 2));
	Em.Output(4, Em.Subscript(TextureRead, 3));
	Em.Output(5, TextureRead); 
}

// Returns this value's textures if it has one (nullptr otherwise).
static EMaterialSamplerType GetValueMaterialSamplerType(FValueRef Value)
{
	if (auto TextureObject = MIR::As<MIR::FTextureObject>(Value))
	{
		return TextureObject->SamplerType;
	}
	else if (auto UniformParameter = MIR::As<MIR::FUniformParameter>(Value))
	{
		return UniformParameter->SamplerType;
	}
	return SAMPLERTYPE_MAX;
}

void UMaterialExpressionTextureSample::Build(MIR::FEmitter& Em)
{
	FValueRef TextureValue = Em.TryInput(&TextureObject);
	if (!TextureValue)
	{
		if (!Texture)
		{
			Em.Error(TEXT("No texture specified for this expression."));
			return;
		}

		TextureValue = Em.TextureObject(Texture.Get(), SamplerType);
	}

	UE_MIR_CHECKPOINT(Em);

	if (TextureValue && GetValueMaterialSamplerType(TextureValue) != SAMPLERTYPE_Color)
	{
		Em.Error(TEXT("Input texture sampler type must be color."));
		return;
	}

	BuildTextureSampleExpression(Em, this, TextureValue);
}

static FValueRef BuildTextureObjectParameter(MIR::FEmitter& Em, UMaterialExpressionTextureSampleParameter* Expr)
{
	FMaterialParameterMetadata Param;
	if (!Expr->GetParameterValue(Param))
	{
		Em.Error(TEXT("Failed to get parameter value"));
		return nullptr;
	}

	FValueRef ParameterValue = Em.Parameter(Expr->GetParameterName(), Param, Expr->SamplerType);
	if (!ParameterValue->Type->IsTexture())
	{
		Em.Error(TEXT("Parameter is not a texture"));
		return nullptr;
	}

	return ParameterValue;
}

void UMaterialExpressionTextureSampleParameter::Build(MIR::FEmitter& Em)
{
	FValueRef ParameterValue = BuildTextureObjectParameter(Em, this);
	UE_MIR_CHECKPOINT(Em);
	BuildTextureSampleExpression(Em, this, ParameterValue);
}

void UMaterialExpressionTextureSampleParameterCube::Build(MIR::FEmitter& Em)
{
	Em.Input(&Coordinates); // Cubemap sampling requires coordinates input specified
	UE_MIR_CHECKPOINT(Em);
	UMaterialExpressionTextureSampleParameter::Build(Em);
}

void UMaterialExpressionTextureObjectParameter::Build(MIR::FEmitter& Em)
{
	Em.Output(0, BuildTextureObjectParameter(Em, this));
}

void UMaterialExpressionTextureCoordinate::Build(MIR::FEmitter& Em)
{
	if (UnMirrorU || UnMirrorV)
	{
		Em.Error(TEXT("Unmirroring unsupported"));
		return;
	}

	FValueRef OutputValue = Em.ExternalInput(MIR::TexCoordIndexToExternalInput(CoordinateIndex));
	
	// Multiply the UV input by the UV tiling constants
	OutputValue = Em.Multiply(OutputValue, Em.ConstantFloat2({ UTiling, VTiling }));
	
	Em.Output(GetOutput(0), OutputValue);
}

void UMaterialExpressionTime::Build(MIR::FEmitter& Em)
{
	const MIR::FType* ScalarFloatType = MIR::FPrimitiveType::GetScalar(MIR::ScalarKind_Float);

	// When pausing the game is ignored for this time expression, use real-time instead of game-time.
	const TCHAR* TimeFieldName = bIgnorePause ? TEXT("View.<PREV>RealTime") : TEXT("View.<PREV>GameTime");
	if (!bOverride_Period)
	{
		Em.Output(0, Em.InlineHLSL(ScalarFloatType, TimeFieldName, {}, MIR::EValueFlags::SubstituteTagsInInlineHLSL));
	}
	else if (Period == 0.0f)
	{
		Em.Output(0, Em.ConstantFloat(0.0f));
	}
	else
	{
		// Note: Don't use IR intrinsic for Fmod() here to avoid conversion to fp16 on mobile.
		// We want full 32 bit float precision until the fmod when using a period.
		FValueRef PeriodValue = Em.ConstantFloat(Period);
		Em.Output(0, Em.InlineHLSL(ScalarFloatType, FString::Printf(TEXT("fmod(%s,$0)"), TimeFieldName), { PeriodValue }, MIR::EValueFlags::SubstituteTagsInInlineHLSL));
	}
}

void UMaterialExpressionReroute::Build(MIR::FEmitter& Em)
{
	FValueRef Value = Em.Input(&Input);
	Em.Output(0, Value);
}

void UMaterialExpressionClamp::Build(MIR::FEmitter& Em)
{
	FValueRef InputValue = Em.Input(&Input);
	FValueRef MinValue = Em.InputDefaultFloat(&Min, MinDefault);
	FValueRef MaxValue = Em.InputDefaultFloat(&Max, MaxDefault);

	FValueRef OutputValue = nullptr;
	if (ClampMode == CMODE_Clamp)
	{
		OutputValue = Em.Clamp(InputValue, MinValue, MaxValue);
	}
	else if (ClampMode == CMODE_ClampMin)
	{
		OutputValue = Em.Max(InputValue, MinValue);
	}
	else if (ClampMode == CMODE_ClampMax)
	{
		OutputValue = Em.Min(InputValue, MaxValue);
	}

	Em.Output(0, OutputValue);
}


void BuildTernaryArithmeticOperator(MIR::FEmitter& Em, MIR::EOperator Op, FExpressionInput* A, float ConstA, FExpressionInput* B, float ConstB, FExpressionInput* C, float ConstC)
{
	FValueRef ValueA = Em.InputDefaultFloat(A, ConstA);
	FValueRef ValueB = Em.InputDefaultFloat(B, ConstB);
	FValueRef ValueC = Em.InputDefaultFloat(C, ConstC);
	Em.Output(0, Em.Operator(Op, ValueA, ValueB, ValueC));
}

void UMaterialExpressionColorRamp::Build(MIR::FEmitter& Em)
{	
	// Check that the ColorCurve is set
	if (!ColorCurve)
	{
		Em.Errorf(TEXT("Missing ColorCurve"));
		return;
	}

	FValueRef InputValue = Em.CastToFloat(Em.InputDefaultFloat(&Input, ConstInput), 1);

	// If the input is constant, evaluate at compile time.
	if (const MIR::FConstant* Constant = MIR::As<MIR::FConstant>(InputValue))
	{
		FLinearColor ColorValue = ColorCurve->GetLinearColorValue(Constant->Float);
		Em.Output(0, Em.ConstantFloat4(ColorValue));
		return;
	}

	// Helper lambda to evaluate a curve
	auto EvaluateCurve = [&Em, InputValue](const FRichCurve& Curve) -> FValueRef
		{
			const int32 NumKeys = Curve.Keys.Num();

			switch (NumKeys)
			{
				case 0:
					return Em.ConstantFloat(0.0f);

				case 1:
					return Em.ConstantFloat(Curve.Keys[0].Value);

				case 2:
				{
					float StartTime = Curve.Keys[0].Time;
					float EndTime = Curve.Keys[1].Time;
					float StartValue = Curve.Keys[0].Value;
					float EndValue = Curve.Keys[1].Value;

					FValueRef TimeDelta = Em.ConstantFloat(EndTime - StartTime);
					FValueRef TimeDiff = Em.Subtract(InputValue, Em.ConstantFloat(StartTime));
					FValueRef Fraction = Em.Divide(TimeDiff, TimeDelta);

					return Em.Lerp(Em.ConstantFloat(StartValue), Em.ConstantFloat(EndValue), Fraction);
				}
			}

			FValueRef InValueVec = Em.Vector4(InputValue, InputValue, InputValue, InputValue);

			FValueRef Result = Em.ConstantFloat(Curve.Keys[0].Value);
			int32 i = 0;

			// Use vector operations for segments of 4
			for (; i < NumKeys - 4; i += 4)
			{
				FVector4f StartTimeVector(
					Curve.Keys[i].Time,
					Curve.Keys[i + 1].Time,
					Curve.Keys[i + 2].Time,
					Curve.Keys[i + 3].Time
				);
				FValueRef StartTimeVec = Em.ConstantFloat4(StartTimeVector);

				FVector4f EndTimeVector(
					Curve.Keys[i + 1].Time,
					Curve.Keys[i + 2].Time,
					Curve.Keys[i + 3].Time,
					Curve.Keys[i + 4].Time
				);
				FValueRef EndTimeVec = Em.ConstantFloat4(EndTimeVector);

				FVector4f StartValueVector(
					Curve.Keys[i].Value,
					Curve.Keys[i + 1].Value,
					Curve.Keys[i + 2].Value,
					Curve.Keys[i + 3].Value
				);
				FValueRef StartValueVec = Em.ConstantFloat4(StartValueVector);

				FVector4f EndValueVector(
					Curve.Keys[i + 1].Value,
					Curve.Keys[i + 2].Value,
					Curve.Keys[i + 3].Value,
					Curve.Keys[i + 4].Value
				);
				FValueRef EndValueVec = Em.ConstantFloat4(EndValueVector);

				FValueRef TimeDeltaVec = Em.Subtract(EndTimeVec, StartTimeVec);
				FValueRef ValueDeltaVec = Em.Subtract(EndValueVec, StartValueVec);

				FValueRef TimeDiffVec = Em.Subtract(InValueVec, StartTimeVec);
				FValueRef FractionVec = Em.Divide(TimeDiffVec, TimeDeltaVec);
				FValueRef SatFractionVec = Em.Saturate(FractionVec);
				FValueRef ContributionVec = Em.Multiply(ValueDeltaVec, SatFractionVec);

				FVector4f Ones(1.0f, 1.0f, 1.0f, 1.0f);
				FValueRef OnesVec = Em.ConstantFloat4(Ones);
				FValueRef ContributionSum = Em.Dot(ContributionVec, OnesVec);

				Result = Em.Add(Result, ContributionSum);
			}
			
			// Use scalar operations for the remaining keys
			for (; i < NumKeys - 1; i++)
			{
				float StartTime = Curve.Keys[i].Time;
				float EndTime = Curve.Keys[i + 1].Time;
				float StartValue = Curve.Keys[i].Value;
				float EndValue = Curve.Keys[i + 1].Value;

				FValueRef TimeDelta = Em.ConstantFloat(EndTime - StartTime);
				FValueRef ValueDelta = Em.ConstantFloat(EndValue - StartValue);
				FValueRef TimeDiff = Em.Subtract(InputValue, Em.ConstantFloat(StartTime));
				FValueRef Fraction = Em.Divide(TimeDiff, TimeDelta);
				FValueRef SatFraction = Em.Saturate(Fraction);
				FValueRef Contribution = Em.Multiply(ValueDelta, SatFraction);
				Result = Em.Add(Result, Contribution);
			}
			return Result;
		};

	FValueRef Red = EvaluateCurve(ColorCurve->FloatCurves[0]);
	FValueRef Green = EvaluateCurve(ColorCurve->FloatCurves[1]);
	FValueRef Blue = EvaluateCurve(ColorCurve->FloatCurves[2]);
	FValueRef Alpha = EvaluateCurve(ColorCurve->FloatCurves[3]);

	FValueRef FinalVector = Em.Vector4(Red, Green, Blue, Alpha);
	Em.Output(0, FinalVector);
}

void UMaterialExpressionLinearInterpolate::Build(MIR::FEmitter& Em)
{
	BuildTernaryArithmeticOperator(Em, MIR::TO_Lerp, &A, ConstA, &B, ConstB, &Alpha, ConstAlpha);
}

void UMaterialExpressionSmoothStep::Build(MIR::FEmitter& Em)
{
	BuildTernaryArithmeticOperator(Em, MIR::TO_Smoothstep, &Min, ConstMin, &Max, ConstMax, &Value, ConstValue);
}

void UMaterialExpressionConvert::Build(MIR::FEmitter& Em)
{
	TArray<FValueRef, TInlineAllocator<8>> InputValues;
	InputValues.Init(nullptr, ConvertInputs.Num());

	for (int32 OutputIndex = 0; OutputIndex < ConvertOutputs.Num(); ++OutputIndex)
	{
		const FMaterialExpressionConvertOutput& ConvertOutput = ConvertOutputs[OutputIndex];
		FValueRef OutComponents[4] = { nullptr, nullptr, nullptr, nullptr };

		for (const FMaterialExpressionConvertMapping& Mapping : ConvertMappings)
		{
			// We only care about mappings relevant to this output
			if (Mapping.OutputIndex != OutputIndex)
			{
				continue;
			}

			const int32 OutputComponentIndex = Mapping.OutputComponentIndex;
			if (!IsValidComponentIndex(OutputComponentIndex, ConvertOutput.Type))
			{
				Em.Errorf(TEXT("Convert mapping's output component `%d` is invalid."), OutputComponentIndex);
				continue;
			}

			const int32 InputIndex = Mapping.InputIndex;
			if (!ConvertInputs.IsValidIndex(InputIndex))
			{
				Em.Errorf(TEXT("Convert mapping's input `%d` is invalid."), InputIndex);
				continue;
			}

			FMaterialExpressionConvertInput& ConvertInput = ConvertInputs[InputIndex];
			const int32 InputComponentIndex = Mapping.InputComponentIndex;
			if (!IsValidComponentIndex(InputComponentIndex, ConvertInput.Type))
			{
				Em.Errorf(TEXT("Convert mapping's input component `%d` is invalid."), InputComponentIndex);
				continue;
			}

			// If not already emitted, read the input value, cast it to the specified input
			// type and cache it into an array, as each input could be used multiple times
			// by output values.
			if (!InputValues[InputIndex])
			{
				// Read the input's value (or read float zero if disconnected).
				InputValues[InputIndex] = Em.InputDefaultFloat4(&ConvertInput.ExpressionInput, ConvertInput.DefaultValue);

				// Expect type to be primitive.
				const MIR::FPrimitiveType* InputPrimitiveType = InputValues[InputIndex]->Type->AsPrimitive();
				if (!InputPrimitiveType)
				{
					Em.Errorf(TEXT("Input `%d` of type `%s` is not primitive."), InputComponentIndex, InputValues[InputIndex]->Type->GetSpelling().GetData());
					continue;
				}

				// Determine the target type.
				const MIR::FType* InputType = MIR::FPrimitiveType::GetVector(InputPrimitiveType->ScalarKind, MaterialExpressionConvertType::GetComponentCount(ConvertInput.Type));

				// Cast the input value to the target type.
				InputValues[InputIndex] = Em.Cast(InputValues[InputIndex], InputType);
			}

			// Subscript the input value to the specified component index.
			OutComponents[OutputComponentIndex] = Em.Subscript(InputValues[InputIndex], InputComponentIndex);
		}

		const int32 OutputNumComponents = MaterialExpressionConvertType::GetComponentCount(ConvertOutput.Type);

		// For any component still unset, give assign it to the default value.
		for (int32 OutputComponentIndex = 0; OutputComponentIndex < OutputNumComponents; ++OutputComponentIndex)
		{
			// If we don't have a compile result here, default it to a that component's default value
			if (!OutComponents[OutputComponentIndex])
			{
				OutComponents[OutputComponentIndex] = Em.ConstantFloat(ConvertOutput.DefaultValue.Component(OutputComponentIndex));
			}
		}

		// Finally create the output dimensional value by combining the output components.
		FValueRef OutValue;
		switch (OutputNumComponents)
		{
			case 1: OutValue = OutComponents[0]; break;
			case 2: OutValue = Em.Vector2(OutComponents[0], OutComponents[1]); break;
			case 3: OutValue = Em.Vector3(OutComponents[0], OutComponents[1], OutComponents[2]); break;
			case 4: OutValue = Em.Vector4(OutComponents[0], OutComponents[1], OutComponents[2], OutComponents[3]); break;
			
			default:
				OutValue = Em.Poison();
				Em.Errorf(TEXT("Convert node has an invalid component count of %d"), OutputNumComponents);
				break;
		}

		Em.Output(OutputIndex, OutValue);
	}
}

static FValueRef BuildViewProperty(MIR::FEmitter& Em, EMaterialExposedViewProperty InProperty, bool bInvProperty = false)
{
	check(InProperty < MEVP_MAX);

	const FMaterialExposedViewPropertyMeta& PropertyMeta = MaterialExternalCodeRegistry::Get().GetExternalViewPropertyCode(InProperty);
	const bool bHasCustomInverseCode = PropertyMeta.InvPropertyCode != nullptr;

	FString HLSLCode = bInvProperty && bHasCustomInverseCode ? PropertyMeta.InvPropertyCode : PropertyMeta.PropertyCode;
	const MIR::FType* HLSLCodeType = MIR::FType::FromMaterialValueType(PropertyMeta.Type);

	if (bInvProperty && !bHasCustomInverseCode)
	{
		// Fall back to compute the property's inverse from PropertyCode
		return Em.Divide(Em.ConstantFloat(1.0f), Em.InlineHLSL(HLSLCodeType, MoveTemp(HLSLCode), {}, MIR::EValueFlags::SubstituteTagsInInlineHLSL));
	}
	else
	{
		// @todo-laura.hermanns - old translator used CastToNonLWCIfDisabled(), but LWC not supported in MIR yet
		return Em.InlineHLSL(HLSLCodeType, MoveTemp(HLSLCode), {}, MIR::EValueFlags::SubstituteTagsInInlineHLSL);
	}
}

void UMaterialExpressionViewProperty::Build(MIR::FEmitter& Em)
{
	for (int32 OutputIndex = 0; OutputIndex < 2; ++OutputIndex)
	{
		const bool bInvProperty = OutputIndex == 1;
		Em.Output(OutputIndex, BuildViewProperty(Em, Property, bInvProperty));
	}
}

void UMaterialExpressionViewSize::Build(MIR::FEmitter& Em)
{
	Em.Output(0, BuildViewProperty(Em, MEVP_ViewSize));
}

void UMaterialExpressionCameraPositionWS::Build(MIR::FEmitter& Em)
{
	Em.Output(0, BuildViewProperty(Em, MEVP_WorldSpaceCameraPosition));
}

void UMaterialExpressionPixelNormalWS::Build(MIR::FEmitter& Em)
{
	FValueRef Output = Em.InlineHLSL(MIR::FPrimitiveType::GetFloat3(), "Parameters.WorldNormal", {}, MIR::EValueFlags::None, MIR::EGraphProperties::ReadsPixelNormal);
	Em.Output(0, Output);
}

void UMaterialExpressionDDX::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.PartialDerivative(Em.Input(&Value), MIR::EDerivativeAxis::X));
}

void UMaterialExpressionDDY::Build(MIR::FEmitter& Em)
{
	Em.Output(0, Em.PartialDerivative(Em.Input(&Value), MIR::EDerivativeAxis::Y));
}

static constexpr MIR::EOperator MaterialExpressionOperatorToMIR(EMaterialExpressionOperatorKind Operator)
{
	return static_cast<MIR::EOperator>(static_cast<uint32>(Operator) + 1);
}

// Checks to make sure the two enums are aligned.
static_assert(MaterialExpressionOperatorToMIR(EMaterialExpressionOperatorKind::BitwiseNot) == MIR::UO_BitwiseNot);
static_assert(MaterialExpressionOperatorToMIR(EMaterialExpressionOperatorKind::Sign) == MIR::UO_Sign);
static_assert(MaterialExpressionOperatorToMIR(EMaterialExpressionOperatorKind::BitwiseAnd) == MIR::BO_BitwiseAnd);
static_assert(MaterialExpressionOperatorToMIR(EMaterialExpressionOperatorKind::Smoothstep) == MIR::TO_Smoothstep);

uint32 GetMaterialExpressionOperatorArity(EMaterialExpressionOperatorKind Operator)
{
	return MIR::GetOperatorArity(MaterialExpressionOperatorToMIR(Operator));
}

void UMaterialExpressionOperator::Build(MIR::FEmitter& Em)
{
	MIR::EOperator OpMIR = MaterialExpressionOperatorToMIR(Operator);
	int32 OperatorArity = MIR::GetOperatorArity(OpMIR);

	FValueRef AValue = Em.InputDefaultFloat(&A, ConstA);
	FValueRef BValue = OperatorArity >= 2 ? Em.InputDefaultFloat(&B, ConstB) : FValueRef{};
	FValueRef CValue = OperatorArity >= 3 ? Em.InputDefaultFloat(&C, ConstC) : FValueRef{};

	Em.Output(0, Em.Operator(OpMIR, AValue, BValue, CValue));
}

#endif // WITH_EDITOR
