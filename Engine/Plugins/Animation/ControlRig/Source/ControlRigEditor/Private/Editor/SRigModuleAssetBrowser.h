// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ContentBrowserDelegates.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"


class IControlRigBaseEditor;

class SRigModuleAssetBrowser : public SBox
{
public:
	SLATE_BEGIN_ARGS(SRigModuleAssetBrowser) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<IControlRigBaseEditor> InEditor);

	void RefreshView();
	
private:

	
	bool OnShouldFilterAsset(const struct FAssetData& AssetData);
	TSharedPtr<SWidget> OnGetAssetContextMenu(const TArray<FAssetData>& SelectedAssets) const;
	void OnAssetDoubleClicked(const FAssetData& AssetData);
	TSharedRef<SToolTip> CreateCustomAssetToolTip(FAssetData& AssetData);
	TSharedRef<SToolTip> CreateCustomAssetToolTipNewStyle(FAssetData& AssetData);

	/** Used to get the currently selected assets */
	FGetCurrentSelectionDelegate GetCurrentSelectionDelegate;
		
	/** editor controller */
	TWeakPtr<IControlRigBaseEditor> ControlRigEditor;

	/** the animation asset browser */
	TSharedPtr<SBox> AssetBrowserBox;


	friend class FControlRigBaseEditor;
};
