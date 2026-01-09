// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "Engine/StaticMesh.h"

#include "ControlRigGizmoLibrary.generated.h"

class UControlRigShapeLibrary;
class UMaterial;

USTRUCT(BlueprintType, meta = (DisplayName = "Shape"))
struct FControlRigShapeDefinition
{
	GENERATED_USTRUCT_BODY()

	FControlRigShapeDefinition()
	{
		ShapeName = TEXT("Default");
		StaticMesh = nullptr;
		Transform = FTransform::Identity;
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	FName ShapeName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Shape")
	FTransform Transform;

	mutable TWeakObjectPtr<UControlRigShapeLibrary> Library;
};

UCLASS(MinimalAPI, BlueprintType, meta = (DisplayName = "Control Rig Shape Library"))
class UControlRigShapeLibrary : public UObject
{
	GENERATED_BODY()

public:

	CONTROLRIG_API UControlRigShapeLibrary();

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	FControlRigShapeDefinition DefaultShape;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary", meta = (DisplayName = "Override Material"))
	TSoftObjectPtr<UMaterial> DefaultMaterial;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	TSoftObjectPtr<UMaterial> XRayMaterial;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	FName MaterialColorParameter;

	UPROPERTY(EditAnywhere, Category = "ShapeLibrary")
	TArray<FControlRigShapeDefinition> Shapes;

	CONTROLRIG_API const FControlRigShapeDefinition* GetShapeByName(const FName& InName, bool bUseDefaultIfNotFound = false) const;
	static CONTROLRIG_API const FControlRigShapeDefinition* GetShapeByName(const FName& InName, const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>& InShapeLibraries, const TMap<FString, FString>& InLibraryNameMap, bool bUseDefaultIfNotFound = true);
	static CONTROLRIG_API const FString GetShapeName(const UControlRigShapeLibrary* InShapeLibrary, bool bUseNameSpace, const TMap<FString, FString>& InLibraryNameMap, const FControlRigShapeDefinition& InShape);

#if WITH_EDITOR

	// UObject interface
	CONTROLRIG_API virtual void PostEditChangeChainProperty(struct FPropertyChangedChainEvent& PropertyChangedEvent) override;

#endif

private:

	const TArray<FName> GetUpdatedNameList(bool bReset = false);

	TArray<FName> NameList;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_6
#include "Materials/Material.h"
#endif
