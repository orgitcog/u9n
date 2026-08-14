// Copyright Epic Games, Inc. All Rights Reserved.
#include "PoseSearch/Chooser/PoseSearchChooserColumn.h"
#include "ChooserIndexArray.h"
#include "ChooserPropertyAccess.h"
#include "IChooserParameterBool.h"
#include "IChooserParameterFloat.h"
#include "ChooserTrace.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "PoseSearch/PoseSearchDatabase.h"
#if WITH_EDITOR
#include "StructUtils/PropertyBag.h"
#endif


bool FPoseHistoryContextProperty::GetValue(FChooserEvaluationContext& Context, FPoseHistoryReference& OutResult) const
{
	const FPoseHistoryReference* Reference;
	if (Binding.GetValuePtr(Context, Reference))
	{
		OutResult = *Reference;
		return true;
	}
	return false;
}


FPoseSearchColumn::FPoseSearchColumn()
{
}

#if WITH_EDITOR
void FPoseSearchColumn::AutoPopulate(int32 RowIndex, UObject* OutputObject)
{
	if (RowValues.IsValidIndex(RowIndex))
	{
		RowValues[RowIndex].ResultAsset = Cast<UAnimationAsset>(OutputObject);
	}
}

bool FPoseSearchColumn::EditorTestFilter(int32 RowIndex) const
{
	// todo: not sure how to do editor display for pose search weights (might need to make a copy of the pose history struct)
	return true;
}


float FPoseSearchColumn::EditorTestCost(int32 RowIndex) const
{
	// todo: not sure how to do editor display for pose search weights (might need to make a copy of the pose history struct)
	return 0.0f;
}

#endif

bool FPoseSearchColumn::HasFilters() const
{
	return true;
}

void FPoseSearchColumn::Filter(FChooserEvaluationContext& Context, const FChooserIndexArray& IndexListIn, FChooserIndexArray& IndexListOut) const
{

	TArray<const UObject*, TInlineAllocator<128>> AssetsToSearch;
	for (const FChooserIndexArray::FIndexData& IndexData : IndexListIn)
	{
		AssetsToSearch.Add(RowValues[IndexData.Index].ResultAsset);
	}


	FPoseHistoryReference PoseHistoryReference;
	if (const FChooserParameterPoseHistoryBase* PoseHistoryParameter = InputValue.GetPtr<FChooserParameterPoseHistoryBase>())
	{
		if (PoseHistoryParameter->GetValue(Context, PoseHistoryReference))
		{
			if (const UE::PoseSearch::IPoseHistory* PoseHistory = PoseHistoryReference.PoseHistory.Get())
			{
				FPoseSearchContinuingProperties ContinuingProperties;
				FPoseSearchFutureProperties Future;

				const UE::PoseSearch::FSearchResult SearchResult = UPoseSearchLibrary::MotionMatch(
					MakeArrayView(&Context, 1),
					MakeArrayView(&UE::PoseSearch::DefaultRole, 1),
					MakeArrayView(&PoseHistory, 1),
					AssetsToSearch,
					ContinuingProperties,
					Future,
					FPoseSearchEvent());

				 if (const UE::PoseSearch::FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset())
				 {
				 	const UPoseSearchDatabase* Database = SearchResult.Database.Get();
				 	if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetDatabaseAnimationAsset<FPoseSearchDatabaseAnimationAssetBase>(*SearchIndexAsset))
				 	{
				 		const UObject* Result = DatabaseAnimationAssetBase->GetAnimationAsset();
				 		int Index;
				 		if (AssetsToSearch.Find(Result, Index))
				 		{
				 			IndexListOut.Push(IndexListIn[Index]);

				 			if (const FChooserParameterFloatBase* StartTime = OutputStartTime.GetPtr<FChooserParameterFloatBase>())
				 			{
				 				StartTime->SetValue(Context, SearchResult.AssetTime);
				 			}
				 			
				 			if (const FChooserParameterFloatBase* Cost = OutputCost.GetPtr<FChooserParameterFloatBase>())
                            {
								Cost->SetValue(Context, SearchResult.PoseCost);
                            }

				 			if (const FChooserParameterBoolBase* Mirror = OutputMirror.GetPtr<FChooserParameterBoolBase>())
				 			{
				 				Mirror->SetValue(Context, SearchIndexAsset->IsMirrored());
				 			}
				 		}
				 	}
				 }
			}
		}
	}

	if (IndexListOut.IsEmpty())
	{
		// if nothing passed the pose match, or if PoseHistory was not found, then ignore pose matching and just pass through everything
		IndexListOut = IndexListIn;

		// set some reasonable defaults for our other outputs in the passthrough  case
		if (const FChooserParameterFloatBase* StartTime = OutputStartTime.GetPtr<FChooserParameterFloatBase>())
		{
			StartTime->SetValue(Context, 0.0);
		}
		
		if (const FChooserParameterFloatBase* Cost = OutputCost.GetPtr<FChooserParameterFloatBase>())
		{
			// set an arbitrary "high" cost value so that cost threshold implementations can still wait for the pose search to find a good match
			Cost->SetValue(Context, 100.0);
		}

		if (const FChooserParameterBoolBase* Mirror = OutputMirror.GetPtr<FChooserParameterBoolBase>())
		{
			Mirror->SetValue(Context, false);
		}
	}
}

#if WITH_EDITOR
	void FPoseSearchColumn::AddToDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FText DisplayName = NSLOCTEXT("PoseSearchColumn","Pose Search", "Pose Search");
		FName PropertyName("RowData", ColumnIndex);
		FPropertyBagPropertyDesc PropertyDesc(PropertyName,  EPropertyBagPropertyType::Struct, FChooserPoseSearchRowData::StaticStruct());
		PropertyDesc.MetaData.Add(FPropertyBagPropertyDescMetaData("DisplayName", DisplayName.ToString()));
		PropertyBag.AddProperties({PropertyDesc});
		
		PropertyBag.SetValueStruct(PropertyName, RowValues[RowIndex]);
	}

	void FPoseSearchColumn::SetFromDetails(FInstancedPropertyBag& PropertyBag, int32 ColumnIndex, int32 RowIndex)
	{
		FName PropertyName("RowData", ColumnIndex);
		
   		TValueOrError<FStructView, EPropertyBagResult> Result = PropertyBag.GetValueStruct(PropertyName, FChooserPoseSearchRowData::StaticStruct());
		if (FStructView* StructView = Result.TryGetValue())
		{
			RowValues[RowIndex] = StructView->Get<FChooserPoseSearchRowData>();
		}
	}
#endif