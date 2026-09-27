// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Skeleton.h"

#include "NamedElementTypes.generated.h"

UCLASS(MinimalAPI)
class USkeletonTemplateBaseType : public UObject
{
	GENERATED_BODY()
};

UCLASS(MinimalAPI)
class USkeletonTemplateTransform : public USkeletonTemplateBaseType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	FTransform Value;
};

UCLASS(MinimalAPI)
class USkeletonTemplateFloat : public USkeletonTemplateBaseType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	float Value;
};

UCLASS(MinimalAPI)
class USkeletonTemplateBool : public USkeletonTemplateBaseType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	bool Value;
};

UCLASS(MinimalAPI)
class USkeletonTemplateTranslationRetargetingMode : public USkeletonTemplateBaseType
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Settings")
	TEnumAsByte<EBoneTranslationRetargetingMode::Type> Value;
};