// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomMaterialUtils.h"

#include "Factories/MaterialFactoryNew.h"

#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "MaterialDomain.h"
#include "UObject/Package.h"

#include "Misc/EngineVersionComparison.h"

void CustomMaterialUtils::SetupExpression(UMaterialExpressionTextureObjectParameter* InExpression, const FName& InName, bool bInUseExternalSampler)
{
	InExpression->ParameterName = InName;
	InExpression->SetDefaultTexture();
	if (bInUseExternalSampler)
	{
		InExpression->SamplerType = EMaterialSamplerType::SAMPLERTYPE_External;
	}
}

void CustomMaterialUtils::SetupExpression(UMaterialExpressionScalarParameter* InExpression, const FName& InName, bool bInUseExternalSampler)
{
	InExpression->ParameterName = InName;
	InExpression->DefaultValue = 0.0;
}

void CustomMaterialUtils::SetupExpression(UMaterialExpressionVectorParameter* InExpression, const FName& InName, bool bInUseExternalSampler)
{
	InExpression->ParameterName = InName;
	InExpression->DefaultValue = FLinearColor{ ForceInit };
}

void CustomMaterialUtils::SetupExpression(UMaterialExpression* InExpression, const FName& InName, bool bInUseExternalSampler)
{
}

UMaterialInstanceDynamic* CustomMaterialUtils::CreateMovieContourDepthMaterial(const FName &InName, bool bInUseExternalSampler, int32 InDepthComponent)
{
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();

	// Material for clip
	UMaterial* ClipMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(UMaterial::StaticClass(), GetTransientPackage(), InName, RF_Transient, NULL, GWarn));

	UMaterialExpressionCustom* ClipCustomNode = NewObject<UMaterialExpressionCustom>(ClipMaterial);

	CustomMaterialUtils::AddInput<UMaterialExpressionTextureObjectParameter>("Movie", ClipMaterial, ClipCustomNode, bInUseExternalSampler);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureObjectParameter>("Contours", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureCoordinate>("TexCoord", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("ShowDarken", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("ShowContours", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthNear", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthFar", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthComponent", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("Undistort", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("cx", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("cy", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("fx", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("fy", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("k1", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("k2", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("k3", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("p1", ClipMaterial, ClipCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("p2", ClipMaterial, ClipCustomNode);

	const FString ClipCode = R"|(

// UV coords for nearest neightbour sampling - reduces artifacts
float2 Resolution;
Movie.GetDimensions(Resolution.x, Resolution.y);

if (Undistort > 0.5)
{
    const float xf = TexCoord.x * Resolution.x;
    const float yf = TexCoord.y * Resolution.y;

    const float ix = (xf - cx) / fx;
    const float iy = (yf - cy) / fy;
    const float r2 = ix * ix + iy * iy;
    const float r4 = r2 * r2;
    const float r6 = r4 * r2;
    const float radial = 1.0 + k1 * r2 + k2 * r4 + k3 * r6;
	// note that in titan, the MetaShape representation is used which flips p1 and p2 so the code below is not identical to the titan hlsl shader
	// note that also, we are not using p3 and p4 in UE as they are not supported in the LensFile distortion parameters   
	const float xdash = ix * radial + (p2 * (r2 + 2.0 * ix * ix) + 2.0 * p1 * ix * iy) ; 
    const float ydash = iy * radial + (p1 * (r2 + 2.0 * iy * iy) + 2.0 * p2 * ix * iy) ;

    const float px = fx * xdash + cx;
    const float py = fy * ydash + cy;

    TexCoord = float2(px/Resolution.x, py/Resolution.y);
}

float SampleX = int(TexCoord.x * Resolution.x) + 0.5;
float SampleY = int(TexCoord.y * Resolution.y) + 0.5;

float2 UV;
UV.x = SampleX / Resolution.x;
UV.y = SampleY / Resolution.y;

// Sample movie
float4 MovieSample = Movie.SampleLevel(MovieSampler, UV, 0);

if (ShowDarken > 0.5)
{
	MovieSample *= 0.1;
}

if (ShowContours > 0.5)
{
	float4 ContoursSample = Contours.SampleLevel(ContoursSampler, UV, 0);
	if (ContoursSample[3] > 0.5)
	{
		MovieSample[0] = ContoursSample[0];
		MovieSample[1] = ContoursSample[1];
		MovieSample[2] = ContoursSample[2];
	}
}

return MovieSample;

	)|";

	ClipCustomNode->Code = ClipCode;
	ClipMaterial->SetShadingModel(EMaterialShadingModel::MSM_Unlit);

	UMaterialEditorOnlyData* ClipMaterialEditorOnly = ClipMaterial->GetEditorOnlyData();
	ClipMaterial->GetExpressionCollection().AddExpression(ClipCustomNode);
	ClipMaterialEditorOnly->EmissiveColor.Expression = ClipCustomNode;

	ClipMaterial->MaterialDomain = EMaterialDomain::MD_UI;

	ClipMaterial->PreEditChange(nullptr);
	ClipMaterial->PostEditChange();

	UMaterialInstanceDynamic* ClipMaterialInstance = UMaterialInstanceDynamic::Create(ClipMaterial, nullptr);

	ClipMaterialInstance->SetScalarParameterValue(FName("DepthComponent"), InDepthComponent);

	return ClipMaterialInstance;
}

UMaterialInstanceDynamic* CustomMaterialUtils::CreateDepthMeshMaterial(const FName& InName)
{
	UMaterial* DepthMaterial = NewObject<UMaterial>(GetTransientPackage(), InName);

	UMaterialEditorOnlyData* DepthMaterialEditorOnly = DepthMaterial->GetEditorOnlyData();

	UMaterialExpressionCustom* DepthCustomNode = NewObject<UMaterialExpressionCustom>(DepthMaterial);

	CustomMaterialUtils::AddInput<UMaterialExpressionTextureObjectParameter>("Movie", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionTextureCoordinate>("TexCoord", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthNear", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthFar", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("InvFocal", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("InvX", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("InvY", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionScalarParameter>("DepthComponent", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionVectorParameter>("InvExtrinsicRow0", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionVectorParameter>("InvExtrinsicRow1", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionVectorParameter>("InvExtrinsicRow2", DepthMaterial, DepthCustomNode);
	CustomMaterialUtils::AddInput<UMaterialExpressionVectorParameter>("InvExtrinsicRow3", DepthMaterial, DepthCustomNode);

	const FString DepthCode = R"|(

// UV coords for nearest neightbour sampling - reduces artifacts
float2 Resolution;
Movie.GetDimensions(Resolution.x, Resolution.y);

float SampleX = int(TexCoord.x * Resolution.x) + 0.5;
float SampleY = int(TexCoord.y * Resolution.y) + 0.5;

float2 UV;
UV.x = SampleX / Resolution.x;
UV.y = SampleY / Resolution.y;

// Sample movie
float4 MovieSample = Movie.SampleLevel(MovieSampler, UV, 0);

if (MovieSample[DepthComponent] > DepthNear && MovieSample[DepthComponent] < DepthFar)
{
	Opacity_Mask = 1;

	float4x4 InverseCameraExtrinsic = { InvExtrinsicRow0[0], InvExtrinsicRow0[1], InvExtrinsicRow0[2], InvExtrinsicRow0[3],
										InvExtrinsicRow1[0], InvExtrinsicRow1[1], InvExtrinsicRow1[2], InvExtrinsicRow1[3],
										InvExtrinsicRow2[0], InvExtrinsicRow2[1], InvExtrinsicRow2[2], InvExtrinsicRow2[3],
										InvExtrinsicRow3[0], InvExtrinsicRow3[1], InvExtrinsicRow3[2], InvExtrinsicRow3[3]};
	float3x3 InverseCameraIntrinsic = { InvFocal, 0, 0,  0, InvFocal, 0,  InvX, InvY, 1 };

	float3 Ray = mul(float3(SampleX, SampleY, 1), InverseCameraIntrinsic); // Vector in camera space of the pixel
	float3 PosPlane = DepthFar * Ray; // The 3D position where that ray hits the plane we are offsetting
	float3 PosSample = MovieSample[DepthComponent] * Ray; // The 3D position of the pixel
	float3 Offset = PosSample - PosPlane; // The 3D offset vector

	// Apply the inverse of the camera extrinsic matrix to the offset to account for the depth camera transform
	Offset = mul(Offset, InverseCameraExtrinsic);

	World_Position_Offset.x = Offset.z; // Account for coordinate system differences
	World_Position_Offset.y = Offset.x;
	World_Position_Offset.z = -Offset.y;
	World_Position_Offset.w = 1.0;

	// Calculate the surface normal using the 3D positions of the neighboring pixels to the sample position

	float3 RayXP= mul(float3(SampleX + 1, SampleY, 1), InverseCameraIntrinsic);
	float3 RayXM= mul(float3(SampleX - 1, SampleY, 1), InverseCameraIntrinsic);
	float3 RayYP= mul(float3(SampleX, SampleY + 1, 1), InverseCameraIntrinsic);
	float3 RayYM= mul(float3(SampleX, SampleY - 1, 1), InverseCameraIntrinsic);

	float OnePixelUVStepX = 1.0 / Resolution.x;
	float OnePixelUVStepY = 1.0 / Resolution.y;

	float4 MovieSampleXP = Movie.SampleLevel(MovieSampler, float2(UV.x + OnePixelUVStepX, UV.y), 0);
	float4 MovieSampleXM = Movie.SampleLevel(MovieSampler, float2(UV.x - OnePixelUVStepX, UV.y), 0);
	float4 MovieSampleYP = Movie.SampleLevel(MovieSampler, float2(UV.x, UV.y + OnePixelUVStepY), 0);
	float4 MovieSampleYM = Movie.SampleLevel(MovieSampler, float2(UV.x, UV.y - OnePixelUVStepY), 0);

	float3 PosXP, PosXM, PosYP, PosYM;

	if (MovieSampleXP[DepthComponent] > DepthNear && MovieSampleXP[DepthComponent] < DepthFar)
	{
		PosXP = MovieSampleXP[DepthComponent] * RayXP;
	}

	if (MovieSampleXM[DepthComponent] > DepthNear && MovieSampleXM[DepthComponent] < DepthFar)
	{
		PosXM = MovieSampleXM[DepthComponent] * RayXM;
	}

	if (MovieSampleYP[DepthComponent] > DepthNear && MovieSampleYP[DepthComponent] < DepthFar)
	{
		PosYP = MovieSampleYP[DepthComponent] * RayYP;
	}

	if (MovieSampleYM[DepthComponent] > DepthNear && MovieSampleYM[DepthComponent] < DepthFar)
	{
		PosYM = MovieSampleYM[DepthComponent] * RayYM;
	}

	float3 AccumulatedNormal = float3(0, 0, 0);

	if (MovieSampleXP[DepthComponent] > DepthNear && MovieSampleXP[DepthComponent] < DepthFar && MovieSampleYM[DepthComponent] > DepthNear && MovieSampleYM[DepthComponent] < DepthFar)
	{
		AccumulatedNormal += cross(PosXP - PosSample, PosYM - PosSample);
	}

	if (MovieSampleYM[DepthComponent] > DepthNear && MovieSampleYM[DepthComponent] < DepthFar && MovieSampleXM[DepthComponent] > DepthNear && MovieSampleXM[DepthComponent] < DepthFar)
	{
		AccumulatedNormal += cross(PosYM - PosSample, PosXM - PosSample);
	}

	if (MovieSampleXM[DepthComponent] > DepthNear && MovieSampleXM[DepthComponent] < DepthFar && MovieSampleYP[DepthComponent] > DepthNear && MovieSampleYP[DepthComponent] < DepthFar)
	{
		AccumulatedNormal += cross(PosXM - PosSample, PosYP - PosSample);
	}

	if (MovieSampleYP[DepthComponent] > DepthNear && MovieSampleYP[DepthComponent] < DepthFar && MovieSampleXP[DepthComponent] > DepthNear && MovieSampleXP[DepthComponent] < DepthFar)
	{
		AccumulatedNormal += cross(PosYP - PosSample, PosXP - PosSample);
	}

	if (((AccumulatedNormal.x * AccumulatedNormal.x) + (AccumulatedNormal.y * AccumulatedNormal.y) + (AccumulatedNormal.z * AccumulatedNormal.z)) > 0)
	{
		Normal = normalize(AccumulatedNormal);
		Normal.z = -Normal.z; // Account for handedness difference
	}
	else
	{
		Normal = float3(0, 0, 1);
	}
}
else
{
	Opacity_Mask = 0;

	World_Position_Offset = float4(0, 0, 0, 1);

	Normal = float3(0, 0, 1);
}

return float4(0.1, 0.1, 0.1, 1);

	)|";

	DepthCustomNode->Code = DepthCode;

	FCustomOutput CustomOutput;
	CustomOutput.OutputName = "Opacity_Mask";
	CustomOutput.OutputType = ECustomMaterialOutputType::CMOT_Float1;
	DepthCustomNode->AdditionalOutputs.Add(CustomOutput);

	CustomOutput.OutputName = "World_Position_Offset";
	CustomOutput.OutputType = ECustomMaterialOutputType::CMOT_Float4; // Must be a float4, float3 does not work!
	DepthCustomNode->AdditionalOutputs.Add(CustomOutput);

	CustomOutput.OutputName = "Normal";
	CustomOutput.OutputType = ECustomMaterialOutputType::CMOT_Float3;
	DepthCustomNode->AdditionalOutputs.Add(CustomOutput);

	DepthMaterial->GetExpressionCollection().AddExpression(DepthCustomNode);

	DepthMaterialEditorOnly->BaseColor.Expression = DepthCustomNode;
	DepthMaterialEditorOnly->BaseColor.OutputIndex = 0;

	DepthMaterialEditorOnly->OpacityMask.Expression = DepthCustomNode;
	DepthMaterialEditorOnly->OpacityMask.OutputIndex = 1;

	DepthMaterialEditorOnly->WorldPositionOffset.Expression = DepthCustomNode;
	DepthMaterialEditorOnly->WorldPositionOffset.OutputIndex = 2;

	DepthMaterialEditorOnly->Normal.Expression = DepthCustomNode;
	DepthMaterialEditorOnly->Normal.OutputIndex = 3;

	DepthMaterial->BlendMode = EBlendMode::BLEND_Masked;

	DepthMaterial->PreEditChange(nullptr);
	DepthMaterial->PostEditChange();

	UMaterialInstanceDynamic* DepthMaterialInstance = UMaterialInstanceDynamic::Create(DepthMaterial, nullptr);

	DepthMaterialInstance->SetScalarParameterValue(FName("DepthComponent"), 0);

	return DepthMaterialInstance;
}