// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NameTypes.h"
#include "Misc/EngineVersionComparison.h"

#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialInstanceDynamic.h"

class METAHUMANIMAGEVIEWEREDITOR_API CustomMaterialUtils
{
public:

	static void SetupExpression(class UMaterialExpressionTextureObjectParameter* InExpression, const FName& InName, bool bInUseExternalSampler);
	static void SetupExpression(class UMaterialExpressionScalarParameter* InExpression, const FName& InName, bool bInUseExternalSampler);
	static void SetupExpression(class UMaterialExpressionVectorParameter* InExpression, const FName& InName, bool bInUseExternalSampler);
	static void SetupExpression(class UMaterialExpression* InExpression, const FName& InName, bool bInUseExternalSampler);

	template<typename T>
	static void AddInput(const FName& InName, UMaterial* InMaterial, UMaterialExpressionCustom* InCustomNode, bool bInUseExternalSampler = false)
	{
		T* Expression = NewObject<T>(InMaterial);

		SetupExpression(Expression, InName, bInUseExternalSampler);

		InMaterial->GetExpressionCollection().AddExpression(Expression);

		FCustomInput CustomInput;
		CustomInput.InputName = InName;
		CustomInput.Input.Expression = Expression;
		InCustomNode->Inputs.Add(CustomInput);
	}

	// A material that can show the raw footage, a contour overlay, and depth data. Material parameters are:
	//		"Movie"			Texture		RGBA or depth texture
	//		"Contours"		Texture		RGBA texture which is overlaid on above
	//		"ShowDarken"	Scalar		If >0.5 image is dimmed down
	//		"ShowContours"	Scalar		If >0.5 overlay is applied
	//		"DepthNear"		Scalar		Minium visible depth value
	//		"DepthFar"		Scalar		Maximum visible depth value
	static UMaterialInstanceDynamic* CreateMovieContourDepthMaterial(const FName& InName, bool bInUseExternalSampler, int32 InDepthComponent);

	// A material that can show depth data as a 3D mesh. Material parameters are:
	//		"Movie"				Texture		Depth texture
	//		"InvFocal"			Scalar		The focal length component of the inverse of the camera intrinsic matrix
	//		"InvX"				Scalar		The X principle point component of the inverse of the camera intrinsic matrix
	//		"InvY"				Scalar		the Y principle point component of the inverse of the camera intrinsic matrix
	//		"DepthNear"			Scalar		Minimum visible depth value
	//		"DepthFar"			Scalar		Maximum visible depth value
	//		"InvExtrinsicRow0	Vector		The first row of the depth camera extrinsic matrix
	//		"InvExtrinsicRow1	Vector		The second row of the depth camera extrinsic matrix
	//		"InvExtrinsicRow2	Vector		The third row of the depth camera extrinsic matrix
	//		"InvExtrinsicRow3	Vector		The fourth row of the depth camera extrinsic matrix
	static UMaterialInstanceDynamic* CreateDepthMeshMaterial(const FName& InName);
};
