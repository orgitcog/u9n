// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorFramework/AssetImportData.h"
#include "FbxAssetImportData.generated.h"

class UFbxSceneImportData;

UENUM(BlueprintType)
enum class ECoordinateSystemPolicy : uint8
{
	MatchUpForwardAxes UMETA(DisplayName = "Match Up and Forward Axes", Tooltip = "The Up and Front axes in the FBX are mapped to the Up and Forward axes in UEFN.\nAfter import, the model will have the same apparent orientation in UEFN as itn oes in the FBX"),
	MatchUpAxis UMETA(DisplayName = "Match Up Axis", Tooltip = "The Up axis in the FBX is mapped to the Up axis in UEFN.\nAfter import, the model will have the same apparent vertical axis in UEFN as it does in the FBX, but its Forward and Left orientations may not match the FBX."),
	KeepXYZAxes UMETA(DisplayName = "Keep XYZ Axes", Tooltip = "The X, Y, and Z axes in the FBX are mapped directly to UEFN's internal X, Y, and Z axes, only flipping the Y axis to change from right - handed to left - handed coordinates.\nThis applies the least change to the data, but is least likely to match UEFN's Left, Up, and Forward axis conventions."),
};


/**
 * Base class for import data and options used when importing any asset from FBX
 */
UCLASS(BlueprintType, config=EditorPerProjectUserSettings, HideCategories=Object, abstract, MinimalAPI)
class UFbxAssetImportData : public UAssetImportData
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FVector ImportTranslation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	FRotator ImportRotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category=Transform, meta=(ImportType="StaticMesh|SkeletalMesh|Animation", ImportCategory="Transform"))
	float ImportUniformScale;

	/** Whether to convert scene from FBX scene. */
	UPROPERTY(EditAnywhere, Transient, config, Category = Miscellaneous, meta = (EditCondition = "bUsingLUFCoordinateSysem", EditConditionHides,  ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Select strategy to map FBX coordinates system to UE coordinates system"))
	ECoordinateSystemPolicy CoordinateSystemPolicy = ECoordinateSystemPolicy::MatchUpForwardAxes;

	/** Whether to convert scene from FBX scene. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (EditCondition = "!bUsingLUFCoordinateSysem", EditConditionHides, ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE coordinate system"))
	bool bConvertScene;

	/** Whether to force the front axis to be align with X instead of -Y. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (EditCondition = "bConvertScene && !bUsingLUFCoordinateSysem", EditConditionHides, ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX coordinate system to UE coordinate system with front X axis instead of -Y"))
	bool bForceFrontXAxis;

	/** Whether to convert the scene from FBX unit to UE unit (centimeter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, config, Category = Miscellaneous, meta = (ImportType = "StaticMesh|SkeletalMesh|Animation", ImportCategory = "Miscellaneous", ToolTip = "Convert the scene from FBX unit to UE unit (centimeter)."))
	bool bConvertSceneUnit;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	bool bImportAsScene;

	/* Use by the reimport factory to answer CanReimport, if true only factory for scene reimport will return true */
	UPROPERTY()
	TObjectPtr<UFbxSceneImportData> FbxSceneImportDataReference;

	/* Use to enable or not the new UI */
	UPROPERTY(Transient, VisibleDefaultsOnly, config, Category = InternalOnly)
	bool bUsingLUFCoordinateSysem;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
};
