// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <IHasContext.h>

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "ChooserPropertyAccess.h"
#include "PoseSearch/Chooser/ChooserParameterPoseHistoryBase.h"
#include "StructUtils/InstancedStruct.h"
#include "Serialization/MemoryReader.h"
#include "Templates/SubclassOf.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearchChooserColumn.generated.h"

#define UE_API POSESEARCH_API

USTRUCT(DisplayName = "Pose History Property Binding")
struct FPoseHistoryContextProperty :  public FChooserParameterPoseHistoryBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Meta = (BindingType = "FPoseHistoryReference", BindingAllowFunctions = "false", BindingColor = "StructPinTypeColor"), Category = "Binding")
	FChooserPropertyBinding Binding;
	
	UE_API virtual bool GetValue(FChooserEvaluationContext& Context, FPoseHistoryReference& OutResult) const override;
	virtual bool IsBound() const override
	{
		return Binding.IsBoundToRoot || !Binding.PropertyBindingChain.IsEmpty();
	}

	CHOOSER_PARAMETER_BOILERPLATE();
};


USTRUCT()
struct FChooserPoseSearchRowData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Data", Meta = (Tooltip = "The result animation asset for this row (filled by autopopulate from the result column)"));
	TObjectPtr<UAnimationAsset> ResultAsset;
};

USTRUCT(DisplayName = "Pose Match", Meta = (Experimental, Category = "Experimental", Tooltip = "This column filters out all assets except the one which is selected by motion matching query.  Results must be AnimationAssets with a PoseSearchBranchIn notify state.  It also outputs OutputStartTime to specify the frame which matched pose best.  To work as intended it must be placed last (furthest right) in the Chooser so that other filters are applied first."))
struct FPoseSearchColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	UE_API FPoseSearchColumn();
	
	// Pose History
	UPROPERTY(EditAnywhere, DisplayName = "Pose History", NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/PoseSearch.ChooserParameterPoseHistoryBase"), Category = "Data")
	FInstancedStruct InputValue;
	
	// Float output for the start time with the best matching pose
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterFloatBase"), Category = "Data")
	FInstancedStruct OutputStartTime;
	
	// Bool output for if asset should be mirrored
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterBoolBase"), Category = "Data")
	FInstancedStruct OutputMirror;

	// Float output for the cost of the selected pose
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterFloatBase"), Category = "Data")
	FInstancedStruct OutputCost;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Data, meta=(ToolTip="DefaultRowValue will be assigned to cells when new rows are created"));
	FChooserPoseSearchRowData DefaultRowValue;
#endif
	
	UPROPERTY()
	// array of results (cells for this column for each row in the table)
	// should match the length of the Results array 
	TArray<FChooserPoseSearchRowData> RowValues;

	UE_API virtual bool HasFilters() const override;
	UE_API virtual void Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const override;

#if WITH_EDITOR
	mutable double TestValue = 0.0;
	UE_API virtual bool EditorTestFilter(int32 RowIndex) const override;
	UE_API virtual float EditorTestCost(int32 RowIndex) const override;
	
	virtual void SetTestValue(TArrayView<const uint8> Value) override
	{
		FMemoryReaderView Reader(Value);
		Reader << TestValue;
	}

	UE_API virtual void AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;
	UE_API virtual void SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex) override;

	virtual bool AutoPopulates() const override { return true; }
	UE_API virtual void AutoPopulate(int32 RowIndex, UObject* OutputObject) override;
#endif
	
	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterPoseHistoryBase);
};

#undef UE_API
