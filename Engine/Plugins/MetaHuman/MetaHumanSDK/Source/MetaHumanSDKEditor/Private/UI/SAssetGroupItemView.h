// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

struct FMetaHumanAssetDescription;

namespace UE::MetaHuman
{

class SAssetGroupItemDetails;

DECLARE_DELEGATE_OneParam(FOnVerify, const TArray<TSharedRef<FMetaHumanAssetDescription>>&);
DECLARE_DELEGATE_OneParam(FOnPackage, const TArray<TSharedRef<FMetaHumanAssetDescription>>&);

// Widget to display details of an AssetGroup: name, icon, contents, verification report etc.
class SAssetGroupItemView final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAssetGroupItemView)
	{
	}
		SLATE_EVENT(FOnVerify, OnVerify)
		SLATE_EVENT(FOnPackage, OnPackage)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void SetItems(const TArray<TSharedRef<FMetaHumanAssetDescription>>& AssetDescriptions);

private:
	// UI Handlers
	FReply OnVerify() const;
	FReply OnPackage() const;
	int32 GetMainSwitcherIndex() const;
	bool IsPackageButtonEnabled() const;

	// Data
	TArray<TSharedRef<FMetaHumanAssetDescription>> CurrentAssetGroups;
	TSharedPtr<SAssetGroupItemDetails> ItemDetails;

	//Callbacks
	FOnVerify OnVerifyCallback;
	FOnPackage OnPackageCallback;
};
}
