// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextAnimGraphWorkspaceAssetUserData.h"

#include "UncookedOnlyUtils.h"
#include "Graph/AnimNextAnimationGraph_EditorData.h"
#include "UObject/AssetRegistryTagsContext.h"

void UAnimNextAnimGraphWorkspaceAssetUserData::GetAssetRegistryTags(FAssetRegistryTagsContext Context) const
{
	Super::GetAssetRegistryTags(Context);

	FWorkspaceOutlinerItemExports Exports;
	{
		UAnimNextAnimationGraph* Asset = CastChecked<UAnimNextAnimationGraph>(GetOuter());
		const UAnimNextAnimationGraph_EditorData* EditorData = UE::AnimNext::UncookedOnly::FUtils::GetEditorData<UAnimNextAnimationGraph_EditorData>(Asset);
		{
			FWorkspaceOutlinerItemExport& RootAssetExport = Exports.Exports.Add_GetRef(FWorkspaceOutlinerItemExport(Asset->GetFName(), Asset));
			RootAssetExport.GetData().InitializeAsScriptStruct(FAnimNextAnimationGraphOutlinerData::StaticStruct());
			FAnimNextRigVMAssetOutlinerData& Data = RootAssetExport.GetData().GetMutable<FAnimNextRigVMAssetOutlinerData>();
			Data.SoftAssetPtr = Asset;
		}

		UE::AnimNext::UncookedOnly::FUtils::GetAssetOutlinerItems(EditorData, Exports, Context);
	}

	FString TagValue;
	FWorkspaceOutlinerItemExports::StaticStruct()->ExportText(TagValue, &Exports, nullptr, nullptr, PPF_None, nullptr);
	Context.AddTag(FAssetRegistryTag(UE::Workspace::ExportsWorkspaceItemsRegistryTag, TagValue, FAssetRegistryTag::TT_Hidden));
}

